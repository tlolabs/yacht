#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
# Local packaging never uploads or consumes ambient signing credentials.
# Signing/notarization require an explicit mode chosen by the caller.
PACKAGE_MODE="${1:---unsigned}"
case "$PACKAGE_MODE" in
  --unsigned) SIGNING_IDENTITY=-; NOTARY_PROFILE= ;;
  --sign) : "${SIGNING_IDENTITY:?Set SIGNING_IDENTITY for --sign}"; NOTARY_PROFILE= ;;
  --notarize) : "${SIGNING_IDENTITY:?Set SIGNING_IDENTITY for --notarize}"; : "${NOTARY_PROFILE:?Set NOTARY_PROFILE for --notarize}" ;;
  *) echo 'Usage: package.sh [--unsigned|--sign|--notarize]' >&2; exit 2 ;;
esac
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer; fi
python3 script/sync_version.py >/dev/null
xcodebuild -project Yacht.xcodeproj -scheme Yacht -configuration Release -derivedDataPath build -destination 'generic/platform=macOS' ARCHS='arm64 x86_64' ONLY_ACTIVE_ARCH=NO build -quiet
CLI_DIR="$ROOT_DIR/target/swift"
mkdir -p dist release/package
# Stage into a fresh task-owned directory to avoid retaining obsolete bundle files.
STAGING_DIR="$(mktemp -d "$ROOT_DIR/dist/yacht-package.XXXXXX")"
trap 'rm -rf "$STAGING_DIR"' EXIT
ditto 'build/Build/Products/Release/YACHT.app' "$STAGING_DIR/YACHT.app"
cp "$CLI_DIR/yacht" "$STAGING_DIR/yacht"
cp LICENSE README.md "$STAGING_DIR/"
SIGNING_IDENTITY="${SIGNING_IDENTITY:--}"
if [[ "$SIGNING_IDENTITY" == '-' ]]; then
  codesign --force --sign - "$STAGING_DIR/yacht"
  codesign --force --sign - "$STAGING_DIR/YACHT.app"
else
  codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$STAGING_DIR/yacht"
  codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$STAGING_DIR/YACHT.app"
fi
codesign --verify --deep --strict "$STAGING_DIR/YACHT.app"
lipo "$STAGING_DIR/YACHT.app/Contents/MacOS/YachtApp" -verify_arch arm64 x86_64
lipo "$STAGING_DIR/yacht" -verify_arch arm64 x86_64
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  [[ "$SIGNING_IDENTITY" != '-' ]] || { echo 'Notarization requires Developer ID signing.' >&2; exit 1; }
  ditto -c -k --keepParent "$STAGING_DIR/YACHT.app" "$STAGING_DIR/notarize.zip"
  xcrun notarytool submit "$STAGING_DIR/notarize.zip" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$STAGING_DIR/YACHT.app"
  xcrun stapler validate "$STAGING_DIR/YACHT.app"
  rm "$STAGING_DIR/notarize.zip"
fi
# dist contains generated artifacts only; replace the complete bundle so an
# earlier Debug build cannot leave unsigned auxiliary files in the release app.
rm -rf -- "$ROOT_DIR/dist/YACHT.app"
ditto "$STAGING_DIR/YACHT.app" 'dist/YACHT.app'
codesign --verify --deep --strict 'dist/YACHT.app'
cp "$STAGING_DIR/yacht" dist/yacht
ln -s /Applications "$STAGING_DIR/Applications"
hdiutil create -volname 'YACHT' -srcfolder "$STAGING_DIR" -ov -format UDZO 'release/YACHT-macos-universal.dmg'
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  codesign --force --timestamp --sign "$SIGNING_IDENTITY" 'release/YACHT-macos-universal.dmg'
  xcrun notarytool submit 'release/YACHT-macos-universal.dmg' --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple 'release/YACHT-macos-universal.dmg'
fi
ditto -c -k --keepParent 'dist/YACHT.app' 'release/YACHT-macos-universal.zip'
cp dist/yacht release/yacht
(cd release && shasum -a 256 YACHT-macos-universal.dmg YACHT-macos-universal.zip yacht > SHA256SUMS-macos)
echo 'Packaged universal macOS app and CLI in release/'
