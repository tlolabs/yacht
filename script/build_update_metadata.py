#!/usr/bin/env python3
"""Create and verify update metadata for already signed/verified production artifacts.
Does not upload, mark latest, install, or claim native installation qualification.
"""
import argparse, datetime, hashlib, json, os, plistlib, shutil, subprocess, tempfile, tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def run(*args, **kw):
    return subprocess.run([str(a) for a in args], check=True, **kw)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("assets", type=Path)
    p.add_argument("--sparkle-tools", type=Path, required=True)
    a = p.parse_args()
    assets = a.assets.resolve()
    run("python3", ROOT / "script/check_update_contract.py", "--production")
    # Check release authorization before using either private signing key.
    run("python3", ROOT / "script/check_release_tag.py")
    trust = json.loads((ROOT / "config/update-trust.json").read_text())
    version = tomllib.loads((ROOT / "Cargo.toml").read_text())["workspace"]["package"]["version"]
    tag = "v" + version
    if os.environ.get("RELEASE_TAG") != tag:
        raise SystemExit("RELEASE_TAG must equal the authoritative stable version")
    base = f'https://github.com/{trust["repository"]}/releases'
    now = int(datetime.datetime.now(datetime.timezone.utc).timestamp())
    manifest = dict(schema=1, application_id=trust["application_id"], repository=trust["repository"], channel="stable", version=version, tag=tag, draft=False, prerelease=False, published_at=now, expires_at=now+180*86400, notes_url=f"{base}/tag/{tag}", restart_required=True, migration="none", assets=[])
    # Native verification reports must come from the same CI run and bind the
    # final packaged bytes. They are evidence inputs, not a signature substitute.
    for platform, minimum, kind, identity in [("macos", "14.0.0", "sparkle-zip", trust["macos_team_id"]), ("windows", "10.0.17763", "inno-setup", trust["windows_publisher"]), ("linux", "2.39.0", "appimage", trust["linux_gpg_fingerprint"])]:
        for arch in ["x64", "arm64"]:
            filename = f"YACHT-macos-{arch}.zip" if platform == "macos" else f"YACHT-{version}-{platform}-{arch}" + ("-setup.exe" if platform == "windows" else ".AppImage")
            artifact = assets / filename
            with artifact.open("rb") as f:
                digest = hashlib.file_digest(f, "sha256").hexdigest()
            evidence = json.loads((assets / (filename + ".verification.json")).read_text())
            expected = dict(application_id=trust["application_id"], version=version, platform=platform, arch=arch, sha256=digest, native_identity=identity, native_verified=True)
            if any(evidence.get(k) != v for k, v in expected.items()):
                raise SystemExit(f"Native verification evidence mismatch: {filename}")
            manifest["assets"].append(dict(platform=platform, arch=arch, min_os=minimum, format=kind, filename=filename, url=f"{base}/download/{tag}/{filename}", size=artifact.stat().st_size, sha256=digest, native_identity=identity))
    with tempfile.TemporaryDirectory(prefix="yacht-update-metadata-") as temporary:
        stage = Path(temporary)
        # Sparkle handles archive, appcast and release-note signing itself.
        for arch in ["x64", "arm64"]:
            folder = stage / arch
            folder.mkdir()
            filename = f"YACHT-macos-{arch}.zip"
            shutil.copy2(assets / filename, folder / filename)
            key = os.environ["SPARKLE_PRIVATE_KEY"]
            run(a.sparkle_tools / "generate_appcast", "--ed-key-file", "-", "--maximum-deltas", "0", "--download-url-prefix", f"{base}/download/{tag}/", "--full-release-notes-url", f"{base}/tag/{tag}", folder, input=key+"\n", text=True)
            feed = folder / f"appcast-macos-{arch}.xml"
            if not feed.is_file():
                raise SystemExit("Sparkle feed filename mismatch; check SUFeedURL in packaged app")
            run(a.sparkle_tools / "sign_update", "--verify", "--ed-key-file", "-", feed, input=key+"\n", text=True)
            shutil.copy2(feed, assets / feed.name)
        spec = stage / "manifest.json"
        spec.write_text(json.dumps(manifest))
        run("cargo", "run", "--locked", "-p", "tlo-updater", "--bin", "tlo-release", "--", "sign", spec, assets, ROOT / "config/update-trust.json", version, cwd=ROOT)
        run("cargo", "run", "--locked", "-p", "tlo-updater", "--bin", "tlo-release", "--", "verify", assets / "tlo-update.json", assets, ROOT / "config/update-trust.json", version, cwd=ROOT)
    print("Metadata prepared. Publish only with verified native artifacts and complete platform qualification evidence.")

if __name__ == "__main__":
    main()
