#!/usr/bin/env python3
"""Check tag/version equality and GitHub's verified stable-tag signature."""

import json
import os
import re
import subprocess
import tempfile
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
if tag_object.get("object", {}).get("type") != "commit":
    raise SystemExit("Stable tag must point directly to a commit")
if tag_object.get("object", {}).get("sha") != subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip():
    raise SystemExit("Stable release tag target differs from checked-out commit")

# GitHub's "verified" flag accepts any recognized signer. Require the
# repository's pinned maintainer key as well, in an isolated public keyring.
with tempfile.TemporaryDirectory(prefix="yacht-tag-verification-") as home:
    env = dict(os.environ, GNUPGHOME=home)
    subprocess.run(["gpg", "--batch", "--import", str(ROOT / "config/release-maintainer.asc")], env=env, check=True, capture_output=True)
    result = subprocess.run(["git", "-c", "gpg.format=openpgp", "-c", "gpg.program=gpg", "verify-tag", "--raw", tag], cwd=ROOT, env=env, capture_output=True, text=True, check=True)
    valid = [line.split() for line in result.stderr.splitlines() if line.startswith("[GNUPG:] VALIDSIG ")]
    fingerprint = "F7E74ED98DB485D03F2565B96B68B73FE752FD16"
    if not any(fields[2] == fingerprint or fields[-1] == fingerprint for fields in valid):
        raise SystemExit("Stable tag was not signed by the pinned release maintainer")

subprocess.run(["python3", str(ROOT / "script/check_update_contract.py"), "--production"], check=True)
print(f"Stable release tag {tag} has a verified signature and matches the project version")
