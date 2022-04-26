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
    _szScope = (0, 0x0d, 'B'),
    _szSummary = (0, 0x0e, 'B'),
  ):
  """meta.db Performance Metrics section."""
  __slots__ = ['_metrics']

  def _init_(self, *, metrics=[]):
    super()._init_()
    self.metrics = {m if isinstance(m, MetricDescription) else MetricDescription(**m)
                    for m in metrics}

  @cached_property('_metrics')
  @VersionedFormat.subunpack(set)
  def metrics(self, *args):
    r = {MetricDescription(*args, offset=self._pMetrics + i*self._szMetric)
         for i in range(self._nMetrics)}
    for m in r: m._szScope, m._szSummary = self._szScope, self._szSummary
    return r

  def identical(self, other):
    if not isinstance(other, PerformanceMetricsSection): raise TypeError(type(other))
    return isomorphic_seq(self.metrics, other.metrics,
                          lambda a,b: a.identical(b), key=lambda m: m.name)

  def __repr__(self):
    return f"PerformanceMetricsSection(metrics={self.metrics!r})"

  def __str__(self):
    return ("metrics:"
            + ('\n' + '\n'.join(' - ' + textwrap.indent(str(m), '   ')[3:]
                   for m in sorted(self.metrics, key=lambda m: m.name))
               if len(self.metrics) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class MetricDescription(VersionedFormat,
    # Added in v4.0
    _pName = (0, 0x00, 'Q'),
    _nScopes = (0, 0x08, 'H'),
    _pScopes = (0, 0x10, 'Q'),
  ):
  """Description for a single performance metric."""
  __slots__ = ['_szScope', '_szSummary', 'name', '_scopes']

  def _init_(self, *, name, scopes=[]):
    super()._init_()
    self.name = name
    self.scopes = {p if isinstance(p, PropagationScope) else PropagationScope(**p)
                   for p in scopes}

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, **kwargs)
    self.name = read_ntstring(src, self._pName)

  @cached_property('_scopes')
  @VersionedFormat.subunpack(set)
  def scopes(self, *args):
    r = {PropagationScope(*args, offset=self._pScopes + i*self._szScope)
         for i in range(self._nScopes)}
    for p in r: p._szSummary = self._szSummary
    return r

  def identical(self, other):
    if not isinstance(other, MetricDescription): raise TypeError(type(other))
    return (self.name == other.name
            and isomorphic_seq(self.scopes, other.scopes,
                               lambda a,b: a.identical(b),
                               key=lambda s: s.scope))

  def __repr__(self):
    return (f"MetricDescription(name={self.name!r}, "
            f"scopes={self.scopes!r})")

  def __str__(self):
    return (f"name: {self.name}\n"
            f"scopes:"
            + ('\n' + '\n'.join(' - '+textwrap.indent(str(s), '   ')[3:]
                   for s in sorted(self.scopes, key=lambda s: s.scope))
               if len(self.scopes) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class PropagationScope(VersionedFormat,
    # Added in v4.0
    _pScope = (0, 0x00, 'Q'),
    _nSummaries = (0, 0x08, 'H'),
    propMetricId = (0, 0x0a, 'H'),
    _pSummaries = (0, 0x10, 'Q'),
  ):
  """Description of a single propagation scope within a Metric."""
  __slots__ = ['_szSummary', 'scope', '_summaries']

  def _init_(self, *, scope, propMetricId, summaries=[]):
    super()._init_()
    self.scope = scope
    self.propMetricId = propMetricId.__index__()
    self.summaries = {s if isinstance(s, SummaryStatistic) else SummaryStatistic(**s)
                      for s in summaries}

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self.scope = read_ntstring(src, self._pScope)

  @cached_property('_summaries')
  @VersionedFormat.subunpack(set)
  def summaries(self, *args):
    return {SummaryStatistic(*args, offset=self._pSummaries + i*self._szSummary)
            for i in range(self._nSummaries)}

  def identical(self, other):
    if not isinstance(other, PropagationScope): raise TypeError(type(other))
    return (self.scope == other.scope and self.propMetricId == other.propMetricId
            and isomorphic_seq(self.summaries, other.summaries,
                               lambda a,b: a.identical(b),
                               key=lambda s: (s.formula, s.combine)))

  def __repr__(self):
    return (f"{self.__class__.__name__}(scope={self.scope!r}, "
            f"propMetricId={self.propMetricId!r}, "
            f"summaries={self.summaries!r})")

  def __str__(self):
    return (f"scope: {self.scope}\n"
            f"propMetricId: {self.propMetricId}\n"
            "summaries:"
            + ('\n' + '\n'.join(' - '+textwrap.indent(str(s), '   ')[3:]
                   for s in sorted(self.summaries,
                                   key=lambda s: (s.formula, s.combine)))
               if len(self.summaries) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class SummaryStatistic(VersionedFormat,
    # Added in v4.0
    _pFormula = (0, 0x00, 'Q'),
    _combine = (0, 0x08, 'B'),
    statMetricId = (0, 0x0a, 'H'),
  ):
  """Description of a single summary static under a Scope."""
  __slots__ = ['formula']

  def _init_(self, *, formula, combine, statMetricId):
    super()._init_()
    self.formula = formula
    self.combine = combine
    self.statMetricId = statMetricId.__index__()

  @enum.unique
  class __Combine(enum.IntEnum):
    # Added in v4.0
    sum = 0
    min = 1
    max = 2

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self.formula = read_ntstring(src, offset=self._pFormula)

  @property
  def combine(self): return self.__Combine(self._combine).name
  @combine.setter
  def combine(self, v): self._combine = self.__Combine[v].value
  @combine.deleter
  def combine(self): del self._combine

  def identical(self, other):
    if not isinstance(other, SummaryStatistic): raise TypeError(type(other))
    return self.formula == other.formula and self.combine == other.combine \
           and self.statMetricId == other.statMetricId

  def __repr__(self):
    return (f"{self.__class__.__name__}("
            f"formula={self.formula!r}, "
            f"combine={self.combine!r}, "
            f"statMetricId={self.statMetricId!r})")

  def __str__(self):
    return (f"formula: {self.formula}\n"
            f"combine: {self.combine}\n"
            f"statMetricId: {self.statMetricId}")
