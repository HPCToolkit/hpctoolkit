#!/usr/bin/env python3

import argparse
import typing as T
from pathlib import Path

import ruamel.yaml

y = ruamel.yaml.YAML(typ="safe")

parser = argparse.ArgumentParser(
    description="Generate a Multi-Level Pipeline for the given pipelines"
)
parser.add_argument("output", type=Path, help="Final output parent pipeline")
parser.add_argument("children", type=Path, nargs="+", help="Child pipelines")
args = parser.parse_args()
del parser

pipeline: T.Dict[str, T.Any] = {
    "workflow": {"rules": [{"when": "always"}]},
    "stages": ["copy", "trigger"],
    "copy": {
        "stage": "copy",
        "tags": ["docker"],
        "script": [
            "if test -d spack-ci-artifacts/; then ls -lR spack-ci-artifacts/; else echo 'Nothing to do!'; fi"
        ],
        "artifacts": {"paths": ["spack-ci-artifacts/"]},
        "needs": [],
    },
}

# For each child pipeline, gather its source pipeline/job and decide whether to include it
for child in args.children:
    with open(child, encoding="utf-8") as f:
        data = list(y.load_all(f))
    if len(data) != 2:
        raise ValueError(f"Invalid child pipeline {child}, missing secondary source document")
    if "source" not in data[1]:
        raise ValueError(f"Invalid child pipeline {child}, no source key")

    src_pipeline = data[1]["source"]["pipeline"]
    src_job = data[1]["source"]["job"]
    name = data[1]["source"]["name"]

    # Only launch pipelines that have more than just the no-op job
    seen_noop_job = False
    seen_real_job = False
    for jobspec in data[0].values():
        if "tags" in jobspec:
            seen_noop_job = seen_noop_job or "noop-job" in jobspec["tags"]
            seen_real_job = seen_real_job or "noop-job" not in jobspec["tags"]
    if seen_noop_job:
        if seen_real_job:
            raise ValueError(f"Invalid child pipeline {child}, contains real and no-op jobs!")
        continue
    if not seen_real_job:
        # Technically a bad pipeline, but we ignore it completely in this case
        continue

    # Generate a trigger job for this pipeline
    pipeline["copy"]["needs"].append(
        {
            "pipeline": src_pipeline,
            "job": src_job,
        }
    )
    pipeline[name] = {
        "stage": "trigger",
        "trigger": {
            "include": [{"job": "copy", "artifact": child.as_posix()}],
            "strategy": "depend",
        },
    }

# Dump the top-level pipeline into the output
with open(args.output, "w", encoding="utf-8") as f:
    y.dump(pipeline, f)
