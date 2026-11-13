"""Read-only release prerequisites shared by selection and publication.

An owner-published, matching-release Mac archive is the local qualification signal,
not an assertion that CI independently verified Apple signing/notarization.
"""
import json
from pathlib import Path
import re

POLICY = Path(__file__).resolve().parents[1] / 'weekly-release-holds.json'


def load_holds(path=POLICY):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate hold policy key')
            result[key] = value
        return result
    value = json.loads(Path(path).read_text(), object_pairs_hook=unique)
    if (not isinstance(value, dict) or set(value) != {'schema', 'held_tags'}
            or type(value['schema']) is not int or value['schema'] != 1
            or not isinstance(value['held_tags'], dict)):
        raise ValueError('Invalid weekly release hold policy')
    for tag, reason in value['held_tags'].items():
        if not re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+(?:-(?:beta|rc)\.[0-9]+)?', tag) or not isinstance(reason, str) or not reason.strip():
            raise ValueError('Invalid held tag or missing hold reason')
    return value['held_tags']


def release_reason(tag, info, assets, holds):
    """Return a skip reason, or None when all publication prerequisites hold."""
    if tag in holds:
        return 'held: ' + holds[tag]
    if info is None:
        return 'no existing release'
    if (info.get('tag_name') != tag or info.get('draft') is not False
            or info.get('prerelease') is not ('-' in tag)):
        return 'release tag/draft/channel mismatch'
    if '<!-- weekly-publication: hold -->' in (info.get('body') or ''):
        return 'release-note publication hold'
    # Accept the canonical package name only within the exact tag/channel release.
    # Development and explicitly unsigned/unnotarized filenames never qualify.
    version = tag.removeprefix('v')
    # The publisher accepts vSOURCE_VERSION-rc.N/beta.N while package scripts
    # keep the source version (without that release-channel suffix).
    source_version = re.sub(r'-(?:beta|rc)\.[0-9]+$', '', version)
    versions = '|'.join(re.escape(v) for v in sorted({version, source_version}))
    name = re.compile(r'(?:YACHT-macos-(?:arm64|x64)|(?:YACHT|yacht)-(?:'
                      + versions + r')-macos-(?:arm64|x64))\.zip\Z')
    matching = [a for a in assets if isinstance(a, dict) and name.fullmatch(a.get('name', ''))]
    names = [a['name'] for a in matching]
    if len(names) != len(set(names)):
        return 'ambiguous Mac assets'
    if not any(a.get('state') == 'uploaded' and type(a.get('size')) is int and a['size'] > 0
               and isinstance(a.get('digest'), str) and re.fullmatch(r'sha256:[0-9a-f]{64}', a['digest'])
               for a in matching):
        return 'no uploaded matching-release Mac archive with size and SHA256 evidence'
    return None


def target_asset_reason(tag, target, assets):
    """Never rebuild any already/partly published target, even an incomplete asset."""
    if target not in {'linux-x64', 'linux-arm64', 'windows-x64', 'windows-arm64'}:
        raise ValueError('Invalid weekly target')
    version = tag.removeprefix('v')
    source_version = re.sub(r'-(?:beta|rc)\.[0-9]+$', '', version)
    versions = {version, source_version}
    suffixes = ('.zip', '-setup.exe') if target.startswith('windows-') else ('.tar.gz', '.deb')
    names = {f'YACHT-{v}-{target}{suffix}' for v in versions for suffix in suffixes}
    names.add(f'SHA256SUMS-{target}')
    if any(isinstance(asset, dict) and asset.get('name') in names for asset in assets):
        return 'native target already or partly published: ' + target
    return None


def check(api, tag, holds, target=None):
    if tag in holds:
        return 'held: ' + holds[tag]
    # Enumerate rather than treating an API failure/403 as a missing release.
    matches = [r for r in api.pages(f'{api.root}/releases') if r.get('tag_name') == tag]
    if len(matches) > 1:
        return 'ambiguous matching releases'
    if not matches:
        return 'no existing release'
    release = matches[0]
    release_id = release.get('id')
    if type(release_id) is not int or release_id <= 0:
        raise ValueError('Invalid release ID')
    assets = list(api.pages(f'{api.root}/releases/{release_id}/assets'))
    reason = release_reason(tag, release, assets, holds)
    if reason is None and target is not None:
        reason = target_asset_reason(tag, target, assets)
    return reason
