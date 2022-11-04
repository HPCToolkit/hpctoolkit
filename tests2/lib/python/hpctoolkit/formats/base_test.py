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

import dataclasses
import io

import pytest
import ruamel.yaml

from .base import *


def test_db_parse():
    with pytest.raises(AttributeError, match=r"type object 'DatabaseFile' has no attribute"):
        DatabaseFile._parse_header(io.BytesIO(b"Anything"))

    class FooDB(DatabaseFile):
        format_code = b"foo_"
        footer_code = b"__foo.db"
        major_version = 2
        max_minor_version = 1

    with pytest.raises(OSError):
        FooDB._parse_header(io.BytesIO(b"too short"))
    with pytest.raises(InvalidFormatError, match=r"^File is not an HPCToolkit data file$"):
        FooDB._parse_header(io.BytesIO(b"BADMAGIC________    ________"))
    with pytest.raises(InvalidFormatError, match=r"'bar_' != 'foo_'"):
        FooDB._parse_header(io.BytesIO(b"HPCTOOLKITbar_\x00\x00   ________"))
    with pytest.raises(InvalidFormatError, match=r"'__bar.db' != '__foo.db'"):
        FooDB._parse_header(io.BytesIO(b"HPCTOOLKITfoo_\x00\x00   __bar.db"))
    with pytest.raises(
        InvalidFormatError, match=r"^This implementation is only able to read v2.X, got v3$"
    ):
        FooDB._parse_header(io.BytesIO(b"HPCTOOLKITfoo_\x03\x00   __foo.db"))
    with pytest.warns(
        UnsupportedFormatWarning,
        match=r"^This implementation is only able to read version <=2.1, got v2.3",
    ):
        assert FooDB._parse_header(io.BytesIO(b"HPCTOOLKITfoo_\x02\x03   __foo.db")) == 3
    assert FooDB._parse_header(io.BytesIO(b"HPCTOOLKITfoo_\x02\x00   __foo.db")) == 0


def test_db_struct():
    struct = DatabaseFile._header_struct(
        Sec1=(0,),
        Sec2=(0,),
        Sec3=(2,),
        Sec4=(1,),
    )
    file = io.BytesIO(
        b"_JUNK_IDENTIFIER"
        + bytes.fromhex(
            """
        1000000000000000 5010000000000000
        2000000000000000 6010000000000000
        3000000000000000 7010000000000000
        4000000000000000 8010000000000000
    """
        )
    )
    data_v0 = {"szSec1": 0x10, "pSec1": 0x1050, "szSec2": 0x20, "pSec2": 0x1060}
    data_v1 = data_v0 | {"szSec4": 0x40, "pSec4": 0x1080}
    data_v2 = data_v1 | {"szSec3": 0x30, "pSec3": 0x1070}

    assert struct.unpack_file(0, file, 0) == data_v0
    assert struct.unpack_file(1, file, 0) == data_v1
    assert struct.unpack_file(2, file, 0) == data_v2


def test_canonical_paths():
    @dataclasses.dataclass(eq=False)
    class X(StructureBase):
        a: int
        b: int

    @dataclasses.dataclass(eq=False, kw_only=True)
    class Y(StructureBase):
        x: X
        l: list[X]
        d: dict[str, X]

    a = Y(x=X(1, 2), l=[X(3, 4), X(5, 6)], d={"a": X(7, 8), "b": X(9, 10)})
    assert canonical_paths(a) == {
        a: tuple(),
        a.x: ("x",),
        a.l[0]: ("l", 0),
        a.l[1]: ("l", 1),
        a.d["a"]: ("d", "a"),
        a.d["b"]: ("d", "b"),
    }


def dump_to_string(yaml, data) -> str:
    sf = io.StringIO()
    yaml.dump(data, sf)
    return sf.getvalue()


def test_enumeration():
    class Test(Enumeration, yaml_tag="!enum.test"):
        value1 = (1, 0)
        value2 = (2, 0)
        value3 = (3, 1)

    assert Test.value1.value == 1
    assert Test.value1.min_version == 0

    assert Test.versioned_decode(0, 2) is Test.value2
    assert Test.versioned_decode(0, 3) is None


def test_enumeration_yaml():
    class Test(Enumeration, yaml_tag="!enum.test"):
        value1 = (1, 0)
        value2 = (2, 0)
        value3 = (3, 0)

    y = ruamel.yaml.YAML(typ="safe")
    y.register_class(Test)

    assert dump_to_string(y, Test.value1) == "!enum.test value1\n...\n"
    assert dump_to_string(y, Test.value2) == "!enum.test value2\n...\n"
    assert dump_to_string(y, Test.value3) == "!enum.test value3\n...\n"

    assert y.load(dump_to_string(y, [Test.value3, Test.value1])) == [Test.value3, Test.value1]


def test_flags():
    class Test(BitFlags, yaml_tag="!flags.test"):
        flag1 = (0, 0)
        flag2 = (1, 0)
        flag3 = (2, 1)

    assert Test.flag1.value == 1
    assert Test.flag2.value == 2
    assert Test.flag3.value == 4
    assert Test.flag1.min_version == 0

    assert Test.versioned_decode(0, 2) is Test.flag2
    assert Test.versioned_decode(0, 5) is Test.flag1
    assert Test.versioned_decode(1, 5) == Test.flag1 | Test.flag3


def test_flags_yaml():
    class Test(BitFlags, yaml_tag="!flags.test"):
        flag1 = (0, 0)
        flag2 = (1, 0)
        flag3 = (2, 0)
        aaa_flag4 = (3, 0)

    y = ruamel.yaml.YAML(typ="safe")
    y.register_class(Test)

    assert dump_to_string(y, Test.flag1) == "!flags.test [flag1]\n"
    assert dump_to_string(y, Test.flag1 | Test.flag2) == "!flags.test [flag1, flag2]\n"
    assert dump_to_string(y, Test.flag3 | Test.aaa_flag4) == "!flags.test [aaa_flag4, flag3]\n"

    assert y.load(dump_to_string(y, Test.flag3 | Test.flag1)) == Test.flag1 | Test.flag3
