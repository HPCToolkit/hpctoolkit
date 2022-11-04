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

import io
import os
from pathlib import Path

import pytest
import ruamel.yaml

from . import base

testdatadir = Path(os.environ["TEST_DATA_DIR"])


@pytest.fixture
def yaml():
    return ruamel.yaml.YAML(typ="safe")


def dump_to_string(yaml, data) -> str:
    sf = io.StringIO()
    yaml.dump(data, sf)
    return sf.getvalue()


def assert_good_traversal(obj, seen=None):
    if seen is None:
        seen = set()

    if obj is None or isinstance(obj, str | int | float | base.Enumeration | base.BitFlags):
        pass
    elif isinstance(obj, base.StructureBase):
        assert obj not in seen, f"Object {obj!r} was observed twice during the traversal!"
        seen.add(obj)
        for f in obj.owning_fields():
            assert_good_traversal(getattr(obj, f.name), seen=seen)
    elif isinstance(obj, dict):
        for k, v in obj.items():
            assert isinstance(
                k, str | int | float | base.Enumeration | base.BitFlags
            ), f"Bad traversal, observed invalid key {k!r}"
            assert_good_traversal(v, seen=seen)
    elif isinstance(obj, list):
        for v in obj:
            assert_good_traversal(v, seen=seen)
    else:
        assert False, f"Bad traversal, encountered non-Structure object {obj!r}"
