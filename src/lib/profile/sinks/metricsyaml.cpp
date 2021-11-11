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

#include "experimentxml4.hpp"

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
  ss << m.name() << '-' << p.combinator() << '-' << p.accumulate() << '-' << s;
  return sanitize(ss.str());
}

// These need to be in the YAML namespace for ADL to work properly
namespace YAML {
static YAML::Emitter& operator<<(YAML::Emitter& e, const Statistic::combination_t c) {
  switch(c) {
  case Statistic::combination_t::sum: return e << "sum";
  case Statistic::combination_t::min: return e << "min";
  case Statistic::combination_t::max: return e << "max";
  }
  std::abort();
}
static YAML::Emitter& operator<<(YAML::Emitter& e, const MetricScope ms) {
  switch(ms) {
  case MetricScope::point: return e << "point";
  case MetricScope::function: return e << "function";
  case MetricScope::execution: return e << "execution";
  }
  std::abort();
}
}

static util::optional_ref<const Metric> claim(std::unordered_map<std::string_view, const Metric&>::node_type&& node) {
  if(!node) return {};
  return node.mapped();
}
static void claim(std::vector<std::reference_wrapper<const Metric>>& into,
    std::unordered_map<std::string_view, const Metric&>::node_type&& node) {
  if(!node) return;
  into.push_back(node.mapped());
}

static bool starts_with(std::string_view a, std::string_view b) {
  return a.substr(0, b.size()) == b;
}

static bool rawFormula(YAML::Emitter& out, const Metric& m, const Statistic& s, MetricScope ms, const std::string& key) {
  using namespace YAML;
  if(!m.scopes().has(ms)) return false;
  out << Key << key << Value << Flow;
  s.finalizeFormula().citerate_all(
    [&](double v){ out << v; },
    [&](Expression::uservalue_t v){
      out << Alias(anchorName(m, m.partials()[v], ms));
    },
    [&](const Expression& e){
      out << BeginMap << Key;
      switch(e.kind()) {
      case Expression::Kind::constant:
      case Expression::Kind::subexpression:
      case Expression::Kind::variable:
        std::abort();
      case Expression::Kind::op_sum: out << "+"; break;
      case Expression::Kind::op_sub:
        if(e.op_args().size() == 1) out << "+";
        else out << "-";
        break;
      case Expression::Kind::op_neg: out << "-"; break;
      case Expression::Kind::op_prod: out << "*"; break;
      case Expression::Kind::op_div: out << "/"; break;
      case Expression::Kind::op_pow: out << "^"; break;
      case Expression::Kind::op_sqrt: out << "sqrt"; break;
      case Expression::Kind::op_log: out << "log"; break;
      case Expression::Kind::op_ln: out << "ln"; break;
      case Expression::Kind::op_min: out << "min"; break;
      case Expression::Kind::op_max: out << "max"; break;
      case Expression::Kind::op_floor: out << "floor"; break;
      case Expression::Kind::op_ceil: out << "ceil"; break;
      }
      out << Value << BeginSeq;
    },
    [&](const Expression& e){
      out << EndSeq << EndMap;
    }
  );
  return true;
}

static void rawLeafVariants(YAML::Emitter& out, const Metric& m) {
  using namespace YAML;
  out << Key << "variants" << Value << BeginMap;
  assert(!m.statistics().empty());
  for(const auto& s: m.statistics()) {
    out << Key << s.suffix() << Value << BeginMap
        << Key << "render" << Value
          << Flow << BeginSeq << "number";
    if(s.showPercent()) out << "percent";
    out << EndSeq
        << Key << "formula" << Value << BeginMap;
    rawFormula(out, m, s, MetricScope::execution, "inclusive");
    if(!rawFormula(out, m, s, MetricScope::function, "exclusive"))
      rawFormula(out, m, s, MetricScope::point, "point");
    out << EndMap
        << EndMap;
  }
  out << EndMap;
}

static void rawLeaf(YAML::Emitter& out, const Metric& m) {
  // TODO: This shouldn't actually happen, but since we're still WIP it does
  // sometimes. Just no-op in this case.
  if(m.statistics().empty()) return;
  using namespace YAML;
  out << BeginMap
      << Key << "name" << Value << m.name()
      << Key << "description" << Value << m.description();
  rawLeafVariants(out, m);
  out << EndMap;
}

namespace {

struct GPU {
  struct Ker {
    bool empty = true;
    util::optional_ref<const Metric> time;
    util::optional_ref<const Metric> count;
    Ker(std::unordered_map<std::string_view, const Metric&>& mets) {
      time = claim(mets.extract("GKER (sec)"));
      if(!time) return;
      empty = false;
      count = claim(mets.extract("GKER:COUNT"));
    }
    void dump(YAML::Emitter& out) {
      using namespace YAML;
      if(!time) return;
      out << BeginMap
          << Key << "name" << Value << "Kernel Execution"
          << Key << "description" << Value << "Time spent running kernels on a GPU.";
      rawLeafVariants(out, *time);
      out << EndMap;
    }
  } c_ker;

  struct Sync {
    bool empty;
    std::vector<std::reference_wrapper<const Metric>> kinds;
    Sync(std::unordered_map<std::string_view, const Metric&>& mets) {
      mets.erase("GSYNC:COUNT");
      claim(kinds, mets.extract("GSYNC:STR (sec)"));
      claim(kinds, mets.extract("GSYNC:STRE (sec)"));
      claim(kinds, mets.extract("GSYNC:EVT (sec)"));
      claim(kinds, mets.extract("GSYNC:CTX (sec)"));
      claim(kinds, mets.extract("GSYNC:UNK (sec)"));
      for(auto it = mets.begin(); it != mets.end(); ) {
        if(starts_with(it->second.name(), "GSYNC:")) {
          kinds.push_back(mets.extract(it++).mapped());
          continue;
        }
        ++it;
      }
      empty = kinds.empty();
    }
    void dump(YAML::Emitter& out) {
      using namespace YAML;
      if(kinds.empty()) return;
      out << BeginMap
          << Key << "name" << Value << "Synchronization"
          << Key << "description" << Value << "Time spent idle waiting for actions from the host / other GPU operations."
          << Key << "variants" << Value << BeginMap
            << Key << "Sum" << Value << BeginMap
              << Key << "render" << Value
                << Flow << BeginSeq << "number" << "percent" << "colorbar" << EndSeq
              << Key << "formula" << Value << "sum"
            << EndMap
          << EndMap
          << Key << "children" << Value << BeginSeq;
      for(const Metric& m: kinds) rawLeaf(out, m);
      out << EndSeq
          << EndMap;
    }
  } c_sync;

  struct MemCpy {
    bool empty = true;
    util::optional_ref<const Metric> time_x;
    std::vector<std::reference_wrapper<const Metric>> sub_x;
    util::optional_ref<const Metric> time_i;
    std::vector<std::reference_wrapper<const Metric>> sub_i;
    MemCpy(std::unordered_map<std::string_view, const Metric&>& mets) {
      time_x = claim(mets.extract("GXCOPY (sec)"));
      time_i = claim(mets.extract("GICOPY (sec)"));
      if(!time_x && !time_i) return;
      empty = false;
      mets.erase("GXCOPY:COUNT");
      claim(sub_x, mets.extract("GXCOPY:H2D (B)"));
      claim(sub_x, mets.extract("GXCOPY:D2H (B)"));
      claim(sub_x, mets.extract("GXCOPY:H2A (B)"));
      claim(sub_x, mets.extract("GXCOPY:A2H (B)"));
      claim(sub_x, mets.extract("GXCOPY:P2P (B)"));
      claim(sub_x, mets.extract("GXCOPY:D2D (B)"));
      claim(sub_x, mets.extract("GXCOPY:D2A (B)"));
      claim(sub_x, mets.extract("GXCOPY:A2D (B)"));
      claim(sub_x, mets.extract("GXCOPY:A2A (B)"));
      claim(sub_x, mets.extract("GXCOPY:H2H (B)"));
      claim(sub_x, mets.extract("GXCOPY:UNK (B)"));
      mets.erase("GICOPY:COUNT");
      claim(sub_i, mets.extract("GICOPY:H2D (B)"));
      claim(sub_i, mets.extract("GICOPY:D2H (B)"));
      claim(sub_i, mets.extract("GICOPY:D2D (B)"));
      claim(sub_i, mets.extract("GICOPY:UNK (B)"));
      // TODO: There are some other GICOPY:* here, not sure what they measure
    }
    void dump(YAML::Emitter& out) {
      using namespace YAML;
      if(!time_x && !time_i) return;
      out << BeginMap
          << Key << "name" << Value << "Memory Transfer"
          << Key << "description" << Value << "Time spent in data transfers to, from and within GPU memory"
          << Key << "variants" << Value << BeginMap
            << Key << "Sum" << Value << BeginMap
              << Key << "render" << Value
                << Flow << BeginSeq << "number" << "percent" << "colorbar" << EndSeq
              << Key << "formula" << Value << "sum"
            << EndMap
          << EndMap
          << Key << "children" << BeginSeq;
      if(time_x) {
        out << BeginMap
            << Key << "name" << Value << "Explicit Memory Transfer"
            << Key << "description" << "Time spent in data transfers explicitly performed by the user"
            << Key << "variants" << Value << BeginMap;
        for(const auto& s: time_x->statistics()) {
          out << Key << s.suffix() << Value << BeginMap
              << Key << "render" << Value
                << Flow << BeginSeq << "number" << "hidden";
          if(s.showPercent()) out << "percent";
          out << EndSeq
              << Key << "formula" << Value << BeginMap;
          rawFormula(out, *time_x, s, MetricScope::execution, "inclusive");
          if(!rawFormula(out, *time_x, s, MetricScope::function, "exclusive"))
            rawFormula(out, *time_x, s, MetricScope::point, "point");
          out << EndMap
              << EndMap;
        }
        out << EndMap
            << Key << "children" << Value << BeginSeq;
        for(const Metric& sm: sub_x) rawLeaf(out, sm);
        out << EndSeq
            << EndMap;
      }
      if(time_i) {
        out << BeginMap
            << Key << "name" << Value << "Implicit Memory Transfer"
            << Key << "description" << "Time spent in data transfers implicitly required by the runtime"
            << Key << "variants" << Value << BeginMap;
        for(const auto& s: time_i->statistics()) {
          out << Key << s.suffix() << Value << BeginMap
              << Key << "render" << Value
                << Flow << BeginSeq << "number" << "hidden";
          if(s.showPercent()) out << "percent";
          out << EndSeq
              << Key << "formula" << Value << BeginMap;
          rawFormula(out, *time_i, s, MetricScope::execution, "inclusive");
          if(!rawFormula(out, *time_i, s, MetricScope::function, "exclusive"))
            rawFormula(out, *time_i, s, MetricScope::point, "point");
          out << EndMap
              << EndMap;
        }
        out << EndMap
            << Key << "children" << Value << BeginSeq;
        for(const Metric& sm: sub_i) rawLeaf(out, sm);
        out << EndSeq
            << EndMap;
      }
      out << EndSeq
          << EndMap;
    }
  } c_memcpy;

  struct MemSet {
    bool empty = true;
    util::optional_ref<const Metric> time;
    std::vector<std::reference_wrapper<const Metric>> sub;
    MemSet(std::unordered_map<std::string_view, const Metric&>& mets) {
      time = claim(mets.extract("GMSET (sec)"));
      if(!time) return;
      empty = false;
      mets.erase("GMSET:COUNT");  // Unused, but we can take it out anyway
      claim(sub, mets.extract("GMSET:DEV (B)"));
      claim(sub, mets.extract("GMSET:PIN (B)"));
      claim(sub, mets.extract("GMSET:ARY (B)"));
      claim(sub, mets.extract("GMSET:PAG (B)"));
      claim(sub, mets.extract("GMSET:MAN (B)"));
      claim(sub, mets.extract("GMSET:DST (B)"));
      claim(sub, mets.extract("GMSET:MST (B)"));
      claim(sub, mets.extract("GMSET:UNK (B)"));
      for(auto it = mets.begin(); it != mets.end(); ) {
        if(starts_with(it->second.name(), "GMSET:")) {
          sub.push_back(mets.extract(it++).mapped());
          continue;
        }
        ++it;
      }
    }
    void dump(YAML::Emitter& out) {
      using namespace YAML;
      if(!time) return;
      out << BeginMap
          << Key << "name" << Value << "Memory Batch Clear"
          << Key << "description" << Value << "Operations that set large regions of GPU-related memory"
          << Key << "variants" << Value << BeginMap;
      for(const auto& s: time->statistics()) {
        out << Key << s.suffix() << Value << BeginMap
            << Key << "render" << Value
              << Flow << BeginSeq << "number" << "hidden";
        if(s.showPercent()) out << "percent";
        out << EndSeq
            << Key << "formula" << Value << BeginMap;
        rawFormula(out, *time, s, MetricScope::execution, "inclusive");
        if(!rawFormula(out, *time, s, MetricScope::function, "exclusive"))
          rawFormula(out, *time, s, MetricScope::point, "point");
        out << EndMap
            << EndMap;
      }
      out << EndMap;
      if(!sub.empty()) {
        out << Key << "children" << Value << BeginSeq
            << BeginMap
            << Key << "name" << Value << "Bytes Cleared"
            << Key << "description" << Value << "Total number of bytes cleared"
            << Key << "variants" << Value << BeginMap
              << Key << "Sum" << Value << BeginMap
                << Key << "render" << Value
                  << Flow << BeginSeq << "number" << "percent" << "colorbar" << EndSeq
                << Key << "formula" << Value << "sum"
              << EndMap
            << EndMap
            << Key << "children" << Value << BeginSeq;
        for(const Metric& sm: sub) rawLeaf(out, sm);
        out << EndSeq
            << EndMap
            << EndSeq;
      }
      out << EndMap;
    }
  } c_memset;

  struct MemAlloc {
    bool empty = true;
    util::optional_ref<const Metric> time;
    std::vector<std::reference_wrapper<const Metric>> sub;
    MemAlloc(std::unordered_map<std::string_view, const Metric&>& mets) {
      time = claim(mets.extract("GMEM (sec)"));
      if(!time) return;
      empty = false;
      mets.erase("GMEM:COUNT");  // Unused, but we can take it out anyway
      claim(sub, mets.extract("GMEM:DEV (B)"));
      claim(sub, mets.extract("GMEM:PIN (B)"));
      claim(sub, mets.extract("GMEM:ARY (B)"));
      claim(sub, mets.extract("GMEM:PAG (B)"));
      claim(sub, mets.extract("GMEM:MAN (B)"));
      claim(sub, mets.extract("GMEM:DST (B)"));
      claim(sub, mets.extract("GMEM:MST (B)"));
      claim(sub, mets.extract("GMEM:UNK (B)"));
      for(auto it = mets.begin(); it != mets.end(); ) {
        if(starts_with(it->second.name(), "GMEM:")) {
          sub.push_back(mets.extract(it++).mapped());
          continue;
        }
        ++it;
      }
    }
    void dump(YAML::Emitter& out) {
      using namespace YAML;
      if(!time) return;
      out << BeginMap
          << Key << "name" << Value << "Memory De/Allocation"
          << Key << "description" << Value << "Operations that allocate or deallocate GPU-related memory"
          << Key << "variants" << Value << BeginMap;
      for(const auto& s: time->statistics()) {
        out << Key << s.suffix() << Value << BeginMap
            << Key << "render" << Value
              << Flow << BeginSeq << "number" << "hidden";
        if(s.showPercent()) out << "percent";
        out << EndSeq
            << Key << "formula" << Value << BeginMap;
        rawFormula(out, *time, s, MetricScope::execution, "inclusive");
        if(!rawFormula(out, *time, s, MetricScope::function, "exclusive"))
          rawFormula(out, *time, s, MetricScope::point, "point");
        out << EndMap
            << EndMap;
      }
      out << EndMap;
      if(!sub.empty()) {
        out << Key << "children" << Value << BeginSeq
            << BeginMap
            << Key << "name" << Value << "Bytes Allocated/Freed"
            << Key << "description" << Value << "Total number of bytes allocated/freed"
            << Key << "variants" << Value << BeginMap
              << Key << "Sum" << Value << BeginMap
                << Key << "render" << Value
                  << Flow << BeginSeq << "number" << "percent" << "colorbar" << EndSeq
                << Key << "formula" << Value << "sum"
              << EndMap
            << EndMap
            << Key << "children" << Value << BeginSeq;
        for(const Metric& sm: sub) rawLeaf(out, sm);
        out << EndSeq
            << EndMap
            << EndSeq;
      }
      out << EndMap;
    }
  } c_memalloc;

  GPU(std::unordered_map<std::string_view, const Metric&>& mets)
    : c_ker(mets), c_sync(mets), c_memcpy(mets), c_memset(mets),
      c_memalloc(mets) {};
  void dump(YAML::Emitter& out) {
    if(c_ker.empty && c_sync.empty && c_memcpy.empty && c_memset.empty
       && c_memalloc.empty) return;
    using namespace YAML;
    out << BeginMap
        << Key << "name" << Value << "GPU"
        << Key << "description" << Value << "GPU-accelerated performance metrics"
        << Key << "variants" << Value << BeginMap
          << Key << "Sum" << Value << BeginMap
            << Key << "render" << Value
              << Flow << BeginSeq << "number" << "percent" << "colorbar" << EndSeq
            << Key << "formula" << Value << "sum"
          << EndMap
        << EndMap
        << Key << "children" << Value << BeginSeq;
    c_ker.dump(out);
    c_sync.dump(out);
    c_memcpy.dump(out);
    c_memset.dump(out);
    c_memalloc.dump(out);
  }
};

struct CPU {
  bool empty;
  util::optional_ref<const Metric> realtime;
  util::optional_ref<const Metric> cputime;
  CPU(std::unordered_map<std::string_view, const Metric&>& mets) {
    auto oldsz = mets.size();
    // REALTIME and CPUTIME are exceptions we want to promote if possible
    realtime = claim(mets.extract("REALTIME (sec)"));
    cputime = claim(mets.extract("CPUTIME (sec)"));
    empty = mets.size() == oldsz;
  }
  void dump(YAML::Emitter& out) {
    if(empty) return;
    using namespace YAML;
    out << BeginMap
        << Key << "name" << Value << "CPU"
        << Key << "description" << Value << "Time spent within the CPU"
        << Key << "variants" << Value << BeginMap
          << Key << "Sum" << Value << BeginMap;
    if(realtime || cputime) {
      out   << Key << "render" << Value
              << Flow << BeginSeq << "number" << "percent" << EndSeq
            << Key << "formula" << Value << "first";
    } else {
      out   << Key << "render" << Value << "hidden";
    }
    out   << EndMap
        << EndMap
        << Key << "children" << Value << BeginSeq;
    if(realtime) rawLeaf(out, *realtime);
    if(cputime) rawLeaf(out, *cputime);
    out << EndSeq
        << EndMap;
  }
};

}

// Standard version where we try our best to fit every Metric somewhere reasonable
void MetricsYAML::standard(std::ostream& os) {
  using namespace YAML;
  Emitter out(os);
  out << BeginMap << Key << "version" << Value << 0;

  std::unordered_map<std::string_view, const Metric&> mets;
  mets.reserve(src.metrics().size());

  // Before doing anything else, make sure we have access to all the inputs
  out << Key << "inputs" << BeginSeq;
  for(const Metric& m: src.metrics().citerate()) {
    mets.insert({m.name(), m});
    const auto f = [&](MetricScope s) {
      if(!m.scopes().has(s)) return false;
      for(const auto& p: m.partials()) {
        out << Anchor(anchorName(m, p, s)) << BeginMap
            << Key << "metric" << Value << m.name()
            << Key << "scope" << Value << s
            << Key << "formula" << Value << ExperimentXML4::accumulateFormulaString(p.accumulate())
            << Key << "combine" << Value << p.combinator()
            << EndMap;
      }
      return true;
    };
    f(MetricScope::execution);
    if(!f(MetricScope::function))
      f(MetricScope::point);
  }
  out << EndSeq;

  // Filter the metrics into top-level roots early, so we can control the order
  // Internally each constructor only takes the Metrics it can handle
  GPU gpu(mets);
  CPU cpu(mets);

  // Emit all the roots, in the order we want in general
  out << Key << "roots" << Value << BeginSeq;
  cpu.dump(out);
  gpu.dump(out);
  if(!mets.empty()) {
    // Whatever's left gets emitted "raw".
    out << BeginMap
        << Key << "name" << Value << "Uncatagorized"
        << Key << "description" << Value << "Metrics that are unrecognized or don't belong well anywhere else."
        << Key << "variants" << Value << BeginMap;
    // Figure out the Statistics that the children have
    std::vector<std::string_view> variants;
    for(const auto& [n,m]: mets) {
      for(const auto& s: m.statistics()) {
        if(std::find(variants.begin(), variants.end(), s.suffix()) == variants.end())
          variants.push_back(s.suffix());
      }
    }
    for(const auto& v: variants) {
      out << Key << std::string(v) << Value << BeginMap
            << Key << "render" << Value << "hidden"
          << EndMap;
    }
    out << EndMap
        << Key << "children" << Value << BeginSeq;
    for(const auto& [n,m]: mets) rawLeaf(out, m);
    out << EndSeq
        << EndMap;
  }
  out << EndSeq;

  // Close it off
  out << EndMap;
  if(!out.good())
    util::log::fatal{} << "YAML::Emitter error: " << out.GetLastError();
  assert(out.good());

}
