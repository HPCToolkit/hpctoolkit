#!/usr/bin/env python3

from argparse import ArgumentParser
from pathlib import Path
from difflib import unified_diff
from io import StringIO
import sys

import ruamel.yaml

if ruamel.yaml.version_info < (0, 15, 0):
    y = ruamel.yaml
else:
    y = ruamel.yaml.YAML(typ="safe")

parser = ArgumentParser()
parser.add_argument("pipelines", type=Path, nargs="+")
parser.add_argument("--add-globals", metavar="YAML", type=lambda x: y.load(x))
parser.add_argument("--add-to-jobs", metavar="YAML", type=lambda x: y.load(x))
parser.add_argument("--ignore-job", metavar="NAME", action="append")
args = parser.parse_args()


def add_globals(data):
    if args.add_globals:
        # Add any globals. Keys in the original take precedence (shallow-merge).
        d2 = args.add_globals.copy()
        d2.update(data)
        return d2
    return data


def add_to_jobs(data):
    if args.add_to_jobs:
        d2 = {}
        for name, job in data.items():
            if "script" not in job or name in args.ignore_job:
                d2[name] = job
                continue
            # Add any values. Keys in the original take precedence (shallow-merge).
            d2[name] = args.add_to_jobs.copy()
            d2[name].update(job)
        return d2
    return data


for ciyml in args.pipelines:
    # Load the original pipeline
    with open(ciyml, "r") as f:
        pipeline = y.load(f)
    with StringIO() as before_s:
        y.dump(pipeline, before_s)
        before = before_s.getvalue()
    # Alter it as we are configured
    pipeline = add_globals(pipeline)
    pipeline = add_to_jobs(pipeline)
    with StringIO() as after_s:
        y.dump(pipeline, after_s)
        after = after_s.getvalue()
    # Print a diff indicating how we changed this one
    sys.stdout.writelines(
        unified_diff(
            before.splitlines(keepends=True),
            after.splitlines(keepends=True),
            fromfile=str(ciyml),
            tofile=str(ciyml),
        )
    )
    # Spit the changed pipeline back out
    with open(ciyml, "w") as f:
        y.dump(pipeline, f)
