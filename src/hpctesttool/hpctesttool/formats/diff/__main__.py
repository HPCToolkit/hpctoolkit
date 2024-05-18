# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import argparse
import sys
import typing
from pathlib import Path, PurePath

from .. import from_path_extended
from . import strict

diffs = {
    "strict": strict.StrictDiff,
}
accuracies = {
    "strict": strict.StrictAccuracy,
}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description='Compare two performance databases and generate a semantically-aware "diff" between the two.'
    )
    parser.add_argument(
        "strategy", choices=diffs.keys(), help="Diff strategy to use to compare the inputs"
    )
    parser.add_argument(
        "-a",
        "--accuracy",
        choices=accuracies.keys(),
        help="Accuracy strategy to use to compare the inputs",
    )
    parser.add_argument("a", type=Path, help="File or database directory on the lefthand side.")
    parser.add_argument("b", type=Path, help="File or database directory on the righthand side.")
    parser.add_argument(
        "-q", "--quiet", default=False, action="store_true", help="Disable all normal output"
    )
    parser.add_argument(
        "--subdir-a",
        type=PurePath,
        help="If a is a tarball, subdirectory within to cosnider the database.",
    )
    parser.add_argument(
        "--subdir-b",
        type=PurePath,
        help="If b is a tarball, subdirectory within to cosnider the database.",
    )
    return parser


def main(inargs: typing.List[str]) -> int:
    args = build_parser().parse_args(inargs)
    args.strategy = diffs[args.strategy]

    # Load the inputs
    a = from_path_extended(args.a, subdir=args.subdir_a)
    if a is None:
        print("Invalid left input, not a recognized performance data format!", file=sys.stderr)
        return 2
    b = from_path_extended(args.b, subdir=args.subdir_b)
    if b is None:
        print("Invalid right input, not a recognized performance data format!", file=sys.stderr)
        return 2

    # Compare the inputs
    diff = args.strategy(a, b)
    acc = None if args.accuracy is None else accuracies[args.accuracy](diff)

    # Print and return the results
    same = len(diff.hunks) == 0 and (acc is None or acc.inaccuracy == 0)
    if not args.quiet:
        if same:
            print("No difference found between inputs")
        else:
            if len(diff.hunks) > 0:
                diff.render(out=sys.stdout)
            else:
                print("No structural difference found between inputs")
            if acc is not None:
                acc.render(out=sys.stdout)

    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
