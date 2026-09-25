#!/usr/bin/env python3
"""Copy license/notice files for the Windows packages used by a publish output."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
LICENSE_NAMES = {"license.txt", "notice.txt", "thirdpartynotices.txt", "third-party-notices.txt"}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def runtime_pack(cache: Path, publish: Path, arch: str) -> Path:
    coreclr = publish / "coreclr.dll"
    if not coreclr.exists():
        raise SystemExit("Self-contained publish is missing coreclr.dll")
    for candidate in (cache / f"microsoft.netcore.app.runtime.win-{arch}").glob("*"):
        if not candidate.is_dir():
            continue
        matches = list(candidate.rglob("coreclr.dll"))
        if any(digest(path) == digest(coreclr) for path in matches):
            return candidate
    raise SystemExit("Cannot match published coreclr.dll to an exact .NET runtime pack")


def sdk_projection_packs(cache: Path, publish: Path) -> list[Path]:
    package = cache / "microsoft.windows.sdk.net.ref"
    matched = []
    for name in ("Microsoft.Windows.SDK.NET.dll", "WinRT.Runtime.dll"):
        binary = publish / name
        if not binary.exists():
            continue
        versions = [
            candidate for candidate in sorted(package.iterdir())
            if candidate.is_dir() and any(digest(path) == digest(binary) for path in candidate.rglob(name))
        ] if package.is_dir() else []
        if not versions:
            raise SystemExit(f"Cannot match published {name} to an exact Windows SDK .NET package")
        for version in versions:
            if version not in matched:
                matched.append(version)
    return matched


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("publish", type=Path)
    parser.add_argument("--architecture", choices=("x64", "arm64"), required=True)
    parser.add_argument("--nuget-cache", type=Path, default=Path.home() / ".nuget/packages")
    args = parser.parse_args()
    publish = args.publish.resolve()
    cache = args.nuget_cache.expanduser().resolve()
    lock = json.loads((ROOT / "platform/windows/YACHT/packages.lock.json").read_text())
    packages = lock["dependencies"]["net8.0-windows10.0.19041"]
    selected = []
    for name, record in packages.items():
        lower = name.lower()
        if lower.startswith("microsoft.windowsappsdk") or lower in {
            "microsoft.windows.ai.machinelearning", "microsoft.windows.sdk.net.ref",
            "microsoft.web.webview2", "system.numerics.tensors",
        }:
            selected.append(cache / lower / record["resolved"])
    # Reference-projection DLLs are implicit SDK inputs, absent from the app lock.
    selected += sdk_projection_packs(cache, publish)
    selected.append(runtime_pack(cache, publish, args.architecture))

    destination = publish / "ThirdPartyLicenses"
    destination.mkdir(exist_ok=True)
    lines = ["# Windows package licenses and notices", "", "The named packages retain their own terms. These copies come from the exact NuGet packages used to build this distribution.", ""]
    for package in selected:
        if not package.is_dir():
            raise SystemExit(f"Required NuGet package missing: {package}")
        notices = [path for path in sorted(package.iterdir()) if path.is_file() and path.name.lower() in LICENSE_NAMES]
        package_destination = destination / package.parent.name / package.name
        package_destination.mkdir(parents=True, exist_ok=True)
        for source in notices:
            shutil.copy2(source, package_destination / source.name)
        links = ", ".join(f"[{path.name}]({package.parent.name}/{package.name}/{path.name})" for path in notices)
        if not links:
            links = "No local license/notice file in the package; inspect its NuGet license metadata."
        lines.append(f"- `{package.parent.name}` {package.name}: {links}")
    lines += ["", "The Microsoft Windows SDK .NET projection NuGet package links to the [Windows SDK license](https://download.microsoft.com/download/0/F/F/0FF2B061-47DD-4F55-89B6-FD1D8C44F14D/sdk_license.rtf) rather than carrying a local license file.", "See the [source audit](https://github.com/tlolabs/yacht/blob/main/docs/LICENSE_AUDIT.md) for exact file-level provenance.", ""]
    (destination / "README.md").write_text("\n".join(lines))
    print(f"Collected notices from {len(selected)} exact package directories")


if __name__ == "__main__":
    main()
