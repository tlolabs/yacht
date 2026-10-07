#!/usr/bin/env python3
"""Enforce the shared Qt host's target and production-update isolation contracts."""
import argparse
import plistlib
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
