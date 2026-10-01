#!/usr/bin/env python3
"""Stamp public updater policy into a staged bundle, before signing it."""
import argparse, base64, json, plistlib
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument("bundle", type=Path)
p.add_argument("--arch", choices=["x64", "arm64"], required=True)
p.add_argument("--production", action="store_true")
a = p.parse_args()
trust = json.loads((Path(__file__).resolve().parents[1] / "config/update-trust.json").read_text())
key = trust["sparkle_public_key"]
if key and len(base64.b64decode(key, validate=True)) != 32:
    raise SystemExit("Invalid Sparkle public key")
if a.production and (not key or not trust["macos_team_id"]):
    raise SystemExit("Production update signing identity/public key is not configured")
p = a.bundle / "Contents/Info.plist"
info = plistlib.loads(p.read_bytes())
info["SUFeedURL"] = f'https://github.com/{trust["repository"]}/releases/latest/download/appcast-macos-{a.arch}.xml'
if key:
    info["SUPublicEDKey"] = key
else:
    info.pop("SUPublicEDKey", None)
p.write_bytes(plistlib.dumps(info))
