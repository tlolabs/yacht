#!/usr/bin/env python3
"""Check tag/version equality and stable release source provenance."""

import argparse
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stable-only", action="store_true", help="Reject beta, rc, and prerelease tags")
    parser.add_argument("--require-clean", action="store_true", help="Require clean working directory without tracked modifications")
    parser.add_argument("--require-remote", action="store_true", help="Require GitHub API remote tag verification")
    args = parser.parse_args()

    tag = os.environ.get("RELEASE_TAG")
    if not tag:
        try:
            tag = subprocess.check_output(["git", "describe", "--tags", "--exact-match"], cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            raise SystemExit("No exact git release tag found on HEAD and RELEASE_TAG is not set")

    match = re.fullmatch(r"v(\d+\.\d+\.\d+)(?:-(beta|rc)\.(\d+))?", tag)
    if not match:
        raise SystemExit(f"Invalid release tag: {tag}")
    version = tomllib.loads((ROOT / "Cargo.toml").read_text())["workspace"]["package"]["version"]
    if match.group(1) != version:
        raise SystemExit(f"Release tag {tag} does not match Cargo version {version}")

    if match.group(2):
        if args.stable_only:
            raise SystemExit(f"Prerelease tag {tag} is not authorized for production signing")
        print(f"Prerelease tag {tag} matches project version; production signing is prohibited")
        raise SystemExit(0)

    if args.require_clean:
        try:
            tracked_diff = subprocess.check_output(["git", "diff", "--name-only", "HEAD"], cwd=ROOT, text=True).strip()
            if tracked_diff:
                raise SystemExit(f"Working tree has uncommitted modifications; production signing requires a clean, source-bound build:\n{tracked_diff}")
        except FileNotFoundError:
            pass

    # Local commit verification if git is available
    try:
        tag_commit = subprocess.check_output(["git", "rev-list", "-n", "1", tag], cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
        head_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        if tag_commit and tag_commit != head_commit:
            raise SystemExit(f"Stable release tag target ({tag_commit}) differs from checked-out commit ({head_commit})")
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    repo = os.environ.get("GH_REPO")
    token = os.environ.get("GH_TOKEN")
    if args.require_remote and (not repo or not token):
        raise SystemExit("Stable tag verification requires GH_REPO and GH_TOKEN")

    if repo and token:
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
            raise SystemExit("Stable release requires an annotated tag")
        tag_object = github("git/tags/" + ref["object"]["sha"])
        if tag_object.get("object", {}).get("type") != "commit":
            raise SystemExit("Stable tag must point directly to a commit")
        if tag_object.get("object", {}).get("sha") != subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip():
            raise SystemExit("Stable release tag target differs from checked-out commit")

    subprocess.run(["python3", str(ROOT / "script/check_update_contract.py")], check=True)
    print(f"Stable release tag {tag} matches the project version and checked-out commit")


if __name__ == "__main__":
    main()
