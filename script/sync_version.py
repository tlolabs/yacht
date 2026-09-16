#!/usr/bin/env python3
"""Cargo workspace version is authoritative on every platform."""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
version = re.search(
    r'(?m)^version = "([0-9.]+)"', (root / "Cargo.toml").read_text()
).group(1)
p = root / "platform/macos/Support/Info.plist"
p.write_text(
    re.sub(
        r"(<key>CFBundleShortVersionString</key><string>)[^<]+",
        lambda m: m[1] + version,
        p.read_text(),
    )
)
(root / "platform/windows/Version.props").write_text(
    f"<Project><PropertyGroup><Version>{version}</Version></PropertyGroup></Project>\n"
)
p = root / "platform/windows/YACHT/app.manifest"
p.write_text(
    re.sub(
        r'(<assemblyIdentity version=")[^"]+',
        lambda m: m[1] + version + ".0",
        p.read_text(),
    )
)
print(version)
