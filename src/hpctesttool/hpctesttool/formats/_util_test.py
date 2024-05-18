# SPDX-FileCopyrightText: 2003-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import io

import pytest

from ._util import VersionedStructure, read_ntstring


def test_versionedstructure():
    v = VersionedStructure("<", foo=(0, 0x0, "B"), bar=(1, 0x1, "B"))
    assert v.size(0) == 1
    assert v.size(1) == 2
    assert v.size(2) == 2

    assert v.unpack_from(0, b"\x10") == {"foo": 0x10}
    assert v.unpack_from(1, b"\x10\x20") == {"foo": 0x10, "bar": 0x20}
    with pytest.raises(ValueError, match=r"buffer is not large enough, must be at least 2 bytes"):
        v.unpack_from(1, b"\x10")

    src = io.BytesIO(b"\x10\x20")
    assert v.unpack_file(0, src, offset=0) == {"foo": 0x10}
    assert v.unpack_file(1, src, offset=0) == {"foo": 0x10, "bar": 0x20}


def test_read_ntstring():
    src = io.BytesIO(b"FooBar\0")
    assert read_ntstring(src, offset=0) == "FooBar"
    assert read_ntstring(src, offset=3) == "Bar"
