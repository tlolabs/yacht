#!/usr/bin/env python3
"""Run a packaged shared UI with isolated data and an explicit result contract."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='yacht-avalonia-') as directory:
    root = Path(directory)
    env = dict(os.environ, YACHT_TEST_DATA=str(root/'preferences'))
    subprocess.run([str(Path(sys.argv[1]).resolve()), '--ui-smoke-test', str(root)], env=env, check=True, timeout=90)
    if (root/'failed.txt').exists():
        raise SystemExit((root/'failed.txt').read_text())
    if not (root/'passed.txt').exists():
        raise SystemExit('Application exited without completing the UI smoke test')
    print((root/'passed.txt').read_text())
