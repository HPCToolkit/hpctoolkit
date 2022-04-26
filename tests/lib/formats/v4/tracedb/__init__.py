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
from ..._util import (cached_property, unpack, array_unpack, VersionedBitFlags,
                      isomorphic_seq)

from collections import namedtuple
from struct import Struct
import itertools
import textwrap

__all__ = [
  'TraceDB',
  # v4.0
  'ContextTraceHeadersSection', 'ContextTraceHeader', 'ContextTraceElement',
]

def tsfmt(ts):
  sec, nsec = divmod(ts.__index__(), 1000000000)
  return f"{sec:d}.{nsec:09d}s"

@VersionedFormat.minimize
class TraceDB(FileHeader,
              format=b'trce', footer=b'trace.db', majorVersion=4, minorVersion=0,
    # Added in v4.0
    CtxTraces = (0,),
  ):
  """The trace.db file format."""
  __slots__ = ['_ctxTraces']

  def _init_(self, *, ctxTraces):
    super()._init_()
    self.ctxTraces = (ctxTraces if isinstance(ctxTraces, ContextTraceHeadersSection)
                      else ContextTraceHeadersSection(**ctxTraces))

  @cached_property('_ctxTraces')
  @VersionedFormat.subunpack(lambda: ContextTraceHeadersSection())
  def ctxTraces(self, *args):
    return ContextTraceHeadersSection(*args, offset=self._pCtxTraces)

  def identical(self, other):
    if not isinstance(other, TraceDB): raise TypeError(type(other))
    return self.ctxTraces.identical(other.ctxTraces)

  def __repr__(self):
    return (f"{self.__class__.__name__}(ctxTraces={self.ctxTraces!r})")

  def __str__(self):
    return (f"TraceDB v4.{self.minorVersion}:\n"
            " Context Traces:\n"
            + textwrap.indent(str(self.ctxTraces), '  ')
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class ContextTraceHeadersSection(VersionedFormat,
    # Added in v4.0
    _pTraces = (0, 0x00, 'Q'),
    _nTraces = (0, 0x08, 'L'),
    _szTrace = (0, 0x0c, 'B'),
    minTimestamp = (0, 0x10, 'Q'),
    maxTimestamp = (0, 0x18, 'Q'),
  ):
  """trace.db Context Trace Headers section."""
  __slots__ = ['_traces']

  def _init_(self, *, traces=[], minTimestamp, maxTimestamp):
    super()._init_()
    self.minTimestamp = minTimestamp.__index__()
    self.maxTimestamp = maxTimestamp.__index__()
    self.traces = [t if isinstance(t, ContextTraceHeader) else ContextTraceHeader(**t)
                   for t in traces]

  @cached_property('_traces')
  @VersionedFormat.subunpack(list)
  def traces(self, *args):
    return [ContextTraceHeader(*args, offset=self._pTraces + i*self._szTrace)
            for i in range(self._nTraces)]

  def identical(self, other):
    if not isinstance(other, ContextTraceHeadersSection): raise TypeError(type(other))
    return (self.minTimestamp == other.minTimestamp
            and self.maxTimestamp == other.maxTimestamp
            and isomorphic_seq(self.traces, other.traces,
                               lambda a,b: a.identical(b),
                               key=lambda t: t.profIndex))

  def __repr__(self):
    return (f"{self.__class__.__name__}(traces={self.traces!r}, "
            f"minTimestamp={self.minTimestamp!r}, "
            f"maxTimestamp={self.maxTimestamp!r})")

  def __str__(self):
    return (f"traces: {tsfmt(self.minTimestamp)} - {tsfmt(self.maxTimestamp)}\n"
            + ('\n'.join(textwrap.indent(str(t), '  ') for t in self.traces)
               if len(self.traces) > 0 else "  []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class ContextTraceHeader(VersionedFormat,
    # Added in v4.0
    profIndex = (0, 0x00, 'L'),
    _pStart = (0, 0x08, 'Q'),
    _pEnd = (0, 0x10, 'Q'),
  ):
  """Header for a single trace of Contexts."""
  __slots__ = ['_trace']
  __elem = Struct('<QL')  # timestamp, ctxId

  def _init_(self, *, profIndex, trace=[]):
    super()._init_()
    self.profIndex = profIndex.__index__()
    self.trace = [ContextTraceElement(ts.__index__(), ctx.__index__())
                  for ts, ctx in trace]

  @cached_property('_trace')
  @VersionedFormat.subunpack(list)
  def trace(self, version, src):
    return [ContextTraceElement(ts, ctx) for ts,ctx in
            array_unpack(self.__elem, (self._pEnd-self._pStart) // 0x0c, src,
                         offset=self._pStart)]

  def identical(self, other):
    if not isinstance(other, ContextTraceHeader): raise TypeError(type(other))
    return self.profIndex == other.profIndex and self.trace == other.trace

  def __repr__(self):
    return (f"{self.__class__.__name__}(profIndex={self.profIndex!r}, "
            f"trace={self.trace!r})")

  def __str__(self):
    if len(self.trace) > 0:
      frag = (f" {tsfmt(self.trace[0][0])}\n"
              + '\n'.join(f"  context #{ctxId:d} @ +{tsfmt(ts - self.trace[0][0])}"
                          for ts, ctxId in self.trace))
    else:
      frag = " []"
    return f"context trace for profile #{self.profIndex:d}: {frag}"

  pack, pack_info = None, None

ContextTraceElement = namedtuple('ContextTraceElement', 'timestamp ctxId')
