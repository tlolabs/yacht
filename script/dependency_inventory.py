#!/usr/bin/env python3
"""Create/check a deterministic inventory and SPDX release SBOM from the committed dependency locks."""

import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs/dependency-inventory.json"
SBOM_OUTPUT = ROOT / "docs/sbom.spdx.json"


def get_cargo_license_map():
    licenses = {}
    try:
        out = subprocess.check_output(
            ["cargo", "metadata", "--format-version", "1"],
            cwd=str(ROOT),
            stderr=subprocess.DEVNULL,
        )
        meta = json.loads(out)
        for pkg in meta.get("packages", []):
            name = pkg.get("name")
            ver = pkg.get("version")
            lic = pkg.get("license") or "NOASSERTION"
            licenses[(name, ver)] = lic
    except Exception:
        pass
    return licenses


def cargo_packages(license_map):
    text = (ROOT / "Cargo.lock").read_text()
    for block in text.split("[[package]]")[1:]:
        fields = dict(re.findall(r'(?m)^(name|version|source) = "([^"]+)"', block))
        if not fields.get("source", "").startswith("registry+"):
            continue
        name = fields["name"]
        version = fields["version"]
        lic = license_map.get((name, version), "NOASSERTION")
        yield {
            "ecosystem": "cargo",
            "name": name,
            "version": version,
            "source": "crates.io",
            "license": lic,
        }


SWIFT_LOCK = "Yacht.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved"


def swift_packages():
    lock = json.loads((ROOT / SWIFT_LOCK).read_text())
    package_lock = json.loads((ROOT / "Package.resolved").read_text())
    if package_lock["pins"] != lock["pins"]:
        raise SystemExit("SwiftPM and Xcode dependency locks disagree")
    for pin in lock["pins"]:
        yield {
            "ecosystem": "swiftpm",
            "name": "Sparkle" if pin["identity"] == "sparkle" else pin["identity"],
            "version": pin["state"]["version"],
            "revision": pin["state"]["revision"],
            "source": pin["location"],
            "license": "MIT" if pin["identity"] == "sparkle" else "NOASSERTION",
        }


def build_inventory():
    license_map = get_cargo_license_map()
    packages = sorted([*cargo_packages(license_map), *swift_packages()], key=lambda p: (p["ecosystem"], p["name"].lower()))
    return {
        "schema": "tlo-labs-dependency-inventory/v1",
        "source_locks": ["Cargo.lock", "Package.resolved", SWIFT_LOCK],
        "license_note": "Registry packages record declared SPDX license expressions from upstream Cargo/Swift manifests. Review release SBOM; see docs/DEPENDENCIES.md and docs/LICENSE_AUDIT.md.",
        "packages": packages,
    }


def build_spdx_sbom(inventory, version="2.1.2"):
    spdx_packages = [
        {
            "SPDXID": "SPDXRef-Package-YACHT",
            "name": "yacht",
            "versionInfo": version,
            "downloadLocation": "https://github.com/tlolabs/yacht",
            "licenseConcluded": "GPL-3.0-or-later",
            "licenseDeclared": "GPL-3.0-or-later",
            "filesAnalyzed": False,
            "supplier": "Organization: TLO Labs",
        }
    ]
    relationships = []
    for idx, p in enumerate(inventory["packages"], start=1):
        clean_name = re.sub(r"[^a-zA-Z0-9.-]", "-", p["name"])
        clean_ver = re.sub(r"[^a-zA-Z0-9.-]", "-", p["version"])
        pkg_id = f"SPDXRef-Package-{p['ecosystem']}-{clean_name}-{clean_ver}"
        lic = p.get("license", "NOASSERTION")
        spdx_packages.append({
            "SPDXID": pkg_id,
            "name": p["name"],
            "versionInfo": p["version"],
            "downloadLocation": p.get("source", "NOASSERTION"),
            "licenseConcluded": lic if lic != "NOASSERTION" else "NOASSERTION",
            "licenseDeclared": lic if lic != "NOASSERTION" else "NOASSERTION",
            "filesAnalyzed": False,
        })
        relationships.append({
            "spdxElementId": "SPDXRef-Package-YACHT",
            "relationshipType": "DEPENDS_ON",
            "relatedSpdxElement": pkg_id,
        })

    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"YACHT-{version}",
        "documentNamespace": f"https://github.com/tlolabs/yacht/spdx/v{version}",
        "creationInfo": {
            "creators": ["Tool: tlo-labs-dependency-inventory/v1"],
            "created": "2026-10-14T00:00:00Z",
        },
        "packages": spdx_packages,
        "relationships": relationships,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--sbom", action="store_true")
    args = parser.parse_args()
    inv = build_inventory()
    data = json.dumps(inv, indent=2, ensure_ascii=False) + "\n"
    sbom_data = json.dumps(build_spdx_sbom(inv), indent=2, ensure_ascii=False) + "\n"

    if args.write:
        OUTPUT.write_text(data)
        SBOM_OUTPUT.write_text(sbom_data)
        print(f"Updated {OUTPUT.relative_to(ROOT)} and {SBOM_OUTPUT.relative_to(ROOT)}")
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != data:
            raise SystemExit("Dependency inventory drift: run python3 script/dependency_inventory.py --write")
        if not SBOM_OUTPUT.exists() or SBOM_OUTPUT.read_text() != sbom_data:
            raise SystemExit("SPDX release SBOM drift: run python3 script/dependency_inventory.py --write")
        print("Dependency inventory and release SBOM match lockfiles")
    if not args.write and not args.check:
        if args.sbom:
            print(sbom_data, end="")
        else:
            print(data, end="")
