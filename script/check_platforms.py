#!/usr/bin/env python3
"""Check build metadata against config/platforms.json without editing it."""

import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
minimum = json.loads((root / "config/platforms.json").read_text())


def require(path: str, pattern: str, expected: str) -> None:
    content = (root / path).read_text()
    values = re.findall(pattern, content, flags=re.MULTILINE)
    if not values or any(value != expected for value in values):
        raise SystemExit(f"{path}: expected {expected!r}, found {values!r}")


macos = minimum["macos_minimum"]
require("Package.swift", r"\.macOS\(\.v(\d+)\)", macos.split(".")[0])
require("Yacht.xcodeproj/project.pbxproj", r'"MACOSX_DEPLOYMENT_TARGET" = "([^"]+)"', macos)
require("platform/macos/Support/Info.plist", r"<key>LSMinimumSystemVersion</key><string>([^<]+)", macos)
windows = minimum["windows_minimum_build"]
require("platform/windows/YACHT/YACHT.csproj", r"<TargetPlatformMinVersion>([^<]+)", windows + ".0")
require("platform/windows/installer/YACHT.iss", r"^MinVersion=([^\n]+)", windows)
linux = minimum["linux_glibc_minimum"]
require("script/package_linux.sh", r"libc6 \(>= ([^)]+)\)", linux)
require("script/package_linux.sh", r"gir1\.2-gtk-4\.0 \(>= ([^)]+)\)", minimum["linux_gtk_minimum"])
require("script/package_linux.sh", r"gir1\.2-adw-1 \(>= ([^)]+)\)", minimum["linux_libadwaita_minimum"])
print("Platform minimum metadata matches config/platforms.json")
