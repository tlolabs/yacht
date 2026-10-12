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


if __name__ == "__main__":
    unittest.main()
