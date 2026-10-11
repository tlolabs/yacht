#!/usr/bin/env python3
"""Collect Qt metadata, licenses and third-party notices into the target distribution."""
import argparse
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('publish', type=Path)
    p.add_argument('--rid', required=True, choices=['win-x64', 'win-arm64', 'linux-x64', 'linux-arm64', 'osx-arm64'])
    a = p.parse_args()
    destination = a.publish / 'ThirdPartyLicenses'
    destination.mkdir(parents=True, exist_ok=True)
    records = [
        {
            "name": "qt6",
            "version": "6.8",
            "license": "LGPL-3.0-only OR GPL-3.0-only",
            "source": "https://www.qt.io/"
        }
    ]
    upstream = destination / 'upstream'
    upstream.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT / 'licenses/qt', upstream, dirs_exist_ok=True)
    (destination / 'packages.json').write_text(json.dumps(records, indent=2) + '\n')
    print(f'Collected Qt third-party notices for {a.rid}')


if __name__ == '__main__':
    main()
