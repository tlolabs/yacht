#!/usr/bin/env python3
"""Exercise the consolidated CI issue lifecycle without GitHub access."""

import os
import unittest
from unittest.mock import patch

import report_ci_failures

ENV = {
    "GITHUB_REPOSITORY": "tlolabs/yacht",
    "GITHUB_RUN_ID": "123",
    "GITHUB_RUN_ATTEMPT": "1",
    "GITHUB_SHA": "a" * 40,
}


class ReportTests(unittest.TestCase):
    def test_failed_platform_creates_one_issue_with_job_link(self):
        calls = []

        def api(method, path, payload=None):
            calls.append((method, path, payload))
            if "/jobs?" in path:
                return {"jobs": [{"name": "Windows x64", "conclusion": "failure", "html_url": "https://github.com/tlolabs/yacht/actions/runs/123/job/9"}, {"name": "Linux x64", "conclusion": "success"}]}
            if method == "GET":
                return []
            return {"number": 7}

        with patch.dict(os.environ, ENV), patch.object(report_ci_failures, "api", side_effect=api):
            report_ci_failures.main()
        writes = [call for call in calls if call[0] != "GET"]
        self.assertEqual(len(writes), 1)
        self.assertEqual(writes[0][0], "POST")
        self.assertIn("Windows x64", writes[0][2]["body"])
        self.assertNotIn("Linux x64", writes[0][2]["body"])

    def test_success_closes_existing_issue(self):
        calls = []

        def api(method, path, payload=None):
            calls.append((method, path, payload))
            if "/jobs?" in path:
                return {"jobs": [{"name": "macOS (ARM64)", "conclusion": "success"}]}
            if method == "GET":
                return [{"number": 7, "title": report_ci_failures.TITLE, "state": "open"}]
            return {"number": 7}

        with patch.dict(os.environ, ENV), patch.object(report_ci_failures, "api", side_effect=api):
            report_ci_failures.main()
        writes = [call for call in calls if call[0] != "GET"]
        self.assertEqual(writes, [("PATCH", "/repos/tlolabs/yacht/issues/7", {"state": "closed", "state_reason": "completed"})])


if __name__ == "__main__":
    unittest.main()
