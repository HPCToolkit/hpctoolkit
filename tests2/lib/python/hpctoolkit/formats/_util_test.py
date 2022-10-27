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
import struct

import pytest

from ._util import *


def test_versionedstructure():
    v = VersionedStructure("<", foo=(0, 0x0, "B"), bar=(1, 0x1, "B"))
    assert v.size(0) == 1 and v.size(1) == 2 and v.size(2) == 2

    assert v.unpack_from(0, b"\x10") == {"foo": 0x10}
    assert v.unpack_from(1, b"\x10\x20") == {"foo": 0x10, "bar": 0x20}
    with pytest.raises(ValueError):
        v.unpack_from(1, b"\x10")

    src = io.BytesIO(b"\x10\x20")
    assert v.unpack_file(0, src, offset=0) == {"foo": 0x10}
    assert v.unpack_file(1, src, offset=0) == {"foo": 0x10, "bar": 0x20}


def test_read_ntstring():
    src = io.BytesIO(b"FooBar\0")
    assert read_ntstring(src, offset=0) == "FooBar"
    assert read_ntstring(src, offset=3) == "Bar"
