# SPDX-FileCopyrightText: 2003-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import contextlib
import dataclasses
import io
import struct
import sys
import typing

import pytest

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .metadb import Context, EntryPoint, File, MetaDB, PropagationScope, SummaryStatistic, _Flex

_ = yaml


@contextlib.contextmanager
def recursionlimit(limit: int):
    old_limit = sys.getrecursionlimit()
    sys.setrecursionlimit(limit)
    try:
        yield
    finally:
        sys.setrecursionlimit(old_limit)


def test_enum_stat_combine_stringification(yaml):
    cls = SummaryStatistic.Combine
    assert dump_to_string(yaml, cls.sum).startswith(f"{cls.yaml_tag} sum")
    assert dump_to_string(yaml, cls.min).startswith(f"{cls.yaml_tag} min")
    assert dump_to_string(yaml, cls.max).startswith(f"{cls.yaml_tag} max")


def test_enum_scope_type_stringification(yaml):
    cls = PropagationScope.Type
    assert dump_to_string(yaml, cls.custom).startswith(f"{cls.yaml_tag} custom")
    assert dump_to_string(yaml, cls.point).startswith(f"{cls.yaml_tag} point")
    assert dump_to_string(yaml, cls.execution).startswith(f"{cls.yaml_tag} execution")
    assert dump_to_string(yaml, cls.transitive).startswith(f"{cls.yaml_tag} transitive")


def test_enum_entrypoint_stringification(yaml):
    cls = EntryPoint.EntryPoint
    assert dump_to_string(yaml, cls.unknown_entry).startswith(f"{cls.yaml_tag} unknown_entry")
    assert dump_to_string(yaml, cls.main_thread).startswith(f"{cls.yaml_tag} main_thread")
    assert dump_to_string(yaml, cls.application_thread).startswith(
        f"{cls.yaml_tag} application_thread"
    )


def test_enum_context_relation_stringification(yaml):
    cls = Context.Relation
    assert dump_to_string(yaml, cls.lexical).startswith(f"{cls.yaml_tag} lexical")
    assert dump_to_string(yaml, cls.call).startswith(f"{cls.yaml_tag} call")
    assert dump_to_string(yaml, cls.inlined_call).startswith(f"{cls.yaml_tag} inlined_call")


def test_enum_context_lextype_stringification(yaml):
    cls = Context.LexicalType
    assert dump_to_string(yaml, cls.function).startswith(f"{cls.yaml_tag} function")
    assert dump_to_string(yaml, cls.loop).startswith(f"{cls.yaml_tag} loop")
    assert dump_to_string(yaml, cls.line).startswith(f"{cls.yaml_tag} line")
    assert dump_to_string(yaml, cls.instruction).startswith(f"{cls.yaml_tag} instruction")


def test_enum_file_flags_stringification(yaml):
    cls = File.Flags
    assert dump_to_string(yaml, cls.copied).startswith(f"{cls.yaml_tag} [copied]")


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).meta

    with (testdatadir / "dbase" / "small.d" / "meta.db").open("rb") as f:
        got = MetaDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def flex_preallocate(*sizes) -> _Flex:
    f = _Flex()
    for s in sizes:
        f.allocate(s)
    return f


def test_flex():
    """Test that the _Flex helper class generates correct results."""
    # Odd sizes should throw a ValueError
    with pytest.raises(ValueError, match=r"Unsupported size for flex-field"):
        _Flex().allocate(3)

    # First field should always be allocated to position 0
    assert _Flex().allocate(1) == 0
    assert _Flex().allocate(2) == 0
    assert _Flex().allocate(4) == 0
    assert _Flex().allocate(8) == 0

    # Second field should follow the minimum alignment
    assert flex_preallocate(1).allocate(1) == 1
    assert flex_preallocate(1).allocate(2) == 2
    assert flex_preallocate(1).allocate(4) == 4
    assert flex_preallocate(1).allocate(8) == 8

    # Later fields should fill in spaces left by earlier fields
    f = _Flex()
    assert f.allocate(2) == 0
    assert f.allocate(8) == 8
    assert f.allocate(2) == 2

    # Size should be in byte-units and include spans
    assert _Flex().size() == 0
    assert flex_preallocate(1).size() == 1
    assert flex_preallocate(4, 4).size() == 8
    assert flex_preallocate(1, 8).size() == 16

    # Test the sequences possible in practice for {Ctx} structures
    f = _Flex()
    assert [f.allocate(s) for s in (8, 8)] == [0, 8]
    assert f.size() == 16
    f = _Flex()
    assert [f.allocate(s) for s in (8, 4, 8, 8)] == [0, 8, 16, 24]
    assert f.size() == 32
    f = _Flex()
    assert [f.allocate(s) for s in (8, 8, 4, 8, 8)] == [0, 8, 16, 24, 32]
    assert f.size() == 40


def test_context_deep_recursion():
    """Test that deep Context trees will not run out of frames during the parsing."""
    # {Ctx} structure for v4.0
    ctx_s = struct.Struct("<QQ LBBBB H6x")
    assert ctx_s.size == 0x20

    def mkctx(*, pchild: typing.Optional[int], ctx_id: int) -> bytes:
        return ctx_s.pack(
            ctx_s.size if pchild is not None else 0, pchild or 0, ctx_id, 0, 1, 0, 0, 0
        )

    with recursionlimit(100):
        nctxs = sys.getrecursionlimit() + 100
        with io.BytesIO() as bf:
            bf.write(mkctx(pchild=None, ctx_id=0))
            for i in range(1, nctxs):
                bf.write(mkctx(pchild=(i - 1) * ctx_s.size, ctx_id=i))

            c = Context.from_file(
                0,
                bf,
                i * ctx_s.size,
                modules_by_offset={},
                files_by_offset={},
                functions_by_offset={},
            )[0]

    got_ctxs = 1
    while c.children:
        c = c.children[0]
        got_ctxs += 1
    assert got_ctxs == nctxs


def test_yaml_rt(yaml):
    with (testdatadir / "dbase" / "small.d" / "meta.db").open("rb") as f:
        orig = MetaDB.from_file(f)

    orig_s = dump_to_string(yaml, orig)
    assert orig_s == dump_to_string(yaml, yaml.load(orig_s))


def test_traversal(yaml):
    with (testdatadir / "dbase" / "small.yaml").open("r", encoding="utf-8") as f:
        db = yaml.load(f).meta

    assert_good_traversal(db)
