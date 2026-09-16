# YACHT icon

Original vector artwork for this repository, covered by the root GPL-3.0 license.

The macOS app uses `YACHT.icon`, an editable Icon Composer document with four
SVG layers (table sail, teal sail, hull, and wave). Open it in Icon Composer to
edit the native background, Liquid Glass material, and Default, Dark, and Mono
appearances. The table grid uses transparent cuts so the background can show
through in every appearance. Xcode 26 compiles this document as the `YACHT` app
icon and generates the asset catalog and an ICNS fallback for older macOS.
Both Intel and Apple Silicon builds use the same layered source.

`YACHT.svg` is the flat vector master for Windows and Linux. `YACHT.png` is its
1024-pixel preview; `YACHT.ico` contains Windows sizes. `YACHT.icns` is retained
as a flat export, but is not copied into the macOS app.

Run `./script/generate_icons.sh` on macOS with librsvg and ImageMagick to
regenerate the flat exports. This does not overwrite the Icon Composer document.
