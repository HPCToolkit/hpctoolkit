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

#include "metricsyaml.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>

#include "lib/profile/sinks/METRICS.yaml.inc"

using namespace hpctoolkit;
using namespace sinks;
namespace fs = stdshim::filesystem;

MetricsYAML::MetricsYAML(stdshim::filesystem::path p)
  : dir(std::move(p)) {};

void MetricsYAML::notifyWavefront(DataClass dc) {
  assert(dc.hasAttributes());
  if(dir.empty()) return;  // Dry-run mode
  auto outdir = dir / "metrics";

  // Create the directory and write out all the files
  stdshim::filesystem::create_directories(outdir);
  {
    std::ofstream example(outdir / "METRICS.yaml.ex");
    example << METRICS_yaml;
  }
  {
    std::ofstream out(outdir / "raw.yaml");
    raw(out);
  }
  {
    std::ofstream out(outdir / "default.yaml");
    standard(out);
  }
}

static std::string sanitize(const std::string& s) {
  std::ostringstream ss;
  ss << std::hex;
  for(const char c: s) {
    if(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')
       || ('0' <= c && c <= '9') || c == '-' || c == '_') {
      ss << c;
    } else {
      ss << 'x' << (int)c << '_';
    }
  }
  return ss.str();
}

static std::string anchorName(const Metric& m, const StatisticPartial& p, MetricScope s) {
  std::ostringstream ss;
  ss << sanitize(m.name()) << '-' << p.combinator() << '-' << p.accumulate() << '-' << s;
  return ss.str();
}

// "Raw" version where all Metrics are output fairly verbatim
void MetricsYAML::raw(std::ostream& os) {
  using namespace YAML;
  Emitter out(os);
  out << BeginMap << Key << "version" << Value << 0;

  // First list all the input Metrics
  out << Key << "inputs" << BeginSeq;
  for(const Metric& m: src.metrics().citerate()) {
    for(const auto s: {MetricScope::point, MetricScope::function, MetricScope::execution}) {
      if(!m.scopes().has(s)) continue;
      for(const auto& p: m.partials()) {
        out << Anchor(anchorName(m, p, s)) << BeginMap
            << Key << "metric" << Value << m.name()
            << Key << "scope" << Value << s
            << Key << "formula" << Value << p.accumulate()
            << Key << "combine" << Value << p.combinator()
            << EndMap;
      }
    }
  }
  out << EndSeq;

  // Then the taxonomic metrics. In the raw case they're the same pretty much.
  out << Key << "roots" << BeginSeq;
  for(const Metric& m: src.metrics().citerate()) {
    out << BeginMap
        << Key << "name" << Value << m.name()
        << Key << "description" << Value << Literal << m.description()
        << Key << "variants" << Value << BeginMap;
    for(const Statistic& s: m.statistics()) {
      out << Key << s.suffix() << Value << BeginMap;
      {
        out << Key << "render" << Value << Flow << BeginSeq << "number";
        if(s.showPercent()) out << "percent";
        out << EndSeq;

        out << Key << "formula" << Value << BeginMap;
        for(const auto sc: {MetricScope::point, MetricScope::function, MetricScope::execution}) {
          if(!m.scopes().has(sc)) continue;
          out << Key;
          switch(sc) {
          case MetricScope::point: out << "point"; break;
          case MetricScope::function: out << "exclusive"; break;
          case MetricScope::execution: out << "inclusive"; break;
          }
          out << Value << Flow << BeginSeq;
          for(const auto& e: s.finalizeFormula()) {
            if(const auto* piece = std::get_if<std::string>(&e)) {
              out << *piece;
            } else if(const auto* idx = std::get_if<size_t>(&e)) {
              out << Alias(anchorName(m, m.partials()[*idx], sc));
            } else std::abort();
          }
          out << EndSeq;
        }
        out << EndMap;
      }
      out << EndMap;
    }
    out << EndMap << EndMap;
  }
  out << EndSeq;

  // Close it off
  out << EndMap;
  if(!out.good())
    util::log::fatal{} << "YAML::Emitter error: " << out.GetLastError();
  assert(out.good());
}

// Standard version where we try our best to fit every Metric somewhere reasonable
void MetricsYAML::standard(std::ostream& os) {
  YAML::Emitter out(os);
  out << "WIP TODO";
}
