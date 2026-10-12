#!/usr/bin/env python3
"""Exercise release identity, package integrity, and immutable asset policy."""

import json
import plistlib
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import publish_release as release

SHA = "a" * 40
VERSION = "2.1.2"


class ReleaseTests(unittest.TestCase):
    def test_development_identity_is_commit_bound(self):
        self.assertEqual(release.identity("refs/heads/main", SHA, VERSION),
                         (f"dev-v{VERSION}-{SHA}", True))
        self.assertEqual(release.identity("refs/tags/v2.1.2", SHA, VERSION),
                         ("v2.1.2", False))
        self.assertEqual(release.identity("refs/tags/v2.1.2-rc.3", SHA, VERSION),
                         ("v2.1.2-rc.3", True))
        with self.assertRaises(ValueError):
            release.identity("refs/heads/topic", SHA, VERSION)

    def package(self, directory):
        name = "YACHT-macos-arm64.zip"
        with zipfile.ZipFile(directory / name, "w") as archive:
            archive.writestr("YACHT.app/Contents/Info.plist", plistlib.dumps({"CFBundleShortVersionString": VERSION}))
            archive.writestr("YACHT.app/Contents/MacOS/YachtApp", b"application")
        (directory / "yacht-macos-arm64").write_bytes(b"cli")
        filenames = [name, "yacht-macos-arm64"]
        (directory / "SHA256SUMS-macos-arm64").write_text("".join(
            f"{release.digest(directory / filename)}  {filename}\n" for filename in filenames))

    def test_manifest_rejects_source_or_checksum_change(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.package(directory)
            release.manifest(directory, "macos-arm64", SHA, "123", VERSION)
            release.verify_manifest(directory, "macos-arm64", SHA, "123", VERSION)
            with self.assertRaisesRegex(ValueError, "identity"):
                release.verify_manifest(directory, "macos-arm64", "b" * 40, "123", VERSION)
            (directory / "yacht-macos-arm64").write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "corrupt"):
                release.verify_manifest(directory, "macos-arm64", SHA, "123", VERSION)

    def test_incomplete_package_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.package(directory)
            with zipfile.ZipFile(directory / "YACHT-macos-arm64.zip", "w") as archive:
                archive.writestr("YACHT.app/Contents/Info.plist", plistlib.dumps({"CFBundleShortVersionString": VERSION}))
            (directory / "SHA256SUMS-macos-arm64").write_text("".join(
                f"{release.digest(directory / filename)}  {filename}\n" for filename in ("YACHT-macos-arm64.zip", "yacht-macos-arm64")))
            with self.assertRaisesRegex(ValueError, "Incomplete"):
                release.verify_package(directory, "macos-arm64", VERSION)

    @patch.object(release, "command")
    def test_debian_field_accepts_labeled_output(self, command):
        command.return_value = SimpleNamespace(stdout="Version: 2.1.2\n")
        self.assertEqual(release.debian_field(Path("package.deb"), "Version"), VERSION)
        command.return_value = SimpleNamespace(stdout="amd64\n")
        self.assertEqual(release.debian_field(Path("package.deb"), "Architecture"), "amd64")

    @patch.object(release, "release_info", return_value={"assets": [{"name": "package.zip"}]})
    @patch.object(release, "remote_digest", return_value="b" * 64)
    def test_existing_different_asset_is_never_replaced(self, _remote, _info):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "package.zip"
            path.write_bytes(b"verified")
            with self.assertRaisesRegex(RuntimeError, "conflicting asset"):
                release.upload("dev-v2.1.2-" + SHA, path)


if __name__ == "__main__":
    unittest.main()
