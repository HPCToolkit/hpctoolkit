#!/bin/sh
# The following is run by sh and ignored by Python.
""":"
for trial in python3.12 python3.11 python3.10 python3.9 python3.8 python3 python; do
  if command -v "$trial" > /dev/null 2>&1; then
    exec "$trial" "$0" "$@"
  fi
done
exit 127
"""  # noqa: D400, D415

import argparse
import collections.abc
import contextlib
import copy
import hashlib
import json
import linecache
import re
import typing
from pathlib import Path


def _gcc_to_cq(project_root: Path, line: str) -> dict:
    # Compiler warning regex:
    #     {path}.{extension}:{line}:{column}: {severity}: {message} [{flag(s)}]
    mat = re.fullmatch(
        r"^(.*)\.([a-z+]{1,3}):(\d+):(\d+:)?\s+(warning|error):\s+(.*)\s+\[((\w|-|=)*)\]$",
        line.strip("\n"),
    )
    if not mat:
        return {}

    report: dict[str, typing.Any] = {
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
    try:
        report["location"]["path"] = path.relative_to(project_root).as_posix()
    except ValueError:
        return {}

    lineno = int(mat.group(3))
    if len(mat.group(4)) > 0:
        col = int(mat.group(4)[:-1])
        report["location"]["positions"] = {"begin": {"line": lineno, "column": col}}
    else:
        report["location"]["lines"] = {"begin": lineno}

    if mat.group(5) == "warning":
        report["severity"] = "major"
    elif mat.group(5) == "error":
        report["severity"] = "critical"
    assert "severity" in report

    # The fingerprint is the hash of the report in JSON form, with parts masked out
    masked_report = copy.deepcopy(report)
    masked_report["location"]["positions"] = None
    masked_report["location"]["lines"] = None
    masked_report["location"]["line"] = linecache.getline(str(path), lineno)
    report["fingerprint"] = hashlib.md5(
        json.dumps(masked_report, sort_keys=True).encode("utf-8")
    ).hexdigest()
    return report


def cc_diagnostics(
    project_root: Path, output: typing.TextIO, logfiles: collections.abc.Sequence
) -> None:
    reports = []
    warnings, errors = 0, 0
    for logfile in logfiles:
        for line in logfile:
            cq = _gcc_to_cq(project_root, line)
            if cq:
                reports.append(cq)
                if cq["severity"] == "major":
                    warnings += 1
                elif cq["severity"] == "critical":
                    errors += 1
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

    json.dump(reports, output)
    if errors > 0:
        print(f"Found {errors:d} errors and {warnings:d} warnings in the logs")
    elif warnings > 0:
        print(f"Found {warnings:d} warnings in the logs")
    else:
        print("Found no diagnostics in the given logs")

    print(f"Generated {len(reports):d} Code Climate reports")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("logfiles", type=Path, nargs="+")
    args = parser.parse_args()
    with open(args.output, "w", encoding="utf-8") as outf, contextlib.ExitStack() as es:
        logfiles = tuple(es.enter_context(open(lf, encoding="utf-8")) for lf in args.logfiles)
        cc_diagnostics(Path(".").resolve(strict=True), outf, logfiles)
