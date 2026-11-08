#!/usr/bin/env python3
"""Exercise stable tag provenance without requiring a cryptographic tag signature."""

import io
import json
import os
import runpy
import subprocess
import tomllib
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SHA = "a" * 40
TAG = "v" + tomllib.loads((ROOT / "Cargo.toml").read_text())["workspace"]["package"]["version"]


def github_response(request, timeout=20):
    if request.full_url.endswith("/git/ref/tags/" + TAG):
        body = {"object": {"type": "tag", "sha": "b" * 40}}
    else:
        body = {"object": {"type": "commit", "sha": SHA}, "verification": {"verified": False, "reason": "unsigned"}}
    return io.BytesIO(json.dumps(body).encode())


class ReleaseTagTests(unittest.TestCase):
    def check_tag(self, head=SHA):
        def git_output(args, **kwargs):
            if args[-1] != "HEAD":
                self.assertEqual(args, ["git", "rev-parse", "--verify", f"refs/tags/{TAG}^{{commit}}"] )
            return (head if args[-1] == "HEAD" else SHA) + "\n"

        env = {"RELEASE_TAG": TAG, "GH_REPO": "tlolabs/yacht", "GH_TOKEN": "fixture"}
        with patch.dict(os.environ, env), patch("urllib.request.urlopen", side_effect=github_response), patch.object(subprocess, "check_output", side_effect=git_output), patch.object(subprocess, "run") as run:
            runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")
        return run

    def test_unsigned_annotated_tag_with_matching_source_is_accepted(self):
        run = self.check_tag()
        self.assertEqual(run.call_count, 1)
        self.assertNotIn("--production", run.call_args.args[0])

    def test_tag_target_must_match_checked_out_commit(self):
        with self.assertRaisesRegex(SystemExit, "differs from checked-out commit"):
            self.check_tag(head="c" * 40)

    def test_prerelease_tag_rejected_for_production(self):
        env = {"RELEASE_TAG": TAG + "-rc.1", "GH_REPO": "tlolabs/yacht", "GH_TOKEN": "fixture"}
        with patch.dict(os.environ, env), patch("sys.argv", ["check_release_tag.py", "--stable-only"]):
            with self.assertRaisesRegex(SystemExit, "not authorized for production signing"):
                runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")

    def test_dirty_working_tree_rejected(self):
        def git_output(args, **kwargs):
            if "diff" in args:
                return "crates/yacht-core/src/lib.rs\n"
            return SHA + "\n"

        env = {"RELEASE_TAG": TAG, "GH_REPO": "tlolabs/yacht", "GH_TOKEN": "fixture"}
        with patch.dict(os.environ, env), patch("urllib.request.urlopen", side_effect=github_response), patch.object(subprocess, "check_output", side_effect=git_output), patch("sys.argv", ["check_release_tag.py", "--require-clean"]):
            with self.assertRaisesRegex(SystemExit, "Working tree has uncommitted modifications"):
                runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")

    def test_missing_tag_or_git_rejects_production_without_remote_configuration(self):
        for error in [FileNotFoundError("git"), subprocess.CalledProcessError(128, ["git", "rev-list"])]:
            with self.subTest(error=type(error).__name__):
                with patch.dict(os.environ, {"RELEASE_TAG": TAG}, clear=True), patch("sys.argv", ["check_release_tag.py", "--stable-only"]), patch.object(subprocess, "check_output", side_effect=error), patch.object(subprocess, "run") as run:
                    with self.assertRaisesRegex(SystemExit, "Cannot verify stable release tag"):
                        runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")
                    run.assert_not_called()

    def test_empty_provenance_is_not_accepted(self):
        with patch.dict(os.environ, {"RELEASE_TAG": TAG}, clear=True), patch("sys.argv", ["check_release_tag.py", "--stable-only"]), patch.object(subprocess, "check_output", return_value=""), patch.object(subprocess, "run") as run:
            with self.assertRaisesRegex(SystemExit, "Cannot verify stable release tag"):
                runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")
            run.assert_not_called()

    def test_unavailable_clean_tree_check_is_not_accepted(self):
        with patch.dict(os.environ, {"RELEASE_TAG": TAG}, clear=True), patch("sys.argv", ["check_release_tag.py", "--require-clean"]), patch.object(subprocess, "check_output", side_effect=FileNotFoundError("git")):
            with self.assertRaisesRegex(SystemExit, "Cannot verify a clean source tree"):
                runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")

    def test_wrong_version_tag_rejected(self):
        env = {"RELEASE_TAG": "v0.0.1", "GH_REPO": "tlolabs/yacht", "GH_TOKEN": "fixture"}
        with patch.dict(os.environ, env):
            with self.assertRaisesRegex(SystemExit, "does not match Cargo version"):
                runpy.run_path(str(ROOT / "script/check_release_tag.py"), run_name="__main__")


if __name__ == "__main__":
    unittest.main()
