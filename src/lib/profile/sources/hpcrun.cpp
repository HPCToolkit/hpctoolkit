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

#include "../util/log.hpp"
#include "lib/prof-lean/hpcrun-fmt.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <stack>

using namespace hpctoolkit;
using namespace sources;
using namespace literals::data;

HpcrunFSv2::HpcrunFSv2(const stdshim::filesystem::path& fn)
  : ProfileSource(), fileValid(true), attrsValid(true), tattrsValid(true),
    thread(nullptr), path(fn), epochs_final(false),
    tracepath(fn.parent_path() / fn.stem().concat(".hpctrace")) {
  // Make sure we can pop the file open.
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if(file == nullptr) {
    fileValid = false;
    return;
  }
  // Read in the file header. Reads more than I would prefer, but since we're
  // falling back on the C-like implementation...
  hpcrun_fmt_hdr_t hdr;
  if(hpcrun_fmt_hdr_fread(&hdr, file, std::malloc) != HPCFMT_OK) {
    std::fclose(file);
    fileValid = false;
    return;
  }
  // The file is now placed right at the first epoch header.
  epoch_offsets.emplace_back(std::ftell(file));
  // Copy over the file attributes
  if(hdr.version != 2.0 && hdr.version != 3.0) {
    hpcrun_fmt_hdr_free(&hdr, std::free);
    std::fclose(file);
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
    else if(k != HPCRUN_FMT_NV_traceMinTime
            && k != HPCRUN_FMT_NV_traceMaxTime) {
      util::log::warning()
      << "Unknown file attribute in " << path.string() << ":\n"
          "  '" << k << "'='" << std::string(v) << "'!";
    }
  }
  // Clean up the mess
  hpcrun_fmt_hdr_free(&hdr, std::free);
  std::fclose(file);

  // Generate the hierarchical tuple from the header fields
  {
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

  // We also support a corrosponding hpctrace file if it exists. Try it.
  // If anything goes wrong, we just won't actually use it.
  if(!setupTrace()) tracepath.clear();
}

bool HpcrunFSv2::valid() const noexcept { return fileValid; }

DataClass HpcrunFSv2::provides() const noexcept {
  Class ret = attributes + references + contexts + metrics + threads;
  if(!tracepath.empty()) ret += timepoints;
  return ret;
}

DataClass HpcrunFSv2::finalizeRequest(const DataClass& d) const noexcept {
  DataClass o = d;
  if(o.hasMetrics()) o += attributes + threads;
  if(o.hasContexts()) o += references;
  return o;
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

// Read an 8-byte uint value from a file, big endian.
static uint64_t read_uint8(std::FILE* f) {
  unsigned char buf[8];
  if(std::fread(&buf[0], sizeof buf[0], 8, f) != 8)
    util::log::fatal() << "EOF while reading uint8!";
  return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48)
         | ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32)
         | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16)
         | ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
}

// Bits for the helper data structure.
HpcrunFSv2::seekfile::seekfile(const stdshim::filesystem::path& p, long at)
  : file(std::fopen(p.c_str(), "rb")), curpos(at), atpos(true) {
  if(!file) {
    char buf[1024];
    char* err = strerror_r(errno, buf, sizeof buf);
    util::log::fatal() << "Unable to open hpcrun file for input: " << err << "!";
  }
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

void HpcrunFSv2::read(const DataClass& needed) {
  if(needed.hasAttributes() && attrsValid) {
    sink.attributes(std::move(attrs));
    attrsValid = false;
  }
  if(needed.anyOf(threads|metrics) && sink.limit().hasThreads() && tattrsValid) {
    thread = &sink.thread(std::move(tattrs));
    tattrsValid = false;
  }

  if(needed.anyOf(attributes + references + contexts + metrics)) {
    seekfile f(path);

    // We want to get all the data out in a single pass if possible, so we keep
    // track of where we would be if we were reading the entire thing and
    // seek up to that point as needed.
    f.seek(epoch_offsets.at(0).header);  // There should always be 1
    std::size_t idx = 0;
    bool skipepoch = false;

    // For each epoch, but we may not know how many there are.
    while(epochs_final ? idx < epoch_offsets.size() : true) {
      if(idx >= epoch_offsets.size()) epoch_offsets.emplace_back(f.curpos);
      auto& epoch = epoch_offsets[idx];
      if(skipepoch) f.seek(epoch.header);  // Special skip value
      else epoch.header = f.curpos;  // We should be currently sitting at the header.

      if(epoch.cct) {
        f.seek(epoch.cct);  // We've already read this header, just skip it
      } else {
        // Seek up and read in the headers
        f.prep();
        // Epoch header
        hpcrun_fmt_epochHdr_t hdr;
        auto x = hpcrun_fmt_epochHdr_fread(&hdr, f.file, std::malloc);
        if(x == HPCFMT_EOF) {
          epoch_offsets.pop_back();
          break;
        } else if(x != HPCFMT_OK)
          util::log::fatal() << "Error reading epoch header!";
        // Metric table
        metric_tbl_t mets;
        metric_aux_info_t *aux;
        if(hpcrun_fmt_metricTbl_fread(&mets, &aux, f.file, 2.0, std::malloc)
            != HPCFMT_OK)
          util::log::fatal() << "Error reading metric table!";
        // Loadmap
        loadmap_t mods;
        if(hpcrun_fmt_loadmap_fread(&mods, f.file, std::malloc) != HPCFMT_OK)
          util::log::fatal() << "Error reading loadmap!";
        // Copy the data over. Not a lot for now
        if(hdr.flags.fields.isLogicalUnwind)
          util::log::fatal() << "Lush is not currently supported!";

        // Now that the header is read, emit the Metrics and Modules
        metric_cnt = mets.len;
        if(sink.limit().hasAttributes()) {
          for(uint32_t i = 0; i < mets.len; i++) {
            const auto& m = mets.lst[i];
            if(m.flags.fields.valFmt == MetricFlags_ValFmt_Real)
              metric_int.emplace_back(false);
            else if(m.flags.fields.valFmt == MetricFlags_ValFmt_Int)
              metric_int.emplace_back(true);
            else
              util::log::fatal() << "Invalid metric value format!";
            metric_order.emplace_back(&sink.metric({m.name, m.description}));
          }
        }
        if(sink.limit().hasReferences()) {
          for(uint32_t i = 0; i < mods.len; i++) {
            const auto& m = mods.lst[i];
            epoch.module_ids.emplace(m.id, sink.module(std::string(m.name)));
          }
        }
        // Mark our position for posterity
        epoch.cct = f.mark();
        // Clean up after ourselves
        hpcrun_fmt_epochHdr_free(&hdr, std::free);
        hpcrun_fmt_metricTbl_free(&mets, std::free);
        std::free(aux);
        hpcrun_fmt_loadmap_free(&mods, std::free);
      }

      // CCT.
      if(epoch.passed && !needed.anyOf(contexts|metrics)) {
        skipepoch = true;  // We have no reason to read the CCT here.
      } else if(epoch.passed && epoch.read_cct && epoch.read_met) {
        skipepoch = true;  // We already read the data here.
      } else {
        f.prep();
        auto cnt = read_uint8(f.file);
        if(!needed.anyOf(contexts|metrics)) {
          // We don't actually need to read the data, we can just seek over.
          f.mark();
          f.advance(cnt*(4+4+2+8 + 8*metric_cnt));
        } else {
          for(uint64_t i = 0; i < cnt; i++) {
            hpcrun_fmt_cct_node_t n;
            std::vector<hpcrun_metricVal_t> ms(metric_cnt);
            n.num_metrics = metric_cnt;
            n.metrics = ms.data();
            f.prep();
            if(hpcrun_fmt_cct_node_fread(&n, {0}, f.file) != HPCFMT_OK)
              util::log::fatal() << "Error reading CCT node!";

            std::optional<ContextRef> here;
            if(sink.limit().hasContexts()) {
              if(epoch.read_cct) {
                // We already read this part, so just use the cached result
                if(n.id == epoch.partial_node_id) continue;  // Global unknown
                if(n.id == epoch.unknown_node_id) continue;  // Stitched unknown
                auto it = epoch.node_ids.find(n.id);
                if(it == epoch.node_ids.end())
                  util::log::fatal() << "Unknown node ID " << n.id << "!";
                here = it->second;
                if(auto chere = std::get_if<Context>(*here))
                  if(chere->scope().type() == Scope::Type::global) continue;  // Global
              } else {
                // Figure out the parent of this node, if it has one.
                util::optional_ref<Context> par;
                if(n.id_parent == 0) {  // Root of some kind
                  if(n.lm_id == 0) {  // Synthetic root, remap to something useful.
                    if(n.lm_ip == HPCRUN_FMT_LMIp_NULL) {
                      // Global Scope, for full or "normal" unwinds. No actual node.
                      epoch.node_ids.emplace(n.id, sink.global());
                      continue;
                    } else if(n.lm_ip == HPCRUN_FMT_LMIp_Flag1) {
                      // Global unknown Scope, for "partial" unwinds.
                      // aka. /lost+found but for CCT trees.
                      epoch.partial_node_id = n.id;
                      continue;
                    } else {
                      // This really shouldn't happen. For now throw, decide how to
                      // handle gracefully later.
                      util::log::fatal() << "Unknown synthetic root!";
                    }
                  } else {
                    // If it looks like a sample but doesn't have a parent,
                    // stitch it to the global unknown (a la /lost+found).
                    auto r = sink.context(sink.global(), {});
                    if(!std::holds_alternative<Context>(r))
                      util::log::fatal{} << "Global unknown is not a valid Context!";
                    par = std::get<Context>(r);
                  }
                } else if(n.id_parent == epoch.partial_node_id ||
                          n.id_parent == epoch.unknown_node_id) {
                  // Global unknown Scope, emitted lazily.
                  auto r = sink.context(sink.global(), {});
                  if(!std::holds_alternative<Context>(r))
                    util::log::fatal{} << "Global unknown is not a valid Context!";
                  par = std::get<Context>(r);
                } else {  // Just nab its parent.
                  auto ppar = epoch.node_ids.find(n.id_parent);
                  if(ppar == epoch.node_ids.end())
                    util::log::fatal() << "CCT nodes not in a preorder!";
                  par = std::get<Context>(ppar->second);
                }

                // Figure out the Scope for this node, if it has one.
                Scope scope;  // Default to the unknown scope
                if(n.lm_id != 0) {
                  auto mod = epoch.module_ids.find(n.lm_id);
                  if(mod == epoch.module_ids.end())
                    util::log::fatal() << "CCT node references unknown module!";
                  scope = {mod->second, n.lm_ip};
                } else if(par->scope().type() == Scope::Type::global) {
                  // Special case: if its written in the .hpcrun file as
                  // global -> unknown, merge with the global unknown.
                  epoch.unknown_node_id = n.id;
                  continue;
                }

                // Emit the Context and record it for later
                here = sink.context(*par, scope);
                epoch.node_ids.emplace(n.id, *here);
              }
            }

            if(epoch.read_met) continue;  // We don't need to repeat ourselves
            if(needed.hasMetrics()) {
              // At this point, here == nullptr iff !sink.limit().hasContexts()
              stdshim::optional<ProfilePipeline::Source::AccumulatorsRef> accum;
              for(std::size_t i = 0; i < ms.size(); i++) {
                auto val = metric_int[i] ? (double)ms[i].i : ms[i].r;
                if(val == 0) continue;
                if(!accum) accum.emplace(sink.accumulateTo(
                  sink.limit().hasContexts() ? *here
                                             : sink.global(),
                  *thread));
                accum->add(*metric_order[i], val);
              }
            }
          }
          f.mark();
          epoch.read_cct = true;  // We always read the CCT
          epoch.read_met = epoch.read_met || needed.hasMetrics();
        }
        epoch.passed = true;
      }

      idx++;
    }

    // By this point, we know where all the epochs are.
    epochs_final = true;
  }

  // We can also do the trace file, if requested.
  if(needed.hasTimepoints() && !tracepath.empty()) {
    seekfile f(tracepath, trace_off);

    hpctrace_fmt_datum_t tpoint;
    while(1) {
      int err = hpctrace_fmt_datum_fread(&tpoint, {0}, f.file);
      if(err == HPCFMT_EOF) break;
      else if(err != HPCFMT_OK)
        util::log::fatal() << "Error reading trace datum!";
      auto it = epoch_offsets[0].node_ids.find(-tpoint.cpId);
      if(it != epoch_offsets[0].node_ids.end()) {
        std::chrono::nanoseconds tp(HPCTRACE_FMT_GET_TIME(tpoint.comp));
        if(thread)
          sink.timepoint(*thread, it->second, tp);
        else
          sink.timepoint(it->second, tp);
      }
    }
  }
}
