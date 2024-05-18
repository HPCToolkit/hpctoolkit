# SPDX-FileCopyrightText: 2003-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import dataclasses

from .._test_util import assert_good_traversal, testdatadir, yaml
from . import Database

_ = yaml


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "small.yaml", encoding="utf-8") as f:
        expected = yaml.load(f)

    got = Database.from_dir(testdatadir / "dbase" / "small.d")

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_traversal(yaml):
    with open(testdatadir / "dbase" / "small.yaml", encoding="utf-8") as f:
        db = yaml.load(f)
    assert_good_traversal(db)
