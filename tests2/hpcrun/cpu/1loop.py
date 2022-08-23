#!/usr/bin/env python3

import sys

from hpctoolkit.test.execution import hpcrun  # noqa: import-error

with hpcrun(sys.argv[1]) as M:
    M.check_standard(threads_per_proc=4)
