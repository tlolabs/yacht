#!/usr/bin/env python3
"""Create/check a deterministic inventory from the committed dependency locks."""

import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs/dependency-inventory.json"


def cargo_packages():
    text = (ROOT / "Cargo.lock").read_text()
    for block in text.split("[[package]]")[1:]:
        fields = dict(re.findall(r'(?m)^(name|version|source) = "([^"]+)"', block))
        if not fields.get("source", "").startswith("registry+"):
            continue
        yield {
            "ecosystem": "cargo",
            "name": fields["name"],
            "version": fields["version"],
            "source": "crates.io",
            "license": "NOASSERTION",
        }


def nuget_packages():
    lock = json.loads((ROOT / "platform/windows/YACHT/packages.lock.json").read_text())
    framework = lock["dependencies"]["net8.0-windows10.0.19041"]
    for name, package in framework.items():
        yield {
            "ecosystem": "nuget",
            "name": name,
            "version": package["resolved"],
            "source": "nuget.org",
            "scope": package["type"].lower(),
            "license": "NOASSERTION",
        }


def build_inventory():
    packages = sorted([*cargo_packages(), *nuget_packages()], key=lambda p: (p["ecosystem"], p["name"].lower()))
    return {
        "schema": "tlo-labs-dependency-inventory/v1",
        "source_locks": ["Cargo.lock", "platform/windows/YACHT/packages.lock.json"],
        "license_note": "NOASSERTION means the lockfile does not record a license. Review upstream package licenses and release SBOM; see docs/DEPENDENCIES.md and docs/LICENSE_AUDIT.md.",
        "packages": packages,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = json.dumps(build_inventory(), indent=2, ensure_ascii=False) + "\n"
    if args.write:
        OUTPUT.write_text(data)
        print(f"Updated {OUTPUT.relative_to(ROOT)}")
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != data:
            raise SystemExit("Dependency inventory drift: run python3 script/dependency_inventory.py --write")
        print("Dependency inventory matches lockfiles")
    if not args.write and not args.check:
        print(data, end="")
