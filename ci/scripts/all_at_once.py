#!/usr/bin/env python3

# Run a series of commands N times in parallel

import argparse
import os
import subprocess
import sys
import tempfile
import shlex

parser = argparse.ArgumentParser()
parser.add_argument("jobs", type=int)
parser.add_argument("commandlines", nargs="+")
args = parser.parse_args()
del parser

if args.jobs <= 0:
    print("Invalid number of jobs, must be at least 1!")
    sys.exit(2)

procs = []
for cmdline in args.commandlines:
    if len(cmdline) == 0 or cmdline.isspace():
        continue

    for i in range(args.jobs):
        if len(procs) == 0:
            # First process outputs directly to the output
            out_f = None
            proc = subprocess.Popen(shlex.split(cmdline))
        else:
            # Rest use temporary log files
            out_f = tempfile.NamedTemporaryFile("w+")
            log_f = open(out_f.name, "w")
            proc = subprocess.Popen(shlex.split(cmdline), stdout=log_f, stderr=log_f)
        procs.append((proc, out_f, cmdline))

ok = True
for i, (p, f, cl) in enumerate(procs):
    retcode = p.wait()
    ok = ok and retcode == 0
    if f is not None:
        print(f"=== Output for #{i} (exited with {retcode}), command: {cl}")
        for line in f:
            print(line, end="")
        f.close()
    else:
        print(f"=== #{i} exited with {retcode}, command: {cl}")


sys.exit(0 if ok else 1)
