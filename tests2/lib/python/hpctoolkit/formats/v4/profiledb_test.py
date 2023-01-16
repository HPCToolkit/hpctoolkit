import dataclasses
from pathlib import Path

from hpctoolkit.test.tarball import extracted

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from .profiledb import ProfileDB

_ = yaml


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f).profile

    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db, open(
        Path(db) / "profile.db", "rb"
    ) as f:
        got = ProfileDB.from_file(f)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_yaml_rt(yaml):
    orig = yaml.load(
        """
        !profile.db/v4
        profile_infos: !profile.db/v4/ProfileInfos
            profiles:
              - !profile.db/v4/Profile
                flags: !profile.db/v4/Profile.Flags []
                id_tuple: !profile.db/v4/IdentifierTuple
                    ids:
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 1, logical_id: 42, physical_id: 48}
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 2, logical_id: 142, physical_id: 148}
                values:
                  0: {1: 2}
                  3: {4: 5}
              - !profile.db/v4/Profile
                flags: !profile.db/v4/Profile.Flags [is_summary]
                id_tuple: !profile.db/v4/IdentifierTuple
                    ids:
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 3, logical_id: 242, physical_id: 248}
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 4, logical_id: 342, physical_id: 348}
                values:
                  0: {1: 2}
                  3: {4: 5}
    """
    )

    orig_s = dump_to_string(yaml, orig)
    assert orig_s == dump_to_string(yaml, yaml.load(orig_s))


def test_traversal(yaml):
    assert_good_traversal(
        yaml.load(
            """
        !profile.db/v4
        profile_infos: !profile.db/v4/ProfileInfos
            profiles:
              - !profile.db/v4/Profile
                flags: !profile.db/v4/Profile.Flags []
                id_tuple: !profile.db/v4/IdentifierTuple
                    ids:
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 1, logical_id: 42, physical_id: 48}
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 2, logical_id: 142, physical_id: 148}
                values:
                  0: {1: 2}
                  3: {4: 5}
              - !profile.db/v4/Profile
                flags: !profile.db/v4/Profile.Flags [is_summary]
                id_tuple: !profile.db/v4/IdentifierTuple
                    ids:
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 3, logical_id: 242, physical_id: 248}
                      - !profile.db/v4/Identifier {flags: !profile.db/v4/Identifier.Flags [], kind: 4, logical_id: 342, physical_id: 348}
                values:
                  0: {1: 2}
                  3: {4: 5}
    """
        )
    )
