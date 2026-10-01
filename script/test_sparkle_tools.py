#!/usr/bin/env python3
"""Offline real-tool feed signature tests. No production credentials or keychain use."""
import argparse
import base64
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tools", type=Path)
    args = parser.parse_args()
    tool = (args.tools / "sign_update").resolve()
    # Intentionally public deterministic fixture seed, never a production key.
    key = base64.b64encode(bytes([7]) * 32).decode() + "\n"
    with tempfile.TemporaryDirectory(prefix="yacht-sparkle-tool-fixture-") as temporary:
        feed = Path(temporary) / "appcast.xml"
        feed.write_text('<?xml version="1.0" encoding="utf-8"?><rss version="2.0"><channel><title>Offline fixture</title></channel></rss>')
        subprocess.run([tool, "--ed-key-file", "-", feed], input=key, text=True, check=True, capture_output=True)
        subprocess.run([tool, "--verify", "--ed-key-file", "-", feed], input=key, text=True, check=True, capture_output=True)
        feed.write_bytes(feed.read_bytes().replace(b"Offline fixture", b"Tampered fixture"))
        invalid = subprocess.run([tool, "--verify", "--ed-key-file", "-", feed], input=key, text=True, capture_output=True)
        if invalid.returncode == 0:
            raise SystemExit("Tampered feed accepted")
    print("Real Sparkle signed-feed roundtrip and tamper rejection passed (offline fixture)")


if __name__ == "__main__":
    main()
