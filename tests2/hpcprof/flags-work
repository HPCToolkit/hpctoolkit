#!/usr/bin/env python3

import sys

from hpctoolkit.test.execution import Measurements, hpcprof

meas = Measurements(sys.argv[1])
has_traces = any(meas.tracefile(stem) for stem in meas.thread_stems)

with hpcprof(meas) as db:
    db.check_standard(tracedb=has_traces)

with hpcprof(meas, "--no-traces") as db:
    db.check_standard(tracedb=False)
