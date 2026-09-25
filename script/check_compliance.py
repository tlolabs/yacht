#!/usr/bin/env python3
"""Fail only on structural project-license and documentation drift."""

from pathlib import Path
import tomllib
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "GPL-3.0-or-later"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


workspace = tomllib.loads((ROOT / "Cargo.toml").read_text())
require(workspace["workspace"]["package"]["license"] == EXPECTED, "Workspace SPDX declaration drift")
for manifest in sorted((ROOT / "crates").glob("*/Cargo.toml")):
    crate = tomllib.loads(manifest.read_text())
    require(crate["package"].get("license", {}).get("workspace") is True, f"{manifest}: license must inherit workspace")

appstream = ElementTree.parse(ROOT / "platform/linux/data/com.local.yacht.csvhtmltranslator.metainfo.xml")
require(appstream.findtext("project_license") == EXPECTED, "AppStream project license drift")
require("GNU GENERAL PUBLIC LICENSE" in (ROOT / "LICENSE").read_text(), "GPLv3 text missing")
require(EXPECTED in (ROOT / "LICENSE-NOTICE.md").read_text(), "Own-code license notice missing")

for path in ["THIRD_PARTY_NOTICES.md", "docs/LICENSE_AUDIT.md", "docs/DEPENDENCIES.md", "PRIVACY.md", "SECURITY.md", "SUPPORT.md", "CODE_SIGNING_POLICY.md"]:
    require((ROOT / path).is_file(), f"Required documentation missing: {path}")

print("Project license and required documentation are structurally consistent")
