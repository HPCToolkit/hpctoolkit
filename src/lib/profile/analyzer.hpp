// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2019-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef HPCTOOLKIT_PROFILE_ANALYZE_H
#define HPCTOOLKIT_PROFILE_ANALYZE_H

#include <iostream>   //cout
#include "pipeline.hpp"

namespace hpctoolkit {

class ProfileAnalyzer {
public:
  virtual ~ProfileAnalyzer() = default;

  void bindPipeline(ProfilePipeline::Source&& se) noexcept {
    sink = std::move(se);
  }

  /// Query what Classes this Source can actually provide to the Pipeline.
  // MT: Safe (const)
  DataClass provides() const noexcept {
    return DataClass::metrics + DataClass::attributes;
  }

  virtual void context(Context& c) {}
  virtual void analysisMetricsFor(const Metric& m) {}
  virtual void analyze(Thread::Temporary& t) {}

protected:
  // Use a subclass to implement the bits.
  ProfileAnalyzer() = default;

  // Source of the Pipeline to use for inserting data: our sink.
  ProfilePipeline::Source sink;
};

struct LatencyBlameAnalyzer : public ProfileAnalyzer {
  LatencyBlameAnalyzer() = default;
  ~LatencyBlameAnalyzer() = default;

  void context(Context& ctx) noexcept override {

    const Scope& s = ctx.scope();
    if(s.type() == Scope::Type::point || s.type() == Scope::Type::call) {
      uint64_t offset = ctx.scope().point_data().second;
      auto mo = s.point_data();
      const auto& c = mo.first.userdata[sink.classification()];
      const std::map<uint64_t, uint32_t> empty_edges = {};
      auto iter = c._def_use_graph.find(offset);
      const std::map<uint64_t, uint32_t> &incoming_edges = (iter != c._def_use_graph.end()) ?
        iter->second: empty_edges;

      for (auto edge: incoming_edges) {
        uint64_t from = edge.first;
        ContextRef cr = sink.context(*ctx.direct_parent(), {ctx.scope().point_data().first, from}, false);
        context_map[offset].insert({from, cr});
      }
    }
  }

  void analysisMetricsFor(const Metric& m) noexcept override {
    const std::string latency_metric_name = "GINS: LAT(cycles)";
    const std::string frequency_metric_name = "GINS: EXC_CNT";
    const std::string name = "GINS: LAT_BLAME(cycles)";
    const std::string desc = "Accumulates the latency blame for a given context";
    if (m.name().find(latency_metric_name) != std::string::npos) {
      latency_metric = &m;
      latency_blame_metric = &(sink.metric(Metric::Settings(name, desc)));
      sink.metricFreeze(*latency_blame_metric);
    } else if (m.name().find(frequency_metric_name) != std::string::npos) {
      frequency_metric = &m;
    }
  }

  void analyze(Thread::Temporary& t) noexcept override {
    for(const auto& cd: t.accumulators().citerate()) {
      const Context& ctx = cd.first;
      const MetricAccumulator *m = cd.second.find(*latency_metric);
      if(m == nullptr) return;
      const Scope& s = ctx.scope();

      if(s.type() == Scope::Type::point || s.type() == Scope::Type::call) {
        auto mo = s.point_data();
        const auto& c = mo.first.userdata[sink.classification()];

        uint64_t offset = mo.second;
        const std::map<uint64_t, uint32_t> empty_edges = {};
        auto iter = c._def_use_graph.find(offset);
        const std::map<uint64_t, uint32_t> &incoming_edges = (iter != c._def_use_graph.end()) ?
          iter->second: empty_edges;

        double denominator;
        for (auto edge: incoming_edges) {
          uint64_t from = edge.first;
          ContextRef cr = context_map[offset].find(from)->second;
          const std::optional<double> ef_ptr = t.accumulators().find(std::get<Context>(cr))->find(*frequency_metric)->get(MetricScope::point);
          if (ef_ptr) {
            double execution_frequency = *ef_ptr;
            double path_length_inv = (double) 1 / (edge.second);
            denominator += execution_frequency * path_length_inv;
          }
        }
        int latency;
        const std::optional<double> l_ptr = t.accumulators().find(ctx)->find(*latency_metric)->get(MetricScope::point);
        if (l_ptr) {
          latency = *l_ptr;
        }
        for (auto edge: incoming_edges) {
          uint64_t from = edge.first;
          ContextRef cr = context_map[offset].find(from)->second;
          const std::optional<double> ef_ptr = t.accumulators().find(std::get<Context>(cr))->find(*frequency_metric)->get(MetricScope::point);
          if (ef_ptr) {
            double execution_frequency = *ef_ptr;
            double path_length_inv = (double) 1 / (edge.second);
            double latency_blame = execution_frequency * path_length_inv / denominator * latency;
            std::cout << "LAT_BLAME (analyze):: offset: " << from << ", val: " << latency_blame;
            sink.accumulateTo(cr, t).add(*latency_blame_metric, latency_blame);
          }
        }
      }
    }
  }

public:
  // offset -> ContextRef
  std::unordered_map <uint64_t, std::unordered_map<uint64_t, ContextRef>>context_map;

  const Metric *latency_metric;
  const Metric *frequency_metric;
  Metric *latency_blame_metric;
};

}

#endif  // HPCTOOLKIT_PROFILE_ANALYZE_H
