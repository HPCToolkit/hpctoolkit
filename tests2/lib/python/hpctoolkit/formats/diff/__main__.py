## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2022-2022, Rice University
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
##
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage.
##
## ******************************************************* EndRiceCopyright *

import argparse
import sys
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


def main(inargs: list[str]) -> int:
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
