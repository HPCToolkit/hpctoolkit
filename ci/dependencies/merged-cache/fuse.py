#!/usr/bin/env python3

import json
import argparse
from pathlib import Path
import re
from reprlib import repr

here = Path(__file__).parent.absolute()

parser = argparse.ArgumentParser(
    description="Fuse together multiple concretized environments into one"
)
parser.add_argument(
    "environments",
    nargs="+",
    type=Path,
    help="Environments to fuse together. Must already be concretetized",
)
args = parser.parse_args()

# Process each file and merge it with the previous one
# Also extract the list of abstract specs that were concretized
data = {}
for env in args.environments:
    lock = env / "spack.lock"
    if not lock.is_file():
        raise ValueError(f"Unable to find any spack.lock in {env}, was it not concretized?")
    with open(lock, "r") as f:
        newdata = json.load(f)
        # Check that the version is something we can use
        if newdata["_meta"]["lockfile-version"] != 4:
            raise ValueError(f'Unknown lockfile version {newdata["_meta"]["lockfile-version"]}')
        for key, val in newdata.items():
            if key not in data:
                data[key] = val
            elif key == "roots":
                assert isinstance(data[key], list), repr(data[key])
                assert isinstance(val, list), repr(val)
                data[key].extend(val)
            elif key == "concrete_specs":
                assert isinstance(data[key], dict), repr(data[key])
                assert isinstance(val, dict), repr(val)
                # Keys are hashes, so this should work without conflict
                data[key].update(val)
            elif data[key] != val:
                raise ValueError(f"Conflict in key {key}: {repr(data[key])} and {repr(val)}")

# Spit the mass out as a spack.lock file for the output
with open(here / "spack.lock", "w") as f:
    json.dump(data, f)

# Alter the spack.yaml to list the abstract specs, so everything works in the end
with open(here / "spack.yaml", "a") as f:
    f.write("\n  specs: ")
    json.dump([r["spec"] for r in data["roots"]], f)
