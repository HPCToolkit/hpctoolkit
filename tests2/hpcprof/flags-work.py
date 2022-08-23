#!/usr/bin/env python3

import sys

from hpctoolkit.test.execution import Measurements, hpcprof  # noqa: import-error
from hpctoolkit.test.tarball import extracted  # noqa: import-error

with extracted(sys.argv[1]) as meas:
    meas = Measurements(meas)
    has_traces = any(meas.tracefile(stem) for stem in meas.thread_stems)

    with hpcprof(meas, "-j2") as db:
        db.check_standard(tracedb=has_traces)

    with hpcprof(meas, "-j2", "--no-traces") as db:
        db.check_standard(tracedb=False)
