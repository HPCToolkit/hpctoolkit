#!/usr/bin/env python3

import argparse
import tarfile
from pathlib import Path

from hpctoolkit.test.execution import hpcrun  # noqa: import-error

parser = argparse.ArgumentParser(
    description="Run hpcrun and package the measurements as an xz-tarball"
)
parser.add_argument("output", type=Path)
parser.add_argument("arguments", nargs="+")
args = parser.parse_args()
del parser


def sanitize(ti):
    ti.uid, ti.gid = 0, 0
    ti.uname, ti.gname = "root", "root"
    return ti


with hpcrun(*args.arguments) as M:
    with tarfile.open(args.output, mode="w:xz") as out:
        out.add(M.basedir, arcname=".", recursive=True, filter=sanitize)
