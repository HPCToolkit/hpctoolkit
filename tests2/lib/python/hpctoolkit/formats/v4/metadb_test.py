import contextlib
import dataclasses
import io
import struct
import sys
from pathlib import Path

from hpctoolkit.test.tarball import extracted

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .metadb import Context, MetaDB

_ = yaml


@contextlib.contextmanager
def recursionlimit(limit: int):
    old_limit = sys.getrecursionlimit()
    sys.setrecursionlimit(limit)
    try:
        yield
    finally:
        sys.setrecursionlimit(old_limit)


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).meta

    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db, open(
        Path(db) / "meta.db", "rb"
    ) as f:
        got = MetaDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_context_deep_recursion():
    """Test that deep Context trees will not run out of frames during the parsing."""
    # {Ctx} structure for v4.0
    ctx_s = struct.Struct("<QQ LBBBB H6x")
    assert ctx_s.size == 0x20

    def mkctx(*, pchild: int | None, ctx_id: int) -> bytes:
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
    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db, open(
        Path(db) / "meta.db", "rb"
    ) as f:
        orig = MetaDB.from_file(f)

    orig_s = dump_to_string(yaml, orig)
    assert orig_s == dump_to_string(yaml, yaml.load(orig_s))


def test_traversal(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        db = yaml.load(f).meta

    assert_good_traversal(db)
