## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2022-2022, Rice University
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
##
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage.
##
## ******************************************************* EndRiceCopyright *

import dataclasses
import io
import os
import sys
from pathlib import Path

import pytest
import ruamel.yaml
from hpctoolkit.test.tarball import extracted

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .tracedb import *


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).trace

    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db:
        with open(Path(db) / "trace.db", "rb") as f:
            got = TraceDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_yaml_rt(yaml):
    orig = yaml.load(
        """
        !trace.db/v4
        CtxTraces: !trace.db/v4/ContextTraceHeaders
            timestampRange:
                min: 42
                max: 48
            traces:
              - !trace.db/v4/ContextTrace
                profIndex: 1
                line:
                - !trace.db/v4/ContextTraceElement {ctxId: 0, timestamp: 42}
                - !trace.db/v4/ContextTraceElement {ctxId: 1, timestamp: 44}
              - !trace.db/v4/ContextTrace
                profIndex: 2
                line:
                - !trace.db/v4/ContextTraceElement {ctxId: 3, timestamp: 46}
                - !trace.db/v4/ContextTraceElement {ctxId: 4, timestamp: 48}
    """
    )

    orig_s = dump_to_string(yaml, orig)
    assert orig_s == dump_to_string(yaml, yaml.load(orig_s))


def test_traversaltest_yaml_rt(yaml):
    assert_good_traversal(
        yaml.load(
            """
        !trace.db/v4
        CtxTraces: !trace.db/v4/ContextTraceHeaders
            timestampRange:
                min: 42
                max: 48
            traces:
              - !trace.db/v4/ContextTrace
                profIndex: 1
                line:
                - !trace.db/v4/ContextTraceElement {ctxId: 0, timestamp: 42}
                - !trace.db/v4/ContextTraceElement {ctxId: 1, timestamp: 44}
              - !trace.db/v4/ContextTrace
                profIndex: 2
                line:
                - !trace.db/v4/ContextTraceElement {ctxId: 3, timestamp: 46}
                - !trace.db/v4/ContextTraceElement {ctxId: 4, timestamp: 48}
    """
        )
    )
