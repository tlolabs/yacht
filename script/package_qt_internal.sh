#!/usr/bin/env bash
# Internal reference output deliberately never enters release/ or production feeds.
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -sm)" == 'Darwin arm64' ]] || { echo 'Requires an Apple Silicon Mac' >&2; exit 1; }
version="$(python3 script/sync_version.py)"
cargo build --release --locked -p yacht-ffi --target aarch64-apple-darwin

app="$PWD/dist/internal/YACHT Qt Internal.app"
if pgrep -f "^$app/Contents/MacOS/YachtApp" >/dev/null; then echo 'Close the internal reference application before packaging' >&2; exit 1; fi
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" build/internal-artifacts

cmake -S platform/qt -B build/qt-internal -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/qt-internal --target YachtApp --config Release

cp build/qt-internal/YachtApp "$app/Contents/MacOS/"
cp target/aarch64-apple-darwin/release/libyacht_ffi.dylib "$app/Contents/MacOS/"
cp assets/icons/YACHT.icns LICENSE LICENSE-NOTICE.md THIRD_PARTY_NOTICES.md PRIVACY.md "$app/Contents/Resources/"

python3 - "$app" "$version" <<'PY'
import plistlib, sys
from pathlib import Path
p=Path(sys.argv[1])
(p/'Contents/Info.plist').write_bytes(plistlib.dumps(dict(
    CFBundleIdentifier='com.tlolabs.yacht.qt.internal',
    CFBundleName='YACHT Qt Internal',
    CFBundleDisplayName='YACHT Qt Internal',
    CFBundleExecutable='YachtApp',
    CFBundlePackageType='APPL',
    CFBundleShortVersionString=sys.argv[2],
    CFBundleVersion=sys.argv[2],
    CFBundleIconFile='YACHT',
    LSMinimumSystemVersion='15.0',
    NSHighResolutionCapable=True,
    YACHTInternalReference=True
)))
if (p/'Contents/MacOS/yacht-update').exists(): raise SystemExit('Internal application must not contain the production updater')
PY

python3 script/collect_qt_notices.py "$app/Contents/MacOS" --rid osx-arm64
mv "$app/Contents/MacOS/ThirdPartyLicenses" "$app/Contents/Resources/"
python3 script/check_qt_contract.py --bundle "$app"
# Ad-hoc signing is only for locally loading ARM64 executable code, never distribution trust.
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
ditto -c -k --keepParent "$app" build/internal-artifacts/YACHT-Qt-INTERNAL-macos-arm64.zip
printf '%s\n' "$app"
