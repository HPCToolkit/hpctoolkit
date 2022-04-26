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

from ..._base import VersionedFormat, FileHeader
from ..._util import cached_property, unpack, array_unpack, VersionedBitFlags

from struct import Struct
import itertools
import textwrap

@VersionedFormat.minimize
class ProfileDB(FileHeader,
                format=b'prof', footer=b'_prof.db', majorVersion=4, minorVersion=0,
    # Added in v4.0
    ProfileInfos = (0,),
    IdTuples = (0,),
  ):
  """The profile.db file format."""
  __slots__ = ['_profiles']

  def _init_(self, *, profiles=None):
    super()._init_()
    if profiles is not None:
      self.profiles = profiles if isinstance(profiles, ProfileInfoSection) \
                      else ProfileInfoSection(**profiles)

  @cached_property('_profiles')
  @VersionedFormat.subunpack(lambda: ProfileInfoSection())
  def profiles(self, *args):
    return ProfileInfoSection(*args, offset=self._pProfileInfos)

  def identical(self, other):
    if not isinstance(other, ProfileDB): raise TypeError(type(other))
    return self.profiles.identical(other.profiles)

  def __repr__(self):
    return (f"{self.__class__.__name__}(profiles={self.profiles!r})")

  def __str__(self):
    return (f"ProfileDB v4.{self.minorVersion}:\n"
            " Profile Info:\n"
            + textwrap.indent(str(self.profiles), '  ')
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class ProfileInfoSection(VersionedFormat,
    # Added in v4.0
    _pProfiles = (0, 0x00, 'Q'),
    _nProfiles = (0, 0x08, 'L'),
    _szProfile = (0, 0x0c, 'B'),
  ):
  """Section containing header metadata for the profiles in this database."""
  __slots__ = ['_profiles']

  def _init_(self, *, profiles=None):
    super()._init_()
    if profiles is not None:
      self.profiles = [p if isinstance(p, ProfileInfo) else ProfileInfo(**p)
                       for p in profiles]

  @cached_property('_profiles')
  @VersionedFormat.subunpack(list)
  def profiles(self, *args):
    return [ProfileInfo(*args, offset=self._pProfiles + i*self._szProfile)
            for i in range(self._nProfiles)]

  def identical(self, other):
    if not isinstance(other, ProfileInfoSection): raise TypeError(type(other))
    return (len(self.profiles) == len(other.profiles)
            and all(a.identical(b) for a,b in zip(self.profiles, other.profiles)))

  def __repr__(self):
    return (f"{self.__class__.__name__}(profiles={self.profiles!r})")

  def __str__(self):
    return (f"profiles:"
            + ('\n' + '\n'.join(' - ' + textwrap.indent(str(p), '   ')[3:]
                                for p in self.profiles)
               if len(self.profiles) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class ProfileInfo(VersionedFormat,
    # Added in v4.0
    _vb_nValues = (0, 0x00, 'Q'),
    _vb_pValues = (0, 0x08, 'Q'),
    _vb_nCtxs = (0, 0x10, 'L'),
    _vb_pCtxIndices = (0, 0x18, 'Q'),
    _pIdTuple = (0, 0x20, 'Q'),
  ):
  """Information on a single profile."""
  __SUMMARY = 'SUMMARY'
  __slots__ = ['idTuple', 'valueBlock']
  __vb_ctxIndex = Struct('<LQ')  # ctxId, startIndex

  def _init_(self, *, idTuple, valueBlock={}):
    super()._init_()
    if isinstance(idTuple, str) and idTuple.lower() == 'summary':
      self.idTuple = self.__SUMMARY
    else:
      self.idTuple = tuple(e if isinstance(e, IdentifierElement) else IdentifierElement(**e)
                           for e in idTuple)
    self.valueBlock = ProfileSparseValueBlock(valueBlock)

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    if self._pIdTuple == 0:
      self.idTuple = self.__SUMMARY
    else:
      nIds, = unpack(Struct('<H'), src, offset=self._pIdTuple)
      self.idTuple = tuple(IdentifierElement(version, src, offset=self._pIdTuple+8+i*0x10)
                           for i in range(nIds))

    self.valueBlock = ProfileSparseValueBlock.lazy(
        array_unpack(self.__vb_ctxIndex, self._vb_nCtxs, src, offset=self._vb_pCtxIndices),
        src, self._vb_nValues, self._vb_pValues)

  def identical(self, other):
    if not isinstance(other, ProfileInfo): raise TypeError(type(other))
    def idtuple_cmp(a, b):
      if a is self.__SUMMARY: return b is self.__SUMMARY
      if b is self.__SUMMARY: return False
      return len(a) == len(b) and all(x.identical(y) for x,y in zip(a,b))
    return (idtuple_cmp(self.idTuple, other.idTuple)
            and self.valueBlock == other.valueBlock)

  def __repr__(self):
    return (f"{self.__class__.__name__}(idTuple={self.idTuple!r}, "
            f"valueBlock={self.valueBlock!r})")

  def __str__(self):
    return ("profile: "
            + (' '.join(str(e) for e in self.idTuple) if self.idTuple is not self.__SUMMARY else "SUMMARY") + "\n"
            "valueBlock: "
            + ('\n' + textwrap.indent(str(self.valueBlock), '  ')
               if len(self.valueBlock) > 0 else " {}")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class IdentifierElement(VersionedFormat,
    # Fixed-size structure
    kind = (0, 0x00, 'B'),
    _flags = (0, 0x02, 'H'),
    logicalId = (0, 0x04, 'L'),
    physicalId = (0, 0x08, 'Q'),
  ):
  """Element of a hierarchical identifier tuple."""

  __flags = VersionedBitFlags(
    # Added in v4.0
    isPhysical = (0, 0),
  )
  __slots__ = ['flags']

  def _init_(self, *, kind, flags=set(), logicalId, physicalId):
    super()._init_()
    self.kind = kind.__index__()
    self.flags = self.__flags.normalize(float('inf'), flags)
    self.logicalId = logicalId.__index__()
    self.physicalId = physicalId.__index__()

  def unpack_from(self, version, src, **kwargs):
    super().unpack_from(version, src, **kwargs)
    self.flags = self.__flags.unpack(version, self._flags)

  def identical(self, other):
    if not isinstance(other, IdentifierElement): raise TypeError(type(other))
    return (self.kind == other.kind and self.flags == other.flags
            and self.logicalId == other.logicalId and self.physicalId == other.physicalId)

  def __repr__(self):
    return (f"{self.__class__.__name__}(kind={self.kind!r}, "
            f"flags={self.flags!r}, logicalId={self.logicalId!r}, "
            f"physicalId={self.physicalId!r})")

  def __str__(self):
    flags = ''
    if 'isPhysical' in self.flags: flags += 'P'
    return f"{self.kind:d}.{flags}[{self.logicalId:d}/{self.physicalId:x}]"


class ProfileSparseValueBlock(dict):
  """Lazily-loaded block of sparse values, profile.db variant."""
  __slots__ = ['_ctxIndices', '_src', '_pValues']
  __value = Struct('<Hd')  # metricID, value

  def __init__(self, initial=None):
    super().__init__()
    if initial is not None:
      for k,v in (initial.items() if isinstance(initial, dict) else initial):
        self[k] = v

  @classmethod
  def lazy(cls, ctxIndices, src, nValues, pValues):
    out = cls()
    out._src, out._pValues = src, pValues.__index__()

    out._ctxIndices = {}
    last = None
    for c, i in ctxIndices:
      if last is not None:
        lc,li = last
        out._ctxIndices[lc] = (li, i - li)
      last = (c, i)
    if last is not None:
      c, i = last
      out._ctxIndices[c] = (i, nValues.__index__() - i)

    return out

  def now(self):
    if not hasattr(self, '_ctxIndices'): return
    for ctxId in list(self._ctxIndices):
      self.__missing__(ctxId)
    return self

  def __missing__(self, ctxId):
    if not hasattr(self, '_ctxIndices'):
      raise IndexError()

    index, cnt = self._ctxIndices[ctxId]
    del self._ctxIndices[ctxId]
    values = dict(array_unpack(self.__value, cnt, self._src, offset=self._pValues + self.__value.size*index))
    super().__setitem__(ctxId, values)
    return values

  def __delitem__(self, key):
    if hasattr(self, '_ctxIndices') and key in self._ctxIndices:
      del self._ctxIndices[key]
    return super().__delitem__(key)

  def __iter__(self):
    if hasattr(self, '_ctxIndices'):
      return itertools.chain(super().__iter__(), iter(self._ctxIndices))
    return super().__iter__()

  def __len__(self):
    return super().__len__() + len(getattr(self, '_ctxIndices', []))

  def __eq__(self, other):
    if not isinstance(other, ProfileSparseValueBlock): raise TypeError(type(other))
    self.now()
    return super().__eq__(other.now())

  def __str__(self):
    return '\n'.join(f"context #{k:d}:"
                     + ("\n" + '\n'.join(f"  metric {kk:d}: {vv:f}"
                                         for kk,vv in sorted(self[k].items()))
                        if len(self[k]) > 0 else " {}")
                     for k in sorted(self))
