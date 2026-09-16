#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
# Local packaging never uploads or consumes ambient signing credentials.
# Signing/notarization require an explicit mode chosen by the caller.
PACKAGE_MODE=--unsigned
PACKAGE_ARCH="$(uname -m)"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --unsigned|--sign|--notarize) PACKAGE_MODE="$1"; shift ;;
    --arch) [[ $# -ge 2 ]] || { echo '--arch requires x64 or arm64' >&2; exit 2; }; PACKAGE_ARCH="$2"; shift 2 ;;
    *) echo 'Usage: package.sh [--unsigned|--sign|--notarize] [--arch x64|arm64]' >&2; exit 2 ;;
  esac
done
case "$PACKAGE_ARCH" in
  x64|x86_64) PACKAGE_ARCH=x64; APPLE_ARCH=x86_64 ;;
  arm64) APPLE_ARCH=arm64 ;;
  *) echo 'Supported macOS architectures: x64, arm64' >&2; exit 2 ;;
esac
case "$PACKAGE_MODE" in
  --unsigned) SIGNING_IDENTITY=-; NOTARY_PROFILE= ;;
  --sign) : "${SIGNING_IDENTITY:?Set SIGNING_IDENTITY for --sign}"; NOTARY_PROFILE= ;;
  --notarize) : "${SIGNING_IDENTITY:?Set SIGNING_IDENTITY for --notarize}"; : "${NOTARY_PROFILE:?Set NOTARY_PROFILE for --notarize}" ;;
esac
BUILD_DIR="$ROOT_DIR/build/package-macos-$PACKAGE_ARCH"
DIST_DIR="$ROOT_DIR/dist/macos-$PACKAGE_ARCH"
ARTIFACT="YACHT-macos-$PACKAGE_ARCH"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer; fi
python3 script/sync_version.py >/dev/null
xcodebuild -project Yacht.xcodeproj -scheme Yacht -configuration Release -derivedDataPath "$BUILD_DIR" -destination 'generic/platform=macOS' ARCHS="$APPLE_ARCH" ONLY_ACTIVE_ARCH=NO build -quiet
CLI_DIR="$ROOT_DIR/target/swift"
mkdir -p "$DIST_DIR" release
# Stage into a fresh task-owned directory to avoid retaining obsolete bundle files.
STAGING_DIR="$(mktemp -d "$ROOT_DIR/dist/yacht-package.XXXXXX")"
trap 'rm -rf "$STAGING_DIR"' EXIT
ditto "$BUILD_DIR/Build/Products/Release/YACHT.app" "$STAGING_DIR/YACHT.app"
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
for binary in "$STAGING_DIR/YACHT.app/Contents/MacOS/YachtApp" "$STAGING_DIR/yacht"; do
  actual_arch="$(lipo -archs "$binary")"
  [[ "$actual_arch" == "$APPLE_ARCH" ]] || { echo "Expected only $APPLE_ARCH in $binary; found $actual_arch" >&2; exit 1; }
  codesign --verify --strict "$binary"
done
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
rm -rf -- "$DIST_DIR/YACHT.app"
ditto "$STAGING_DIR/YACHT.app" "$DIST_DIR/YACHT.app"
codesign --verify --deep --strict "$DIST_DIR/YACHT.app"
cp "$STAGING_DIR/yacht" "$DIST_DIR/yacht"
ln -s /Applications "$STAGING_DIR/Applications"
hdiutil create -volname 'YACHT' -srcfolder "$STAGING_DIR" -ov -format UDZO "release/$ARTIFACT.dmg"
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  codesign --force --timestamp --sign "$SIGNING_IDENTITY" "release/$ARTIFACT.dmg"
  xcrun notarytool submit "release/$ARTIFACT.dmg" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "release/$ARTIFACT.dmg"
fi
ditto -c -k --keepParent "$DIST_DIR/YACHT.app" "release/$ARTIFACT.zip"
cp "$DIST_DIR/yacht" "release/yacht-macos-$PACKAGE_ARCH"
(cd release && shasum -a 256 "$ARTIFACT.dmg" "$ARTIFACT.zip" "yacht-macos-$PACKAGE_ARCH" > "SHA256SUMS-macos-$PACKAGE_ARCH")
echo "Packaged macOS $PACKAGE_ARCH app and CLI in release/"
