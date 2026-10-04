#!/usr/bin/env python3
"""Fast source contract gate; production mode fails closed on missing trust."""
import argparse, base64, json, plistlib, re, tomllib
from pathlib import Path
def require(condition, message):
    if not condition:
        raise SystemExit(message)

p = argparse.ArgumentParser()
p.add_argument("--production", action="store_true")
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
v = tomllib.loads((r / "Cargo.toml").read_text())["workspace"]["package"]["version"]
require(re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", v), "Invalid stable version")
info = plistlib.loads((r / "platform/macos/Support/Info.plist").read_bytes())
require(info["CFBundleShortVersionString"] == info["CFBundleVersion"] == v, "macOS version drift")
require(f"<Version>{v}</Version>" in (r / "platform/windows/Version.props").read_text(), "Windows version drift")
require(f'version="{v}.0"' in (r / "platform/windows/app.manifest").read_text(), "Windows assembly version drift")
for key in ["SURequireSignedFeed", "SUVerifyUpdateBeforeExtraction"]:
    require(info[key] is True, key)
require(info["SUSignedFeedFailureExpirationInterval"] == 0, 'Release requirement failed: info["SUSignedFeedFailureExpirationInterval"] == 0')
require(info["SUEnableSystemProfiling"] is False, 'Release requirement failed: info["SUEnableSystemProfiling"] is False')
trust = json.loads((r / "config/update-trust.json").read_text())
require(trust["application_id"] == "com.tlolabs.yacht", 'Release requirement failed: trust["application_id"] == "com.tlolabs.yacht"')
require(trust["repository"] == "tlolabs/yacht", 'Release requirement failed: trust["repository"] == "tlolabs/yacht"')
for key in trust["keys"].values():
    require(len(base64.b64decode(key, validate=True)) == 32, "Invalid Ed25519 key")
if a.production:
    require(trust["keys"], "Production manifest public keys are not configured")
    require(len(base64.b64decode(trust["sparkle_public_key"], validate=True)) == 32, "Sparkle public key missing")
    require(re.fullmatch(r"[A-Z0-9]{10}", trust["macos_team_id"]), "Developer ID team missing")
    require(trust["windows_publisher"], "Authenticode publisher missing")
    require(re.fullmatch(r"[A-F0-9]{40}|[A-F0-9]{64}", trust["linux_gpg_fingerprint"]), "Linux fingerprint missing")
print("Update source contract verified" + (" (production trust configured)" if a.production else " (production trust may be unconfigured)"))
