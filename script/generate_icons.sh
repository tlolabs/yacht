#!/usr/bin/env bash
# Regenerate flat exports from the SVG master; the macOS app uses YACHT.icon.
# Requires librsvg (rsvg-convert), ImageMagick (magick), and Apple's iconutil.
set -euo pipefail
cd "$(dirname "$0")/.."
for tool in rsvg-convert magick iconutil; do command -v "$tool" >/dev/null; done
icon_stage="$(mktemp -d)"
trap 'rm -rf "$icon_stage"' EXIT
mkdir "$icon_stage/YACHT.iconset"
for size in 16 32 128 256 512; do
  rsvg-convert -w "$size" -h "$size" assets/icons/YACHT.svg -o "$icon_stage/YACHT.iconset/icon_${size}x${size}.png"
  double=$((size * 2))
  rsvg-convert -w "$double" -h "$double" assets/icons/YACHT.svg -o "$icon_stage/YACHT.iconset/icon_${size}x${size}@2x.png"
done
cp "$icon_stage/YACHT.iconset/icon_512x512@2x.png" assets/icons/YACHT.png
iconutil -c icns "$icon_stage/YACHT.iconset" -o assets/icons/YACHT.icns
magick assets/icons/YACHT.png -define icon:auto-resize=256,128,64,48,32,24,16 assets/icons/YACHT.ico
