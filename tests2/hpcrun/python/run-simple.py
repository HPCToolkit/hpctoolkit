#!/usr/bin/env python3

import argparse
import collections
import sys
from pathlib import Path

import ruamel.yaml
from hpctoolkit.formats.v4 import Database
from hpctoolkit.formats.v4.metadb import Context, EntryPoint
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
        db = Database.from_dir(raw_db.basedir)

        try:
            # <program root>
            ep = next(
                ep
                for ep in db.meta.Context.entryPoints
                if ep.entryPoint == EntryPoint.EntryPoint.main_thread
            )

            # -call> script.py:N
            fm = next(
                c
                for c in ep.children
                if c.relation == Context.Relation.call
                and c.lexicalType == Context.LexicalType.line
                and c.file.path.endswith(str(args.script))
            )

            # -call> func_hi -lexical> script.py:N
            f1 = next(
                c
                for c in fm.children
                if c.relation == Context.Relation.call
                and c.lexicalType == Context.LexicalType.function
                and c.function.name == "func_hi"
            )
            f1l = next(
                c
                for c in f1.children
                if c.relation == Context.Relation.lexical
                and c.lexicalType == Context.LexicalType.line
                and c.file == fm.file
            )

            # -call> func_mid -lexical> script.py:N
            f2 = next(
                c
                for c in f1l.children
                if c.relation == Context.Relation.call
                and c.lexicalType == Context.LexicalType.function
                and c.function.name == "func_mid"
            )
            f2l = next(
                c
                for c in f2.children
                if c.relation == Context.Relation.lexical
                and c.lexicalType == Context.LexicalType.line
                and c.file == fm.file
            )

            # -call> func_lo
            f3 = next(
                c
                for c in f2l.children
                if c.relation == Context.Relation.call
                and c.lexicalType == Context.LexicalType.function
                and c.function.name == "func_lo"
            )
        except StopIteration as e:
            sys.setrecursionlimit(100000)
            ruamel.yaml.YAML(typ="rt").dump(db.meta.Context, sys.stdout)
            raise e from None
