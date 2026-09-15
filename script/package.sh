#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer; fi
xcodebuild -project Yacht.xcodeproj -scheme Yacht -configuration Release -derivedDataPath build -destination 'generic/platform=macOS' ARCHS='arm64 x86_64' ONLY_ACTIVE_ARCH=NO build -quiet
swift build -c release --arch arm64 --arch x86_64 --product yacht
CLI_DIR="$(swift build -c release --arch arm64 --arch x86_64 --show-bin-path)"
mkdir -p dist release/package
# Stage into a fresh task-owned directory to avoid retaining obsolete bundle files.
STAGING_DIR="$(mktemp -d "$ROOT_DIR/dist/yacht-package.XXXXXX")"
trap 'rm -rf "$STAGING_DIR"' EXIT
ditto 'build/Build/Products/Release/Y.A.C.H.T..app' "$STAGING_DIR/Y.A.C.H.T..app"
cp "$CLI_DIR/yacht" "$STAGING_DIR/yacht"
cp LICENSE README.md "$STAGING_DIR/"
SIGNING_IDENTITY="${SIGNING_IDENTITY:--}"
if [[ "$SIGNING_IDENTITY" == '-' ]]; then
  codesign --force --sign - "$STAGING_DIR/yacht"
  codesign --force --sign - "$STAGING_DIR/Y.A.C.H.T..app"
else
  codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$STAGING_DIR/yacht"
  codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$STAGING_DIR/Y.A.C.H.T..app"
fi
codesign --verify --deep --strict "$STAGING_DIR/Y.A.C.H.T..app"
lipo "$STAGING_DIR/Y.A.C.H.T..app/Contents/MacOS/YachtApp" -verify_arch arm64 x86_64
lipo "$STAGING_DIR/yacht" -verify_arch arm64 x86_64
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  [[ "$SIGNING_IDENTITY" != '-' ]] || { echo 'Notarization requires Developer ID signing.' >&2; exit 1; }
  ditto -c -k --keepParent "$STAGING_DIR/Y.A.C.H.T..app" "$STAGING_DIR/notarize.zip"
  xcrun notarytool submit "$STAGING_DIR/notarize.zip" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$STAGING_DIR/Y.A.C.H.T..app"
  xcrun stapler validate "$STAGING_DIR/Y.A.C.H.T..app"
  rm "$STAGING_DIR/notarize.zip"
fi
ditto "$STAGING_DIR/Y.A.C.H.T..app" 'dist/Y.A.C.H.T..app'
cp "$STAGING_DIR/yacht" dist/yacht
ln -s /Applications "$STAGING_DIR/Applications"
hdiutil create -volname 'Y.A.C.H.T.' -srcfolder "$STAGING_DIR" -ov -format UDZO 'release/Y.A.C.H.T.-macos-universal.dmg'
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  codesign --force --timestamp --sign "$SIGNING_IDENTITY" 'release/Y.A.C.H.T.-macos-universal.dmg'
  xcrun notarytool submit 'release/Y.A.C.H.T.-macos-universal.dmg' --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple 'release/Y.A.C.H.T.-macos-universal.dmg'
fi
ditto -c -k --keepParent 'dist/Y.A.C.H.T..app' 'release/Y.A.C.H.T.-macos-universal.zip'
cp dist/yacht release/yacht
(cd release && shasum -a 256 Y.A.C.H.T.-macos-universal.dmg Y.A.C.H.T.-macos-universal.zip yacht > SHA256SUMS)
echo 'Packaged universal macOS app and CLI in release/'
