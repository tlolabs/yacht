#!/usr/bin/env python3
"""Publish one verified platform package without replacing release assets."""

import argparse
import hashlib
import json
import os
import plistlib
import re
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {"macos-arm64", "macos-x64", "windows-x64", "windows-arm64", "linux-x64", "linux-arm64"}
SHA_PATTERN = re.compile(r"[0-9a-f]{40}\Z")


def command(*args, check=True):
    return subprocess.run(args, cwd=ROOT, check=check, text=True, capture_output=True)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def version():
    return tomllib.loads((ROOT / "Cargo.toml").read_text())["workspace"]["package"]["version"]


def files_for(target, source_version):
    platform, arch = target.split("-")
    if platform == "macos":
        return [f"YACHT-macos-{arch}.zip", f"yacht-macos-{arch}"], f"SHA256SUMS-macos-{arch}"
    if platform == "windows":
        prefix = f"YACHT-{source_version}-windows-{arch}"
        return [f"{prefix}-setup.exe", f"{prefix}.zip"], f"SHA256SUMS-windows-{arch}"
    prefix = f"YACHT-{source_version}-linux-{arch}"
    return [f"{prefix}.deb", f"{prefix}.tar.gz"], f"SHA256SUMS-linux-{arch}"


def source_commit(source_sha):
    if not SHA_PATTERN.fullmatch(source_sha):
        raise ValueError("Invalid source SHA")
    commit = command("git", "rev-parse", f"{source_sha}^{{commit}}").stdout.strip()
    if commit != command("git", "rev-parse", "HEAD").stdout.strip():
        raise ValueError("Checked-out source differs from workflow source")
    return commit


def identity(ref, commit, source_version):
    if ref == "refs/heads/main":
        return f"dev-v{source_version}-{commit}", True
    tag = ref.removeprefix("refs/tags/")
    if ref.startswith("refs/tags/") and re.fullmatch(
        rf"v{re.escape(source_version)}(?:-(?:beta|rc)\.[0-9]+)?", tag
    ):
        return tag, tag != f"v{source_version}"
    raise ValueError(f"Ineligible release ref: {ref}")


def verify_checksum(directory, filenames, checksum_name):
    lines = (directory / checksum_name).read_text().splitlines()
    found = {}
    for line in lines:
        match = re.fullmatch(r"([0-9a-fA-F]{64})\s+\*?([^/\\]+)", line)
        if not match or match.group(2) in found:
            raise ValueError("Malformed or duplicate checksum entry")
        found[match.group(2)] = match.group(1).lower()
    if set(found) != set(filenames):
        raise ValueError("Checksum manifest has missing or extra files")
    for name in filenames:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0 or digest(path) != found[name]:
            raise ValueError(f"Invalid or corrupt package: {name}")


def verify_package(directory, target, source_version):
    filenames, checksum_name = files_for(target, source_version)
    verify_checksum(directory, filenames, checksum_name)
    platform = target.split("-")[0]
    archive = directory / next(name for name in filenames if name.endswith(".zip")) if platform != "linux" else None
    if archive:
        with zipfile.ZipFile(archive) as package:
            names = set(package.namelist())
            if not names or package.testzip():
                raise ValueError("Invalid ZIP package")
            if platform == "macos":
                prefix = "YACHT.app/Contents/"
                if not {prefix + "Info.plist", prefix + "MacOS/YachtApp"} <= names:
                    raise ValueError("Incomplete macOS application")
                info = plistlib.loads(package.read(prefix + "Info.plist"))
                if info.get("CFBundleShortVersionString") != source_version:
                    raise ValueError("macOS package version mismatch")
            elif not {"YachtApp.exe", "yacht.exe", "yacht_ffi.dll"} <= names:
                raise ValueError("Incomplete Windows application")
    else:
        with tarfile.open(directory / filenames[1], "r:gz") as package:
            names = set(package.getnames())
            if not any(name.endswith("/bin/yacht") for name in names) or not any(name.endswith("/lib/yacht/YachtApp") for name in names):
                raise ValueError("Incomplete Linux application")
        debian_arch = "amd64" if target == "linux-x64" else "arm64"
        metadata = command("dpkg-deb", "-f", str(directory / filenames[0]), "Version", "Architecture").stdout.splitlines()
        if metadata != [source_version, debian_arch]:
            raise ValueError("Linux package version or architecture mismatch")
    return filenames + [checksum_name]


def manifest(directory, target, commit, run_id, source_version):
    names = verify_package(directory, target, source_version)
    data = {"target": target, "source_commit": commit, "run_id": run_id,
            "version": source_version, "files": {name: digest(directory / name) for name in names}}
    (directory / f"publication-{target}.json").write_text(json.dumps(data, sort_keys=True) + "\n")
    return data


def verify_manifest(directory, target, commit, run_id, source_version):
    names = verify_package(directory, target, source_version)
    expected = {"target": target, "source_commit": commit, "run_id": run_id,
                "version": source_version, "files": {name: digest(directory / name) for name in names}}
    recorded = json.loads((directory / f"publication-{target}.json").read_text())
    if recorded != expected or {p.name for p in directory.iterdir()} != set(names) | {f"publication-{target}.json"}:
        raise ValueError("Artifact identity or file set differs from verified build")
    return names


def github_json(endpoint, *args, check=True):
    result = command("gh", "api", endpoint, *args, check=check)
    return json.loads(result.stdout) if result.returncode == 0 else None


def release_info(tag):
    return github_json(f"repos/{os.environ['GITHUB_REPOSITORY']}/releases/tags/{tag}", check=False)


def verify_tag(tag, commit, development):
    repo = os.environ["GITHUB_REPOSITORY"]
    endpoint = f"repos/{repo}/git/ref/tags/{tag}"
    ref = github_json(endpoint, check=False)
    if ref is None and development:
        github_json(f"repos/{repo}/git/refs", "-X", "POST", "-f", f"ref=refs/tags/{tag}", "-f", f"sha={commit}", check=False)
        ref = github_json(endpoint, check=False)
    if ref is None:
        raise RuntimeError(f"Release tag {tag} does not exist")
    obj = ref["object"]
    if obj["type"] == "tag":
        obj = github_json(f"repos/{repo}/git/tags/{obj['sha']}")["object"]
    if obj["type"] != "commit" or obj["sha"] != commit:
        raise RuntimeError(f"Release tag {tag} points to a different source commit")


def ensure_release(tag, commit, prerelease):
    repo = os.environ["GITHUB_REPOSITORY"]
    info = release_info(tag)
    if info is None:
        args = ["gh", "release", "create", tag, "--repo", repo, "--verify-tag", "--draft",
                "--title", tag, "--notes", f"Direct-download YACHT build from commit {commit}. Artifacts are unsigned or ad-hoc signed. Automatic signed updates are not included.",
                f"--latest={'false' if prerelease else 'true'}"]
        if prerelease:
            args.append("--prerelease")
        command(*args, check=False)
        info = release_info(tag)
    if info is None or info["prerelease"] != prerelease:
        raise RuntimeError(f"Release {tag} is missing or has the wrong channel")
    return info


def remote_digest(tag, name):
    with tempfile.TemporaryDirectory(prefix="yacht-release-") as temporary:
        result = command("gh", "release", "download", tag, "--repo", os.environ["GITHUB_REPOSITORY"],
                         "--pattern", name, "--dir", temporary, check=False)
        path = Path(temporary) / name
        return digest(path) if result.returncode == 0 and path.is_file() else None


def upload(tag, path):
    info = release_info(tag)
    existing = {asset["name"] for asset in info["assets"]}
    local_hash = digest(path)
    if path.name in existing:
        if remote_digest(tag, path.name) != local_hash:
            raise RuntimeError(f"Refusing to replace conflicting asset {path.name}")
        return
    command("gh", "release", "upload", tag, str(path), "--repo", os.environ["GITHUB_REPOSITORY"], check=False)
    if remote_digest(tag, path.name) != local_hash:
        raise RuntimeError(f"Uploaded asset {path.name} failed byte-for-byte verification")
    print(f"Verified {path.name}: sha256={local_hash}")


def publish(directory, target, source_sha, source_ref, run_id):
    if target not in TARGETS or not re.fullmatch(r"[0-9]+", run_id):
        raise ValueError("Invalid target or workflow run")
    commit = source_commit(source_sha)
    source_version = version()
    tag, prerelease = identity(source_ref, commit, source_version)
    names = verify_manifest(directory, target, commit, run_id, source_version)
    if source_ref.startswith("refs/tags/"):
        env = {**os.environ, "GH_REPO": os.environ["GITHUB_REPOSITORY"], "RELEASE_TAG": tag}
        subprocess.run(["python3", "script/check_release_tag.py"], cwd=ROOT, env=env, check=True)
    verify_tag(tag, commit, source_ref == "refs/heads/main")
    ensure_release(tag, commit, prerelease)
    for name in [names[-1], *names[:-1]]:
        upload(tag, directory / name)
    command("gh", "release", "edit", tag, "--repo", os.environ["GITHUB_REPOSITORY"], "--draft=false",
            f"--latest={'false' if prerelease else 'true'}")
    print(f"Published {target} in https://github.com/{os.environ['GITHUB_REPOSITORY']}/releases/tag/{tag}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "publish"])
    parser.add_argument("--target", required=True, choices=sorted(TARGETS))
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    try:
        commit = source_commit(args.source_sha)
        if args.mode == "prepare":
            manifest(args.directory, args.target, commit, args.run_id, version())
        else:
            publish(args.directory, args.target, args.source_sha, args.source_ref, args.run_id)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, json.JSONDecodeError, zipfile.BadZipFile, tarfile.TarError) as error:
        print(f"Release artifact error: {error}", file=sys.stderr)
        raise SystemExit(1)
