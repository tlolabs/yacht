#!/usr/bin/env python3
"""Check tag/version equality and GitHub's verified stable-tag signature."""

import json
import os
import re
import subprocess
import tomllib
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
tag = os.environ.get("RELEASE_TAG") or subprocess.check_output(["git", "describe", "--tags", "--exact-match"], cwd=ROOT, text=True).strip()
match = re.fullmatch(r"v(\d+\.\d+\.\d+)(?:-(beta|rc)\.(\d+))?", tag)
if not match:
    raise SystemExit(f"Invalid release tag: {tag}")
version = tomllib.loads((ROOT / "Cargo.toml").read_text())["workspace"]["package"]["version"]
if match.group(1) != version:
    raise SystemExit(f"Release tag {tag} does not match Cargo version {version}")

if match.group(2):
    print(f"Prerelease tag {tag} matches project version; production signing is prohibited")
    raise SystemExit(0)

repo = os.environ.get("GH_REPO")
token = os.environ.get("GH_TOKEN")
if not repo or not token:
    raise SystemExit("Stable tag verification requires GH_REPO and GH_TOKEN")


def github(path: str) -> dict:
    request = Request(
        f"https://api.github.com/repos/{repo}/{path}",
        headers={"Accept": "application/vnd.github+json", "Authorization": f"Bearer {token}", "X-GitHub-Api-Version": "2022-11-28"},
    )
    try:
        with urlopen(request, timeout=20) as response:
            return json.load(response)
    except HTTPError as error:
        raise SystemExit(f"GitHub tag verification request failed ({error.code})") from error


ref = github("git/ref/tags/" + quote(tag, safe=""))
if ref.get("object", {}).get("type") != "tag":
    raise SystemExit("Stable release requires a signed annotated tag")
tag_object = github("git/tags/" + ref["object"]["sha"])
verification = tag_object.get("verification", {})
if not verification.get("verified"):
    raise SystemExit(f"Stable release tag is unverified: {verification.get('reason', 'unknown')}")
if tag_object.get("object", {}).get("sha") != subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip():
    raise SystemExit("Stable release tag target differs from checked-out commit")

print(f"Stable release tag {tag} has a verified signature and matches the project version")
