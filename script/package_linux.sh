#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version="$(python3 script/sync_version.py)"
arch="$(dpkg --print-architecture)"
case "$arch" in amd64) label=x64;; arm64) label=arm64;; *) echo "Unsupported architecture: $arch" >&2; exit 1;; esac
cargo build --release --workspace --locked
mkdir -p release
stage="$(mktemp -d "$PWD/target/linux-package.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/usr/lib/yacht" "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/metainfo" "$stage/usr/share/doc/yacht" "$stage/DEBIAN"
dotnet restore platform/avalonia/YACHT/YACHT.csproj --locked-mode
dotnet publish platform/avalonia/YACHT/YACHT.csproj --no-restore -c Release -r "linux-$label" --self-contained true -o "$stage/usr/lib/yacht"
cp target/release/yacht-update target/release/libyacht_ffi.so "$stage/usr/lib/yacht/"
python3 script/collect_avalonia_notices.py "$stage/usr/lib/yacht" --rid "linux-$label"
cp target/release/yacht "$stage/usr/bin/"
cat > "$stage/usr/bin/yacht-gui" <<'LAUNCH'
#!/bin/sh
exec "$(dirname "$(readlink -f "$0")")/../lib/yacht/YachtApp" "$@"
LAUNCH
chmod +x "$stage/usr/bin/yacht-gui"
mkdir -p "$stage/usr/share/icons/hicolor/scalable/apps"
cp assets/icons/YACHT.svg "$stage/usr/share/icons/hicolor/scalable/apps/com.local.yacht.csvhtmltranslator.svg"
cp platform/linux/data/*.desktop "$stage/usr/share/applications/"
cp platform/linux/data/*.xml "$stage/usr/share/metainfo/"
cp LICENSE LICENSE-NOTICE.md README.md THIRD_PARTY_NOTICES.md PRIVACY.md docs/DEPENDENCIES.md "$stage/usr/share/doc/yacht/"
cat > "$stage/DEBIAN/control" <<CONTROL
Package: yacht
Version: $version
Architecture: $arch
Maintainer: Thomas Lothian <TBD>
Section: utils
Priority: optional
Depends: libc6 (>= 2.39), libicu74, libfontconfig1, libx11-6, libice6, libsm6, libgtk-3-0t64, libwebkit2gtk-4.1-0, libssl3t64
Description: Yet Another CSV HTML Translator
 Shared Avalonia interface and shared Rust CLI for styled HTML tables.
CONTROL
dpkg-deb --build --root-owner-group "$stage" "release/YACHT-$version-linux-$label.deb"
tar -czf "release/YACHT-$version-linux-$label.tar.gz" -C "$stage/usr" .
(cd release && sha256sum "YACHT-$version-linux-$label.deb" "YACHT-$version-linux-$label.tar.gz" > "SHA256SUMS-linux-$label")
if [[ -n "${YACHT_GPG_KEY:-}" ]]; then gpg --batch --yes --local-user "$YACHT_GPG_KEY" --armor --detach-sign "release/SHA256SUMS-linux-$label"; fi
# Verify the staged runtime, permissions, metadata and actual CLI before upload.
"$stage/usr/bin/yacht" --help >/dev/null
YACHT_LIBRARY="$stage/usr/lib/yacht/libyacht_ffi.so" python3 script/test_native_binding.py "$stage/usr/lib/yacht/libyacht_ffi.so"
dpkg-deb --info "release/YACHT-$version-linux-$label.deb" >/dev/null
