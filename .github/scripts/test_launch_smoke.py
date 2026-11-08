import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HELPER = Path(__file__).with_name('launch_smoke.py')

@unittest.skipIf(os.name == 'nt', 'Local executable fixture requires a POSIX shebang')
class LaunchSmokeTests(unittest.TestCase):
    def run_fixture(self, code, expected):
        with tempfile.TemporaryDirectory() as folder:
            script=Path(folder)/'fixture'
            script.write_text('#!'+sys.executable+'\n'+code+'\n')
            script.chmod(0o700)
            result=subprocess.run([sys.executable,str(HELPER),str(script),'--seconds','1'],capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,expected,result.stdout+result.stderr)
    def test_live_process_passes_and_is_terminated(self):
        self.run_fixture('import time; time.sleep(30)',0)
    def test_early_successful_exit_fails(self):
        self.run_fixture('raise SystemExit(0)',1)
    def test_crash_fails(self):
        self.run_fixture('raise SystemExit(7)',1)
    def test_missing_executable_fails(self):
        result=subprocess.run([sys.executable,str(HELPER),'/definitely/missing'],capture_output=True,text=True,timeout=5)
        self.assertNotEqual(result.returncode,0)

if __name__ == '__main__': unittest.main()
