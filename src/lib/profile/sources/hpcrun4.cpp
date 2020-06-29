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

using namespace hpctoolkit;
using namespace sources;
using namespace literals::data;

Hpcrun4::Hpcrun4(const stdshim::filesystem::path& fn)
  : ProfileSource(), attrsValid(true), tattrsValid(true), thread(nullptr),
    path(fn), partial_node_id(0), unknown_node_id(0),
    tracepath(fn.parent_path() / fn.stem().concat(".hpctrace")) {
  // Try to open up the file. Errors handled inside somewhere.
  file = hpcrun_sparse_open(path.c_str());
  if(file == nullptr) throw std::logic_error("Unable to open file!");
  // We still need to check the header before we know for certain whether
  // this is the right version. A hassle, I know.
  hpcrun_fmt_hdr_t hdr;
  if(hpcrun_sparse_read_hdr(file, &hdr) != 0) {
    hpcrun_sparse_close(file);
    throw std::logic_error("Invalid header!");
  }
  if(hdr.version != 4.0) {
    hpcrun_fmt_hdr_free(&hdr, std::free);
    hpcrun_sparse_close(file);
    throw std::logic_error("Invalid version of .hpcrun file!");
  }
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
      tattrs.mpirank(std::strtol(v, nullptr, 10));
    else if(k == HPCRUN_FMT_NV_tid)
      tattrs.threadid(std::strtol(v, nullptr, 10));
    else if(k == HPCRUN_FMT_NV_hostid)
      tattrs.hostid(std::strtol(v, nullptr, 16));
    else if(k == HPCRUN_FMT_NV_pid)
      tattrs.procid(std::strtol(v, nullptr, 10));
    else if(k != HPCRUN_FMT_NV_traceMinTime
            && k != HPCRUN_FMT_NV_traceMaxTime) {
      util::log::warning()
      << "Unknown file attribute in " << path.string() << ":\n"
          "  '" << k << "'='" << std::string(v) << "'!";
    }
  }
  hpcrun_fmt_hdr_free(&hdr, std::free);
  // If all went well, we can pause the file here.
  hpcrun_sparse_pause(file);

  // Also check for a corrosponding tracefile. If anything goes wrong, we'll
  // just skip it.
  if(!setupTrace()) tracepath.clear();
}

Hpcrun4::~Hpcrun4() {
  hpcrun_sparse_close(file);
}

DataClass Hpcrun4::provides() const noexcept {
  Class ret = attributes + references + contexts + DataClass::metrics + threads;
  if(!tracepath.empty()) ret += timepoints;
  return ret;
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
  std::fclose(file);
  return true;
}

void Hpcrun4::read(const DataClass& needed) {
  // If attributes or threads are requested, we emit 'em.
  if(needed.hasAttributes() && attrsValid) {
    sink.attributes(std::move(attrs));
    attrsValid = false;
  }
  if(needed.anyOf(threads|DataClass::metrics) && sink.limit().hasThreads() && tattrsValid) {
    thread = &sink.thread(std::move(tattrs));
    tattrsValid = false;
  }

  // Most likely we need something. So just resume the file.
  hpcrun_sparse_resume(file, path.c_str());

  if(needed.anyOf(attributes|DataClass::metrics) && sink.limit().hasAttributes()) {
    int id;
    metric_desc_t m;
    metric_aux_info_t maux;
    while((id = hpcrun_sparse_next_metric(file, &m, &maux, 4.0)) > 0) {
      bool isInt = false;
      if(m.flags.fields.valFmt == MetricFlags_ValFmt_Real) isInt = false;
      else if(m.flags.fields.valFmt == MetricFlags_ValFmt_Int) isInt = true;
      else util::log::fatal() << "Invalid metric value format!";
      metrics.emplace(id, sink.metric(m.name, m.description, Metric::Type::linear));
      metricInt.emplace(id, isInt);
      hpcrun_fmt_metricDesc_free(&m, std::free);
    }
    if(id < 0) util::log::fatal() << "Hpcrun4: Error reading metric entry!";
  }
  if(needed.anyOf(references|contexts) && sink.limit().hasReferences()) {
    int id;
    loadmap_entry_t lm;
    while((id = hpcrun_sparse_next_lm(file, &lm)) > 0) {
      modules.emplace(id, sink.module(lm.name));
      hpcrun_fmt_loadmapEntry_free(&lm, std::free);
    }
    if(id < 0) util::log::fatal() << "Hpcrun4: Error reading load module entry!";
  }
  if(needed.anyOf(contexts)) {
    int id;
    hpcrun_fmt_cct_node_t n;
    while((id = hpcrun_sparse_next_context(file, &n)) > 0) {
      // Figure out the parent of this node, if it has one.
      Context* par;
      if(n.id_parent == 0) {  // Root of some kind
        if(n.lm_id == 0) {  // Synthetic root, remap to something useful.
          if(n.lm_ip == HPCRUN_FMT_LMIp_NULL) {
            // Global Scope, for full or "normal" unwinds. No actual node.
            nodes.emplace(id, nullptr);
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
          par = &sink.context({});
        }
      } else if(n.id_parent == partial_node_id || n.id_parent == unknown_node_id) {
        // Global unknown Scope, emitted lazily.
        par = &sink.context({});
      } else {  // Just nab its parent
        auto ppar = nodes.find(n.id_parent);
        if(ppar == nodes.end())
          util::log::fatal() << "CCT nodes not in a preorder!";
        par = ppar->second;
      }

      // Figure out the Scope for this node, if it has one.
      Scope scope;  // Default to the unknown Scope.
      if(n.lm_id != 0) {
        auto it = modules.find(n.lm_id);
        if(it == modules.end())
          util::log::fatal() << "Erroneous module id " << n.lm_id << " in " << path.string() << "!";
        scope = {it->second, n.lm_ip};
      } else if(par == nullptr) {
        // Special case: merge global -> unknown to the global unknown.
        unknown_node_id = id;
        continue;
      }

      // Emit the Context and record for later.
      nodes.emplace(id, &(par ? sink.context(*par, scope) : sink.context(scope)));
    }
    if(id < 0) util::log::fatal() << "Hpcrun4: Error reading context entry!";
  }
  if(needed.hasMetrics()) {
    int cid;
    while((cid = hpcrun_sparse_next_block(file)) > 0) {
      int mid;
      hpcrun_metricVal_t val;
      if(sink.limit().hasContexts()) {
        int mid = hpcrun_sparse_next_entry(file, &val);
        if(mid == 0) continue;
        auto it = nodes.find(cid);
        if(it == nodes.end())
          util::log::fatal() << "Erroneous CCT id " << cid << " in " << path.string() << "!";
        auto& here = *it->second;
        while(mid > 0) {
          auto v = metricInt.at(mid) ? (double)val.i : val.r;
          sink.add(here, *thread, metrics.at(mid), v);
          mid = hpcrun_sparse_next_entry(file, &val);
        }
      } else {
        while((mid = hpcrun_sparse_next_entry(file, &val)) > 0) {
          auto v = metricInt.at(mid) ? (double)val.i : val.r;
          sink.add(*thread, metrics.at(mid), v);
        }
      }
    }
  }

  // Pause the file, we're at a good point here
  hpcrun_sparse_pause(file);

  if(needed.hasTimepoints() && !tracepath.empty()) {
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
        if(thread)
          sink.timepoint(*thread, *it->second, tp);
        else
          sink.timepoint(*it->second, tp);
      }
    }
    std::fclose(f);
  }
}
