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
from ..metadb import (MetaDB, ContextTreeSection, Context, MetricDescription,
                      PropagationScope, SummaryStatistic)

from struct import Struct
import collections.abc
import itertools
import textwrap

__all__ = [
  'ProfileDB',
  # v4.0
  'ProfileInfoSection', 'ProfileInfo', 'IdentifierElement',
]

@VersionedFormat.minimize
class ProfileDB(FileHeader,
                format=b'prof', footer=b'_prof.db', majorVersion=4, minorVersion=0,
    # Added in v4.0
    ProfileInfos = (0,),
    IdTuples = (0,),
  ):
  """The profile.db file format."""
  __slots__ = ['_metadb', '_profiles']

  def _init_(self, *, metadb, profiles=None):
    super()._init_()
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    self._metadb = metadb
    if profiles is not None:
      self.profiles = profiles if isinstance(profiles, ProfileInfoSection) \
                      else ProfileInfoSection(metadb=self._metadb, **profiles)

  def unpack_from(self, *args, metadb, **kwargs):
    super().unpack_from(*args, **kwargs)
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    self._metadb = metadb

  @cached_property('_profiles')
  @VersionedFormat.subunpack(lambda: ProfileInfoSection(metadb=self._metadb))
  def profiles(self, *args):
    return ProfileInfoSection(*args, metadb=self._metadb, offset=self._pProfileInfos)

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
  __slots__ = ['_metadb', '_profiles']

  def _init_(self, *, metadb, profiles=None):
    super()._init_()
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    self._metadb = metadb
    if profiles is not None:
      self.profiles = [p if isinstance(p, ProfileInfo) else ProfileInfo(metadb=self._metadb, **p)
                       for p in profiles]

  def unpack_from(self, *args, metadb, **kwargs):
    super().unpack_from(*args, **kwargs)
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    self._metadb = metadb

  @cached_property('_profiles')
  @VersionedFormat.subunpack(list)
  def profiles(self, *args):
    return [ProfileInfo(*args, metadb=self._metadb, offset=self._pProfiles + i*self._szProfile)
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

  def _valueBlockKwargs(self, metadb):
    return {
      'innerclass': SummaryProfileValues if self.idTuple is self.__SUMMARY else NonSummaryProfileValues,
      'byCtxId': metadb.context.byCtxId(),
      'byMetricId': metadb.metrics.byStatMetricId() if self.idTuple is self.__SUMMARY else metadb.metrics.byPropMetricId(),
    }

  def _init_(self, *, metadb, idTuple, valueBlock={}):
    super()._init_()
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))

    if isinstance(idTuple, str) and idTuple.lower() == 'summary':
      self.idTuple = self.__SUMMARY
    else:
      self.idTuple = tuple(e if isinstance(e, IdentifierElement) else IdentifierElement(**e)
                           for e in idTuple)
    self.valueBlock = ProfileSparseValueBlock(valueBlock, **self._valueBlockKwargs(metadb))

  def unpack_from(self, version, src, /, *args, metadb, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))

    if self._pIdTuple == 0:
      self.idTuple = self.__SUMMARY
    else:
      nIds, = unpack(Struct('<H'), src, offset=self._pIdTuple)
      self.idTuple = tuple(IdentifierElement(version, src, offset=self._pIdTuple+8+i*0x10)
                           for i in range(nIds))

    self.valueBlock = ProfileSparseValueBlock(lazy=(
        array_unpack(self.__vb_ctxIndex, self._vb_nCtxs, src, offset=self._vb_pCtxIndices),
        src, self._vb_nValues, self._vb_pValues), **self._valueBlockKwargs(metadb))

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

  @property
  def shorthand(self):
    return (' '.join(str(e) for e in self.idTuple)
            if self.idTuple is not self.__SUMMARY else "SUMMARY")

  def __str__(self):
    return (f"profile: {self.shorthand}\n"
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


class ProfileSparseValueBlock(collections.abc.MutableMapping):
  """Lazily-loaded block of sparse values, profile.db variant."""
  __slots__ = ['_real', '_innerclass', '_byCtxId', '_byMetricId',
               '_ctxIndices', '_src', '_pValues']
  __value = Struct('<Hd')  # metricID, value

  def __init__(self, initial=None, *, innerclass, byCtxId, byMetricId, lazy=None):
    super().__init__()
    self._real = {}
    self._innerclass = innerclass
    self._byCtxId, self._byMetricId = byCtxId, byMetricId
    self._ctxIndices = {}

    if initial is not None:
      for k,v in (initial.items() if isinstance(initial, collections.abc.Mapping) else initial):
        self[k] = v

    if lazy is not None:
      ctxIndices, self._src, nValues, pValues = lazy
      self._pValues = pValues.__index__()

      last = None
      for c, i in ctxIndices:
        if last is not None:
          lc,li = last
          self._ctxIndices[lc] = (li, i - li)
        last = (c, i)
      if last is not None:
        c, i = last
        self._ctxIndices[c] = (i, nValues.__index__() - i)

  def __getitem__(self, ctx):
    if not isinstance(ctx, Context) and not isinstance(ctx, ContextTreeSection):
      raise TypeError(type(ctx))
    if ctx in self._real: return self._real[ctx]
    if ctx.ctxId not in self._ctxIndices: raise KeyError(ctx)

    # Load the missing block
    index, cnt = self._ctxIndices[ctx.ctxId]
    del self._ctxIndices[ctx.ctxId]
    block = self._innerclass((self._byMetricId[m], v) for m,v in
        array_unpack(self.__value, cnt, self._src, offset=self._pValues + self.__value.size*index))
    self._real[ctx] = block
    return block

  def __setitem__(self, ctx, block):
    if not isinstance(ctx, Context) and not isinstance(ctx, ContextTreeSection):
      raise TypeError(type(ctx))
    if not isinstance(block, self._innerclass): block = self._innerclass(block)
    self._real[ctx] = block
    if ctx.ctxId in self._ctxIndices: del self._ctxIndices[ctx.ctxId]

  def __delitem__(self, ctx):
    if not isinstance(ctx, Context) and not isinstance(ctx, ContextTreeSection):
      raise TypeError(type(ctx))
    if ctx in self._real: del self._real[ctx]
    elif ctx.ctxId in self._ctxIndices: del self._ctxIndices[ctx.ctxId]
    else: raise KeyError(ctx)

  def __iter__(self):
    return itertools.chain(self._real, list(self._byCtxId[ctxId] for ctxId in self._ctxIndices))

  def __len__(self):
    return len(self._real) + len(self._ctxIndices)

  def __str__(self):
    return '\n'.join(f"context #{c.ctxId:d}:"
                     + ("\n" + '\n'.join(textwrap.indent(str(b), '  '))
                        if len(b) > 0 else " {}")
                     for c,b in sorted(self.items(), key=lambda x: x[0].ctxId))


class NonSummaryProfileValues(collections.abc.MutableMapping):
  """Metric-value mapping for non-summary profiles"""
  __slots__ = ['_real']

  def __init__(self, initial=None):
    self._real = {}
    for k,v in (initial.items() if isinstance(initial, collections.abc.Mapping) else initial):
      self[k] = v

  @staticmethod
  def _key(key):
    met, scope = key
    if not isinstance(met, MetricDescription): raise TypeError(type(met))
    if scope not in met.scopes: raise ValueError(key)
    if not isinstance(scope, PropagationScope): raise TypeError(type(scope))
    return (met, scope)

  def __getitem__(self, key):
    return self._real[self._key(key)]

  def __setitem__(self, key, value):
    self._real[self._key(key)] = float(value)

  def __delitem__(self, key):
    del self._real[self._key(key)]

  def __iter__(self): return iter(self._real)
  def __len__(self): return len(self._real)

  def __repr__(self):
    return f"{self.__class__.__name__}({self._real!r})"

  def __str__(self):
    return '\n'.join(f"metric {m.name}/{p.scope} #{p.propMetricId}: {v}"
                     for (m,p),v in sorted(self.items(), key=lambda x: x[0][1].propMetricId))


class SummaryProfileValues(collections.abc.MutableMapping):
  """Metric-value mapping for summary profiles"""
  __slots__ = ['_real']

  def __init__(self, initial=None):
    self._real = {}
    for k,v in (initial.items() if isinstance(initial, collections.abc.Mapping) else initial):
      self[k] = v

  @staticmethod
  def _key(key):
    met, scope, stat = key
    if not isinstance(met, MetricDescription): raise TypeError(type(met))
    if scope not in met.scopes: raise ValueError(key)
    if not isinstance(scope, PropagationScope): raise TypeError(type(scope))
    if stat not in scope.summaries: raise ValueError(key)
    if not isinstance(stat, SummaryStatistic): raise TypeError(type(stat))
    return (met, scope, stat)

  def __getitem__(self, key):
    return self._real[self._key(key)]

  def __setitem__(self, key, value):
    self._real[self._key(key)] = float(value)

  def __delitem__(self, key):
    del self._real[self._key(key)]

  def __iter__(self): return iter(self._real)
  def __len__(self): return len(self._real)

  def __repr__(self):
    return f"{self.__class__.__name__}({self._real!r})"

  def __str__(self):
    return '\n'.join(f"statistic {m.name}/{p.scope}/{s.combine}({s.formula}) #{s.statMetricId}: {v}"
                     for (m,p,s),v in sorted(self.items(), key=lambda x: x[0][2].statMetricId))

