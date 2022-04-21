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

from ._util import VersionedStruct, _view

from collections import OrderedDict
import functools
import warnings

class _AsDict:
  __slots__ = ['_real']

  def __init__(self, real):
    self._real = real

  def __getitem__(self, key):
    return getattr(self._real, key)

class VersionedFormat:
  """
  Wrapper around VersionedStruct that exposes the fields as attributes.
  Note that the version to read from and write to must be passed in during
  packing and unpacking.
  """
  __slots__ = ['_VersionedFormat__unpackVersionSrc']

  def __init_subclass__(cls, /, _endian='<', **fields):
    super().__init_subclass__()

    try:
      for k,v in cls.__fields.items():
        if k in fields:
          raise ValueError(f"Conflict on field {k}")
        fields[k] = v
    except AttributeError:
      pass
    cls.__fields = fields
    cls.__vstruct = VersionedStruct(_endian, **fields)

  def __init__(self, *args, **kwargs):
    if len(args) > 0:
      self.unpack_from(*args, **kwargs)
      self.__unpackVersionSrc = args[0], args[1]
    else:
      self._init_(**kwargs)

  def _init_(self): pass

  @staticmethod
  def minimize(cls):
    """Helper for subclasses to automatically add fiels to __slots__ ."""
    if not issubclass(cls, VersionedFormat):
      raise TypeError(f"minimize_format only works of VersionedFormat subclasses, got {cls}")

    exslots = tuple(cls.__fields.keys())
    class Derived(cls):
      __slots__ = exslots
    Derived.__name__ = cls.__name__
    return Derived

  def size(self, version):
    return self.__vstruct.size(version)

  def unpack_from(self, version, src, /, offset=0):
    for k,v in self.__vstruct.unpack_from(version, src, offset=offset).items():
      setattr(self, k, v)

  @staticmethod
  def subunpack(default_func):
    def apply(func):
      def final(self, *args, **kwargs):
        try:
          args = self.__unpackVersionSrc
        except AttributeError:
          return default_func()
        else:
          return func(self, *args, **kwargs)
      return final
    return apply

  def pack(self, version):
    x = bytearray(self.__vstruct.size(version))
    self.pack_into(version, x)
    return bytes(x)

  def pack_into(self, version, dst, offset=0):
    self.__vstruct.pack_into(version, dst, _AsDict(self), offset=offset)


class FileHeader(VersionedFormat,
    _magic = (0, 0x0, '10s'),
    _format = (0, 0xa, '4s'),
    _majorVersion = (0, 0xe, 'B'),
    minorVersion = (-1, 0xf, 'B'),
  ):
  """
  Concrete base class for file headers, for those that use them. Handles details
  concering magic and versions.
  """
  __slots__ = ['_footer']

  def __init_subclass__(cls, /, format=None, footer=None,
                        majorVersion=None, minorVersion=None,
                        _endian='<', **kw):
    fields, offset = OrderedDict(), 0x10
    for name, (version,) in kw.items():
      fields['_sz'+name] = (version, offset, 'Q')
      fields['_p'+name] = (version, offset+8, 'Q')
      offset += 0x10
    super().__init_subclass__(_endian=_endian, **fields)

    if format is not None:
      cls.__format = bytes(format)
      if len(cls.__format) != 4:
        raise ValueError(f"Format identifier code must be 4 bytes!")
    if footer is not None:
      cls.__footer = bytes(footer)
      if len(cls.__footer) != 8:
        raise ValueError(f"Format identifier footer must be 8 bytes!")
    if majorVersion is not None: cls.__majorVersion = int(majorVersion)
    if minorVersion is not None: cls.__minorVersion = int(minorVersion)

  def __init__(self, *args, **kwargs):
    if len(args) > 0:
      self.unpack_from(*args, **kwargs)
      self._VersionedFormat__unpackVersionSrc = self.minorVersion, args[0]
    else:
      self._init_(**kwargs)

  def _init_(self):
    super()._init_()
    self._magic = b'HPCTOOLKIT'
    self._format = self.__format
    self._footer = self.__footer
    self._majorVersion = self.__majorVersion
    self.minorVersion = self.__minorVersion

  @property
  def size(self):
    return super().size(self.minorVersion)

  def unpack_from(self, src, *args, **kwargs):
    # This is a self-versioned format, so we read minorVersion (v-1) first.
    src = _view(src)
    super().unpack_from(-1, src, *args, **kwargs)
    super().unpack_from(self.minorVersion, src, *args, **kwargs)

    # Last 8 bytes are the footer identifier
    self._footer = bytes(src[max(len(src)-8, 0):len(src)])

    if self._magic != b'HPCTOOLKIT':
      raise ValueError(f"Attempt to initialize a {self.__class__} from an "
                       f"invalid source: magic is {self._magic!r}")
    if self._format != self.__format:
      raise ValueError(f"Attempt to initialize a {self.__class__} from an "
                       f"invalid source: format is {self._format!r}")
    if self._footer != self.__footer:
      raise ValueError(f"Attempt to initialize a {self.__class__} from an "
                       f"invalid source: footer is {self._footer!r}")
    if self._majorVersion != self.__majorVersion:
      raise ValueError(f"Attempt to initialize a {self.__class__} "
                       f"v{self.__majorVersion} from an invalid source: "
                       f"majorVersion is {self._majorVersion:d}")
    if self.minorVersion > self.__minorVersion:
      warnings.warn(f"Initializing a {self.__class__} "
                    f"v{self.__majorVersion}.{self.__minorVersion:d} from a "
                    f"v{self.__majorVersion}.{self.minorVersion:d} source, "
                    "some data will be lost!")

  def pack(self):
    x = bytearray(self.size)
    self.pack_into(x)
    return bytes(x), self._footer

  def pack_into(self, *args, **kwargs):
    super().pack_into(self.minorVersion, *args, **kwargs)

