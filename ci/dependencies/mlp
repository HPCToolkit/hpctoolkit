#!/usr/bin/env python3

import argparse
import typing
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

pipeline: dict[str, typing.Any] = {
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

name_counts: dict[str, int] = {}

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

    # Add the appropriate need to the copy job
    need = {
        "pipeline": src_pipeline,
        "job": src_job,
    }
    if need not in pipeline["copy"]["needs"]:
        pipeline["copy"]["needs"].append(need)

    # Generate a trigger job for this pipeline
    num = name_counts.get(name, 0)
    name_counts[name] = num + 1
    pipeline[f"{name} {num:d} 0"] = {
        "stage": "trigger",
        "trigger": {
            "include": [{"job": "copy", "artifact": child.as_posix()}],
            "strategy": "depend",
        },
    }

# If the needs: for the copy job gets too long, clone the job to make GitLab happy
if len(pipeline["copy"]["needs"]) > 5:
    needs = pipeline["copy"]["needs"]
    pipeline["copy"]["needs"] = needs[:5]
    for i, _start in enumerate(range(5, len(pipeline["copy"]["needs"]), 5)):
        pipeline[f"copy {i+2}"] = pipeline["copy"].copy()
        pipeline[f"copy {i+2}"]["needs"] = needs[i : i + 5]

# Dump the top-level pipeline into the output
with open(args.output, "w", encoding="utf-8") as f:
    y.dump(pipeline, f)
