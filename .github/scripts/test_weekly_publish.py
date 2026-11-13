import importlib.util
import os
import json
import tempfile
from pathlib import Path
from types import SimpleNamespace
import unittest
import weekly_eligibility
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location('weekly_publish', Path(__file__).with_name('weekly_publish.py'))
w = importlib.util.module_from_spec(spec)
spec.loader.exec_module(w)

class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.p = Mock()
        self.p.source_commit.return_value = 'a' * 40
        self.p.version.return_value = '2.1.3'
        self.p.identity.return_value = ('v2.1.3', False)
        self.p.verify_manifest.return_value = ['package.zip', 'SHA256SUMS-windows-x64']
        self.p.release_info.return_value = {'draft': False, 'prerelease': False, 'assets': []}
        self.env = patch.dict(os.environ, RELEASE_TARGET='windows-x64', RELEASE_TAG='v2.1.3', RELEASE_SHA='a'*40, GITHUB_RUN_ID='123', GITHUB_REPOSITORY='tlolabs/yacht')
        self.env.start()
        self.loader = patch.object(w, 'load_publisher', return_value=self.p)
        self.loader.start()
        self.eligibility = patch.object(weekly_eligibility, 'check', return_value=None)
        self.check = self.eligibility.start()
    def tearDown(self):
        self.env.stop(); self.loader.stop(); self.eligibility.stop()
    def run_publish(self):
        w.process('publish', Path('source'), Path('packages'))
    def test_verified_existing_release_uploads_without_edit(self):
        self.run_publish()
        self.assertEqual(self.p.upload.call_count, 2)
        self.p.ensure_release.assert_not_called()
        self.p.command.assert_not_called()
        self.p.verify_tag.assert_called_with('v2.1.3', 'a'*40, development=False)
        self.assertEqual(self.p.verify_tag.call_count, 3)
    def test_prerelease_uploads_without_tag_creation_permission(self):
        os.environ['RELEASE_TAG']='v2.1.3-rc.1'
        self.p.identity.return_value=('v2.1.3-rc.1', True)
        self.p.release_info.return_value['prerelease']=True
        self.run_publish()
        self.p.verify_tag.assert_called_with('v2.1.3-rc.1', 'a'*40, development=False)
        self.assertEqual(self.p.verify_tag.call_count, 3)
        self.assertEqual(self.p.upload.call_count, 2)
        self.p.ensure_release.assert_not_called()
    def test_prerelease_cannot_use_stable_release(self):
        os.environ['RELEASE_TAG']='v2.1.3-rc.1'
        self.p.identity.return_value=('v2.1.3-rc.1', True)
        with self.assertRaises(ValueError): self.run_publish()
        self.p.upload.assert_not_called()
    def test_missing_release_rejected(self):
        self.p.release_info.return_value = None
        with self.assertRaises(ValueError): self.run_publish()
        self.p.upload.assert_not_called()
    def test_draft_or_channel_mismatch_rejected(self):
        for flags in [(True,False),(False,True)]:
            self.p.release_info.return_value = dict(draft=flags[0], prerelease=flags[1], assets=[])
            with self.assertRaises(ValueError): self.run_publish()
        self.p.upload.assert_not_called()
    def test_conflict_prevents_all_uploads(self):
        self.p.release_info.return_value['assets'] = [{'name':'SHA256SUMS-windows-x64'}]
        self.p.remote_digest.return_value = 'old'
        self.p.digest.return_value = 'new'
        with self.assertRaises(ValueError): self.run_publish()
        self.p.upload.assert_not_called()
    def test_manifest_failure_prevents_writes(self):
        self.p.verify_manifest.side_effect = ValueError('corrupt')
        with self.assertRaises(ValueError): self.run_publish()
        self.p.upload.assert_not_called()
    def test_hold_or_missing_mac_rechecked_before_upload(self):
        for reason in ['held', 'no existing release', 'missing Mac asset']:
            self.check.return_value = reason
            with self.assertRaisesRegex(ValueError, 'prerequisite'):
                self.run_publish()
        self.p.upload.assert_not_called()

    def test_new_hold_between_uploads_stops_remaining_assets(self):
        self.check.side_effect = [None, None, 'release-note publication hold']
        with self.assertRaisesRegex(ValueError, 'prerequisite'):
            self.run_publish()
        self.p.upload.assert_called_once_with('v2.1.3', Path('packages/package.zip'))

    def test_moved_tag_between_uploads_stops_remaining_assets(self):
        self.p.verify_tag.side_effect = [None, None, RuntimeError('Tag moved')]
        with self.assertRaisesRegex(RuntimeError, 'Tag moved'):
            self.run_publish()
        self.assertEqual(self.p.upload.call_count, 1)

    def test_prepare_validates_file_set(self):
        w.process('prepare', Path('source'), Path('packages'))
        self.p.manifest.assert_called_once()
        self.p.verify_manifest.assert_called_once()
        self.p.upload.assert_not_called()
    def test_unsupported_target_rejected(self):
        os.environ['RELEASE_TARGET']='macos-arm64'
        with self.assertRaises(ValueError): self.run_publish()
        self.p.upload.assert_not_called()

class ArtifactSelectionTests(unittest.TestCase):
    def check(self, conclusion, artifacts):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'output'
            env = dict(GITHUB_REPOSITORY='tlolabs/yacht', GITHUB_RUN_ID='123',
                       ARTIFACT_NAME='expected', BUILD_JOB_NAME='Smoke v2.1.3 (linux-x64)', GITHUB_OUTPUT=str(output))
            jobs = [{'jobs':[{'name':env['BUILD_JOB_NAME'], 'conclusion':conclusion}]}]
            with patch.dict(os.environ, env), patch.object(w.subprocess, 'check_output', side_effect=[json.dumps(jobs), json.dumps([{'artifacts':artifacts}])]):
                w.find()
            return output.read_text()
    def test_failed_job_cannot_publish_existing_artifact(self):
        with self.assertRaisesRegex(ValueError, 'build did not finish'):
            self.check('failure',[{'name':'expected','expired':False}])
    def test_only_successful_matching_artifact(self):
        self.assertEqual(self.check('success',[{'name':'expected','expired':False}]), 'found=true\n')
    def test_missing_and_expired_artifacts_fail(self):
        for artifacts in [[], [{'name':'expected','expired':True}], [{'name':'expected','expired':False}]*2]:
            with self.assertRaisesRegex(ValueError, 'exactly one'):
                self.check('success', artifacts)

if __name__ == '__main__': unittest.main()
