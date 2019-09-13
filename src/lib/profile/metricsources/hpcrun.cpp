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

#include "hpcrun.hpp"

#include "lib/prof-lean/hpcrun-fmt.h"

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <stack>

using namespace hpctoolkit;
using namespace metricsources;
using namespace literals;

HpcrunFSv2::HpcrunFSv2(const stdshim::filesystem::path& fn)
  : MetricSource(), can_provide("armc"_dat), path(fn),
    read_headers(false), read_cct(false), warned(false),
    tracepath(fn.parent_path() / fn.stem().concat(".hpctrace")),
    read_trace(false) {
  // Make sure we can pop the file open.
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if(file == nullptr) throw std::logic_error("Unable to open file!");
  // Read in the file header. Reads more than I would prefer, but since we're
  // falling back on the C-like implementation...
  hpcrun_fmt_hdr_t hdr;
  if(hpcrun_fmt_hdr_fread(&hdr, file, std::malloc) != HPCFMT_OK) {
    std::fclose(file);
    throw std::logic_error("Invalid header!");
  }
  // The file is now placed right at the first epoch header.
  epoch_offsets.emplace_back(std::ftell(file));
  // Copy over the file attributes
  if(hdr.version != 2.0 && hdr.version != 3.0) {
    std::fclose(file);
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
      std::cerr << "WARNING: Unknown file attribute in "
                << path.string() << ", key `" << k << "', value `"
                << std::string(v) << "'!\n";
    }
  }
  // Clean up the mess
  hpcrun_fmt_hdr_free(&hdr, std::free);
  std::fclose(file);

  // We also support a corrosponding hpctrace file if it exists. Try it.
  // If anything goes wrong, we just won't actually use it.
  if(!setupTrace()) tracepath.clear();
  else can_provide += DataClass::trace;
}

bool HpcrunFSv2::setupTrace() noexcept {
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

void HpcrunFSv2::bindPipeline(ProfilePipeline::SourceEnd&& se) noexcept {
  MetricSource::bindPipeline(std::move(se));
  sink.attributes(std::move(attrs));
  thread = &sink.thread(std::move(tattrs));
}

// Read an 8-byte uint value from a file, big endian.
static uint64_t read_uint8(std::FILE* f) {
  unsigned char buf[8];
  if(std::fread(&buf[0], sizeof buf[0], 8, f) != 8)
    throw std::logic_error("EOF while reading uint8!");
  return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48)
         | ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32)
         | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16)
         | ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
}

// Bits for the helper data structure.
HpcrunFSv2::seekfile::seekfile(const stdshim::filesystem::path& p, long at)
  : file(std::fopen(p.c_str(), "rb")), curpos(at), atpos(true) {
  if(file == nullptr)
    throw std::logic_error("Error opening file!");
  if(at != 0) std::fseek(file, at, SEEK_SET);
}

HpcrunFSv2::seekfile::~seekfile() {
  std::fclose(file);
}

long HpcrunFSv2::seekfile::mark() {
  return (curpos = std::ftell(file));
}

void HpcrunFSv2::seekfile::seek(long to) {
  if(curpos == to) return;
  curpos = to;
  atpos = false;
}

void HpcrunFSv2::seekfile::advance(long delta) {
  curpos += delta;
  atpos = false;
}

void HpcrunFSv2::seekfile::prep() {
  if(atpos) return;
  std::fseek(file, curpos, SEEK_SET);
  atpos = true;
}

bool HpcrunFSv2::read(const DataClass& req, const DataClass&, ProfilePipeline::timeout_t) {
  DataClass needed = req;
  needed -= DataClass::attributes;  // Handled on bind.
  // First add in dependencies between the stages
  if(req.has_trace()) needed += DataClass::contexts;
  // Then remove anything we've already done.
  if(read_headers) needed -= DataClass::references + DataClass::metrics;
  if(read_cct) needed -= DataClass::contexts;
  if(read_trace) needed -= DataClass::trace;
  // Check early whether we actually *need* to do anything.
  if(!needed.has_any()) return true;

  if(needed.has_references() || needed.has_metrics() || needed.has_contexts())
  {
  seekfile f(path);

  // We want to get all the data out in a single pass if possible, so we keep
  // track of where we would be if we were reading the entire thing and
  // seek up to that point as needed.
  f.seek(epoch_offsets.at(0).header);  // There should always be 1
  std::size_t idx = 0;
  bool skipepoch = false;

  // For each epoch, but we may not know how many there are.
  while(read_headers ? idx < epoch_offsets.size() : true) {
    if(idx >= epoch_offsets.size()) epoch_offsets.emplace_back(f.curpos);
    auto& epoch = epoch_offsets[idx];
    if(skipepoch) f.seek(epoch.header);  // Special skip value
    else epoch.header = f.curpos;  // We should be currently sitting at the header.

    // We won't know where anything is until we've read the header
    if(epoch.cct == 0) {
      f.prep();
      // Epoch header
      hpcrun_fmt_epochHdr_t hdr;
      auto x = hpcrun_fmt_epochHdr_fread(&hdr, f.file, std::malloc);
      if(x == HPCFMT_EOF) {
        epoch_offsets.pop_back();
        break;
      } else if(x != HPCFMT_OK)
        throw std::logic_error("Error reading epoch header!");
      // Metric table
      metric_tbl_t mets;
      metric_aux_info_t *aux;
      if(hpcrun_fmt_metricTbl_fread(&mets, &aux, f.file, 2.0, std::malloc)
          != HPCFMT_OK)
        throw std::logic_error("Error reading metric table!");
      // Loadmap
      loadmap_t mods;
      if(hpcrun_fmt_loadmap_fread(&mods, f.file, std::malloc) != HPCFMT_OK)
        throw std::logic_error("Error reading loadmap!");
      // Copy the data over. Not a lot for now
      if(hdr.flags.fields.isLogicalUnwind)
        throw std::logic_error("Lush is not currently supported!");
      for(uint32_t i = 0; i < mets.len; i++) {
        const auto& m = mets.lst[i];
        if(m.flags.fields.valFmt == MetricFlags_ValFmt_Real)
          metric_int.emplace_back(false);
        else if(m.flags.fields.valFmt == MetricFlags_ValFmt_Int)
          metric_int.emplace_back(true);
        else
          throw std::logic_error("Invalid metric value format!");
        metric_order.emplace_back(&sink.metric(std::string(m.name),
                                               std::string(m.description)));
      }
      for(uint32_t i = 0; i < mods.len; i++) {
        const auto& m = mods.lst[i];
        epoch.module_ids.emplace(m.id, sink.module(std::string(m.name)));
      }
      // Mark our position for posterity
      epoch.cct = f.mark();
      // Clean up after ourselves
      hpcrun_fmt_epochHdr_free(&hdr, std::free);
      hpcrun_fmt_metricTbl_free(&mets, std::free);
      std::free(aux);
      hpcrun_fmt_loadmap_free(&mods, std::free);
    } else {  // Skip over, we know it already
      f.seek(epoch.cct);
    }

    // CCT.
    if(!epoch.passed || (needed.has_contexts() && !epoch.read_cct)) {
      f.prep();
      auto cnt = read_uint8(f.file);
      if(needed.has_contexts()) {  // Read it all in
        for(uint64_t i = 0; i < cnt; i++) {
          std::vector<hpcrun_metricVal_t> ms(metric_order.size());
          hpcrun_fmt_cct_node_t n;
          n.num_metrics = ms.size();
          n.metrics = ms.data();
          if(hpcrun_fmt_cct_node_fread(&n, {0}, f.file) != HPCFMT_OK)
            throw std::logic_error("Error reading CCT node!");

          // Figure out the parent of this node, if it has one.
          Context* par;
          if(n.id_parent == 0) {  // Root of some kind
            if(n.lm_id == 0) {  // Synthetic root, remap to something useful.
              if(n.lm_ip == HPCRUN_FMT_LMIp_NULL) {
                // Global Scope, for full or "normal" unwinds. No actual node.
                epoch.node_ids.emplace(n.id, nullptr);
                continue;
              } else if(n.lm_ip == HPCRUN_FMT_LMIp_Flag1) {
                // Global unknown Scope, for "partial" unwinds.
                // aka. /lost+found but for CCT trees.
                epoch.partial_node_id = n.id;
                continue;
              } else {
                // This really shouldn't happen. For now throw, decide how to
                // handle gracefully later.
                std::cerr << "Encountered a wild unknown syth root!\n";
                throw std::logic_error("Unknown synthetic root!");
              }
            } else {
              // If it looks like a sample but doesn't have a parent,
              // stitch it to the global unknown (a la /lost+found).
              par = &sink.context({});
            }
          } else if(n.id_parent == epoch.partial_node_id ||
                    n.id_parent == epoch.unknown_node_id) {
            // Global unknown Scope, emitted lazily.
            par = &sink.context({});
          } else {  // Just nab its parent.
            auto ppar = epoch.node_ids.find(n.id_parent);
            if(ppar == epoch.node_ids.end())
              throw std::logic_error("CCT nodes not in a preorder!");
            par = ppar->second;
          }

          // Figure out the Scope for this node, if it has one.
          Scope scope;  // Default to the unknown scope
          if(n.lm_id != 0) {
            auto mod = epoch.module_ids.find(n.lm_id);
            if(mod == epoch.module_ids.end())
              throw std::logic_error("CCT node references unknown module!");
            scope = {mod->second, n.lm_ip};
          } else if(par == nullptr) {
            // Special case: if its written in the .hpcrun file as
            // global -> unknown, merge with the global unknown.
            epoch.unknown_node_id = n.id;
            continue;
          }

          // Emit, record and attribute.
          Context& next = sink.context(par, scope);
          epoch.node_ids.emplace(n.id, &next);
          for(std::size_t i = 0; i < ms.size(); i++)
            sink.add(next, *thread, *metric_order[i],
              metric_int[i] ? (double)ms[i].i : ms[i].r);
        }
        f.mark();
        epoch.read_cct = true;
      } else {  // Make a guess at the length. Its a simple format.
        f.advance(8 + cnt*(4+4+2+8+8*metric_order.size()));  // 8 for cnt
      }
      epoch.passed = true;
    } else {  // Skip over, we handled this once before
      skipepoch = true;  // Use the given header value at the next epoch.
    }

    idx++;
  }

  read_headers = true;
  if(needed.has_contexts()) read_cct = true;
  }  // needed.has_references() || needed.has_metrics() || needed.has_contexts()

  // We can also do the trace file, if requested.
  if(needed.has_trace() && !tracepath.empty()) {
    seekfile f(tracepath, trace_off);

    hpctrace_fmt_datum_t tpoint;
    ProfilePipeline::SourceEnd::Trace trace;
    while(1) {
      int err = hpctrace_fmt_datum_fread(&tpoint, {0}, f.file);
      if(err == HPCFMT_EOF) break;
      else if(err != HPCFMT_OK)
        throw std::logic_error("Error reading trace datum!");
      auto it = epoch_offsets[0].node_ids.find(-tpoint.cpId);
      if(it != epoch_offsets[0].node_ids.end()) {
        if(!trace)
          trace = sink.traceStart(*thread, std::chrono::nanoseconds(0));
        trace.trace(std::chrono::nanoseconds(HPCTRACE_FMT_GET_TIME(tpoint.comp)), *it->second);
      }
    }
    read_trace = true;
  }

  return true;
}
