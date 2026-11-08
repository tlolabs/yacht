#!/usr/bin/env python3
"""Launch an unpublished native app briefly and fail on premature exit."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable")
    parser.add_argument("--seconds", type=float, default=5)
    args = parser.parse_args()
    executable = Path(args.executable).resolve(strict=True)
    if not 1 <= args.seconds <= 30:
        parser.error("smoke duration must be between 1 and 30 seconds")
    with tempfile.TemporaryDirectory(prefix="weekly-native-smoke-") as directory:
        env = dict(os.environ, XDG_CONFIG_HOME=directory, XDG_DATA_HOME=directory,
                   YACHT_TEST_DATA=directory, QT_QPA_PLATFORM="offscreen")
        process = subprocess.Popen([str(executable)], cwd=str(executable.parent), env=env)
        try:
            try:
                result = process.wait(timeout=args.seconds)
            except subprocess.TimeoutExpired:
                print(f"Native startup smoke passed: process stayed alive for {args.seconds:g} seconds.")
            else:
                raise SystemExit(f"Native startup smoke failed: app exited early with status {result}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


if __name__ == "__main__":
    main()
