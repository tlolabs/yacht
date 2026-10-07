#!/usr/bin/env python3
"""Enforce the shared Qt host's target and production-update isolation contracts."""
import argparse
import plistlib
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(value, message):
    if not value:
        raise SystemExit(message)


p = argparse.ArgumentParser()
p.add_argument('--bundle', type=Path)
a = p.parse_args()

cmake = (ROOT / 'platform/qt/CMakeLists.txt').read_text()
require('CMAKE_CXX_STANDARD 17' in cmake or 'cxx_std_17' in cmake, 'C++17 standard requirement missing')
require('find_package(Qt6' in cmake, 'Qt6 dependency missing')

for retired in [
    'platform/windows/YACHT',
    'platform/windows/Tests',
    'platform/linux/yacht.py',
    'platform/linux/test_ui.py',
    'platform/avalonia',
]:
    require(not (ROOT / retired).exists(), f'Retired UI path remains: {retired}')

# Check tracked sources and build inputs, not ignored local build caches or
# historical migration records. A clean checkout must never resolve .NET UI code.
tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
active_roots = ('platform/', 'crates/', 'bindings/', 'config/', '.github/', 'updater/', 'script/')
retired_suffixes = ('.axaml', '.csproj', '.sln', '.slnx', '.dll', '.deps.json')
text_suffixes = ('.swift', '.cpp', '.h', '.py', '.sh', '.ps1', '.yml', '.yaml', '.toml', '.json', '.props', '.targets', '.txt')
for relative in filter(None, tracked):
    lower = relative.lower()
    require('avalonia' not in lower, f'Tracked Avalonia artifact remains: {relative}')
    require(not lower.endswith(retired_suffixes), f'Tracked .NET UI artifact remains: {relative}')
    if (relative.startswith(active_roots) or relative in ('Cargo.toml', 'Cargo.lock', 'Package.swift', 'Package.resolved')) and lower.endswith(text_suffixes):
        if relative not in ('script/check_qt_contract.py', 'script/check_windows_runtime.py'):
            require('avalonia' not in (ROOT / relative).read_text(errors='ignore').lower(),
                    f'Active Avalonia reference remains: {relative}')

internal = (ROOT / 'script/package_qt_internal.sh').read_text()
require('dist/internal/' in internal and 'build/internal-artifacts/' in internal, 'Internal artifacts must have isolated output paths')
require('com.tlolabs.yacht.qt.internal' in internal, 'Internal bundle identity missing')

workflow = (ROOT / '.github/workflows/native-macos.yml').read_text().split('  qt-macos-internal:')[1]
require('contents: read' in workflow and 'actions/upload-artifact@' in workflow, 'Internal CI must only upload workflow artifacts')
require('path: release/' not in workflow and 'gh release' not in workflow, 'Internal CI must not publish production release assets')

if a.bundle:
    info = plistlib.loads((a.bundle / 'Contents/Info.plist').read_bytes())
    require(info['CFBundleIdentifier'] == 'com.tlolabs.yacht.qt.internal', 'Internal bundle ID drift')
    require(info.get('YACHTInternalReference') is True, 'Missing internal marker')
    require(not any(k.startswith('SU') for k in info), 'Internal host must not carry Sparkle metadata')
    require(not list(a.bundle.rglob('yacht-update*')), 'Internal host must not bundle the production updater')
    require(not list(a.bundle.rglob('Sparkle.framework')), 'Internal host must not bundle Sparkle')
    require((a.bundle / 'Contents/MacOS/YachtApp').is_file(), 'Shared presentation binary missing')

print('Shared Qt target and release-isolation contracts verified')
