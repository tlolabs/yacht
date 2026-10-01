#!/usr/bin/env python3
"""Collect exact locked NuGet metadata, licenses and native third-party notices."""
import argparse
import json
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / 'platform/avalonia/YACHT/packages.lock.json'

def packages():
    result = {}
    for dependencies in json.loads(LOCK.read_text())['dependencies'].values():
        for name, package in dependencies.items():
            result[(name.lower(), package['resolved'])] = package
    return result

def main():
    p = argparse.ArgumentParser()
    p.add_argument('publish', type=Path)
    p.add_argument('--rid', required=True, choices=['win-x64','win-arm64','linux-x64','linux-arm64','osx-arm64'])
    p.add_argument('--cache', type=Path, default=Path.home()/'.nuget/packages')
    a = p.parse_args()
    destination = a.publish/'ThirdPartyLicenses'
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    selected = list(packages())
    # Runtime pack version is identified from the published dependency manifest.
    deps = json.loads((a.publish/'YachtApp.deps.json').read_text())
    for name in deps['libraries']:
        if name.startswith('runtimepack.Microsoft.NETCore.App.Runtime.'):
            package, version = name.split('/')
            selected.append((package.removeprefix('runtimepack.').lower(), version))
    for name, version in sorted(set(selected)):
        folder = a.cache/name/version
        nuspec = next(folder.glob('*.nuspec'))
        meta = ET.parse(nuspec)
        license_node = next((n for n in meta.iter() if n.tag.split('}')[-1] == 'license'), None)
        if license_node is None:
            raise SystemExit(f'Missing license metadata: {name} {version}')
        license_text = license_node.text
        if license_node.attrib.get('type') == 'expression' and license_text not in ('MIT', 'Apache-2.0', 'BSD-3-Clause'):
            raise SystemExit(f'Unreviewed license: {name}: {license_text}')
        if license_node.attrib.get('type') == 'file' and name != 'avalonia.angle.windows.natives':
            raise SystemExit(f'Unreviewed license file: {name}')
        out = destination/name/version
        out.mkdir(parents=True, exist_ok=True)
        shutil.copy2(nuspec, out/nuspec.name)
        for item in folder.rglob('*'):
            if item.is_file() and any(s in item.name.lower() for s in ('license','licence','notice')) and item.suffix not in ('.dll','.pdb'):
                relative = item.relative_to(folder)
                (out/relative).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(item, out/relative)
        records.append({'name': name, 'version': version, 'license': license_text})
    # Upstreams whose NuGet package declares MIT without bundling its text.
    shutil.copytree(ROOT/'licenses/avalonia', destination/'upstream', dirs_exist_ok=True)
    (destination/'packages.json').write_text(json.dumps(records, indent=2)+'\n')
    print(f'Collected {len(records)} locked NuGet/runtime notices')

if __name__ == '__main__': main()
