#!/usr/bin/env python3
"""Verify final notarized ZIP identity/security and emit digest-bound release evidence."""
import argparse
import hashlib
import json
import plistlib
import re
import subprocess
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def run(*args):
    return subprocess.run([str(x) for x in args], check=True, capture_output=True, text=True)


def verify_bundle(app, trust, version, arch):
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    require(not info.get("YACHTInternalReference") and not list(app.rglob("*Qt*")), "Internal Qt builds cannot enter the production macOS update channel")
    require(info["CFBundleIdentifier"] == trust["application_id"], "Application identity mismatch")
    require(info["CFBundleShortVersionString"] == info["CFBundleVersion"] == version, "Packaged version mismatch")
    require(info["SUPublicEDKey"] == trust["sparkle_public_key"], "Sparkle key mismatch")
    require(info["SURequireSignedFeed"] is True and info["SUVerifyUpdateBeforeExtraction"] is True, "Signature verification must be enabled")
    require(info["SUSignedFeedFailureExpirationInterval"] == 0, "Signed-feed expiry fallback is prohibited")
    require(info["SUFeedURL"] == f'https://github.com/{trust["repository"]}/releases/latest/download/appcast-macos-{arch}.xml', "Feed mismatch")
    # Verify Apple Developer ID certificate class, bundle ID and the exact team;
    # a printable Authority line alone is not a designated requirement.
    require(re.fullmatch(r"[A-Z0-9]{10}", trust["macos_team_id"]), "Invalid team ID")
    require(re.fullmatch(r"[A-Za-z0-9.-]+", trust["application_id"]), "Invalid application ID")
    requirement = (f'anchor apple generic and identifier "{trust["application_id"]}" '
                   'and certificate 1[field.1.2.840.113635.100.6.2.6] exists '
                   'and certificate leaf[field.1.2.840.113635.100.6.1.13] exists '
                   f'and certificate leaf[subject.OU] = "{trust["macos_team_id"]}"')
    run("codesign", "--verify", "--deep", "--strict", "-R", requirement, app)
    signature = run("codesign", "-dv", "--verbose=4", app).stderr
    require(f'TeamIdentifier={trust["macos_team_id"]}' in signature.splitlines(), "Team mismatch")
    require(re.search(r"flags=0x[0-9a-fA-F]+\([^\n)]*\bruntime\b[^\n)]*\)", signature), "Hardened runtime missing")
    binary = app / "Contents/MacOS" / info["CFBundleExecutable"]
    require(run("lipo", "-archs", binary).stdout.strip() == {"x64": "x86_64", "arm64": "arm64"}[arch], "Architecture mismatch")
    require((app / "Contents/Frameworks/Sparkle.framework").is_dir(), "Embedded Sparkle framework missing")
    run("xcrun", "stapler", "validate", app)
    run("spctl", "--assess", "--type", "execute", app)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("artifact", type=Path)
    p.add_argument("--arch", choices=["x64", "arm64"], required=True)
    a = p.parse_args()
    trust = json.loads((ROOT / "config/update-trust.json").read_text())
    version = tomllib.loads((ROOT / "Cargo.toml").read_text())["workspace"]["package"]["version"]
    # A failed re-verification must not leave an older positive receipt behind.
    output = a.artifact.with_name(a.artifact.name + ".verification.json")
    output.unlink(missing_ok=True)
    require(trust["macos_team_id"] and trust["sparkle_public_key"], "Pinned production Mac identity/key is required")
    with tempfile.TemporaryDirectory(prefix="yacht-native-verification-") as temporary:
        run("ditto", "-x", "-k", a.artifact, temporary)
        verify_bundle(Path(temporary) / "YACHT.app", trust, version, a.arch)
    with a.artifact.open("rb") as f:
        digest = hashlib.file_digest(f, "sha256").hexdigest()
    evidence = dict(application_id=trust["application_id"], version=version, platform="macos", arch=a.arch, sha256=digest, native_identity=trust["macos_team_id"], native_verified=True)
    output.write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Verified notarized package: {output}")


if __name__ == "__main__":
    main()
