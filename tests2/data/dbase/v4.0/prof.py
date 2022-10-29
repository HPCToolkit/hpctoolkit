#!/usr/bin/env python3

import argparse
import tarfile
from pathlib import Path

import ruamel.yaml
from hpctoolkit.formats.v4 import Database  # noqa: import-error
from hpctoolkit.test.execution import hpcprof  # noqa: import-error
from hpctoolkit.test.tarball import extracted  # noqa: import-error

parser = argparse.ArgumentParser(
    description="Run hpcprof on some measurements and package the database as an xz-tarball"
)
parser.add_argument("input", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("-y", "--yaml-output", type=Path)
args = parser.parse_args()
del parser


def sanitize(ti):
    ti.uid, ti.gid = 0, 0
    ti.uname, ti.gname = "root", "root"
    return ti


with extracted(args.input) as inp:
    with hpcprof(inp, "-j4", "--foreign") as dbase:
        with tarfile.open(args.output, mode="w:xz") as out:
            out.add(dbase.basedir, arcname=".", recursive=True, filter=sanitize)

        if args.yaml_output:
            db = Database.from_dir(dbase.basedir)
            with open(args.yaml_output, "w", encoding="utf-8") as f:
                ruamel.yaml.YAML(typ="rt").dump(db, f)
