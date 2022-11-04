## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2022-2022, Rice University
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
##
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage.
##
## ******************************************************* EndRiceCopyright *

import copy
import os
from pathlib import Path

import pytest
import ruamel.yaml

from .. import v4
from .._test_util import testdatadir, yaml
from ..v4._test_data import v4_data_small, v4_data_small_path
from . import __main__ as main


@pytest.mark.parametrize(
    "db1,db2",
    [
        (
            testdatadir / "dbase" / "v4.0" / "small.tar.xz",
            testdatadir / "dbase" / "v4.0" / "small.yaml",
        )
    ],
    ids=["small4.0"],
)
def test_same(capsys, db1, db2):
    assert main.main(["strict", str(db1), str(db2)]) == 0
    assert capsys.readouterr() == ("No difference found between inputs\n", "")
    assert main.main(["strict", str(db2), str(db1)]) == 0
    assert capsys.readouterr() == ("No difference found between inputs\n", "")


def test_diff(capsys, yaml, tmp_path, v4_data_small, v4_data_small_path):
    # Alter the small database in particular ways for testing purposes
    data = copy.deepcopy(v4_data_small)
    data.meta.IdNames.names[-1] = "FOO"
    data.meta.Metrics.scopes.append(
        v4.metadb.PropagationScope(
            scopeName="foo", type=v4.metadb.PropagationScope.Type.custom, propagationIndex=255
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
   IdNames:
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
   Metrics:
     scopes[3]:   # foo [custom]
+      !meta.db/v4/PropagationScope
+      scopeName: foo
+      type: !meta.db/v4/PropagationScope.Type custom
+      propagationIndex: 255
"""
    )

    assert main.main(["strict", altered_yaml, str(v4_data_small_path)]) == 1
    out, err = capsys.readouterr()
    assert err == ""
    assert (
        out
        == """\
 meta:
   IdNames:
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
   Metrics:
     scopes[3]:   # foo [custom]
-      !meta.db/v4/PropagationScope
-      scopeName: foo
-      type: !meta.db/v4/PropagationScope.Type custom
-      propagationIndex: 255
"""
    )
