import dataclasses
from pathlib import Path

from hpctoolkit.test.tarball import extracted

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .metadb import MetaDB

_ = yaml


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).meta

    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db, open(
        Path(db) / "meta.db", "rb"
    ) as f:
        got = MetaDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


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
