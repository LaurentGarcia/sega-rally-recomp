#!/usr/bin/env python3
"""Check the CI frontend executable refuses player startup before SDL init."""
import argparse
import subprocess
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable')
args = parser.parse_args()
result = subprocess.run([args.executable], capture_output=True, text=True, timeout=10)
assert result.returncode == 2, (result.returncode, result.stdout, result.stderr)
assert result.stdout == '', result.stdout
assert result.stderr == 'ROM-free frontend compilation fixture only; no game code or assets.\n', result.stderr
print('ROM-free compile fixture refuses game startup before SDL: PASS')
