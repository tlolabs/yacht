#!/usr/bin/env python3
"""Publish existing-release assets only; never create a tag/release or change Latest."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def load_publisher(source):
    # Load trusted controller code, never execute publication code from a release tag.
    path = Path(__file__).resolve().parents[2] / 'script/publish_release.py'
    spec = importlib.util.spec_from_file_location('publisher', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = source.resolve()
    return module


def find():
    repo, run = os.environ['GITHUB_REPOSITORY'], os.environ['GITHUB_RUN_ID']
    jobs_raw = subprocess.check_output(['gh', 'api', f'repos/{repo}/actions/runs/{run}/jobs', '--paginate', '--slurp'], text=True)
    jobs = [j for page in json.loads(jobs_raw) for j in page['jobs'] if j['name'] == os.environ['BUILD_JOB_NAME']]
    successful = len(jobs) == 1 and jobs[0]['conclusion'] == 'success'
    raw = subprocess.check_output(['gh', 'api', f'repos/{repo}/actions/runs/{run}/artifacts', '--paginate', '--slurp'], text=True)
    matches = [a for page in json.loads(raw) for a in page['artifacts']
               if a['name'] == os.environ['ARTIFACT_NAME'] and not a['expired']]
    if not successful:
        raise ValueError('Target build did not finish successfully; refusing publication')
    if len(matches) != 1:
        raise ValueError('Expected exactly one unexpired verified target artifact')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as out:
        out.write('found=true\n')


def process(mode, source, directory):
    p = load_publisher(source)
    target, tag = os.environ['RELEASE_TARGET'], os.environ['RELEASE_TAG']
    sha, run = os.environ['RELEASE_SHA'], os.environ['GITHUB_RUN_ID']
    if target not in {'linux-x64', 'linux-arm64', 'windows-x64', 'windows-arm64'} or not run.isdigit():
        raise ValueError('Invalid target/run')
    commit = p.source_commit(sha)
    version = p.version()
    expected_tag, prerelease = p.identity('refs/tags/' + tag, commit, version)
    if expected_tag != tag:
        raise ValueError('Tag/version mismatch')
    if mode == 'prepare':
        p.manifest(directory, target, commit, run, version)
        p.verify_manifest(directory, target, commit, run, version)
        return
    names = p.verify_manifest(directory, target, commit, run, version)
    # Third argument is development/tag-creation permission, NOT prerelease.
    # Stable and beta/RC tags must both already exist; never allow tag creation.
    p.verify_tag(tag, commit, development=False)
    info = p.release_info(tag)
    if info is None or info['draft'] or info['prerelease'] != prerelease:
        raise ValueError('Matching published release required; no release/channel will be created or changed')
    import weekly_eligibility
    from weekly_release_smoke import GitHub
    reason = weekly_eligibility.check(GitHub(os.environ['GITHUB_REPOSITORY']), tag,
                                      weekly_eligibility.load_holds())
    if reason is not None:
        raise ValueError('Publication prerequisite no longer satisfied: ' + reason)
    # Check all existing names for conflicts before uploading any new file.
    existing = {a['name'] for a in info['assets']}
    for name in names:
        if name in existing and p.remote_digest(tag, name) != p.digest(directory / name):
            raise ValueError(f'Refusing conflicting existing asset {name}')
    for name in names:
        # A multi-file upload can outlive a release edit. Recheck the tag and
        # current release/hold/Mac signal before every non-clobbering asset step.
        p.verify_tag(tag, commit, development=False)
        reason = weekly_eligibility.check(GitHub(os.environ['GITHUB_REPOSITORY']), tag,
                                          weekly_eligibility.load_holds())
        if reason is not None:
            raise ValueError('Publication prerequisite no longer satisfied: ' + reason)
        p.upload(tag, directory / name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['find', 'prepare', 'publish'])
    parser.add_argument('--source', type=Path)
    parser.add_argument('--directory', type=Path)
    args = parser.parse_args()
    if args.mode == 'find':
        find()
    elif args.source is None or args.directory is None:
        parser.error('--source and --directory are required')
    else:
        process(args.mode, args.source, args.directory.resolve())
