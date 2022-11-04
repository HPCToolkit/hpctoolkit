#!/usr/bin/env python3

import sys

from hpctoolkit.formats import from_path  # noqa: import-error
from hpctoolkit.formats.diff.strict import StrictAccuracy, StrictDiff  # noqa: import-error
from hpctoolkit.test.execution import hpcprof, thread_disruptive  # noqa: import-error
from hpctoolkit.test.tarball import extracted  # noqa: import-error

with extracted(sys.argv[2]) as dbase:
    base = from_path(dbase)

with extracted(sys.argv[1]) as meas:
    for threadcnt in [1, 4, 16, 64]:
        with thread_disruptive():
            with hpcprof(meas, f"-j{threadcnt:d}", "--foreign") as db:
                diff = StrictDiff(base, from_path(db.basedir))
                acc = StrictAccuracy(diff)
                if len(diff.hunks) > 0 or acc.inaccuracy:
                    diff.render(sys.stdout)
                    acc.render(sys.stdout)
                    sys.exit(1)
