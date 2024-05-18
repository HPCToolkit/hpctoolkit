# SPDX-FileCopyrightText: 2003-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import os
from pathlib import Path

from . import from_path, v4

testdatadir = Path(os.environ["TEST_DATA_DIR"])


def test_small_v4_0():
    # pylint: disable=unidiomatic-typecheck
    db = testdatadir / "dbase" / "small.d"
    assert type(from_path(db)) is v4.Database
    assert type(from_path(db / "meta.db")) is v4.metadb.MetaDB
    assert type(from_path(db / "profile.db")) is v4.profiledb.ProfileDB
    assert type(from_path(db / "cct.db")) is v4.cctdb.ContextDB
    assert type(from_path(db / "trace.db")) is v4.tracedb.TraceDB
