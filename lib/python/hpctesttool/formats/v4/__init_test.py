import dataclasses

from hpctesttool.test.tarball import extracted

from .._test_util import assert_good_traversal, testdatadir, yaml
from . import Database

_ = yaml


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f)

    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db:
        got = Database.from_dir(db)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_traversal(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        db = yaml.load(f)
    assert_good_traversal(db)
