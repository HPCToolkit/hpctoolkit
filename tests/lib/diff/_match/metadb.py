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

from .common import *
from ...formats.v4.metadb import *

from functools import singledispatch

@singledispatch
def match(a, b): raise NotImplementedError

@match.register
def match_metadb(a: MetaDB, b: MetaDB) -> MatchResult:
  """
  Compare two MetaDB objects and find similarities. See match().
  """
  check_tyb(b, MetaDB)
  out = MatchResult.matchif(a, b, a.minorVersion == b.minorVersion)
  out |= match_generalSec(a.general, b.general)
  out |= match_idNamesSec(a.idNames, b.idNames)
  out |= match_metricsSec(a.metrics, b.metrics)
  out |= match_modulesSec(a.modules, b.modules)
  out |= match_filesSec(a.files, b.files)
  out |= match_functionsSec(a.functions, b.functions, modules=out, files=out)
  out |= match_contextSec(a.context, b.context)
  return out

@match.register
def match_generalSec(a: GeneralPropertiesSection, b: GeneralPropertiesSection) -> MatchResult:
  check_tyb(b, GeneralPropertiesSection)
  # Terminal object, so __eq__ is completely local
  return MatchResult.matchif(a, b, a == b)

@match.register
def match_idNamesSec(a: IdentifierNamesSection, b: IdentifierNamesSection) -> MatchResult:
  check_tyb(b, IdentifierNamesSection)
  # Terminal object, so __eq__ is completely local
  return MatchResult.matchif(a, b, a == b)

@match.register
def match_metricsSec(a: PerformanceMetricsSection, b: PerformanceMetricsSection) -> MatchResult:
  check_tyb(b, PerformanceMetricsSection)
  return (MatchResult.matchif(a, b)
          | merge_unordered(a.metrics, b.metrics, match_metricDes))

@match.register
def match_metricDes(a: MetricDescription, b: MetricDescription) -> MatchResult:
  check_tyb(b, MetricDescription)
  return (MatchResult.matchif(a, b, a.name == b.name)
          | merge_unordered(a.scopes, b.scopes, match_propScope))

@match.register
def match_propScope(a: PropagationScope, b: PropagationScope) -> MatchResult:
  check_tyb(b, PropagationScope)
  return (MatchResult.matchif(a, b, a.scope == b.scope)
          | merge_unordered(a.summaries, b.summaries, match_summaryStat))

@match.register
def match_summaryStat(a: SummaryStatistic, b: SummaryStatistic) -> MatchResult:
  check_tyb(b, SummaryStatistic)
  return MatchResult.matchif(a, b, a.formula == b.formula and a.combine == b.combine)

@match.register
def match_modulesSec(a: LoadModulesSection, b: LoadModulesSection) -> MatchResult:
  check_tyb(b, LoadModulesSection)
  return (MatchResult.matchif(a, b)
          | merge_unordered(a.modules, b.modules, match=match_module))

@match.register
def match_module(a: LoadModule, b: LoadModule) -> MatchResult:
  check_tyb(b, LoadModule)
  # Terminal object, so __eq__ is completely local
  return MatchResult.matchif(a, b, a == b)

@match.register
def match_filesSec(a: SourceFilesSection, b: SourceFilesSection) -> MatchResult:
  check_tyb(b, SourceFilesSection)
  return (MatchResult.matchif(a, b)
          | merge_unordered(a.files, b.files, match=match_file))

@match.register
def match_file(a: SourceFile, b: SourceFile) -> MatchResult:
  check_tyb(b, SourceFile)
  # Terminal object, so __eq__ is completely local
  return MatchResult.matchif(a, b, a == b)

@match.register
def match_functionsSec(a: FunctionsSection, b: FunctionsSection, /, *,
                       modules, files) -> MatchResult:
  if not isinstance(modules, MatchResult): raise TypeError(type(modules))
  if not isinstance(files, MatchResult): raise TypeError(type(files))
  check_tyb(b, FunctionsSection)
  f = lambda a, b: match_function(a, b, modules=modules, files=files)
  return (MatchResult.matchif(a, b)
          | merge_unordered(a.functions, b.functions, match=f))

@match.register
def match_function(a: Function, b: Function, /, *, modules, files) -> MatchResult:
  check_tyb(b, Function)
  if not isinstance(modules, MatchResult): raise TypeError(type(modules))
  if not isinstance(files, MatchResult): raise TypeError(type(files))
  return MatchResult.matchif(a, b,
      a.name == b.name and a.flags == b.flags
      and cmp_id(a.module, b.module, modules) and a.offset == b.offset
      and cmp_id(a.file, b.file, files) and a.line == b.line)

@match.register
def match_contextSec(a: ContextTreeSection, b: ContextTreeSection) -> MatchResult:
  check_tyb(b, ContextTreeSection)
  # TODO
  return MatchResult.matchif(a, b)
