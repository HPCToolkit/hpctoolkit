#!/usr/bin/env python3

import pickle
import sys

from hpctoolkit.formats import from_path  # noqa: import-error
from hpctoolkit.formats.diff.strict import StrictAccuracy, StrictDiff  # noqa: import-error
from hpctoolkit.test.execution import hpcprof_mpi  # noqa: import-error
from hpctoolkit.test.tarball import extracted  # noqa: import-error

rankcnt = int(sys.argv[1])
threadcnt = int(sys.argv[2])

with open(sys.argv[4], "rb") as f:
    base = pickle.load(f)

with hpcprof_mpi(rankcnt, sys.argv[3], f"-j{threadcnt:d}", "--foreign") as db:
    diff = StrictDiff(base, from_path(db.basedir))
    acc = StrictAccuracy(diff)
    if len(diff.hunks) > 0 or acc.inaccuracy:
        diff.render(sys.stdout)
        acc.render(sys.stdout)
        sys.exit(1)
