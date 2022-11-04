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

from pathlib import Path

import pytest


@pytest.fixture
def v4_data_small(yaml):
    yield yaml.load(_data_small)


@pytest.fixture(scope="session")
def v4_data_small_path(tmp_path_factory):
    out: Path = tmp_path_factory.mktemp("data-v4") / "small.yaml"
    with open(out, "w", encoding="utf-8") as f:
        f.write(_data_small)
    yield out


_data_small = """\
!db/v4
meta: !meta.db/v4
  General: !meta.db/v4/GeneralProperties
    title: foo
    description: TODO database description
  IdNames: !meta.db/v4/IdentifierNames
    names:
    - SUMMARY
    - NODE
    - RANK
    - THREAD
    - GPUDEVICE
    - GPUCONTEXT
    - GPUSTREAM
    - CORE
  Metrics: !meta.db/v4/PerformanceMetrics
    scopes:
    - &id001 !meta.db/v4/PropagationScope
      scopeName: point
      type: !meta.db/v4/PropagationScope.Type point
      propagationIndex: 255
    - &id002 !meta.db/v4/PropagationScope
      scopeName: function
      type: !meta.db/v4/PropagationScope.Type transitive
      propagationIndex: 0
    - &id003 !meta.db/v4/PropagationScope
      scopeName: execution
      type: !meta.db/v4/PropagationScope.Type execution
      propagationIndex: 255
    metrics:
    - !meta.db/v4/Metric
      name: cycles
      scopeInsts:
      - !meta.db/v4/PropagationScopeInstance
        scope: *id001
        propMetricId: 15
      - !meta.db/v4/PropagationScopeInstance
        scope: *id002
        propMetricId: 16
      - !meta.db/v4/PropagationScopeInstance
        scope: *id003
        propMetricId: 17
      summaries:
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: '1'
        combine: &id004 !meta.db/v4/SummaryStatistic.Combine sum
        statMetricId: 15
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id004
        statMetricId: 18
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: ($$^2)
        combine: *id004
        statMetricId: 21
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: &id005 !meta.db/v4/SummaryStatistic.Combine min
        statMetricId: 24
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: &id006 !meta.db/v4/SummaryStatistic.Combine max
        statMetricId: 27
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: '1'
        combine: *id004
        statMetricId: 16
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id004
        statMetricId: 19
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: ($$^2)
        combine: *id004
        statMetricId: 22
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id005
        statMetricId: 25
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id006
        statMetricId: 28
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: '1'
        combine: *id004
        statMetricId: 17
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id004
        statMetricId: 20
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: ($$^2)
        combine: *id004
        statMetricId: 23
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id005
        statMetricId: 26
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id006
        statMetricId: 29
    - !meta.db/v4/Metric
      name: instructions
      scopeInsts:
      - !meta.db/v4/PropagationScopeInstance
        scope: *id001
        propMetricId: 0
      - !meta.db/v4/PropagationScopeInstance
        scope: *id002
        propMetricId: 1
      - !meta.db/v4/PropagationScopeInstance
        scope: *id003
        propMetricId: 2
      summaries:
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: '1'
        combine: *id004
        statMetricId: 0
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id004
        statMetricId: 3
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: ($$^2)
        combine: *id004
        statMetricId: 6
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id005
        statMetricId: 9
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id006
        statMetricId: 12
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: '1'
        combine: *id004
        statMetricId: 1
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id004
        statMetricId: 4
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: ($$^2)
        combine: *id004
        statMetricId: 7
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id005
        statMetricId: 10
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id006
        statMetricId: 13
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: '1'
        combine: *id004
        statMetricId: 2
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id004
        statMetricId: 5
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: ($$^2)
        combine: *id004
        statMetricId: 8
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id005
        statMetricId: 11
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id006
        statMetricId: 14
  Modules: !meta.db/v4/LoadModules
    modules:
    - &id007 !meta.db/v4/Module
      flags: !meta.db/v4/Module.Flags []
      path: /tmp/foo
  Files: !meta.db/v4/SourceFiles
    files:
    - &id008 !meta.db/v4/File
      flags: !meta.db/v4/File.Flags [copied]
      path: src/tmp/foo.c
  Functions: !meta.db/v4/Functions
    functions:
    - &id014 !meta.db/v4/Function
      name: bar
      module: *id007
      offset: 4466
      file: *id008
      line: 8
      flags: &id009 !meta.db/v4/Function.Flags []
    - &id018 !meta.db/v4/Function
      name: foo
      module: *id007
      offset: 4393
      file: *id008
      line: 3
      flags: *id009
    - &id010 !meta.db/v4/Function
      name: main
      module: *id007
      offset: 4483
      file: *id008
      line: 12
      flags: *id009
  Context: !meta.db/v4/ContextTree
    entryPoints:
    - !meta.db/v4/EntryPoint
      ctxId: 22
      entryPoint: !meta.db/v4/EntryPoint.EntryPoint unknown_entry
      prettyName: unknown entry
      children: []
    - !meta.db/v4/EntryPoint
      ctxId: 1
      entryPoint: !meta.db/v4/EntryPoint.EntryPoint main_thread
      prettyName: main thread
      children:
      - !meta.db/v4/Context
        ctxId: 2
        flags: &id011 !meta.db/v4/Context.Flags [hasFunction]
        relation: &id012 !meta.db/v4/Context.Relation call
        lexicalType: &id013 !meta.db/v4/Context.LexicalType function
        propagation: 0
        function: *id010
        file:
        line:
        module:
        offset:
        children:
        - !meta.db/v4/Context
          ctxId: 3
          flags: &id015 !meta.db/v4/Context.Flags [hasSrcLoc]
          relation: &id016 !meta.db/v4/Context.Relation lexical
          lexicalType: &id017 !meta.db/v4/Context.LexicalType line
          propagation: 1
          function:
          file: *id008
          line: 1464
          module:
          offset:
          children:
          - !meta.db/v4/Context
            ctxId: 12
            flags: *id011
            relation: *id012
            lexicalType: *id013
            propagation: 0
            function: *id014
            file:
            line:
            module:
            offset:
            children:
            - !meta.db/v4/Context
              ctxId: 13
              flags: *id015
              relation: *id016
              lexicalType: *id017
              propagation: 1
              function:
              file: *id008
              line: 1464
              module:
              offset:
              children:
              - !meta.db/v4/Context
                ctxId: 15
                flags: *id011
                relation: *id012
                lexicalType: *id013
                propagation: 0
                function: *id018
                file:
                line:
                module:
                offset:
                children:
                - !meta.db/v4/Context
                  ctxId: 20
                  flags: *id015
                  relation: *id016
                  lexicalType: *id017
                  propagation: 1
                  function:
                  file: *id008
                  line: 1464
                  module:
                  offset:
                  children: []
                - !meta.db/v4/Context
                  ctxId: 16
                  flags: *id015
                  relation: *id016
                  lexicalType: &id019 !meta.db/v4/Context.LexicalType loop
                  propagation: 1
                  function:
                  file: *id008
                  line: 1464
                  module:
                  offset:
                  children:
                  - !meta.db/v4/Context
                    ctxId: 17
                    flags: *id015
                    relation: *id016
                    lexicalType: *id017
                    propagation: 1
                    function:
                    file: *id008
                    line: 1464
                    module:
                    offset:
                    children: []
          - !meta.db/v4/Context
            ctxId: 5
            flags: *id011
            relation: *id012
            lexicalType: *id013
            propagation: 0
            function: *id018
            file:
            line:
            module:
            offset:
            children:
            - !meta.db/v4/Context
              ctxId: 9
              flags: *id015
              relation: *id016
              lexicalType: *id017
              propagation: 1
              function:
              file: *id008
              line: 1464
              module:
              offset:
              children: []
            - !meta.db/v4/Context
              ctxId: 6
              flags: *id015
              relation: *id016
              lexicalType: *id019
              propagation: 1
              function:
              file: *id008
              line: 1464
              module:
              offset:
              children:
              - !meta.db/v4/Context
                ctxId: 7
                flags: *id015
                relation: *id016
                lexicalType: *id017
                propagation: 1
                function:
                file: *id008
                line: 1464
                module:
                offset:
                children: []
profile: !profile.db/v4
  ProfileInfos: !profile.db/v4/ProfileInfos
    profiles:
    - !profile.db/v4/Profile
      idTuple:
      flags: !profile.db/v4/Profile.Flags [isSummary]
      values:
        0: {}
        65536: {}
        21: {}
        1835008: {}
        5:
          15: 1.0
          16: 1.0
          17: 1.0
          19: 1000000000.0
          20: 1000000000.0
          22: 1000000000.0
          23: 1000000000.0
          25: 1000000000.0
          26: 1000000000.0
          28: 1000000000.0
          29: 1000000000.0
          18: 1000000000.0
          21: 1000000000.0
          24: 1000000000.0
          27: 1000000000.0
        393216: {}
        68: {}
        5439488: {}
        12:
          15: 1.0
          16: 1.0
          17: 1.0
          20: 1000000000.0
          23: 1000000000.0
          26: 1000000000.0
          29: 1000000000.0
          19: 1000000000.0
          22: 1000000000.0
          25: 1000000000.0
          28: 1000000000.0
          18: 1000000000.0
          21: 1000000000.0
          24: 1000000000.0
          27: 1000000000.0
        851968: {}
        111: {}
        7995392: {}
    - !profile.db/v4/Profile
      idTuple: !profile.db/v4/IdentifierTuple
        ids:
        - !profile.db/v4/Identifier
          kind: 1
          flags: !profile.db/v4/Identifier.Flags [isPhysical]
          logicalId: 0
          physicalId: 8323329
        - !profile.db/v4/Identifier
          kind: 3
          flags: !profile.db/v4/Identifier.Flags []
          logicalId: 0
          physicalId: 0
      flags: !profile.db/v4/Profile.Flags []
      values:
        0: {}
        65536: {}
        3: {}
        262144: {}
        5:
          16: 1000000000.0
          17: 1000000000.0
          15: 1000000000.0
        393216: {}
        11: {}
        917504: {}
        12:
          17: 1000000000.0
          16: 1000000000.0
          15: 1000000000.0
        851968: {}
        18: {}
        1310720: {}
context: !cct.db/v4
  CtxInfos: !cct.db/v4/ContextInfos
    contexts:
    - !cct.db/v4/PerContext
      values:
        17:
          1: 3000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 3000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 3000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 3000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 1000000000.0
    - !cct.db/v4/PerContext
      values:
        16:
          1: 1000000000.0
        1: {}
    - !cct.db/v4/PerContext
      values:
        16:
          1: 1000000000.0
        1: {}
    - !cct.db/v4/PerContext
      values:
        16:
          1: 1000000000.0
        1: {}
    - !cct.db/v4/PerContext
      values:
        15:
          1: 1000000000.0
        1: {}
        0: {}
    - !cct.db/v4/PerContext
      values: {}
    - !cct.db/v4/PerContext
      values: {}
    - !cct.db/v4/PerContext
      values:
        17:
          1: 2000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 2000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 2000000000.0
    - !cct.db/v4/PerContext
      values:
        17:
          1: 2000000000.0
    - !cct.db/v4/PerContext
      values:
        16:
          1: 2000000000.0
        1: {}
    - !cct.db/v4/PerContext
      values:
        16:
          1: 2000000000.0
        1: {}
    - !cct.db/v4/PerContext
      values:
        16:
          1: 2000000000.0
        1: {}
    - !cct.db/v4/PerContext
      values:
        15:
          1: 1000000000.0
        1: {}
        0: {}
    - !cct.db/v4/PerContext
      values:
        15:
          1: 1000000000.0
        1: {}
        0: {}
    - !cct.db/v4/PerContext
      values: {}
    - !cct.db/v4/PerContext
      values: {}
    - !cct.db/v4/PerContext
      values: {}
trace: !trace.db/v4
  CtxTraces: !trace.db/v4/ContextTraceHeaders
    timestampRange:
      min: 1656693533757421000
      max: 1656693534448716000
    traces:
    - !trace.db/v4/ContextTrace
      profIndex: 1
      line:
      - !trace.db/v4/ContextTraceElement
        timestamp: 1656693533987950000
        ctxId: 9
      - !trace.db/v4/ContextTraceElement
        timestamp: 1656693534218386000
        ctxId: 20
      - !trace.db/v4/ContextTraceElement
        timestamp: 1656693534448716000
        ctxId: 20
"""
