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


def main(rawargs: list[str]) -> int:
    args = build_parser().parse_args(rawargs)

    data = from_path_extended(args.src, subdir=args.subdir)
    if data is None:
        print("Invalid input, not a recognized performance data format!", file=sys.stderr)
        return 1

    if not args.silent:
        ruamel.yaml.YAML(typ="rt").dump(data, sys.stdout)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
