#!/usr/bin/env python3
"""Offline production-gate regressions; run normally and with python3 -O."""
import importlib.util
import json
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify_macos_update", ROOT / "script/verify_macos_update.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class ProductionContractTests(unittest.TestCase):
    def test_missing_production_keys_fail_even_with_python_optimization(self):
        for flags in [[], ["-O"]]:
            # Use a separate fixture so adding production keys does not weaken this test.
            with tempfile.TemporaryDirectory() as root:
                root = Path(root)
                for name in ["script/check_update_contract.py", "Cargo.toml", "config/update-trust.json", "platform/macos/Support/Info.plist", "platform/windows/Version.props", "platform/windows/app.manifest"]:
                    destination = root / name
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(ROOT / name, destination)
                trust = json.loads((root / "config/update-trust.json").read_text())
                trust["keys"] = {}
                (root / "config/update-trust.json").write_text(json.dumps(trust))
                result = subprocess.run([sys.executable, *flags, root / "script/check_update_contract.py", "--production"], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Production manifest public keys are not configured", result.stderr)


class MacPackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.app = Path(self.temporary.name) / "YACHT.app"
        (self.app / "Contents/Frameworks/Sparkle.framework").mkdir(parents=True)
        self.trust = dict(application_id="com.tlolabs.yacht", repository="tlolabs/yacht", sparkle_public_key="fixture-public-key", macos_team_id="VR64M92P2M")
        self.info = dict(CFBundleIdentifier=self.trust["application_id"], CFBundleVersion="2.2.0", CFBundleShortVersionString="2.2.0", CFBundleExecutable="YachtApp", SUPublicEDKey=self.trust["sparkle_public_key"], SURequireSignedFeed=True, SUVerifyUpdateBeforeExtraction=True, SUSignedFeedFailureExpirationInterval=0, SUFeedURL="https://github.com/tlolabs/yacht/releases/latest/download/appcast-macos-arm64.xml")
        self.signature = "TeamIdentifier=VR64M92P2M\nCodeDirectory v=20500 flags=0x10000(runtime)\n"
        self.arch = "arm64"
        self.fail_tool = None

    def run_tool(self, *args):
        if args[0] == self.fail_tool:
            raise subprocess.CalledProcessError(1, args)
        return subprocess.CompletedProcess(args, 0, stdout=self.arch if args[0] == "lipo" else "", stderr=self.signature)

    def verify(self):
        (self.app / "Contents/Info.plist").write_bytes(plistlib.dumps(self.info))
        with patch.object(verifier, "run", side_effect=self.run_tool) as commands:
            verifier.verify_bundle(self.app, self.trust, "2.2.0", "arm64")
            return commands

    def test_valid_package_requires_native_tools(self):
        commands = self.verify()
        self.assertTrue(any("-R" in call.args and "certificate leaf" in " ".join(map(str, call.args)) for call in commands.call_args_list))
        self.assertTrue(any(call.args[:3] == ("xcrun", "stapler", "validate") for call in commands.call_args_list))
        self.assertTrue(any(call.args[0] == "spctl" for call in commands.call_args_list))

    def test_wrong_application_version_key_and_feed(self):
        for field in ["CFBundleIdentifier", "CFBundleVersion", "CFBundleShortVersionString", "SUPublicEDKey", "SUFeedURL"]:
            with self.subTest(field=field):
                previous = self.info[field]
                self.info[field] = "wrong"
                with self.assertRaises(ValueError): self.verify()
                self.info[field] = previous

    def test_internal_qt_build_is_never_a_production_update(self):
        self.info["YACHTInternalReference"] = True
        with self.assertRaisesRegex(ValueError, "Internal Qt"):
            self.verify()
        self.info.pop("YACHTInternalReference")
        # Even relabeling the bundle cannot promote shared Mac UI artifacts.
        runtime = self.app / "Contents/MacOS"
        runtime.mkdir(parents=True)
        (runtime / "libQt6Core.dylib").write_bytes(b"fixture")
        with self.assertRaisesRegex(ValueError, "Internal Qt"):
            self.verify()

    def test_disabled_security_flags(self):
        for field, value in [("SURequireSignedFeed", False), ("SUVerifyUpdateBeforeExtraction", False), ("SUSignedFeedFailureExpirationInterval", 7)]:
            previous = self.info[field]
            self.info[field] = value
            with self.assertRaises(ValueError): self.verify()
            self.info[field] = previous

    def test_wrong_architecture_and_missing_runtime(self):
        self.arch = "x86_64"
        with self.assertRaises(ValueError): self.verify()
        self.arch = "arm64"
        self.signature = "TeamIdentifier=VR64M92P2M\nflags=0x0(none)\n"
        with self.assertRaises(ValueError): self.verify()

    def test_invalid_signature_notarization_and_gatekeeper_reject(self):
        for tool in ["codesign", "xcrun", "spctl"]:
            with self.subTest(tool=tool):
                self.fail_tool = tool
                with self.assertRaises(subprocess.CalledProcessError): self.verify()


if __name__ == "__main__":
    unittest.main()
