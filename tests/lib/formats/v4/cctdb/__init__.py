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
from ..profiledb import ProfileDB, ProfileInfo

from struct import Struct
import collections.abc
import itertools
import textwrap

__all__ = [
  'ContextDB',
  # v4.0
  'ContextInfoSection', 'ContextInfo',
]

@VersionedFormat.minimize
class ContextDB(FileHeader,
                format=b'ctxt', footer=b'__ctx.db', majorVersion=4, minorVersion=0,
    # Added in v4.0
    CtxInfos = (0,),
  ):
  """The cct.db file format."""
  __slots__ = ['_metadb', '_profiledb', '_ctxs']

  def _init_(self, *, metadb, profiledb, ctxs=None):
    super()._init_()
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    if not isinstance(profiledb, ProfileDB): raise TypeError(type(profiledb))
    self._metadb, self._profiledb = metadb, profiledb
    if ctxs is not None:
      self.ctxs = (ctxs if isinstance(ctxs, ContextInfoSection)
                   else ContextInfoSection(metadb=self._metadb,
                                           profiledb=self._profiledb, **ctxs))

  def unpack_from(self, *args, metadb, profiledb, **kwargs):
    super().unpack_from(*args, **kwargs)
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    if not isinstance(profiledb, ProfileDB): raise TypeError(type(profiledb))
    self._metadb, self._profiledb = metadb, profiledb

  @cached_property('_ctxs')
  @VersionedFormat.subunpack(lambda: ContextInfoSection(profiledb=self._profiledb))
  def ctxs(self, *args):
    return ContextInfoSection(*args, metadb=self._metadb,
                              profiledb=self._profiledb, offset=self._pCtxInfos)

  def identical(self, other):
    if not isinstance(other, ContextDB): raise TypeError(type(other))
    return self.ctxs.identical(other.ctxs)

  def __repr__(self):
    return (f"{self.__class__.__name__}(ctxs={self.ctxs!r})")

  def __str__(self):
    return (f"ContextDB v4.{self.minorVersion}:\n"
            " Context Info:\n"
            + textwrap.indent(str(self.ctxs), '  ')
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class ContextInfoSection(VersionedFormat,
    # Added in v4.0
    _pCtxs = (0, 0x00, 'Q'),
    _nCtxs = (0, 0x08, 'L'),
    _szCtx = (0, 0x0c, 'B'),
  ):
  """Section containing header metadata for the contexts in this database."""
  __slots__ = ['_metadb', '_profiledb', '_ctxs']

  def _init_(self, *, metadb, profiledb, ctxs=None):
    super()._init_()
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    if not isinstance(profiledb, ProfileDB): raise TypeError(type(profiledb))
    self._metadb, self._profiledb = metadb, profiledb
    if ctxs is not None:
      self.ctxs = [c if isinstance(c, ContextInfo) else
                   ContextInfo(metadb=self._metadb, profiledb=self._profiledb, **c)
                   for c in ctxs]

  def unpack_from(self, *args, metadb, profiledb, **kwargs):
    super().unpack_from(*args, **kwargs)
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    if not isinstance(profiledb, ProfileDB): raise TypeError(type(profiledb))
    self._metadb, self._profiledb = metadb, profiledb

  @cached_property('_ctxs')
  @VersionedFormat.subunpack(list)
  def ctxs(self, *args):
    return [ContextInfo(*args, metadb=self._metadb, profiledb=self._profiledb,
                        offset=self._pCtxs + i*self._szCtx)
            for i in range(self._nCtxs)]

  def identical(self, other):
    if not isinstance(other, ContextInfoSection): raise TypeError(type(other))
    return (len(self.ctxs) == len(other.ctxs)
            and all(a.identical(b) for a,b in zip(self.ctxs, other.ctxs)))

  def __repr__(self):
    return (f"{self.__class__.__name__}(ctxs={self.ctxs!r})")

  def __str__(self):
    return (f"contexts:"
            + ('\n' + '\n'.join(f"context #{i:d}:\n" + textwrap.indent(str(c), '  ')
                                for i,c in enumerate(self.ctxs))
               if len(self.ctxs) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class ContextInfo(VersionedFormat,
    # Added in v4.0
    _vb_nValues = (0, 0x00, 'Q'),
    _vb_pValues = (0, 0x08, 'Q'),
    _vb_nMetrics = (0, 0x10, 'H'),
    _vb_pMetricIndices = (0, 0x18, 'Q'),
  ):
  """Information on a single context."""
  __slots__ = ['valueBlock']
  __vb_metricIndex = Struct('<HQ')  # metricId, startIndex

  def _init_(self, *, metadb, profiledb, valueBlock=None):
    super()._init_()
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    if not isinstance(profiledb, ProfileDB): raise TypeError(type(profiledb))
    if valueBlock is not None:
      self.valueBlock = ContextSparseValueBlock(valueBlock,
          byProfIndex=profiledb.profiles.profiles, byMetricId=metadb.metrics.byPropMetricId())

  def unpack_from(self, version, src, /, *args, metadb, profiledb, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    if not isinstance(metadb, MetaDB): raise TypeError(type(metadb))
    if not isinstance(profiledb, ProfileDB): raise TypeError(type(profiledb))
    self.valueBlock = ContextSparseValueBlock(lazy=(
        array_unpack(self.__vb_metricIndex, self._vb_nMetrics, src, offset=self._vb_pMetricIndices),
        src, self._vb_nValues, self._vb_pValues),
        byProfIndex=profiledb.profiles.profiles, byMetricId=metadb.metrics.byPropMetricId())

  def identical(self, other):
    if not isinstance(other, ContextInfo): raise TypeError(type(other))
    return self.valueBlock == other.valueBlock

  def __repr__(self):
    return (f"{self.__class__.__name__}("
            f"valueBlock={self.valueBlock!r})")

  def __str__(self):
    return ("valueBlock: "
            + ('\n' + textwrap.indent(str(self.valueBlock), '  ')
               if len(self.valueBlock) > 0 else " {}")
           )

  pack, pack_into = None, None


class ContextSparseValueBlock(collections.abc.MutableMapping):
  """Lazily-loaded block of sparse values, cct.db variant."""
  __slots__ = ['_real', '_byProfIndex', '_byMetricId', '_metricIndices',
               '_src', '_pValues']
  __value = Struct('<Ld')  # profIndex, value

  def __init__(self, initial=None, *, byProfIndex, byMetricId, lazy=None):
    super().__init__()
    self._real = {}
    self._byProfIndex, self._byMetricId = byProfIndex, byMetricId
    self._metricIndices = {}

    if initial is not None:
      for k,v in (initial.items() if isinstance(initial, collections.abc.Mapping) else initial):
        self[k] = v

    if lazy is not None:
      metricIndices, self._src, nValues, pValues = lazy
      self._pValues = pValues.__index__()

      last = None
      for m, i in metricIndices:
        if last is not None:
          lm,li = last
          self._metricIndices[lm] = (li, i - li)
        last = (m, i)
      if last is not None:
        m, i = last
        self._metricIndices[m] = (i, nValues.__index__() - i)

  @staticmethod
  def _key(key):
    met, scope = key
    if not isinstance(met, MetricDescription): raise TypeError(type(met))
    if scope not in met.scopes: raise ValueError(key)
    if not isinstance(scope, PropagationScope): raise TypeError(type(scope))
    return (met, scope)

  def __getitem__(self, key):
    key = self._key(key)
    if key in self._real: return self._real[key]
    met, scope = key
    if scope.propMetricId not in self._metricIndices: raise KeyError(key)

    # Load the missing block
    index, cnt = self._metricIndices[scope.propMetricId]
    del self._metricIndices[scope.propMetricId]
    block = ContextValues((self._byProfIndex[p], v) for p,v in
        array_unpack(self.__value, cnt, self._src, offset=self._pValues + self.__value.size*index))
    self._real[key] = block
    return block

  def __setitem__(self, key, block):
    key = self._key(key)
    if not isinstance(block, ContextValues): block = ContextValues(block)
    self._real[key] = block
    met, scope = key
    if scope.propMetricId in self._metricIndices: del self._metricIndices[scope.propMetricId]

  def __delitem__(self, key):
    key = self._key(key)
    met, scope = key
    if key in self._real: del self._real[key]
    elif scope.propMetricId in self._metricIndices: del self._metricIndices[scope.propMetricId]
    else: raise KeyError(key)

  def __iter__(self):
    return itertools.chain(self._real, list(self._byMetricId[metricId] for metricId in self._metricIndices))

  def __len__(self):
    return len(self._real) + len(self._metricIndices)

  def __str__(self):
    return '\n'.join(f"metric {m.name}/{p.scope} #{p.propMetricId}:"
                     + ("\n" + '\n'.join(textwrap.indent(str(b), '  '))
                        if len(b) > 0 else " {}")
                     for (m,p),b in sorted(self.items(), key=lambda x: x[0][1].propMetricId))


class ContextValues(collections.abc.MutableMapping):
  """Profile-value mapping for contexts"""
  __slots__ = ['_real']

  def __init__(self, initial=None):
    self._real = {}
    for k,v in (initial.items() if isinstance(initial, collections.abc.Mapping) else initial):
      self[k] = v

  def __getitem__(self, key):
    if not isinstance(key, ProfileInfo): raise TypeError(type(key))
    return self._real[key]

  def __setitem__(self, key, value):
    if not isinstance(key, ProfileInfo): raise TypeError(type(key))
    self._real[key] = float(value)

  def __delitem__(self, key):
    if not isinstance(key, ProfileInfo): raise TypeError(type(key))
    del self._real[key]

  def __iter__(self): return iter(self._real)
  def __len__(self): return len(self._real)

  def __repr__(self):
    return f"{self.__class__.__name__}({self._real!r})"

  def __str__(self):
    return '\n'.join(f"profile {p.shorthand}: {v}" for p,v in self.items())
