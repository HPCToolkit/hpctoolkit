#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description="Merge multiple code quality reports into one")
parser.add_argument("output", type=Path, help="Path to save the resulting merged report to")
parser.add_argument("report", nargs="*", help="Reports to merge together")
args = parser.parse_args()
del parser

reports_by_fingerprint = {}
for reportglob in args.report:
    for report in Path().glob(reportglob):
        with open(report, encoding="utf-8") as f:
            for issue in json.load(f):
                reports_by_fingerprint[issue["fingerprint"]] = issue

print(f"Collected {len(reports_by_fingerprint)} issues:")
for sev in ("info", "minor", "major", "critical", "blocker"):
    reps = [i for i in reports_by_fingerprint.values() if i["severity"] == sev]
    print(f"   {len(reps)} {sev} issues")

with open(args.output, "w", encoding="utf-8") as f:
    json.dump(list(reports_by_fingerprint.values()), f)
