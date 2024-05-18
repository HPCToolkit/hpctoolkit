# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import dataclasses

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .tracedb import TraceDB

_ = yaml


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).trace

    with (testdatadir / "dbase" / "small.d" / "trace.db").open("rb") as f:
        got = TraceDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_yaml_rt(yaml):
    orig = yaml.load(
        """
        !trace.db/v4
        ctx_traces: !trace.db/v4/ContextTraceHeaders
            timestamp_range:
                min: 42
                max: 48
            traces:
              - !trace.db/v4/ContextTrace
                prof_index: 1
                line:
                - !trace.db/v4/ContextTraceElement {ctx_id: 0, timestamp: 42}
                - !trace.db/v4/ContextTraceElement {ctx_id: 1, timestamp: 44}
              - !trace.db/v4/ContextTrace
                prof_index: 2
                line:
                - !trace.db/v4/ContextTraceElement {ctx_id: 3, timestamp: 46}
                - !trace.db/v4/ContextTraceElement {ctx_id: 4, timestamp: 48}
    """
    )

    orig_s = dump_to_string(yaml, orig)
    assert orig_s == dump_to_string(yaml, yaml.load(orig_s))


def test_traversaltest_yaml_rt(yaml):
    assert_good_traversal(
        yaml.load(
            """
        !trace.db/v4
        ctx_traces: !trace.db/v4/ContextTraceHeaders
            timestamp_range:
                min: 42
                max: 48
            traces:
              - !trace.db/v4/ContextTrace
                prof_index: 1
                line:
                - !trace.db/v4/ContextTraceElement {ctx_id: 0, timestamp: 42}
                - !trace.db/v4/ContextTraceElement {ctx_id: 1, timestamp: 44}
              - !trace.db/v4/ContextTrace
                prof_index: 2
                line:
                - !trace.db/v4/ContextTraceElement {ctx_id: 3, timestamp: 46}
                - !trace.db/v4/ContextTraceElement {ctx_id: 4, timestamp: 48}
    """
        )
    )
