# SPDX-FileCopyrightText: 2008-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import argparse
import sys
import typing
from pathlib import Path, PurePath

import ruamel.yaml

from . import from_path_extended


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Debugging pretty-printer for performance data formats. Prints the input in an equivalent YAML representation."
    )
    parser.add_argument(
        "src",
        type=Path,
        help="File or database directory to dump. Also accepts tarballed databases.",
    )
    parser.add_argument(
        "-s",
        "--silent",
        default=False,
        action="store_true",
        help="Don't actually output, just parse the inputs",
    )
    parser.add_argument(
        "--subdir",
        type=PurePath,
        help="If src is a tarball, subdirectory within to consider the database.",
    )
    return parser


def main(rawargs: typing.List[str]) -> int:
    args = build_parser().parse_args(rawargs)

    data = from_path_extended(args.src, subdir=args.subdir)
    if data is None:
        print(
            "Invalid input, not a recognized performance data format!", file=sys.stderr
        )
        return 1

    if not args.silent:
        ruamel.yaml.YAML(typ="rt").dump(data, sys.stdout)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
