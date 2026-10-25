"""Run a bounded fuzzer without modifying the checked-in seed corpus."""

import pathlib
import subprocess
import sys
import tempfile

binary, seeds, artifacts, seconds = sys.argv[1:]
pathlib.Path(artifacts).mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='stories-corpus-') as corpus:
    command = [binary, corpus, seeds, f'-max_total_time={seconds}', '-timeout=5', '-max_len=8192', f'-artifact_prefix={artifacts}/']
    result = subprocess.run(command)
    if result.returncode:
        print(f'Reproduce a saved artifact with: {binary} {artifacts}/<artifact>', file=sys.stderr)
    sys.exit(result.returncode)
