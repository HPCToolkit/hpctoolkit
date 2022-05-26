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

from ..._base import VersionedFormat
from ..._util import (read_ntstring, array_unpack, cached_property,
                      isomorphic_seq)

import enum
import textwrap

@VersionedFormat.minimize
class PerformanceMetricsSection(VersionedFormat,
    # Added in v4.0
    _pMetrics = (0, 0x00, 'Q'),
    _nMetrics = (0, 0x08, 'L'),
    _szMetric = (0, 0x0c, 'B'),
    _szScopeInst = (0, 0x0d, 'B'),
    _szSummary = (0, 0x0e, 'B'),
    _pScopes = (0, 0x10, 'Q'),
    _nScopes = (0, 0x18, 'H'),
    _szScope = (0, 0x1a, 'B'),
  ):
  """meta.db Performance Metrics section."""
  __slots__ = ['_metrics', '_scopes', '_scopeTable']

  def _init_(self, *, metrics=tuple(), scopes=tuple()):
    super()._init_()
    self.metrics = {m if isinstance(m, MetricDescription) else MetricDescription(**m)
                    for m in metrics}
    self.scopes = {p if isinstance(p, PropagationScope) else PropagationScope(**p)
                   for p in scopes}

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self._scopeTable = {s._srcoffset: s for s in self.scopes if hasattr(s, '_srcoffset')}

  @cached_property('_metrics')
  @VersionedFormat.subunpack(set)
  def metrics(self, *args):
    r = {MetricDescription(*args, offset=self._pMetrics + i*self._szMetric)
         for i in range(self._nMetrics)}
    for m in r:
      m._szScopeInst, m._szSummary = self._szScopeInst, self._szSummary
      m._scopeTable = self._scopeTable
    return r

  @cached_property('_scopes')
  @VersionedFormat.subunpack(set)
  def scopes(self, *args):
    return {PropagationScope(*args, offset=self._pScopes + i*self._szScope)
            for i in range(self._nScopes)}

  def byPropMetricId(self):
    return {pi.propMetricId: (m, pi) for m in self.metrics for pi in m.scopeInsts}
  def byStatMetricId(self):
    return {s.statMetricId: (m, s) for m in self.metrics for s in m.summaries}

  def identical(self, other):
    if not isinstance(other, PerformanceMetricsSection): raise TypeError(type(other))
    return (isomorphic_seq(self.metrics, other.metrics,
                          lambda a,b: a.identical(b), key=lambda m: m.name)
            and isomorphic_seq(self.scopes, other.scopes,
                               lambda a,b: a.identical(b), key=lambda p: p.name))

  def __repr__(self):
    return f"PerformanceMetricsSection(metrics={self.metrics!r}, scopes={self.scopes!r})"

  def __str__(self):
    return ("metrics:"
            + ('\n' + '\n'.join(' - ' + textwrap.indent(str(m), '   ')[3:]
                   for m in sorted(self.metrics, key=lambda m: m.name))
               if len(self.metrics) > 0 else " []") + "\n"
            "scopes:"
            + ('\n' + '\n'.join(' - ' + textwrap.indent(str(m), '   ')[3:]
                   for m in sorted(self.scopes, key=lambda s: s.name))
               if len(self.scopes) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class MetricDescription(VersionedFormat,
    # Added in v4.0
    _pName = (0, 0x00, 'Q'),
    _pScopeInsts = (0, 0x08, 'Q'),
    _pSummaries = (0, 0x10, 'Q'),
    _nScopeInsts = (0, 0x18, 'H'),
    _nSummaries = (0, 0x1a, 'H'),
  ):
  """Description for a single performance metric."""
  __slots__ = ['_szScopeInst', '_szSummary', '_section', 'name',
               '_scopeInsts', '_summaries', '_scopeTable']

  def _init_(self, *, name, scopeInsts=tuple(), summaries=tuple()):
    super()._init_()
    self.name = name
    self.scopeInsts = {pi if isinstance(pi, PropagationScopeInstance) else PropagationScopeInstance(**pi)
                       for pi in scopeInsts}
    self.summaries = {s if isinstance(s, SummaryStatistic) else SummaryStatistic(**s)
                      for s in summaries}

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, **kwargs)
    self.name = read_ntstring(src, self._pName)

  @cached_property('_scopeInsts')
  @VersionedFormat.subunpack(set)
  def scopeInsts(self, *args):
    return {PropagationScopeInstance(*args, offset=self._pScopeInsts + i*self._szScopeInst,
                                     scopeTable=self._scopeTable)
            for i in range(self._nScopeInsts)}

  @cached_property('_summaries')
  @VersionedFormat.subunpack(set)
  def summaries(self, *args):
    return {SummaryStatistic(*args, offset=self._pSummaries + i*self._szSummary,
                             scopeTable=self._scopeTable)
            for i in range(self._nSummaries)}

  def identical(self, other):
    if not isinstance(other, MetricDescription): raise TypeError(type(other))
    return (self.name == other.name
            and isomorphic_seq(self.scopeInsts, other.scopeInsts,
                               lambda a,b: a.identical(b),
                               key=lambda p: p.scope.name)
            and isomorphic_seq(self.summaries, other.summaries,
                               lambda a,b: a.identical(b),
                               key=lambda s: (s.scope.name, s.formula, s.combine)))

  def __repr__(self):
    return (f"MetricDescription(name={self.name!r}, "
            f"scopeInsts={self.scopeInsts!r}), "
            f"summaries={self.summaries!r})")

  def __str__(self):
    return (f"name: {self.name}\n"
            "scope instances:"
            + ('\n' + '\n'.join(' - '+textwrap.indent(str(p), '   ')[3:]
                   for p in sorted(self.scopeInsts, key=lambda p: p.scope.name))
               if len(self.scopeInsts) > 0 else " []") + "\n"
            "summary statistics:"
            + ('\n' + '\n'.join(' - '+textwrap.indent(str(s), '   ')[3:]
                   for s in sorted(self.summaries, key=lambda s: (s.scope.name, s.formula, s.combine)))
               if len(self.summaries) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class PropagationScopeInstance(VersionedFormat,
    # Added in v4.0
    _pScope = (0, 0x00, 'Q'),
    propMetricId = (0, 0x08, 'H'),
  ):
  """Description of a single instantated propagation scope within a Metric."""
  __slots__ = ['scope']

  def _init_(self, *, scope, propMetricId, summaries=[]):
    super()._init_()
    if not isinstance(scope, PropagationScope):
      raise TypeError(type(scope))
    self.scope = scope
    self.propMetricId = propMetricId.__index__()

  def unpack_from(self, version, src, /, *args, scopeTable, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self.scope = scopeTable[self._pScope]

  def identical(self, other):
    if not isinstance(other, PropagationScopeInstance): raise TypeError(type(other))
    return (self.scope.identical(other.scope)
            and self.propMetricId == other.propMetricId)

  def __repr__(self):
    return (f"{self.__class__.__name__}(scope={self.scope!r}, "
            f"propMetricId={self.propMetricId!r})")

  def __str__(self):
    return (f"propMetricId: {self.propMetricId}\n"
            "scope:\n" + textwrap.indent(str(self.scope), '  ')
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class SummaryStatistic(VersionedFormat,
    # Added in v4.0
    _pScope = (0, 0x00, 'Q'),
    _pFormula = (0, 0x08, 'Q'),
    _combine = (0, 0x10, 'B'),
    statMetricId = (0, 0x12, 'H'),
  ):
  """Description of a single summary static under a Scope."""
  __slots__ = ['scope', 'formula']

  def _init_(self, *, scope, formula, combine, statMetricId):
    super()._init_()
    if not isinstance(scope, PropagationScope):
      raise TypeError(type(scope))
    self.scope = scope
    self.formula = formula
    self.combine = combine
    self.statMetricId = statMetricId.__index__()

  @enum.unique
  class __Combine(enum.IntEnum):
    # Added in v4.0
    sum = 0
    min = 1
    max = 2

  def unpack_from(self, version, src, /, *args, scopeTable, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self.scope = scopeTable[self._pScope]
    self.formula = read_ntstring(src, offset=self._pFormula)

  @property
  def combine(self): return self.__Combine(self._combine).name
  @combine.setter
  def combine(self, v): self._combine = self.__Combine[v].value
  @combine.deleter
  def combine(self): del self._combine

  def identical(self, other):
    if not isinstance(other, SummaryStatistic): raise TypeError(type(other))
    return (self.scope.identical(other.scope) and self.formula == other.formula
            and self.combine == other.combine
            and self.statMetricId == other.statMetricId)

  def __repr__(self):
    return (f"{self.__class__.__name__}("
            f"scope={self.scope!r}, "
            f"formula={self.formula!r}, "
            f"combine={self.combine!r}, "
            f"statMetricId={self.statMetricId!r})")

  def __str__(self):
    return (f"formula: {self.formula}\n"
            f"combine: {self.combine}\n"
            f"statMetricId: {self.statMetricId}\n"
            "scope:\n" + textwrap.indent(str(self.scope), '  ')
            )


@VersionedFormat.minimize
class PropagationScope(VersionedFormat,
    # Added in v4.0
    _pScopeName = (0, 0x00, 'Q'),
    _type = (0, 0x08, 'B'),
    propagationIndex = (0, 0x09, 'B'),
  ):
  """Description of a single propagation scope."""
  __slots__ = ['_srcoffset', 'name']

  def _init_(self, *, name, type, propagationIndex=255):
    super()._init_()
    self.name = name
    self.type = type
    self.propagationIndex = propagationIndex.__index__()

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self._srcoffset = kwargs.get('offset', 0).__index__()
    self.name = read_ntstring(src, self._pScopeName)

  @enum.unique
  class __Type(enum.IntEnum):
    # Added in v4.0
    custom = 0
    point = 1
    execution = 2
    transitive = 3

  @property
  def type(self): return self.__Type(self._type).name
  @type.setter
  def type(self, v): self._type = self.__Type[v].value
  @type.deleter
  def type(self): del self._type

  def identical(self, other):
    if not isinstance(other, PropagationScope): raise TypeError(type(other))
    return (self.name == other.name and self.type == other.type
            and self.propagationIndex == other.propagationIndex)

  def __repr__(self):
    return (f"{self.__class__.__name__}(name={self.name!r}, "
            f"type={self.type!r}, "
            f"propagationIndex={self.propagationIndex!r})")

  def __str__(self):
    return (f"name: {self.name}\n"
            f"type: {self.type}"
            + (f"\npropagationIndex: {self.propagationIndex}"
               if self.type == 'transitive' else ''))

  pack, pack_into = None, None
