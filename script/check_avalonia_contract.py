#!/usr/bin/env python3
"""Enforce the shared host's target and production-update isolation contracts."""
import argparse
import json
import plistlib
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[1]
def require(value, message):
    if not value: raise SystemExit(message)
p = argparse.ArgumentParser()
p.add_argument('--bundle', type=Path)
a = p.parse_args()
project = ET.parse(ROOT/'platform/avalonia/YACHT/YACHT.csproj')
require(project.findtext('.//RuntimeIdentifiers') == 'win-x64;win-arm64;linux-x64;linux-arm64;osx-arm64', 'Shared target matrix drift')
require(project.findtext('.//TargetFramework') == 'net10.0', 'Shared runtime drift')
require(project.find(".//Target[@Name='RejectUnsupportedMac']") is not None, 'Missing unsupported Mac target gate')
for retired in ['platform/windows/YACHT','platform/windows/Tests','platform/linux/yacht.py','platform/linux/test_ui.py']:
    require(not (ROOT/retired).exists(), f'Retired UI path remains: {retired}')
lock = json.loads((ROOT/'platform/avalonia/YACHT/packages.lock.json').read_text())
for dependencies in lock['dependencies'].values():
    require(not any('windowsappsdk' in name.lower() for name in dependencies), 'Retired Windows App SDK dependency')
internal = (ROOT/'script/package_avalonia_internal.sh').read_text()
require('dist/internal/' in internal and 'build/internal-artifacts/' in internal, 'Internal artifacts must have isolated output paths')
require('com.tlolabs.yacht.avalonia.internal' in internal, 'Internal bundle identity missing')
workflow = (ROOT/'.github/workflows/native-macos.yml').read_text().split('  avalonia-macos-internal:')[1]
require('contents: read' in workflow and 'actions/upload-artifact@' in workflow, 'Internal CI must only upload workflow artifacts')
require('path: release/' not in workflow and 'gh release' not in workflow, 'Internal CI must not publish production release assets')
if a.bundle:
    info = plistlib.loads((a.bundle/'Contents/Info.plist').read_bytes())
    require(info['CFBundleIdentifier'] == 'com.tlolabs.yacht.avalonia.internal', 'Internal bundle ID drift')
    require(info.get('YACHTInternalReference') is True, 'Missing internal marker')
    require(not any(k.startswith('SU') for k in info), 'Internal host must not carry Sparkle metadata')
    require(not list(a.bundle.rglob('yacht-update*')), 'Internal host must not bundle the production updater')
    require(not list(a.bundle.rglob('Sparkle.framework')), 'Internal host must not bundle Sparkle')
    require((a.bundle/'Contents/MacOS/Avalonia.Controls.dll').is_file(), 'Shared presentation missing')
print('Shared Avalonia target and release-isolation contracts verified')
