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
// Copyright ((c)) 2020, Rice University
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

#include "hpcrun4.hpp"

#include "../util/log.hpp"
#include "lib/prof-lean/hpcrun-fmt.h"

// TODO: Remove this after catching the develop commits
#define HPCRUN_FMT_NV_traceOrdered "trace-time-ordered"

// TODO: Remove and change this once new-cupti is finalized
#define HPCRUN_GPU_RANGE_NODE 65533

using namespace hpctoolkit;
using namespace sources;
using namespace literals::data;

Hpcrun4::Hpcrun4(const stdshim::filesystem::path& fn)
  : ProfileSource(), fileValid(true), attrsValid(true), tattrsValid(true),
    thread(nullptr), path(fn), partial_node_id(0), unknown_node_id(0),
    tracepath(fn.parent_path() / fn.stem().concat(".hpctrace")),
    trace_sort(false) {
  // Try to open up the file. Errors handled inside somewhere.
  file = hpcrun_sparse_open(path.c_str(), 0, 0);
  if(file == nullptr) {
    fileValid = false;
    return;
  }
  // We still need to check the header before we know for certain whether
  // this is the right version. A hassle, I know.
  hpcrun_fmt_hdr_t hdr;
  if(hpcrun_sparse_read_hdr(file, &hdr) != 0) {
    hpcrun_sparse_close(file);
    fileValid = false;
    return;
  }
  if(hdr.version != 4.0) {
    hpcrun_fmt_hdr_free(&hdr, std::free);
    hpcrun_sparse_close(file);
    fileValid = false;
    return;
  }
  stdshim::optional<unsigned long> mpirank;
  stdshim::optional<unsigned long> threadid;
  stdshim::optional<unsigned long> hostid;
  for(uint32_t i = 0; i < hdr.nvps.len; i++) {
    const std::string k(hdr.nvps.lst[i].name);
    const auto v = hdr.nvps.lst[i].val;
    if(k == HPCRUN_FMT_NV_prog) attrs.name(std::string(v));
    else if(k == HPCRUN_FMT_NV_progPath)
      attrs.path(stdshim::filesystem::path(v));
    else if(k == HPCRUN_FMT_NV_envPath)
      attrs.environment("PATH", std::string(v));
    else if(k == HPCRUN_FMT_NV_jobId)
      attrs.job(std::strtol(v, nullptr, 10));
    else if(k == HPCRUN_FMT_NV_mpiRank)
      mpirank = std::strtol(v, nullptr, 10);
    else if(k == HPCRUN_FMT_NV_tid)
      threadid = std::strtol(v, nullptr, 10);
    else if(k == HPCRUN_FMT_NV_hostid)
      hostid = std::strtol(v, nullptr, 16);
    else if(k == HPCRUN_FMT_NV_pid)
      tattrs.procid(std::strtol(v, nullptr, 10));
    else if(k == HPCRUN_FMT_NV_traceOrdered) {
      if(std::string(v) == "1") trace_sort = true;
    } else if(k != HPCRUN_FMT_NV_traceMinTime
            && k != HPCRUN_FMT_NV_traceMaxTime) {
      util::log::warning()
      << "Unknown file attribute in " << path.string() << ":\n"
          "  '" << k << "'='" << std::string(v) << "'!";
    }
  }
  hpcrun_fmt_hdr_free(&hdr, std::free);
  // Try to read the hierarchical tuple, if we fail synth from the header data
  id_tuple_t sfTuple;
  if(hpcrun_sparse_read_id_tuple(file, &sfTuple) == SF_SUCCEED) {
    std::vector<pms_id_t> tuple;
    tuple.reserve(sfTuple.length);
    for(size_t i = 0; i < sfTuple.length; i++)
      tuple.push_back(sfTuple.ids[i]);
    id_tuple_free(&sfTuple);
    tattrs.idTuple(std::move(tuple));
  } else {
    util::log::warning{} << "Synthesizing hierarchical tuple for: "
                         << path.string();
    std::vector<pms_id_t> tuple;
    if(hostid) tuple.push_back({.kind = IDTUPLE_NODE, .index=*hostid});
    if(threadid && *threadid >= 500)  // GPUDEVICE goes before RANK
      tuple.push_back({.kind = IDTUPLE_GPUDEVICE, .index=0});
    if(mpirank) tuple.push_back({.kind = IDTUPLE_RANK, .index=*mpirank});
    if(threadid) {
      if(*threadid >= 500) {  // GPU stream
        tuple.push_back({.kind = IDTUPLE_GPUCONTEXT, .index=0});
        tuple.push_back({.kind = IDTUPLE_GPUSTREAM, .index=*threadid - 500});
      } else
        tuple.push_back({.kind = IDTUPLE_THREAD, .index=*threadid});
    }
    tattrs.idTuple(std::move(tuple));
  }
  // If all went well, we can pause the file here.
  hpcrun_sparse_pause(file);

  // Also check for a corrosponding tracefile. If anything goes wrong, we'll
  // just skip it.
  if(!setupTrace()) tracepath.clear();
}

bool Hpcrun4::valid() const noexcept { return fileValid; }

Hpcrun4::~Hpcrun4() {
  if(fileValid) hpcrun_sparse_close(file);
}

DataClass Hpcrun4::provides() const noexcept {
  Class ret = attributes + references + contexts + DataClass::metrics + threads;
  if(!tracepath.empty()) ret += timepoints;
  return ret;
}

DataClass Hpcrun4::finalizeRequest(const DataClass& d) const noexcept {
  DataClass o = d;
  if(o.hasMetrics()) o += attributes + contexts + threads;
  if(o.hasTimepoints()) o += contexts;
  if(o.hasContexts()) o += references;
  return o;
}

bool Hpcrun4::setupTrace() noexcept {
  std::FILE* file = std::fopen(tracepath.c_str(), "rb");
  if(!file) return false;
  // Read in the file header.
  hpctrace_fmt_hdr_t thdr;
  if(hpctrace_fmt_hdr_fread(&thdr, file) != HPCFMT_OK) {
    std::fclose(file);
    return false;
  }
  if(thdr.version != 1.01) { std::fclose(file); return false; }
  if(HPCTRACE_HDR_FLAGS_GET_BIT(thdr.flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS)) {
    std::fclose(file);
    return false;
  }
  // The file is now placed right at the start of the data.
  trace_off = std::ftell(file);

  // Count the number of timepoints in the file, and save it for later.
  std::fseek(file, 0, SEEK_END);
  auto trace_end = std::ftell(file);
  if((trace_end - trace_off) % (8+4) != 0) {
    std::fclose(file);
    return false;
  }
  tattrs.timepointCnt((trace_end - trace_off) / (8+4));

  std::fclose(file);
  return true;
}

void Hpcrun4::read(const DataClass& needed) {
  // If attributes or threads are requested, we emit 'em.
  if(needed.hasAttributes() && attrsValid) {
    sink.attributes(std::move(attrs));
    attrsValid = false;
  }
  if(needed.hasThreads() && tattrsValid) {
    thread = &sink.thread(std::move(tattrs));
    tattrsValid = false;
  }

  // Most likely we need something. So just resume the file.
  hpcrun_sparse_resume(file, path.c_str());

  if(needed.hasAttributes()) {
    int id;
    metric_desc_t m;
    metric_aux_info_t maux;
    std::vector<std::pair<std::string, ExtraStatistic::Settings>> estats;
    while((id = hpcrun_sparse_next_metric(file, &m, &maux, 4.0)) > 0) {
      Metric::Settings settings{m.name, m.description};
      switch(m.flags.fields.show) {
      case HPCRUN_FMT_METRIC_SHOW: break;  // Default
      case HPCRUN_FMT_METRIC_HIDE:
        settings.visibility = Metric::Settings::visibility_t::hiddenByDefault;
        break;
      case HPCRUN_FMT_METRIC_SHOW_INCLUSIVE:
        util::log::error{} << "Show parameter SHOW_INCLUSIVE does not have a clear definition,"
                              " " << m.name << " may be presented incorrectly.";
        settings.scopes &= {MetricScope::execution, MetricScope::point};
        break;
      case HPCRUN_FMT_METRIC_SHOW_EXCLUSIVE:
        settings.scopes &= {MetricScope::function, MetricScope::point};
        break;
      case HPCRUN_FMT_METRIC_INVISIBLE:
        settings.visibility = Metric::Settings::visibility_t::invisible;
        break;
      default:
        util::log::error{} << "Unknown show parameter " << (unsigned int)m.flags.fields.show
                           << ", " << m.name << " may be presented incorrectly.";
        break;
      }

      if(m.formula != nullptr && m.formula[0] != '\0') {
        ExtraStatistic::Settings es_settings{std::move(settings)};
        es_settings.showPercent = m.flags.fields.showPercent;
        es_settings.format = m.format;
        estats.emplace_back(m.formula, std::move(es_settings));
      } else {
        bool isInt = false;
        if(m.flags.fields.valFmt == MetricFlags_ValFmt_Real) isInt = false;
        else if(m.flags.fields.valFmt == MetricFlags_ValFmt_Int) isInt = true;
        else util::log::fatal() << "Invalid metric value format!";
        metricInt.emplace(id, isInt);
        metrics.emplace(id, sink.metric(std::move(settings)));
      }
      hpcrun_fmt_metricDesc_free(&m, std::free);
    }
    if(id < 0) util::log::fatal() << "Hpcrun4: Error reading metric entry!";

    for(auto&& [rawFormula, es_settings]: std::move(estats)) {
      const auto& ct = std::use_facet<std::ctype<char>>(std::locale::classic());
      std::size_t idx = 0;
      while(idx != std::string::npos && idx < rawFormula.size()) {
        if(rawFormula[idx] == '#' || rawFormula[idx] == '$'
           || rawFormula[idx] == '@') {
          // Variable specification, collect the digits and decode
          auto id = ({
            idx++;
            std::string id;
            while(idx < rawFormula.size()
                  && ct.is(std::ctype<char>::digit, rawFormula[idx])) {
              id += rawFormula[idx];
              idx++;
            }
            if(id.size() == 0) util::log::fatal{} << "Invalid formula string!";
            std::stoll(id);
          });
          id += 1;  // Adjust to sparse metric ids
          const auto it = metrics.find(id);
          if(it == metrics.end()) util::log::fatal{} << "Unknown metric id " << id;
          es_settings.formula.emplace_back(ExtraStatistic::MetricPartialRef{
            it->second, it->second.statsAccess().requestSumPartial()});
        } else {
          // C-like formula components
          std::size_t next = rawFormula.find_first_of("#$@", idx);
          if(next == std::string::npos)
            es_settings.formula.emplace_back(rawFormula.substr(idx));
          else
            es_settings.formula.emplace_back(rawFormula.substr(idx, next-idx));
          idx = next;
        }
      }
      sink.extraStatistic(std::move(es_settings));
    }
    for(const auto& im: metrics) sink.metricFreeze(im.second);
  }
  if(needed.hasReferences()) {
    int id;
    loadmap_entry_t lm;
    while((id = hpcrun_sparse_next_lm(file, &lm)) > 0) {
      modules.emplace(id, sink.module(lm.name));
      hpcrun_fmt_loadmapEntry_free(&lm, std::free);
    }
    if(id < 0) util::log::fatal() << "Hpcrun4: Error reading load module entry!";
  }
  if(needed.hasContexts()) {
    int id;
    hpcrun_fmt_cct_node_t n;
    while((id = hpcrun_sparse_next_context(file, &n)) > 0) {
      // Figure out the parent of this node, if it has one.
      std::optional<ContextRef> par;
      if(n.id_parent == 0) {  // Root of some kind
        if(n.lm_id == 0) {  // Synthetic root, remap to something useful.
          if(n.lm_ip == HPCRUN_FMT_LMIp_NULL) {
            // Global Scope, for full or "normal" unwinds. No actual node.
            nodes.emplace(id, sink.global());
            continue;
          } else if(n.lm_ip == HPCRUN_FMT_LMIp_Flag1) {
            // Global unknown Scope, for "partial" unwinds.
            partial_node_id = id;
            continue;
          } else {
            // This really shouldn't happen.
            util::log::fatal() << "Unknown synthetic root!";
          }
        } else {
          // If it looks like a sample but doesn't have a parent,
          // stitch it to the global unknown.
          par = sink.context(sink.global(), {});
        }
      } else if(n.id_parent == partial_node_id || n.id_parent == unknown_node_id) {
        // Global unknown Scope, emitted lazily.
        par = sink.context(sink.global(), {});
      } else {  // Just nab its parent
        auto ppar = nodes.find(n.id_parent);
        if(ppar == nodes.end()) {
          // It may be in template form, in which case promote it to a call
          auto tmp = templates.find(n.id_parent);
          if(tmp == templates.end())
              util::log::fatal() << "CCT nodes not in a preorder!";
          auto mo = tmp->second.second.point_data();
          ppar = nodes.emplace(n.id_parent,
            sink.context(tmp->second.first, {Scope::call, mo.first, mo.second})).first;
          templates.erase(tmp);
        }
        par = ppar->second;
      }

      // Figure out the Scope for this node, if it has one.
      Scope scope;  // Default to the unknown Scope.
      if(n.lm_id == HPCRUN_GPU_RANGE_NODE) {
        // Special case, this is a collaborative marker node
        nodes.emplace(id, sink.collaborate(par.value(), sink.collabContext(n.lm_ip)));
        continue;
      } else if(n.lm_id != 0) {
        auto it = modules.find(n.lm_id);
        if(it == modules.end())
          util::log::fatal() << "Erroneous module id " << n.lm_id << " in " << path.string() << "!";
        scope = {it->second, n.lm_ip};
      } else if(!par) {
        // Special case: merge global -> unknown to the global unknown.
        unknown_node_id = id;
        continue;
      }

      if(scope.type() == Scope::Type::point) {
        // It might be a call node, it might not. Delay it until we know whether it has children.
        templates.insert({id, {*par, scope}});
      } else {  // Just emit it, it doesn't need much thought
        nodes.emplace(id, sink.context(*par, scope));
      }
    }
    if(id < 0) util::log::fatal() << "Hpcrun4: Error reading context entry!";

    // If there are remaining unpromoted templates, emit them as normal points.
    for(const auto& tmp: templates) {
      nodes.emplace(tmp.first, sink.context(tmp.second.first, tmp.second.second));
    }
    templates.clear();
  }
  if(needed.hasMetrics()) {
    int cid;
    while((cid = hpcrun_sparse_next_block(file)) > 0) {
      hpcrun_metricVal_t val;
      int mid = hpcrun_sparse_next_entry(file, &val);
      if(mid == 0) continue;
      ContextRef here = !sink.limit().hasContexts() ? sink.global() : ({
        auto it = nodes.find(cid);
        if(it == nodes.end())
          util::log::fatal() << "Erroneous CCT id " << cid << " in " << path.string() << "!";
        it->second;
      });
      auto accum = sink.accumulateTo(here, *thread);
      while(mid > 0) {
        accum.add(metrics.at(mid), metricInt.at(mid) ? (double)val.i : val.r);
        mid = hpcrun_sparse_next_entry(file, &val);
      }
    }
  }

  // Pause the file, we're at a good point here
  hpcrun_sparse_pause(file);

  if(needed.hasTimepoints() && !tracepath.empty()) {
    std::vector<std::pair<std::chrono::nanoseconds, ContextRef>> tps;

    std::FILE* f = std::fopen(tracepath.c_str(), "rb");
    std::fseek(f, trace_off, SEEK_SET);
    hpctrace_fmt_datum_t tpoint;
    while(1) {
      int err = hpctrace_fmt_datum_fread(&tpoint, {0}, f);
      if(err == HPCFMT_EOF) break;
      else if(err != HPCFMT_OK)
        util::log::fatal() << "Hpcrun4: Error reading trace datum in " << tracepath.string() << "!";
      auto it = nodes.find(tpoint.cpId);
      if(it != nodes.end()) {
        std::chrono::nanoseconds tp(HPCTRACE_FMT_GET_TIME(tpoint.comp));
        if(trace_sort)
          tps.emplace_back(tp, it->second);
        else {
          if(thread)
            sink.timepoint(*thread, it->second, tp);
          else
            sink.timepoint(it->second, tp);
        }
      }
    }
    std::fclose(f);

    if(trace_sort) {
      std::sort(tps.begin(), tps.end(),
        [](const decltype(tps)::value_type& a, const decltype(tps)::value_type& b) -> bool {
          return a.first < b.first;
        });
      for(const auto& tp: tps) {
        if(thread)
          sink.timepoint(*thread, tp.second, tp.first);
        else
          sink.timepoint(tp.second, tp.first);
      }
    }
  }
}
