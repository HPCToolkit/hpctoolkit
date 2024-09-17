# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import copy

import pytest

from .. import v4
from .._test_util import testdatadir, yaml
from ..v4._test_data import v4_data_small, v4_data_small_path
from . import __main__ as main

_ = yaml, v4_data_small, v4_data_small_path


@pytest.mark.parametrize(
    ("db1", "db2"),
    [
        (
            testdatadir / "dbase" / "small.d",
            testdatadir / "dbase" / "small.yaml",
        )
    ],
    ids=["small"],
)
def test_same(capsys, db1, db2):
    assert main.main(["strict", str(db1), str(db2)]) == 0
    assert capsys.readouterr() == ("No difference found between inputs\n", "")
    assert main.main(["strict", str(db2), str(db1)]) == 0
    assert capsys.readouterr() == ("No difference found between inputs\n", "")


def test_diff(capsys, yaml, tmp_path, v4_data_small, v4_data_small_path):
    # Alter the small database in particular ways for testing purposes
    data = copy.deepcopy(v4_data_small)
    data.meta.id_names.names[-1] = "FOO"
    data.meta.metrics.scopes.append(
        v4.metadb.PropagationScope(
            scope_name="foo",
            type=v4.metadb.PropagationScope.Type.custom,
            propagation_index=255,
        )
    )
    altered_yaml = str(tmp_path / "altered.yaml")
    with open(altered_yaml, "w", encoding="utf-8") as f:
        yaml.dump(data, f)

    # Compare and ensure the diff output is what we expect
    assert main.main(["strict", str(v4_data_small_path), altered_yaml]) == 1
    out, err = capsys.readouterr()
    assert err == ""
    assert (
        out
        == """\
 meta:
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
"""
    )

    assert main.main(["strict", altered_yaml, str(v4_data_small_path)]) == 1
    out, err = capsys.readouterr()
    assert err == ""
    assert (
        out
        == """\
 meta:
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
"""
    )
