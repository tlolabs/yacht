"""Offline pre-claim eligibility and publication-ledger regressions."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

import weekly_eligibility as e
import weekly_release_smoke as g
from test_weekly_release_smoke import FakeAPI, tagrow, item, status, SHA, PATTERN, TARGETS, URL, baseline, OTHER_SHA

TAG = 'v2.1.4'
INFO = dict(id=1, tag_name=TAG, draft=False, prerelease=False, body='')
ASSET = dict(name='yacht-2.1.4-macos-arm64.zip', state='uploaded', size=100, digest='sha256:'+'a'*64)


class EligibilityTests(unittest.TestCase):
    def reason(self, info=INFO, assets=None, holds=None):
        return e.release_reason(TAG, info, [ASSET] if assets is None else assets, holds or {})

    def test_exact_mac_release_is_ready(self):
        self.assertIsNone(self.reason())

    def test_tag_only_draft_channel_and_hold_are_not_ready(self):
        for info in [None, {**INFO, 'draft': True}, {**INFO, 'prerelease': True}, {**INFO, 'tag_name': 'v9.0.0'},
                     {**INFO, 'body': '<!-- weekly-publication: hold -->'}]:
            self.assertIsNotNone(self.reason(info=info))
        self.assertIsNotNone(self.reason(holds={TAG: 'UI failure'}))

    def test_missing_incomplete_malformed_and_wrong_version_mac_assets(self):
        for assets in [[], [ASSET, ASSET]] + [[{**ASSET, key: value}] for key, value in [
                ('size', 0), ('size', True), ('size', '100'), ('digest', None), ('digest', 'a'*64),
                ('state', 'new'), ('name', 'yacht-dev-macos-arm64.zip'), ('name', 'yacht-2.1.3-macos-arm64.zip'),
                ('name', 'yacht-2.1.4-macos-arm64-signed-unnotarized.zip')]]:
            self.assertIsNotNone(self.reason(assets=assets))

    def test_prerelease_exact_version_and_channel(self):
        tag = TAG + '-rc.1'
        self.assertIsNone(e.release_reason(tag, {**INFO, 'tag_name': tag, 'prerelease': True},
                         [{**ASSET, 'name': 'YACHT-2.1.4-rc.1-macos-x64.zip'}], {}))

    def test_prerelease_source_version_archive_matches_publisher_identity(self):
        import importlib.util
        path = Path(__file__).resolve().parents[2] / 'script/publish_release.py'
        spec = importlib.util.spec_from_file_location('identity_publisher', path)
        publisher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(publisher)
        for suffix in ['rc.1', 'beta.2']:
            tag, prerelease = publisher.identity('refs/tags/v2.1.4-' + suffix, SHA, '2.1.4')
            self.assertIsNone(e.release_reason(tag, {**INFO, 'tag_name': tag, 'prerelease': prerelease},
                                             [ASSET], {}))
            self.assertIsNotNone(e.release_reason(tag, {**INFO, 'tag_name': tag, 'prerelease': prerelease},
                                [{**ASSET, 'name': 'yacht-2.1.3-macos-arm64.zip'}], {}))

    def test_canonical_package_name_from_actual_publisher_is_supported(self):
        import importlib.util
        path = Path(__file__).resolve().parents[2] / 'script/publish_release.py'
        spec = importlib.util.spec_from_file_location('canonical_publisher', path)
        publisher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(publisher)
        for arch in ['arm64', 'x64']:
            names, checksum = publisher.files_for('macos-' + arch, '2.1.4')
            archive = next(name for name in names if name.endswith('.zip'))
            self.assertIsNone(self.reason(assets=[{**ASSET, 'name': archive}]))
            self.assertIsNotNone(self.reason(assets=[{**ASSET, 'name': archive}], holds={TAG: 'UI hold'}))

    def test_api_listing_and_asset_pagination(self):
        api = Mock(root='repos/example/project')
        api.pages.side_effect = [[INFO], [ASSET]]
        self.assertIsNone(e.check(api, TAG, {}))
        self.assertEqual(api.pages.call_args.args[0], 'repos/example/project/releases/1/assets')

    def test_missing_release_skips_but_api_failure_is_not_no_change(self):
        api = Mock(root='repos/example/project')
        api.pages.return_value = []
        self.assertIsNotNone(e.check(api, TAG, {}))
        api.pages.side_effect = g.GateError('HTTP 403')
        with self.assertRaises(g.GateError): e.check(api, TAG, {})

    def test_hold_short_circuits_api_even_if_asset_exists(self):
        api = Mock()
        self.assertIsNotNone(e.check(api, TAG, {TAG: 'hold'}))
        api.pages.assert_not_called()
        self.assertIn('v2.1.3', e.load_holds())

    def test_corrupt_hold_policy_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'holds.json'
            for content in ['{}', '{"schema":1,"schema":1,"held_tags":{}}',
                            json.dumps(dict(schema=True, held_tags={})),
                            json.dumps(dict(schema=1, held_tags={TAG:''}))]:
                path.write_text(content)
                with self.assertRaises(ValueError): e.load_holds(path)

    def test_ineligible_not_claimed_then_ready_is_selected(self):
        api = FakeAPI([tagrow()])
        gate = g.Gate(api)
        readiness = Mock(return_value='no Mac release')
        matrix, summary = gate.select(PATTERN, TARGETS, eligibility=readiness)
        gate.claim(matrix, URL)
        self.assertEqual(api.writes, [])
        self.assertEqual(summary['ineligible'], 4)
        self.assertEqual(readiness.call_count, 4)
        matrix, _ = gate.select(PATTERN, TARGETS, eligibility=lambda tag, target: None)
        self.assertEqual(len(matrix['include']), 4)

    def test_real_attempts_not_retried_when_release_becomes_ready(self):
        for state in ['success', 'failure', 'error', 'pending']:
            api = FakeAPI([tagrow()], {SHA: [status(state=state)]})
            readiness = Mock(return_value=None)
            matrix, _ = g.Gate(api).select(PATTERN, ['linux-x64'], eligibility=readiness)
            self.assertEqual(matrix['include'], [])
            readiness.assert_not_called()

    def test_existing_or_partial_target_assets_skip_only_that_target(self):
        for name in ['YACHT-2.1.4-linux-x64.deb', 'YACHT-2.1.4-linux-x64.tar.gz',
                     'SHA256SUMS-linux-x64']:
            assets = [ASSET, {'name': name, 'state': 'new', 'size': 0}]
            self.assertIsNotNone(e.target_asset_reason(TAG, 'linux-x64', assets))
            self.assertIsNone(e.target_asset_reason(TAG, 'linux-arm64', assets))
            api = Mock(root='repos/example/project')
            api.pages.side_effect = [[INFO], assets]
            self.assertIsNotNone(e.check(api, TAG, {}, target='linux-x64'))
        for name in ['YACHT-2.1.4-windows-arm64.zip', 'YACHT-2.1.4-windows-arm64-setup.exe',
                     'SHA256SUMS-windows-arm64']:
            self.assertIsNotNone(e.target_asset_reason(TAG + '-rc.1', 'windows-arm64', [{'name': name}]))
        self.assertIsNotNone(e.target_asset_reason(TAG + '-rc.1', 'linux-x64',
                             [{'name': 'YACHT-2.1.4-rc.1-linux-x64.deb'}]))

    def test_eligibility_is_target_specific(self):
        api = FakeAPI([tagrow()])
        readiness = lambda tag, target: 'already published' if target == 'linux-x64' else None
        matrix, summary = g.Gate(api).select(PATTERN, TARGETS, eligibility=readiness)
        self.assertEqual([i['target'] for i in matrix['include']], TARGETS[1:])
        self.assertEqual(summary['ineligible'], 1)
        self.assertEqual(api.writes, [])

    def test_moved_baseline_tag_is_rejected_for_publication(self):
        api = FakeAPI([tagrow(sha=OTHER_SHA)])
        with self.assertRaisesRegex(g.GateError, 'Baselined release tag moved'):
            g.Gate(api).select(PATTERN, TARGETS, baseline=baseline([item()]),
                               eligibility=lambda tag, target: None)
        self.assertEqual(api.writes, [])

    def test_preflight_rejects_moved_or_missing_tag_before_writes(self):
        for rows in [[], [tagrow(sha=OTHER_SHA)]]:
            api = FakeAPI(rows)
            with self.assertRaisesRegex(g.GateError, 'moved or disappeared'):
                g.Gate(api).preflight({'include': [item()]}, PATTERN, lambda tag, target: None)
            self.assertEqual(api.writes, [])

    def test_cli_refreshes_all_readiness_before_any_claim(self):
        api = FakeAPI([tagrow()])
        # First four selection reads pass. The last target changes during refresh.
        responses = [None] * 7 + ['release-note publication hold']
        with patch.object(g, 'GitHub', return_value=api), patch.object(e, 'load_holds', return_value={}), \
             patch.object(e, 'check', side_effect=responses) as readiness, \
             patch.object(g, 'current_run', return_value=(123, 1, URL)), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            result = g.main(['select', '--repo', api.repo, '--pattern', PATTERN,
                             '--targets', ','.join(TARGETS), '--require-mac-release', '--claim'])
        self.assertEqual(result, 1)
        self.assertEqual(readiness.call_count, 8)
        self.assertEqual(api.writes, [])

    def test_cli_claims_when_fresh_readiness_passes(self):
        api = FakeAPI([tagrow()])
        with patch.object(g, 'GitHub', return_value=api), patch.object(e, 'load_holds', return_value={}), \
             patch.object(e, 'check', return_value=None) as readiness, \
             patch.object(g, 'current_run', return_value=(123, 1, URL)), \
             contextlib.redirect_stdout(io.StringIO()):
            result = g.main(['select', '--repo', api.repo, '--pattern', PATTERN,
                             '--targets', ','.join(TARGETS), '--require-mac-release', '--claim'])
        self.assertEqual(result, 0)
        self.assertEqual(readiness.call_count, 8)
        self.assertEqual(len(api.writes), 4)

    def test_active_workflow_wires_both_gates_and_keeps_schedule(self):
        workflow = Path(__file__).resolve().parents[1] / 'workflows/weekly-release-smoke.yml'
        text = workflow.read_text()
        self.assertIn("cron: '17 16 * * 0'", text)
        self.assertIn('--require-mac-release --claim', text)
        self.assertIn('needs: [select, smoke, publish]', text)
        self.assertIn('--require-publication', text)
        self.assertNotIn('workflow_dispatch:', text)
        self.assertNotIn('release:', text)

    def test_finalizer_requires_smoke_and_publication_success(self):
        smoke = dict(name='Smoke v1.2.3 (linux-x64)', status='completed', conclusion='success')
        publish = dict(name='Publish v1.2.3 (linux-x64)', status='completed', conclusion='success')
        for publishers, expected in [([publish], 'success'), ([], 'error'), ([publish, publish], 'error'),
                ([{**publish, 'conclusion':'failure'}], 'failure'),
                ([{**publish, 'conclusion':'skipped'}], 'error'),
                ([{**publish, 'status':'in_progress'}], 'error')]:
            api = FakeAPI(jobs=[smoke]+publishers)
            summary = g.Gate(api).finalize({'include':[item()]}, 123, 1, URL, require_publication=True)
            self.assertEqual(summary[expected], 1)
            self.assertEqual(api.writes[0][1]['state'], expected)

    def test_successful_publish_does_not_hide_failed_smoke(self):
        api = FakeAPI(jobs=[dict(name='Smoke v1.2.3 (linux-x64)', status='completed', conclusion='failure'),
                           dict(name='Publish v1.2.3 (linux-x64)', status='completed', conclusion='success')])
        summary = g.Gate(api).finalize({'include':[item()]}, 123, 1, URL, require_publication=True)
        self.assertEqual(summary['failure'], 1)


if __name__ == '__main__': unittest.main()
