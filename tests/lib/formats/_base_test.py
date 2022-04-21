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

from ._base import *

import pytest

def test_format_unpacking():
  class Base(VersionedFormat, foo=(0, 0x0, 'B')):
    __slots__ = ['foo']
  assert Base(0, b'\x10').foo == 0x10

  @VersionedFormat.minimize
  class Derived(Base, bar=(1, 0x1, 'B')):
    __slots__ = []
  assert Derived.__name__ == 'Derived'
  d = Derived()
  d.unpack_from(0, b'\x10')
  assert d.foo == 0x10
  assert not hasattr(d, 'bar')
  d.unpack_from(1, b'\x10\x20')
  assert d.foo == 0x10
  assert d.bar == 0x20

def test_format_packing():
  @VersionedFormat.minimize
  class Base(VersionedFormat, foo=(0, 0x0, 'B')):
    __slots__ = []
  assert Base.__name__ == 'Base'
  b = Base()
  b.foo = 0x10
  assert b.pack(0) == b'\x10'

  @VersionedFormat.minimize
  class Derived(Base, bar=(1, 0x1, 'B')):
    __slots__ = []
  assert Derived.__name__ == 'Derived'
  d = Derived()
  d.foo = 0x10
  d.bar = 0x20
  assert d.pack(0) == b'\x10'
  assert d.pack(1) == b'\x10\x20'


def test_fileheader():
  with pytest.raises(ValueError):
    class Foo(FileHeader, format=b'foo', footer=b'_foo.bar', majorVersion=0, minorVersion=0):
      pass

  class Foo(FileHeader, format=b'foo_', footer=b'_foo.bar', majorVersion=1, minorVersion=1):
    pass
  f = Foo()
  assert f.minorVersion == 1
  assert f.pack() == (b'HPCTOOLKITfoo_\x01\x01', b'_foo.bar')

  with pytest.raises(ValueError):
    Foo(b'FOOBARBAZZfoo_\x01\x01  _foo.bar')
  with pytest.raises(ValueError):
    Foo(b'HPCTOOLKITbar_\x01\x01  _foo.bar')
  with pytest.raises(ValueError):
    Foo(b'HPCTOOLKITfoo_\xFF\x01  _foo.bar')
  with pytest.raises(ValueError):
    Foo(b'HPCTOOLKITfoo_\x01\x01  _bar.foo')
  with pytest.warns(UserWarning):
    Foo(b'HPCTOOLKITfoo_\x01\xFF  _foo.bar')

  f = Foo(b'HPCTOOLKITfoo_\x01\x00  _foo.bar')
  assert f.minorVersion == 0

  class FoBa(FileHeader, format=b'foba', footer=b'_foo.bar', majorVersion=1, minorVersion=1,
             _endian='>',
             Foo=(0,), Bar=(1,)):
    pass
  f = FoBa(b'HPCTOOLKITfoba\x01\x00'
           b'\x00\x00\x00\x00\x00\x00\x10\x00'
           b'\x00\x00\x00\x00\x00\x00\x20\x00'
           b'_foo.bar')
  assert f._szFoo == 0x1000 and f._pFoo == 0x2000
  assert not hasattr(f, '_szBar') and not hasattr(f, '_pBar')
  f = FoBa(b'HPCTOOLKITfoba\x01\x01'
           b'\x00\x00\x00\x00\x00\x00\x10\x00'
           b'\x00\x00\x00\x00\x00\x00\x20\x00'
           b'\x00\x00\x00\x00\x00\x00\x30\x00'
           b'\x00\x00\x00\x00\x00\x00\x40\x00'
           b'_foo.bar')
  assert f._szFoo == 0x1000 and f._pFoo == 0x2000
  assert f._szBar == 0x3000 and f._pBar == 0x4000

  f = FoBa()
  f._szFoo, f._pFoo, f._szBar, f._pBar = 0x1500, 0x2500, 0x3500, 0x4500
  assert f.pack() == (b'HPCTOOLKITfoba\x01\x01'
                      b'\x00\x00\x00\x00\x00\x00\x15\x00'
                      b'\x00\x00\x00\x00\x00\x00\x25\x00'
                      b'\x00\x00\x00\x00\x00\x00\x35\x00'
                      b'\x00\x00\x00\x00\x00\x00\x45\x00',
                      b'_foo.bar')
