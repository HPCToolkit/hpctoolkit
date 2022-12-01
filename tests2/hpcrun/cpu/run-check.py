#!/usr/bin/env python3

import argparse

from hpctoolkit.test.execution import hpcrun

parser = argparse.ArgumentParser()
parser.add_argument("-t", "--threads-per-proc", type=int, default=1)
parser.add_argument("-p", "--processes", type=int, default=1)
parser.add_argument("cmd", nargs="+")
args = parser.parse_args()
del parser

with hpcrun(args.cmd) as M:
    M.check_standard(procs=args.processes, threads_per_proc=args.threads_per_proc)
