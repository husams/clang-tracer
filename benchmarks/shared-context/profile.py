#!/usr/bin/env python3
"""Intrusive ptrace sampling; run separately from timing trials."""
import json
from pathlib import Path
import subprocess
import sys
import time
pid, directory, count = int(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3])
for name in ['maps','smaps_rollup','status']:
    (directory/('profile-'+name+'.txt')).write_bytes(Path(f'/proc/{pid}/{name}').read_bytes())
for i in range(count):
    start=time.monotonic_ns()
    result=subprocess.run(['eu-stack','-p',str(pid),'-n','18','-i'],capture_output=True,timeout=10)
    (directory/f'stack-{i:03}.txt').write_bytes(result.stdout+result.stderr)
    with (directory/'sampling.jsonl').open('a') as f:
        f.write(json.dumps(dict(index=i,start_ns=start,end_ns=time.monotonic_ns(),rc=result.returncode))+'\n')
    time.sleep(.037+(i%5)*.011)
