#!/usr/bin/env python3
"""Match Windows ZIP files to local NuGet cache by hash for license review.

The output is evidence for a particular published artifact, not a legal conclusion.
Run against an exact release ZIP on a machine with the locked NuGet packages cached.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import zipfile
from xml.etree import ElementTree


def sha(data):
    return hashlib.sha256(data).hexdigest()


def classify(matches, project_license):
    if not matches:
        return "project", project_license
    package = matches[0].split("/")[0]
    if package.startswith("microsoft.netcore.app.runtime"):
        return "bundled .NET runtime", "MIT"
    if package == "microsoft.web.webview2":
        return "bundled WebView2 SDK/loader", "BSD-3-Clause"
    if package == "microsoft.windows.ai.machinelearning":
        return "bundled Windows ML runtime", "LicenseRef-Microsoft-Windows-ML"
    if package == "microsoft.windows.sdk.net.ref":
        return "bundled Windows SDK .NET projection", "LicenseRef-Microsoft-Windows-SDK"
    if package.startswith("microsoft.windowsappsdk"):
        return "bundled Windows App SDK runtime", "LicenseRef-Microsoft-WindowsAppSDK"
    if package == "system.numerics.tensors":
        return "bundled .NET library", "MIT"
    return "unclassified dependency", "NOASSERTION"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--nuget-cache", type=Path, default=Path.home() / ".nuget/packages")
    parser.add_argument("--extra-nupkg", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--project-license", default="GPL-3.0-only", help="Historical v2.1.1 default; use GPL-3.0-or-later for later releases")
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as archive:
        files = [entry for entry in archive.infolist() if not entry.is_dir()]
        sizes = {entry.file_size for entry in files}
        digest_map = defaultdict(list)
        for package_dir in args.nuget_cache.iterdir():
            if not package_dir.is_dir():
                continue
            for version_dir in package_dir.iterdir():
                if not version_dir.is_dir():
                    continue
                for source in version_dir.rglob("*"):
                    if not source.is_file():
                        continue
                    try:
                        if source.stat().st_size not in sizes:
                            continue
                        digest_map[sha(source.read_bytes())].append(
                            f"{package_dir.name}/{version_dir.name}/{source.relative_to(version_dir)}"
                        )
                    except OSError:
                        continue
        for nupkg in args.extra_nupkg:
            with zipfile.ZipFile(nupkg) as package:
                nuspec = next(name for name in package.namelist() if name.endswith(".nuspec"))
                metadata = ElementTree.fromstring(package.read(nuspec))
                package_id = next(node.text for node in metadata.iter() if node.tag.rsplit("}", 1)[-1] == "id")
                version = next(node.text for node in metadata.iter() if node.tag.rsplit("}", 1)[-1] == "version")
                for member in package.infolist():
                    if member.is_dir() or member.file_size not in sizes:
                        continue
                    digest_map[sha(package.read(member))].append(f"{package_id.lower()}/{version}/{member.filename}")
        records = []
        for entry in files:
            digest = sha(archive.read(entry))
            matches = sorted(digest_map.get(digest, []))
            role, license_id = classify(matches, args.project_license)
            records.append({"file": entry.filename, "size": entry.file_size, "sha256": digest, "nuget_matches": matches, "role": role, "license": license_id})
    result = {"archive": args.archive.name, "files": records}
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    origins = Counter((record["nuget_matches"][0].split("/")[0] if record["nuget_matches"] else "UNMATCHED") for record in records)
    for name, count in sorted(origins.items()):
        print(f"{name}: {count}")
    print("Unmatched:")
    for record in records:
        if not record["nuget_matches"]:
            print(record["file"])


if __name__ == "__main__":
    main()
