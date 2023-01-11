#!/usr/bin/env python3

import argparse
import collections
import sys
from pathlib import Path

import ruamel.yaml
from hpctoolkit.formats import from_path
from hpctoolkit.match.context import MatchCtx, MatchEntryPoint, chainmatch
from hpctoolkit.test.execution import hpcprof, hpcrun

parser = argparse.ArgumentParser()
parser.add_argument("-t", "--threads-per-proc", type=int, default=1)
parser.add_argument("script", type=Path)
parser.add_argument("script_args", default=[], nargs="*")
args = parser.parse_args()
del parser

if not sys.executable:
    raise RuntimeError("No Python interpreter executable!")

args.script = args.script.resolve(strict=True)
with hpcrun("-e", "REALTIME", [sys.executable, str(args.script)] + args.script_args) as meas:
    meas.check_standard(procs=1, threads_per_proc=args.threads_per_proc)

    with hpcprof(meas) as raw_db:
        db = from_path(raw_db.basedir)

        matches = list(
            chainmatch(
                db.meta.Context,
                MatchEntryPoint(entryPoint="main_thread"),
                MatchCtx(relation="call", lexicalType="line", file=str(args.script)),
                MatchCtx(relation="call", lexicalType="function", function="func_hi"),
                MatchCtx(relation="lexical", lexicalType="line", file=str(args.script)),
                MatchCtx(relation="call", lexicalType="function", function="func_mid"),
                MatchCtx(relation="lexical", lexicalType="line", file=str(args.script)),
                MatchCtx(relation="call", lexicalType="function", function="func_lo"),
            )
        )
        if not matches:
            sys.setrecursionlimit(100000)
            ruamel.yaml.YAML(typ="rt").dump(db.meta.Context, sys.stdout)
            raise ValueError("Unable to find match!")
        if len(matches) > 1:
            sys.setrecursionlimit(100000)
            ruamel.yaml.YAML(typ="rt").dump(db.meta.Context, sys.stdout)
            raise ValueError(f"Found {len(matches)} matches, expected 1!")
