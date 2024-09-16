#!/bin/sh

# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

# The following is run by sh and ignored by Python.
""":"
for trial in python3.12 python3.11 python3.10 python3.9 python3.8 python3 python; do
  if command -v "$trial" > /dev/null 2>&1; then
    exec "$trial" "$0" "$@"
  fi
done
exit 127
"""  # noqa: D400, D415

# pylint: disable=invalid-name

import argparse
import collections.abc
import configparser
import contextlib
import copy
import hashlib
import json
import linecache
import re
import typing
from pathlib import Path


def _collect_wrap_subprojects(project_root: Path) -> typing.Iterator[Path]:
    subprojects_root = project_root / "subprojects"
    for trial in subprojects_root.iterdir():
        if trial.suffix == ".wrap":
            cfg = configparser.ConfigParser(interpolation=None)
            cfg.read(trial, encoding="utf-8")
            directory = cfg.get(
                cfg.sections()[0], "directory", fallback=str(trial.stem)
            )
            yield subprojects_root / directory


def _severity(mode: str, messagekind: str, flags: str) -> str:
    if mode == "cc":
        if messagekind == "error":
            return "blocker"  # Compile failure, merge disallowed
        if messagekind == "warning":
            return "critical"  # Highest possible severity without blocking merge
        raise ValueError(messagekind)
    if mode == "clang-tidy":
        if messagekind == "error":
            return "critical"  # Highest possible severity without blocking merge
        if messagekind != "warning":
            raise ValueError(messagekind)
        if (
            "clang-analyzer-" in flags
            and "clang-analyzer-optin.performance" not in flags
        ):
            return "critical"  # Clang-analyzer reports are basically compiler warnings
        if any(cat in flags for cat in ["modernize-", "readability-"]):
            return "minor"  # Stylistic and modernization errors are usually less problematic
        return "major"  # Middling severity for all others
    raise ValueError(mode)


def _cc_to_cq(
    mode: str, project_root: Path, exclude_dirs: typing.List[Path], line: str
) -> dict:
    # Compiler warning regex:
    #     {path}.{extension}:{line}:{column}: {severity}: {message} [{flag(s)}]
    mat = re.fullmatch(
        r"^(.*)\.([a-z+]{1,3}):(\d+):(\d+:)?\s+(warning|error):\s+(.*)\s+\[([^]]*)\]$",
        line.strip("\n"),
    )
    if not mat:
        return {}

    report: typing.Dict[str, typing.Any] = {
        "type": "issue",
        "check_name": mat.group(7),  # flag(2)
        "description": mat.group(6),  # message
        "categories": ["compiler"],
        "location": {},
    }

    path = Path(mat.group(1) + "." + mat.group(2))
    if not path.is_absolute():
        # Strip off prefixes and see if we can find the file we're looking for
        for i in range(1, len(path.parts) - 1):
            newpath = project_root / Path(*path.parts[i:])
            if newpath.is_file():
                path = newpath
                break
        else:
            return {}
    for exdir in exclude_dirs:
        try:
            path.relative_to(exdir)
            return {}
        except ValueError:
            pass
    try:
        report["location"]["path"] = path.resolve().relative_to(project_root).as_posix()
    except ValueError:
        return {}

    lineno = int(mat.group(3))
    if len(mat.group(4)) > 0:
        col = int(mat.group(4)[:-1])
        report["location"]["positions"] = {"begin": {"line": lineno, "column": col}}
    else:
        report["location"]["lines"] = {"begin": lineno}

    report["severity"] = _severity(mode, mat.group(5), mat.group(7))

    # The fingerprint is the hash of the report in JSON form, with parts masked out
    masked_report = copy.deepcopy(report)
    masked_report["location"]["positions"] = None
    masked_report["location"]["lines"] = None
    masked_report["location"]["line"] = linecache.getline(str(path), lineno)
    masked_report["location"]["column"] = (
        report["location"].get("positions", {}).get("begin", {}).get("column")
    )
    masked_report["description"] = None
    report["fingerprint"] = hashlib.md5(
        json.dumps(masked_report, sort_keys=True).encode("utf-8")
    ).hexdigest()
    return report


class Diagnostics(typing.NamedTuple):
    """Result from cc_diagnotics"""

    reports: list
    warnings: int
    errors: int


def cc_diagnostics(
    mode: str, project_root: Path, logfiles: collections.abc.Sequence
) -> Diagnostics:
    """Main function"""

    exclude_dirs = list(_collect_wrap_subprojects(project_root))

    reports = []
    warnings, errors = 0, 0
    for logfile in logfiles:
        for line in logfile:
            cq = _cc_to_cq(mode, project_root, exclude_dirs, line)
            if cq:
                reports.append(cq)
            elif re.match(r"[^:]+:(\d+:){1,2}\s+warning:", line):  # Warning from GCC
                warnings += 1
            elif re.match(r"[^:]+:(\d+:){1,2}\s+error:", line) or re.match(
                r"[^:]+:\s+undefined reference to", line
            ):  # Error from GCC or ld
                errors += 1

    # Unique the reports by their fingerprints before saving. We don't care which report we end
    # up with so long as we get one.
    reports_by_fp = {rep["fingerprint"]: rep for rep in reports}
    reports = list(reports_by_fp.values())
    del reports_by_fp

    return Diagnostics(reports, warnings, errors)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["cc", "clang-tidy"])
    parser.add_argument("output", type=Path)
    parser.add_argument("logfiles", type=Path, nargs="+")
    args = parser.parse_args()
    with contextlib.ExitStack() as es:
        diagnostics = cc_diagnostics(
            args.mode,
            Path(".").resolve(strict=True),
            tuple(es.enter_context(open(lf, encoding="utf-8")) for lf in args.logfiles),
        )
    with open(args.output, "w", encoding="utf-8") as outf:
        json.dump(diagnostics.reports, outf)
    print(f"Generated {len(diagnostics.reports):d} Code Climate reports")
    if diagnostics.errors > 0:
        print(
            f"Excluded {diagnostics.errors:d} errors and {diagnostics.warnings:d} warnings"
        )
    elif diagnostics.warnings > 0:
        print(f"Excluded {diagnostics.warnings:d} warnings")
