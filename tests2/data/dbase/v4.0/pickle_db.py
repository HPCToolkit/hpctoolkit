#!/usr/bin/env python3

import argparse
import pickle
from pathlib import Path

from hpctoolkit.formats import from_path_extended  # noqa: import-error

parser = argparse.ArgumentParser(
    description="Parse a database tarball and export the equivalent pickled Database object"
)
parser.add_argument("input", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
del parser

data = from_path_extended(args.input)
if data is None:
    raise ValueError()
with open(args.output, "wb") as f:
    pickle.dump(data, f)
