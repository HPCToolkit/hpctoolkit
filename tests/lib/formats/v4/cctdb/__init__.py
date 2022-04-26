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

__all__ = [
  'ContextDB',
  # v4.0
  'ContextInfoSection', 'ContextInfo', 'ContextSparseValueBlock',
]

@VersionedFormat.minimize
class ContextDB(FileHeader,
                format=b'ctxt', footer=b'__ctx.db', majorVersion=4, minorVersion=0,
    # Added in v4.0
    CtxInfos = (0,),
  ):
  """The cct.db file format."""
  __slots__ = ['_ctxs']

  def _init_(self, *, ctxs=None):
    super()._init_()
    if ctxs is not None:
      self.ctxs = ctxs if isinstance(ctxs, ContextInfoSection) \
                  else ContextInfoSection(**ctxs)

  @cached_property('_ctxs')
  @VersionedFormat.subunpack(lambda: ContextInfoSection())
  def ctxs(self, *args):
    return ContextInfoSection(*args, offset=self._pCtxInfos)

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
  __slots__ = ['_ctxs']

  def _init_(self, *, ctxs=None):
    super()._init_()
    if ctxs is not None:
      self.ctxs = [c if isinstance(c, ContextInfo) else ContextInfo(**c)
                   for c in ctxs]

  @cached_property('_ctxs')
  @VersionedFormat.subunpack(list)
  def ctxs(self, *args):
    return [ContextInfo(*args, offset=self._pCtxs + i*self._szCtx)
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

  def _init_(self, *, valueBlock=None):
    super()._init_()
    if valueBlock is not None:
      self.valueBlock = ContextSparseValueBlock(valueBlock)

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self.valueBlock = ContextSparseValueBlock.lazy(
        array_unpack(self.__vb_metricIndex, self._vb_nMetrics, src, offset=self._vb_pMetricIndices),
        src, self._vb_nValues, self._vb_pValues)

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


class ContextSparseValueBlock(dict):
  """Lazily-loaded block of sparse values, cct.db variant."""
  __slots__ = ['_metricIndices', '_src', '_pValues']
  __value = Struct('<Ld')  # profIndex, value

  def __init__(self, initial=None):
    super().__init__()
    if initial is not None:
      for k,v in (initial.items() if isinstance(initial, dict) else initial):
        self[k] = v

  @classmethod
  def lazy(cls, metricIndices, src, nValues, pValues):
    out = cls()
    out._src, out._pValues = src, pValues.__index__()

    out._metricIndices = {}
    last = None
    for c, i in metricIndices:
      if last is not None:
        lc,li = last
        out._metricIndices[lc] = (li, i - li)
      last = (c, i)
    if last is not None:
      c, i = last
      out._metricIndices[c] = (i, nValues.__index__() - i)

    return out

  def now(self):
    if not hasattr(self, '_metricIndices'): return
    for metricId in list(self._metricIndices):
      self.__missing__(metricId)
    return self

  def __missing__(self, metricId):
    if not hasattr(self, '_metricIndices'):
      raise IndexError()

    index, cnt = self._metricIndices[metricId]
    del self._metricIndices[metricId]
    values = dict(array_unpack(self.__value, cnt, self._src, offset=self._pValues + self.__value.size*index))
    super().__setitem__(metricId, values)
    return values

  def __delitem__(self, key):
    if hasattr(self, '_metricIndices') and key in self._metricIndices:
      del self._metricIndices[key]
    return super().__delitem__(key)

  def __iter__(self):
    if hasattr(self, '_metricIndices'):
      return itertools.chain(super().__iter__(), iter(self._metricIndices))
    return super().__iter__()

  def __len__(self):
    return super().__len__() + len(getattr(self, '_metricIndices', []))

  def __eq__(self, other):
    if not isinstance(other, ContextSparseValueBlock): raise TypeError(type(other))
    self.now()
    return super().__eq__(other.now())

  def __str__(self):
    return '\n'.join(f"metric #{k:d}:"
                     + ("\n" + '\n'.join(f"  profile #{p:d}: {v:f}"
                                         for p,v in sorted(self[k].items()))
                        if len(self[k]) > 0 else " {}")
                     for k in sorted(self))

