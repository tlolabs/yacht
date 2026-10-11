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
cmake -S platform/qt -B build/qt-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/qt-linux --target YachtApp
cp build/qt-linux/YachtApp "$stage/usr/lib/yacht/"
cp target/release/yacht-update target/release/libyacht_ffi.so "$stage/usr/lib/yacht/"
python3 script/collect_qt_notices.py "$stage/usr/lib/yacht" --rid "linux-$label"
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
Depends: libc6 (>= 2.39), libqt6widgets6 (>= 6.4.0) | libqt6widgets6t64 (>= 6.4.0), libqt6gui6 (>= 6.4.0) | libqt6gui6t64 (>= 6.4.0), libqt6core6t64 (>= 6.4.0) | libqt6core6 (>= 6.4.0), libqt6network6 (>= 6.4.0) | libqt6network6t64 (>= 6.4.0), xdg-utils
Description: Yet Another CSV HTML Translator
 Shared Qt interface and shared Rust CLI for styled HTML tables.
CONTROL
dpkg-deb --build --root-owner-group "$stage" "release/YACHT-$version-linux-$label.deb"
tar -czf "release/YACHT-$version-linux-$label.tar.gz" -C "$stage/usr" .
(cd release && sha256sum "YACHT-$version-linux-$label.deb" "YACHT-$version-linux-$label.tar.gz" > "SHA256SUMS-linux-$label")
if [[ -n "${YACHT_GPG_KEY:-}" ]]; then gpg --batch --yes --local-user "$YACHT_GPG_KEY" --armor --detach-sign "release/SHA256SUMS-linux-$label"; fi
# Verify the staged runtime, permissions, metadata and actual CLI before upload.
"$stage/usr/bin/yacht" --help >/dev/null
YACHT_LIBRARY="$stage/usr/lib/yacht/libyacht_ffi.so" python3 script/test_native_binding.py "$stage/usr/lib/yacht/libyacht_ffi.so"
dpkg-deb --info "release/YACHT-$version-linux-$label.deb" >/dev/null
