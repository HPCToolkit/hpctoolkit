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

import collections.abc
import copy
import dataclasses
import functools
import io
import math
import os
from pathlib import Path

import pytest
import ruamel.yaml

from .. import from_path_extended, v4
from .._test_util import testdatadir, yaml
from ..base import canonical_paths, get_from_path
from ..v4._test_data import v4_data_small
from .base import DiffStrategy
from .strict import *


def all_objects(obj):
    if dataclasses.is_dataclass(obj):
        return {obj}.union(
            *[all_objects(getattr(obj, field.name)) for field in dataclasses.fields(obj)]
        )
    if isinstance(obj, collections.abc.Mapping):
        return set().union(*[all_objects(k).union(all_objects(v)) for k, v in obj.items()])
    if isinstance(obj, collections.abc.Iterable):
        return set().union(*[all_objects(v) for v in obj])
    return set()


@pytest.mark.parametrize("attr", [None, "meta", "profile", "context", "trace"])
def test_no_diff(yaml, v4_data_small, attr):
    db = v4_data_small
    if attr:
        db = getattr(db, attr)
    diff = StrictDiff(db, copy.deepcopy(db))
    del db

    full_map = {x: get_from_path(diff.b, p) for x, p in canonical_paths(diff.a).items()}
    if attr is None:
        for i in (4, 8, 10, 11, 14, 18, 19, 21):
            del full_map[diff.a.context.CtxInfos.contexts[i]]

    assert len(list(diff.contexts())) == 1
    assert set(diff.best_context().keys()) == set(full_map.keys())
    assert diff.best_context() == full_map
    assert len(diff.hunks) == 0


def test_small_diff(capsys, yaml, tmp_path, v4_data_small):
    # Alter the small database in particular ways for testing purposes
    altered = copy.deepcopy(v4_data_small)
    altered.meta.IdNames.names[-1] = "FOO"
    altered.meta.Metrics.scopes.append(
        v4.metadb.PropagationScope(
            scopeName="foo", type=v4.metadb.PropagationScope.Type.custom, propagationIndex=255
        )
    )
    altered.meta.Context.entryPoints[1].children[0].children[0].children[
        1
    ].function = altered.meta.Functions.functions[2]

    with io.StringIO() as buf:
        StrictDiff(v4_data_small, altered).render(buf)
        assert (
            buf.getvalue()
            == """\
 context:
   CtxInfos:
     contexts[5]:   # for -call> [function] foo  #5
-      !cct.db/v4/PerContext
-      values:
-        16:  # for function  #16
-          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
-        1: {} # for function  #1
     contexts[5]:   # for -call> [function] main  #5
+      !cct.db/v4/PerContext
+      values:
+        16:  # for function  #16
+          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
+        1: {} # for function  #1
     contexts[6]:   # for -lexical> [loop] /foo.c:1464  #6
-      !cct.db/v4/PerContext
-      values:
-        16:  # for function  #16
-          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
-        1: {} # for function  #1
+      !cct.db/v4/PerContext
+      values:
+        16:  # for function  #16
+          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
+        1: {} # for function  #1
     contexts[7]:   # for -lexical> [line] /foo.c:1464  #7
-      !cct.db/v4/PerContext
-      values:
-        16:  # for function  #16
-          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
-        1: {} # for function  #1
+      !cct.db/v4/PerContext
+      values:
+        16:  # for function  #16
+          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
+        1: {} # for function  #1
     contexts[9]:   # for -lexical> [line] /foo.c:1464  #9
-      !cct.db/v4/PerContext
-      values: {}
+      !cct.db/v4/PerContext
+      values: {}
 meta:
   Context:
     entryPoints[1]:   # main thread (= main_thread)  #1
       children[0]:   # -call> [function] main  #2
         children[0]:   # -lexical> [line] /foo.c:1464  #3
           children[1]:   # -call> [function] foo  #5
-            !meta.db/v4/Context
-            ctxId: 5
-            flags: !meta.db/v4/Context.Flags [hasFunction]
-            relation: !meta.db/v4/Context.Relation call
-            lexicalType: !meta.db/v4/Context.LexicalType function
-            propagation: 0
-            function: !meta.db/v4/Function  # foo /foo.c:3 /foo+0x1129 []
-              name: foo
-              module: !meta.db/v4/Module  # /foo []
-                flags: !meta.db/v4/Module.Flags []
-                path: /tmp/foo
-              offset: 4393
-              file: &id001 !meta.db/v4/File  # /foo.c [copied]
-                flags: !meta.db/v4/File.Flags [copied]
-                path: src/tmp/foo.c
-              line: 3
-              flags: !meta.db/v4/Function.Flags []
-            file:
-            line:
-            module:
-            offset:
-            children:
-            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9
-              ctxId: 9
-              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexicalType: !meta.db/v4/Context.LexicalType line
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children: []
-            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6
-              ctxId: 6
-              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexicalType: !meta.db/v4/Context.LexicalType loop
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children:
-              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7
-                ctxId: 7
-                flags: !meta.db/v4/Context.Flags [hasSrcLoc]
-                relation: !meta.db/v4/Context.Relation lexical
-                lexicalType: !meta.db/v4/Context.LexicalType line
-                propagation: 1
-                function:
-                file: *id001
-                line: 1464
-                module:
-                offset:
-                children: []
           children[1]:   # -call> [function] main  #5
+            !meta.db/v4/Context
+            ctxId: 5
+            flags: !meta.db/v4/Context.Flags [hasFunction]
+            relation: !meta.db/v4/Context.Relation call
+            lexicalType: !meta.db/v4/Context.LexicalType function
+            propagation: 0
+            function: !meta.db/v4/Function  # main /foo.c:12 /foo+0x1183 []
+              name: main
+              module: !meta.db/v4/Module  # /foo []
+                flags: !meta.db/v4/Module.Flags []
+                path: /tmp/foo
+              offset: 4483
+              file: &id001 !meta.db/v4/File  # /foo.c [copied]
+                flags: !meta.db/v4/File.Flags [copied]
+                path: src/tmp/foo.c
+              line: 12
+              flags: !meta.db/v4/Function.Flags []
+            file:
+            line:
+            module:
+            offset:
+            children:
+            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9
+              ctxId: 9
+              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexicalType: !meta.db/v4/Context.LexicalType line
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children: []
+            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6
+              ctxId: 6
+              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexicalType: !meta.db/v4/Context.LexicalType loop
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children:
+              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7
+                ctxId: 7
+                flags: !meta.db/v4/Context.Flags [hasSrcLoc]
+                relation: !meta.db/v4/Context.Relation lexical
+                lexicalType: !meta.db/v4/Context.LexicalType line
+                propagation: 1
+                function:
+                file: *id001
+                line: 1464
+                module:
+                offset:
+                children: []
   IdNames:
     !meta.db/v4/IdentifierNames
      names:
      - SUMMARY
      - NODE
      - RANK
      - THREAD
      - GPUDEVICE
      - GPUCONTEXT
      - GPUSTREAM
-     - CORE
?       ^ ^^
+     - FOO
?       ^ ^
   Metrics:
     scopes[3]:   # foo [custom]
+      !meta.db/v4/PropagationScope
+      scopeName: foo
+      type: !meta.db/v4/PropagationScope.Type custom
+      propagationIndex: 255
 trace:
   CtxTraces:
     traces[0]:   # 0.460766000s (1656693533.987950087-1656693534.448715925) for {NODE 0 [0x7f0101] / THREAD 0}
       line[0]:   # +0.000000000s at -lexical> [line] /foo.c:1464  #9
         !trace.db/v4/ContextTraceElement
          timestamp: 1656693533987950000
          ctxId: 9
"""
        )

    with io.StringIO() as buf:
        StrictDiff(altered, v4_data_small).render(buf)
        assert (
            buf.getvalue()
            == """\
 context:
   CtxInfos:
     contexts[5]:   # for -call> [function] foo  #5
+      !cct.db/v4/PerContext
+      values:
+        16:  # for function  #16
+          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
+        1: {} # for function  #1
     contexts[5]:   # for -call> [function] main  #5
-      !cct.db/v4/PerContext
-      values:
-        16:  # for function  #16
-          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
-        1: {} # for function  #1
     contexts[6]:   # for -lexical> [loop] /foo.c:1464  #6
-      !cct.db/v4/PerContext
-      values:
-        16:  # for function  #16
-          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
-        1: {} # for function  #1
+      !cct.db/v4/PerContext
+      values:
+        16:  # for function  #16
+          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
+        1: {} # for function  #1
     contexts[7]:   # for -lexical> [line] /foo.c:1464  #7
-      !cct.db/v4/PerContext
-      values:
-        16:  # for function  #16
-          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
-        1: {} # for function  #1
+      !cct.db/v4/PerContext
+      values:
+        16:  # for function  #16
+          1: 1000000000.0  # for {NODE 0 [0x7f0101] / THREAD 0}
+        1: {} # for function  #1
     contexts[9]:   # for -lexical> [line] /foo.c:1464  #9
-      !cct.db/v4/PerContext
-      values: {}
+      !cct.db/v4/PerContext
+      values: {}
 meta:
   Context:
     entryPoints[1]:   # main thread (= main_thread)  #1
       children[0]:   # -call> [function] main  #2
         children[0]:   # -lexical> [line] /foo.c:1464  #3
           children[1]:   # -call> [function] foo  #5
+            !meta.db/v4/Context
+            ctxId: 5
+            flags: !meta.db/v4/Context.Flags [hasFunction]
+            relation: !meta.db/v4/Context.Relation call
+            lexicalType: !meta.db/v4/Context.LexicalType function
+            propagation: 0
+            function: !meta.db/v4/Function  # foo /foo.c:3 /foo+0x1129 []
+              name: foo
+              module: !meta.db/v4/Module  # /foo []
+                flags: !meta.db/v4/Module.Flags []
+                path: /tmp/foo
+              offset: 4393
+              file: &id001 !meta.db/v4/File  # /foo.c [copied]
+                flags: !meta.db/v4/File.Flags [copied]
+                path: src/tmp/foo.c
+              line: 3
+              flags: !meta.db/v4/Function.Flags []
+            file:
+            line:
+            module:
+            offset:
+            children:
+            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9
+              ctxId: 9
+              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexicalType: !meta.db/v4/Context.LexicalType line
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children: []
+            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6
+              ctxId: 6
+              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexicalType: !meta.db/v4/Context.LexicalType loop
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children:
+              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7
+                ctxId: 7
+                flags: !meta.db/v4/Context.Flags [hasSrcLoc]
+                relation: !meta.db/v4/Context.Relation lexical
+                lexicalType: !meta.db/v4/Context.LexicalType line
+                propagation: 1
+                function:
+                file: *id001
+                line: 1464
+                module:
+                offset:
+                children: []
           children[1]:   # -call> [function] main  #5
-            !meta.db/v4/Context
-            ctxId: 5
-            flags: !meta.db/v4/Context.Flags [hasFunction]
-            relation: !meta.db/v4/Context.Relation call
-            lexicalType: !meta.db/v4/Context.LexicalType function
-            propagation: 0
-            function: !meta.db/v4/Function  # main /foo.c:12 /foo+0x1183 []
-              name: main
-              module: !meta.db/v4/Module  # /foo []
-                flags: !meta.db/v4/Module.Flags []
-                path: /tmp/foo
-              offset: 4483
-              file: &id001 !meta.db/v4/File  # /foo.c [copied]
-                flags: !meta.db/v4/File.Flags [copied]
-                path: src/tmp/foo.c
-              line: 12
-              flags: !meta.db/v4/Function.Flags []
-            file:
-            line:
-            module:
-            offset:
-            children:
-            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9
-              ctxId: 9
-              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexicalType: !meta.db/v4/Context.LexicalType line
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children: []
-            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6
-              ctxId: 6
-              flags: !meta.db/v4/Context.Flags [hasSrcLoc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexicalType: !meta.db/v4/Context.LexicalType loop
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children:
-              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7
-                ctxId: 7
-                flags: !meta.db/v4/Context.Flags [hasSrcLoc]
-                relation: !meta.db/v4/Context.Relation lexical
-                lexicalType: !meta.db/v4/Context.LexicalType line
-                propagation: 1
-                function:
-                file: *id001
-                line: 1464
-                module:
-                offset:
-                children: []
   IdNames:
     !meta.db/v4/IdentifierNames
      names:
      - SUMMARY
      - NODE
      - RANK
      - THREAD
      - GPUDEVICE
      - GPUCONTEXT
      - GPUSTREAM
-     - FOO
?       ^ ^
+     - CORE
?       ^ ^^
   Metrics:
     scopes[3]:   # foo [custom]
-      !meta.db/v4/PropagationScope
-      scopeName: foo
-      type: !meta.db/v4/PropagationScope.Type custom
-      propagationIndex: 255
 trace:
   CtxTraces:
     traces[0]:   # 0.460766000s (1656693533.987950087-1656693534.448715925) for {NODE 0 [0x7f0101] / THREAD 0}
       line[0]:   # +0.000000000s at -lexical> [line] /foo.c:1464  #9
         !trace.db/v4/ContextTraceElement
          timestamp: 1656693533987950000
          ctxId: 9
"""
        )


class DummyDiff(DiffStrategy):
    def __init__(self):
        super().__init__(None, None)

    def contexts(self):
        yield {}

    @property
    def hunks(self):
        return []

    def render(self, out):
        pass


def test_float_compare():
    # NB: We do all this as if the calculations were done with a 4-bit mantissa. This makes the
    # ULPs nice and big for the purposes of testing.
    prec = 4
    ulp0 = math.pow(2, -prec)
    ulp1 = 2 * ulp0

    # Test the edge cases
    sa = StrictAccuracy(DummyDiff(), grace=1, precision=prec)
    assert sa._float_cmp(0.0, -0.0) == 0.0
    assert sa._float_cmp(math.nextafter(0, -1), math.nextafter(0, 1)) == sa._cmp_bad_sign
    assert sa._float_cmp(0.5, 2.0) == sa._cmp_exp_diff

    # Test the boundaries in the normalized region
    sa = StrictAccuracy(DummyDiff(), grace=1, precision=prec)
    assert sa._float_cmp(0.5, 0.5) == 0.0
    assert sa._float_cmp(0.5, math.nextafter(0.5, 1)) == 0.0
    assert sa._float_cmp(0.5, 0.5 + ulp0) == 0.0
    assert sa._float_cmp(0.5, math.nextafter(0.5 + ulp0, 1)) == pytest.approx(ulp0)
    sa = StrictAccuracy(DummyDiff(), grace=2, precision=prec)
    assert sa._float_cmp(0.5, 0.5) == 0.0
    assert sa._float_cmp(0.5, math.nextafter(0.5, 1)) == 0.0
    assert sa._float_cmp(0.5, 0.5 + 2 * ulp0) == 0.0
    assert sa._float_cmp(0.5, math.nextafter(0.5 + 2 * ulp0, 1)) == pytest.approx(2 * ulp0)

    # Test the boundaries across neighboring exponents
    sa = StrictAccuracy(DummyDiff(), grace=1, precision=prec)
    assert sa._float_cmp(math.nextafter(1.0, 0), math.nextafter(1.0 + ulp1, 0)) == 0.0
    assert sa._float_cmp(math.nextafter(1.0, 0), 1.0 + ulp1) == pytest.approx(ulp0)
    sa = StrictAccuracy(DummyDiff(), grace=2, precision=prec)
    assert sa._float_cmp(math.nextafter(1.0, 0), math.nextafter(1.0 + 2 * ulp1, 0)) == 0.0
    assert sa._float_cmp(math.nextafter(1.0, 0), 1.0 + 2 * ulp1) == pytest.approx(2 * ulp0)


def test_accuracy_of_invalid_diff():
    sa = StrictAccuracy(DummyDiff())
    assert sa.inaccuracy == 1.0


@pytest.mark.parametrize(
    "attr,vcnt", [(None, 48), ("meta", 0), ("profile", 36), ("context", 18), ("trace", 0)]
)
def test_no_diff_accuracy(yaml, v4_data_small, attr, vcnt):
    db = v4_data_small
    if attr:
        db = getattr(db, attr)
    diff = StrictDiff(db, copy.deepcopy(db))
    del db

    acc = StrictAccuracy(diff)
    assert acc.total_cnt == vcnt
    assert acc.failed_cnt == 0
    assert acc.inaccuracy == (1.0 if vcnt == 0 else 0.0)

    with io.StringIO() as buf:
        acc.render(buf)
        out = buf.getvalue().splitlines()
    assert len(out) == (2 if vcnt == 0 else 1)
    assert out[0] == f"Identified {100 if vcnt == 0 else 0:.2f}% inaccuracies in {vcnt:d} values"
    if vcnt == 0:
        assert out[1] == "  No values were compared!"
