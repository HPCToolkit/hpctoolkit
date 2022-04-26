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

from .metadb import *

def test_metadb():
  a = MetaDB(general = {'title': 'foo', 'description': 'bar'}, idNames = {},
             metrics = {}, modules = {}, files = {}, functions = {},
             context = {})
  sections = {'general', 'idNames', 'metrics', 'modules', 'files', 'functions',
              'context'}

  m = match_metadb(a, a)
  assert m and m.matches[a] is a
  for n in sections:
    assert m.matches[getattr(a, n)] is getattr(a, n)

  b = MetaDB(general = {'title': 'bar', 'description': 'foo'}, idNames = {},
             metrics = {}, modules = {}, files = {}, functions = {},
             context = {})
  m = match_metadb(a, b)
  assert not m and m.matches[a] is b
  for n in sections:
    if n == 'general':
      assert getattr(a, n) not in m.matches
      assert getattr(b, n) not in m.matches
    else:
      assert m.matches[getattr(a, n)] is getattr(b, n)

def test_idNamesSec():
  a = IdentifierNamesSection(names=['foo', 'bar'])
  b = IdentifierNamesSection(names=['bar'])
  assert match_idNamesSec(a, a)
  assert not match_idNamesSec(a, b)

def test_modulesSec():
  foo = LoadModule(path='foo.so')
  bar = LoadModule(path='bar.so')

  assert match_module(foo, foo)
  assert match_module(bar, bar)
  assert not match_module(foo, bar)

  a = LoadModulesSection(modules=[foo])
  m = match_modulesSec(a, a)
  assert m
  assert m.matches[a] is a
  assert m.matches[foo] is foo

  b = LoadModulesSection(modules=[foo, bar])
  m = match_modulesSec(a, b)
  assert not m
  assert m.matches[a] is b
  assert m.matches[foo] is foo
  assert bar not in m.matches

def test_filesSec():
  foo = SourceFile(path='foo.c')
  bar = SourceFile(path='bar.c')

  assert not match_file(foo, bar)

  a = SourceFilesSection(files=[foo])
  m = match_filesSec(a, a)
  assert m
  assert m.matches[a] is a
  assert m.matches[foo] is foo

  b = SourceFilesSection(files=[foo, bar])
  m = match_filesSec(a, b)
  assert not m
  assert m.matches[a] is b
  assert m.matches[foo] is foo
  assert bar not in m.matches

def test_functionsSec():
  lm_foo, sf_foo = LoadModule(path='foo.so'), SourceFile(path='foo.c')
  lm_bar, sf_bar = LoadModule(path='bar.so'), SourceFile(path='bar.c')
  lm_match = MatchResult.match(lm_foo, lm_bar)
  sf_match = MatchResult.match(sf_foo, sf_bar)

  foo = Function(name='foo', module=lm_foo, offset=0x10, file=sf_foo, line=10)
  barfoo = Function(name='foo', module=lm_bar, offset=0x10, file=sf_bar, line=10)
  bar = Function(name='bar', module=lm_bar, offset=0x20, file=sf_bar, line=20)

  assert match_function(foo, foo, modules=MatchResult.empty(), files=MatchResult.empty())
  assert match_function(foo, barfoo, modules=lm_match, files=sf_match)
  assert not match_function(foo, bar, modules=lm_match, files=sf_match)

  a = FunctionsSection(functions=[foo])
  m = match_functionsSec(a, a, modules=MatchResult.empty(), files=MatchResult.empty())
  assert m
  assert m.matches[a] is a
  assert m.matches[foo] is foo

  b = FunctionsSection(functions=[foo, bar])
  m = match_functionsSec(a, b, modules=MatchResult.empty(), files=MatchResult.empty())
  assert not m
  assert m.matches[a] is b
  assert m.matches[foo] is foo
  assert bar not in m.matches

  c = FunctionsSection(functions=[barfoo, bar])
  m = match_functionsSec(a, c, modules=lm_match, files=sf_match)
  assert not m
  assert m.matches[a] is c
  assert m.matches[foo] is barfoo
  assert bar not in m.matches

def test_metricsSec():
  s_foo = SummaryStatistic(formula='foo($$)', combine='sum', statMetricId=10)
  s_foo2 = SummaryStatistic(formula='foo($$)', combine='sum', statMetricId=20)
  s_bar = SummaryStatistic(formula='bar($$)', combine='min', statMetricId=20)

  assert match_summaryStat(s_foo, s_foo2)
  assert not match_summaryStat(s_foo, s_bar)

  p_foo = PropagationScope(scope='foo', propMetricId=10, summaries=[s_foo])
  p_foobar = PropagationScope(scope='foo', propMetricId=20, summaries=[s_foo, s_bar])
  p_bar = PropagationScope(scope='bar', propMetricId=10, summaries=[s_bar])

  m = match_propScope(p_foo, p_foo)
  assert m
  assert m.matches[p_foo] is p_foo
  assert m.matches[s_foo] is s_foo
  m = match_propScope(p_foo, p_foobar)
  assert not m
  assert m.matches[p_foo] is p_foobar
  assert m.matches[s_foo] is s_foo
  assert s_bar not in m.matches

  m_foo = MetricDescription(name='foo', scopes=[p_foo])
  m_foobar = MetricDescription(name='foo', scopes=[p_foo, p_bar])
  m_bar = MetricDescription(name='bar', scopes=[p_bar])

  m = match_metricDes(m_foo, m_foo)
  assert m
  assert m.matches[m_foo] is m_foo
  assert m.matches[p_foo] is p_foo
  m = match_metricDes(m_foo, m_foobar)
  assert not m
  assert m.matches[m_foo] is m_foobar
  assert m.matches[p_foo] is p_foo
  assert p_bar not in m.matches

  a = PerformanceMetricsSection(metrics=[m_foo])
  m = match_metricsSec(a, a)
  assert m
  assert m.matches[a] is a
  assert m.matches[m_foo] is m_foo

  b = PerformanceMetricsSection(metrics=[m_foo, m_bar])
  m = match_metricsSec(a, b)
  assert not m
  assert m.matches[a] is b
  assert m.matches[m_foo] is m_foo
  assert m_bar not in m.matches

def test_contextSec():
  foo = Function(name='foo')
  bar = Function(name='bar')

  a = ContextTreeSection(roots=[
    { 'ctxId': 1, 'relation': 'call', 'lexicalType': 'function', 'function': foo,
      'children': [
        { 'ctxId': 2, 'relation': 'lexical', 'lexicalType': 'function', 'function': bar },
      ]
    },
  ])
  m = match_contextSec(a, a, modules=MatchResult.empty(), files=MatchResult.empty(), functions=MatchResult.empty())
  assert m
  assert m.matches[a] is a
  assert m.matches[a.roots[0]] is a.roots[0]
  assert m.matches[a.roots[0].children[0]] is a.roots[0].children[0]

  b = ContextTreeSection(roots=[
    {'ctxId': 2, 'relation': 'call', 'lexicalType': 'function', 'function': foo},
    {'ctxId': 3, 'relation': 'call', 'lexicalType': 'function', 'function': bar},
  ])
  m = match_contextSec(a, b, modules=MatchResult.empty(), files=MatchResult.empty(), functions=MatchResult.empty())
  assert not m
  assert m.matches[a] is b
  assert m.matches[a.roots[0]] is b.roots[0]
  assert a.roots[0].children[0] not in m.matches
  assert b.roots[1] not in m.matches

  c = ContextTreeSection(roots=[
    { 'ctxId': 2, 'relation': 'call', 'lexicalType': 'function', 'function': bar,
      'children': [
        { 'ctxId': 4, 'relation': 'lexical', 'lexicalType': 'function', 'function': bar },
      ],
    },
    { 'ctxId': 3, 'relation': 'call', 'lexicalType': 'function', 'function': foo},
  ])
  m = match_contextSec(a, c, modules=MatchResult.empty(), files=MatchResult.empty(), functions=MatchResult.empty())
  assert not m
  assert m.matches[a] is c
  assert m.matches[a.roots[0]] is c.roots[1]
  assert a.roots[0].children[0] not in m.matches
  assert c.roots[0] not in m.matches
  assert c.roots[0].children[0] not in m.matches

