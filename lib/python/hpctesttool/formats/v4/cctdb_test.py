import dataclasses

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .cctdb import ContextDB

_ = yaml


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).context

    with (testdatadir / "dbase" / "small.d" / "cct.db").open("rb") as f:
        got = ContextDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_yaml_rt(yaml):
    orig = yaml.load(
        """
        !cct.db/v4
        ctx_infos: !cct.db/v4/ContextInfos
            contexts:
              - !cct.db/v4/PerContext
                values:
                    0: {1: 2}
                    3: {4: 5}
              - !cct.db/v4/PerContext
                values:
                    6: {7: 8}
                    9: {10: 11}
        """
    )

    orig_s = dump_to_string(yaml, orig)
    assert orig_s == dump_to_string(yaml, yaml.load(orig_s))


def test_traversal(yaml):
    assert_good_traversal(
        yaml.load(
            """
        !cct.db/v4
        ctx_infos: !cct.db/v4/ContextInfos
            contexts:
              - !cct.db/v4/PerContext
                values:
                    0: {1: 2}
                    3: {4: 5}
              - !cct.db/v4/PerContext
                values:
                    6: {7: 8}
                    9: {10: 11}
        """
        )
    )
