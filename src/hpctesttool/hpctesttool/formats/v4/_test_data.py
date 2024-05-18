# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

from pathlib import Path

import pytest


@pytest.fixture()
def v4_data_small(yaml):
    return yaml.load(_data_small)


@pytest.fixture(scope="session")
def v4_data_small_path(tmp_path_factory):
    out: Path = tmp_path_factory.mktemp("data-v4") / "small.yaml"
    with open(out, "w", encoding="utf-8") as f:
        f.write(_data_small)
    return out


_data_small = """\
!db/v4
meta: !meta.db/v4
  general: !meta.db/v4/GeneralProperties
    title: foo
    description: TODO database description
  id_names: !meta.db/v4/IdentifierNames
    names:
    - SUMMARY
    - NODE
    - RANK
    - THREAD
    - GPUDEVICE
    - GPUCONTEXT
    - GPUSTREAM
    - CORE
  metrics: !meta.db/v4/PerformanceMetrics
    scopes:
    - &id001 !meta.db/v4/PropagationScope
      scope_name: point
      type: !meta.db/v4/PropagationScope.Type point
      propagation_index: 255
    - &id002 !meta.db/v4/PropagationScope
      scope_name: function
      type: !meta.db/v4/PropagationScope.Type transitive
      propagation_index: 0
    - &id003 !meta.db/v4/PropagationScope
      scope_name: execution
      type: !meta.db/v4/PropagationScope.Type execution
      propagation_index: 255
    metrics:
    - !meta.db/v4/Metric
      name: cycles
      scope_insts:
      - !meta.db/v4/PropagationScopeInstance
        scope: *id001
        prop_metric_id: 15
      - !meta.db/v4/PropagationScopeInstance
        scope: *id002
        prop_metric_id: 16
      - !meta.db/v4/PropagationScopeInstance
        scope: *id003
        prop_metric_id: 17
      summaries:
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: '1'
        combine: &id004 !meta.db/v4/SummaryStatistic.Combine sum
        stat_metric_id: 15
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id004
        stat_metric_id: 18
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: ($$^2)
        combine: *id004
        stat_metric_id: 21
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: &id005 !meta.db/v4/SummaryStatistic.Combine min
        stat_metric_id: 24
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: &id006 !meta.db/v4/SummaryStatistic.Combine max
        stat_metric_id: 27
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: '1'
        combine: *id004
        stat_metric_id: 16
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id004
        stat_metric_id: 19
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: ($$^2)
        combine: *id004
        stat_metric_id: 22
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id005
        stat_metric_id: 25
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id006
        stat_metric_id: 28
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: '1'
        combine: *id004
        stat_metric_id: 17
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id004
        stat_metric_id: 20
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: ($$^2)
        combine: *id004
        stat_metric_id: 23
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id005
        stat_metric_id: 26
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id006
        stat_metric_id: 29
    - !meta.db/v4/Metric
      name: instructions
      scope_insts:
      - !meta.db/v4/PropagationScopeInstance
        scope: *id001
        prop_metric_id: 0
      - !meta.db/v4/PropagationScopeInstance
        scope: *id002
        prop_metric_id: 1
      - !meta.db/v4/PropagationScopeInstance
        scope: *id003
        prop_metric_id: 2
      summaries:
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: '1'
        combine: *id004
        stat_metric_id: 0
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id004
        stat_metric_id: 3
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: ($$^2)
        combine: *id004
        stat_metric_id: 6
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id005
        stat_metric_id: 9
      - !meta.db/v4/SummaryStatistic
        scope: *id001
        formula: $$
        combine: *id006
        stat_metric_id: 12
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: '1'
        combine: *id004
        stat_metric_id: 1
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id004
        stat_metric_id: 4
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: ($$^2)
        combine: *id004
        stat_metric_id: 7
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id005
        stat_metric_id: 10
      - !meta.db/v4/SummaryStatistic
        scope: *id002
        formula: $$
        combine: *id006
        stat_metric_id: 13
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: '1'
        combine: *id004
        stat_metric_id: 2
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id004
        stat_metric_id: 5
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: ($$^2)
        combine: *id004
        stat_metric_id: 8
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id005
        stat_metric_id: 11
      - !meta.db/v4/SummaryStatistic
        scope: *id003
        formula: $$
        combine: *id006
        stat_metric_id: 14
  modules: !meta.db/v4/LoadModules
    modules:
    - &id007 !meta.db/v4/Module
      flags: !meta.db/v4/Module.Flags []
      path: /tmp/foo
  files: !meta.db/v4/SourceFiles
    files:
    - &id008 !meta.db/v4/File
      flags: !meta.db/v4/File.Flags [copied]
      path: src/tmp/foo.c
  functions: !meta.db/v4/Functions
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
  context: !meta.db/v4/ContextTree
    entry_points:
    - !meta.db/v4/EntryPoint
      ctx_id: 22
      entry_point: !meta.db/v4/EntryPoint.EntryPoint unknown_entry
      pretty_name: unknown entry
      children: []
    - !meta.db/v4/EntryPoint
      ctx_id: 1
      entry_point: !meta.db/v4/EntryPoint.EntryPoint main_thread
      pretty_name: main thread
      children:
      - !meta.db/v4/Context
        ctx_id: 2
        flags: &id011 !meta.db/v4/Context.Flags [has_function]
        relation: &id012 !meta.db/v4/Context.Relation call
        lexical_type: &id013 !meta.db/v4/Context.LexicalType function
        propagation: 0
        function: *id010
        file:
        line:
        module:
        offset:
        children:
        - !meta.db/v4/Context
          ctx_id: 3
          flags: &id015 !meta.db/v4/Context.Flags [has_srcloc]
          relation: &id016 !meta.db/v4/Context.Relation lexical
          lexical_type: &id017 !meta.db/v4/Context.LexicalType line
          propagation: 1
          function:
          file: *id008
          line: 1464
          module:
          offset:
          children:
          - !meta.db/v4/Context
            ctx_id: 12
            flags: *id011
            relation: *id012
            lexical_type: *id013
            propagation: 0
            function: *id014
            file:
            line:
            module:
            offset:
            children:
            - !meta.db/v4/Context
              ctx_id: 13
              flags: *id015
              relation: *id016
              lexical_type: *id017
              propagation: 1
              function:
              file: *id008
              line: 1464
              module:
              offset:
              children:
              - !meta.db/v4/Context
                ctx_id: 15
                flags: *id011
                relation: *id012
                lexical_type: *id013
                propagation: 0
                function: *id018
                file:
                line:
                module:
                offset:
                children:
                - !meta.db/v4/Context
                  ctx_id: 20
                  flags: *id015
                  relation: *id016
                  lexical_type: *id017
                  propagation: 1
                  function:
                  file: *id008
                  line: 1464
                  module:
                  offset:
                  children: []
                - !meta.db/v4/Context
                  ctx_id: 16
                  flags: *id015
                  relation: *id016
                  lexical_type: &id019 !meta.db/v4/Context.LexicalType loop
                  propagation: 1
                  function:
                  file: *id008
                  line: 1464
                  module:
                  offset:
                  children:
                  - !meta.db/v4/Context
                    ctx_id: 17
                    flags: *id015
                    relation: *id016
                    lexical_type: *id017
                    propagation: 1
                    function:
                    file: *id008
                    line: 1464
                    module:
                    offset:
                    children: []
          - !meta.db/v4/Context
            ctx_id: 5
            flags: *id011
            relation: *id012
            lexical_type: *id013
            propagation: 0
            function: *id018
            file:
            line:
            module:
            offset:
            children:
            - !meta.db/v4/Context
              ctx_id: 9
              flags: *id015
              relation: *id016
              lexical_type: *id017
              propagation: 1
              function:
              file: *id008
              line: 1464
              module:
              offset:
              children: []
            - !meta.db/v4/Context
              ctx_id: 6
              flags: *id015
              relation: *id016
              lexical_type: *id019
              propagation: 1
              function:
              file: *id008
              line: 1464
              module:
              offset:
              children:
              - !meta.db/v4/Context
                ctx_id: 7
                flags: *id015
                relation: *id016
                lexical_type: *id017
                propagation: 1
                function:
                file: *id008
                line: 1464
                module:
                offset:
                children: []
profile: !profile.db/v4
  profile_infos: !profile.db/v4/ProfileInfos
    profiles:
    - !profile.db/v4/Profile
      id_tuple:
      flags: !profile.db/v4/Profile.Flags [is_summary]
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
      id_tuple: !profile.db/v4/IdentifierTuple
        ids:
        - !profile.db/v4/Identifier
          kind: 1
          flags: !profile.db/v4/Identifier.Flags [is_physical]
          logical_id: 0
          physical_id: 8323329
        - !profile.db/v4/Identifier
          kind: 3
          flags: !profile.db/v4/Identifier.Flags []
          logical_id: 0
          physical_id: 0
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
  ctx_infos: !cct.db/v4/ContextInfos
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
  ctx_traces: !trace.db/v4/ContextTraceHeaders
    timestamp_range:
      min: 1656693533757421000
      max: 1656693534448716000
    traces:
    - !trace.db/v4/ContextTrace
      prof_index: 1
      line:
      - !trace.db/v4/ContextTraceElement
        timestamp: 1656693533987950000
        ctx_id: 9
      - !trace.db/v4/ContextTraceElement
        timestamp: 1656693534218386000
        ctx_id: 20
      - !trace.db/v4/ContextTraceElement
        timestamp: 1656693534448716000
        ctx_id: 20
"""
