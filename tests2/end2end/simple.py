#!/usr/bin/env python3

import sys

from hpctoolkit.test.execution import hpcprof, hpcrun, hpcstruct

with hpcrun(sys.argv[1]) as M:
    hpcstruct(M)

    with hpcprof(M) as db:
        db.check_standard(tracedb=False)
