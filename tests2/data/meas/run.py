#!/usr/bin/env python3

import argparse
import re
import sys
import tarfile
from pathlib import Path

from hpctoolkit.test.execution import hpcrun, hpcstruct  # noqa: import-error

parser = argparse.ArgumentParser(
    description="Run hpcrun and package the measurements as an xz-tarball"
)
parser.add_argument("output", type=Path)
parser.add_argument("arguments", nargs="+")
parser.add_argument("--no-struct", default=True, action="store_false", dest="struct")
parser.add_argument("-m", "--min-samples", default=100, type=int)
args = parser.parse_args()
del parser


def sanitize(ti):
    ti.uid, ti.gid = 0, 0
    ti.uname, ti.gname = "root", "root"
    return ti


def check_ok(M):
    sample_cnts = []
    logfiles = 0
    for stem in M.thread_stems:
        if M.logfile(stem):
            logfiles += 1
            with open(M.logfile(stem), encoding="utf-8") as f:
                for line in f:
                    mat = re.match(r"SUMMARY:\s+samples:\s+\d+\s+\(recorded:\s+(\d+)", line)
                    if mat:
                        sample_cnt = int(mat.group(1))
                        break
                else:
                    print(
                        f"{stem}: Did not find SUMMARY line in log output. Retrying...",
                        file=sys.stderr,
                    )
                    return False
            if sample_cnt < args.min_samples:
                print(
                    f"{stem}: Achieved {sample_cnt} samples, wanted at least {args.min_samples}. Retrying...",
                    file=sys.stderr,
                )
                return False
            sample_cnts.append(sample_cnt)
    if logfiles == 0:
        print("Did not find any log output. Retrying...", file=sys.stderr)
        return False
    print(
        f"Achieved {sum(sample_cnts)} total samples (min/avg/max {min(sample_cnts)}/{sum(sample_cnts)/len(sample_cnts)}/{max(sample_cnts)} per process)"
    )
    return True


for i in range(10):
    with hpcrun(*args.arguments) as M:
        # Verify that the measurements contains enough samples to call it useful
        if not check_ok(M):
            continue

        if args.struct:
            # Generate and include all the structure files
            hpcstruct(M, "--gpucfg=yes")

        with tarfile.open(args.output, mode="w:xz") as out:
            out.add(M.basedir, arcname=".", recursive=True, filter=sanitize)
            break
else:
    raise RuntimeError("All 10 attempts failed to collect enough data, adjust the arguments!")
