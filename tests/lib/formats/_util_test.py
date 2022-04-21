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

from ._util import *
from ._util import _FileView

from struct import Struct
from tempfile import TemporaryFile
import io
import pytest
import struct

def test_fileview():
  with pytest.raises(OSError):
    _FileView(io.StringIO('foobar'))
  with TemporaryFile() as tmp:
    fv = _FileView(tmp)
    with pytest.raises(TypeError): fv[12]
    with pytest.raises(ValueError): fv[12:]
    with pytest.raises(ValueError): fv[10:20:30]
    v = fv[:0]
    assert type(v) == memoryview
    assert len(v) == 0
    v = fv[3:6]
    assert type(v) == memoryview
    assert len(v) == 3
    v[:] = b'123'
    tmp.seek(3)
    assert tmp.read(3) == b'123'

def test_versionedstruct():
  v = VersionedStruct(foo=(0, 0x0, 'B'), bar=(1, 0x1, 'B'))
  assert v.size(0) == 1 and v.size(1) == 2 and v.size(2) == 2

  assert v.unpack_from(0, b'\x10') == {'foo': 0x10}
  assert v.unpack_from(1, b'\x10\x20') == {'foo': 0x10, 'bar': 0x20}
  with pytest.raises(struct.error):
    v.unpack_from(1, b'\x10')

  b = bytearray(1)
  v.pack_into(0, b, {'foo': 0x10})
  assert b == b'\x10'
  with pytest.raises(struct.error):
    v.pack_into(1, b, {'foo': 0x10, 'bar': 0x20})

  b = bytearray(2)
  v.pack_into(1, b, {'foo': 0x10, 'bar': 0x20})
  assert b == b'\x10\x20'
  v.pack_into(4, b, {'foo': 0x10, 'bar': 0x20})
  assert b == b'\x10\x20'
  with pytest.raises(KeyError):
    v.pack_into(1, b, {'foo': 0x10})

def test_read_ntstring():
  b = b'FooBar\0'
  assert read_ntstring(b) == 'FooBar'
  assert read_ntstring(b, offset=3) == 'Bar'
