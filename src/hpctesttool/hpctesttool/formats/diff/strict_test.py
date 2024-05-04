# SPDX-FileCopyrightText: 2022-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import collections.abc
import copy
import dataclasses
import io
import math
import sys
import typing

import pytest

from .. import v4
from .._test_util import yaml
from ..base import DatabaseBase, canonical_paths, get_from_path
from ..v4._test_data import v4_data_small
from .base import DiffStrategy
from .strict import CmpError, StrictAccuracy, StrictDiff

_ = yaml, v4_data_small


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
def test_no_diff(v4_data_small, attr):
    db = v4_data_small
    if attr:
        db = getattr(db, attr)
    diff = StrictDiff(db, copy.deepcopy(db))
    del db

    full_map = {x: get_from_path(diff.b, p) for x, p in canonical_paths(diff.a).items()}
    if attr is None:
        assert isinstance(diff.a, v4.Database)
        for i in (4, 8, 10, 11, 14, 18, 19, 21):
            del full_map[diff.a.context.ctx_infos.contexts[i]]

    assert len(list(diff.contexts())) == 1
    assert set(diff.best_context().keys()) == set(full_map.keys())
    assert diff.best_context() == full_map
    assert len(diff.hunks) == 0


def test_small_diff(v4_data_small):
    # Alter the small database in particular ways for testing purposes
    altered = copy.deepcopy(v4_data_small)
    altered.meta.id_names.names[-1] = "FOO"
    altered.meta.metrics.scopes.append(
        v4.metadb.PropagationScope(
            scope_name="foo", type=v4.metadb.PropagationScope.Type.custom, propagation_index=255
        )
    )
    altered.meta.context.entry_points[1].children[0].children[0].children[1].function = (
        altered.meta.functions.functions[2]
    )

    with io.StringIO() as buf:
        StrictDiff(v4_data_small, altered).render(buf)
        assert (
            buf.getvalue()
            == """\
 context:
   ctx_infos:
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
     contexts[6]:   # for -lexical> [loop] /foo.c:1464  #6 ^M[0]
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
     contexts[7]:   # for -lexical> [line] /foo.c:1464  #7 ^M[0]
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
     contexts[9]:   # for -lexical> [line] /foo.c:1464  #9 ^M[0]
-      !cct.db/v4/PerContext
-      values: {}
+      !cct.db/v4/PerContext
+      values: {}
 meta:
   context:
     entry_points[1]:   # main thread (= main_thread)  #1
       children[0]:   # -call> [function] main  #2
         children[0]:   # -lexical> [line] /foo.c:1464  #3 ^M[0]
           children[1]:   # -call> [function] foo  #5
-            !meta.db/v4/Context
-            ctx_id: 5
-            flags: !meta.db/v4/Context.Flags [has_function]
-            relation: !meta.db/v4/Context.Relation call
-            lexical_type: !meta.db/v4/Context.LexicalType function
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
-            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9 ^M[0]
-              ctx_id: 9
-              flags: !meta.db/v4/Context.Flags [has_srcloc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexical_type: !meta.db/v4/Context.LexicalType line
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children: []
-            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6 ^M[0]
-              ctx_id: 6
-              flags: !meta.db/v4/Context.Flags [has_srcloc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexical_type: !meta.db/v4/Context.LexicalType loop
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children:
-              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7 ^M[0]
-                ctx_id: 7
-                flags: !meta.db/v4/Context.Flags [has_srcloc]
-                relation: !meta.db/v4/Context.Relation lexical
-                lexical_type: !meta.db/v4/Context.LexicalType line
-                propagation: 1
-                function:
-                file: *id001
-                line: 1464
-                module:
-                offset:
-                children: []
           children[1]:   # -call> [function] main  #5
+            !meta.db/v4/Context
+            ctx_id: 5
+            flags: !meta.db/v4/Context.Flags [has_function]
+            relation: !meta.db/v4/Context.Relation call
+            lexical_type: !meta.db/v4/Context.LexicalType function
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
+            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9 ^M[0]
+              ctx_id: 9
+              flags: !meta.db/v4/Context.Flags [has_srcloc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexical_type: !meta.db/v4/Context.LexicalType line
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children: []
+            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6 ^M[0]
+              ctx_id: 6
+              flags: !meta.db/v4/Context.Flags [has_srcloc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexical_type: !meta.db/v4/Context.LexicalType loop
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children:
+              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7 ^M[0]
+                ctx_id: 7
+                flags: !meta.db/v4/Context.Flags [has_srcloc]
+                relation: !meta.db/v4/Context.Relation lexical
+                lexical_type: !meta.db/v4/Context.LexicalType line
+                propagation: 1
+                function:
+                file: *id001
+                line: 1464
+                module:
+                offset:
+                children: []
   id_names:
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
   metrics:
     scopes[3]:   # foo [custom]
+      !meta.db/v4/PropagationScope
+      scope_name: foo
+      type: !meta.db/v4/PropagationScope.Type custom
+      propagation_index: 255
 trace:
   ctx_traces:
     traces[0]:   # 0.460766000s (1656693533.987950087-1656693534.448715925) for {NODE 0 [0x7f0101] / THREAD 0}
       line[0]:   # +0.000000000s at -lexical> [line] /foo.c:1464  #9 ^M[0]
         !trace.db/v4/ContextTraceElement
          timestamp: 1656693533987950000
          ctx_id: 9
"""
        )

    with io.StringIO() as buf:
        StrictDiff(altered, v4_data_small).render(buf)
        assert (
            buf.getvalue()
            == """\
 context:
   ctx_infos:
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
     contexts[6]:   # for -lexical> [loop] /foo.c:1464  #6 ^M[0]
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
     contexts[7]:   # for -lexical> [line] /foo.c:1464  #7 ^M[0]
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
     contexts[9]:   # for -lexical> [line] /foo.c:1464  #9 ^M[0]
-      !cct.db/v4/PerContext
-      values: {}
+      !cct.db/v4/PerContext
+      values: {}
 meta:
   context:
     entry_points[1]:   # main thread (= main_thread)  #1
       children[0]:   # -call> [function] main  #2
         children[0]:   # -lexical> [line] /foo.c:1464  #3 ^M[0]
           children[1]:   # -call> [function] foo  #5
+            !meta.db/v4/Context
+            ctx_id: 5
+            flags: !meta.db/v4/Context.Flags [has_function]
+            relation: !meta.db/v4/Context.Relation call
+            lexical_type: !meta.db/v4/Context.LexicalType function
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
+            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9 ^M[0]
+              ctx_id: 9
+              flags: !meta.db/v4/Context.Flags [has_srcloc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexical_type: !meta.db/v4/Context.LexicalType line
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children: []
+            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6 ^M[0]
+              ctx_id: 6
+              flags: !meta.db/v4/Context.Flags [has_srcloc]
+              relation: !meta.db/v4/Context.Relation lexical
+              lexical_type: !meta.db/v4/Context.LexicalType loop
+              propagation: 1
+              function:
+              file: *id001
+              line: 1464
+              module:
+              offset:
+              children:
+              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7 ^M[0]
+                ctx_id: 7
+                flags: !meta.db/v4/Context.Flags [has_srcloc]
+                relation: !meta.db/v4/Context.Relation lexical
+                lexical_type: !meta.db/v4/Context.LexicalType line
+                propagation: 1
+                function:
+                file: *id001
+                line: 1464
+                module:
+                offset:
+                children: []
           children[1]:   # -call> [function] main  #5
-            !meta.db/v4/Context
-            ctx_id: 5
-            flags: !meta.db/v4/Context.Flags [has_function]
-            relation: !meta.db/v4/Context.Relation call
-            lexical_type: !meta.db/v4/Context.LexicalType function
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
-            - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #9 ^M[0]
-              ctx_id: 9
-              flags: !meta.db/v4/Context.Flags [has_srcloc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexical_type: !meta.db/v4/Context.LexicalType line
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children: []
-            - !meta.db/v4/Context # -lexical> [loop] /foo.c:1464  #6 ^M[0]
-              ctx_id: 6
-              flags: !meta.db/v4/Context.Flags [has_srcloc]
-              relation: !meta.db/v4/Context.Relation lexical
-              lexical_type: !meta.db/v4/Context.LexicalType loop
-              propagation: 1
-              function:
-              file: *id001
-              line: 1464
-              module:
-              offset:
-              children:
-              - !meta.db/v4/Context  # -lexical> [line] /foo.c:1464  #7 ^M[0]
-                ctx_id: 7
-                flags: !meta.db/v4/Context.Flags [has_srcloc]
-                relation: !meta.db/v4/Context.Relation lexical
-                lexical_type: !meta.db/v4/Context.LexicalType line
-                propagation: 1
-                function:
-                file: *id001
-                line: 1464
-                module:
-                offset:
-                children: []
   id_names:
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
   metrics:
     scopes[3]:   # foo [custom]
-      !meta.db/v4/PropagationScope
-      scope_name: foo
-      type: !meta.db/v4/PropagationScope.Type custom
-      propagation_index: 255
 trace:
   ctx_traces:
     traces[0]:   # 0.460766000s (1656693533.987950087-1656693534.448715925) for {NODE 0 [0x7f0101] / THREAD 0}
       line[0]:   # +0.000000000s at -lexical> [line] /foo.c:1464  #9 ^M[0]
         !trace.db/v4/ContextTraceElement
          timestamp: 1656693533987950000
          ctx_id: 9
"""
        )


class DummyDatabase(DatabaseBase):
    @classmethod
    def from_dir(cls, dbdir):
        raise NotImplementedError


class DummyDiff(DiffStrategy):
    def __init__(self):
        super().__init__(DummyDatabase(), DummyDatabase())

    def contexts(self):
        yield {}

    @property
    def hunks(self):
        return []

    def render(self, out):
        pass


@pytest.mark.skipif(
    sys.version_info < (3, 9), reason="Precise floating-point math requires Python 3.9"
)
def test_float_compare():
    if typing.TYPE_CHECKING:

        def nextafter(x: float, y: float, steps: int = 1) -> float:
            return x + y / steps

    else:
        nextafter = math.nextafter  # novm

    # NB: We do all this as if the calculations were done with a 4-bit mantissa. This makes the
    # ULPs nice and big for the purposes of testing.
    prec = 4
    ulp0 = math.pow(2, -prec)
    ulp1 = 2 * ulp0

    # Test the edge cases
    sa = StrictAccuracy(DummyDiff(), grace=1, precision=prec)
    assert sa._float_cmp(0.0, -0.0) == 0.0
    assert sa._float_cmp(nextafter(0, -1), nextafter(0, 1)) == CmpError.bad_sign
    assert sa._float_cmp(0.5, 2.0) == CmpError.exp_diff

    # Test the boundaries in the normalized region
    sa = StrictAccuracy(DummyDiff(), grace=1, precision=prec)
    assert sa._float_cmp(0.5, 0.5) == 0.0
    assert sa._float_cmp(0.5, nextafter(0.5, 1)) == 0.0
    assert sa._float_cmp(0.5, 0.5 + ulp0) == 0.0
    assert sa._float_cmp(0.5, nextafter(0.5 + ulp0, 1)) == pytest.approx(ulp0)
    sa = StrictAccuracy(DummyDiff(), grace=2, precision=prec)
    assert sa._float_cmp(0.5, 0.5) == 0.0
    assert sa._float_cmp(0.5, nextafter(0.5, 1)) == 0.0
    assert sa._float_cmp(0.5, 0.5 + 2 * ulp0) == 0.0
    assert sa._float_cmp(0.5, nextafter(0.5 + 2 * ulp0, 1)) == pytest.approx(2 * ulp0)

    # Test the boundaries across neighboring exponents
    sa = StrictAccuracy(DummyDiff(), grace=1, precision=prec)
    assert sa._float_cmp(nextafter(1.0, 0), nextafter(1.0 + ulp1, 0)) == 0.0
    assert sa._float_cmp(nextafter(1.0, 0), 1.0 + ulp1) == pytest.approx(ulp0)
    sa = StrictAccuracy(DummyDiff(), grace=2, precision=prec)
    assert sa._float_cmp(nextafter(1.0, 0), nextafter(1.0 + 2 * ulp1, 0)) == 0.0
    assert sa._float_cmp(nextafter(1.0, 0), 1.0 + 2 * ulp1) == pytest.approx(2 * ulp0)


def test_accuracy_of_invalid_diff():
    sa = StrictAccuracy(DummyDiff())
    assert sa.inaccuracy == 1.0


@pytest.mark.parametrize(
    ("attr", "vcnt"), [(None, 48), ("meta", 0), ("profile", 36), ("context", 18), ("trace", 0)]
)
def test_no_diff_accuracy(v4_data_small, attr, vcnt):
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
