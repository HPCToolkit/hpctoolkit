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
// Copyright ((c)) 2019-2022, Rice University
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
#include "lib/prof-lean/placeholders.h"

// TODO: Remove and change this once new-cupti is finalized
#define HPCRUN_GPU_ROOT_NODE 65533
#define HPCRUN_GPU_RANGE_NODE 65532
#define HPCRUN_GPU_CONTEXT_NODE 65531

using namespace hpctoolkit;
using namespace sources;

Hpcrun4::Hpcrun4(const stdshim::filesystem::path& fn)
  : ProfileSource(), fileValid(true), attrsValid(true), tattrsValid(true),
    thread(nullptr), path(fn),
    tracepath(fn.parent_path() / fn.stem().concat(".hpctrace")) {
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
  unsigned int traceDisorder = 0;  // Assume traces are ordered.
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
    else if(k == HPCRUN_FMT_NV_traceDisorder) {
      traceDisorder = std::strtoul(v, nullptr, 10);
    } else if(k != HPCRUN_FMT_NV_traceMinTime && k != HPCRUN_FMT_NV_traceMaxTime
              && k != HPCRUN_FMT_NV_mpiRank && k != HPCRUN_FMT_NV_tid
              && k != HPCRUN_FMT_NV_hostid && k != HPCRUN_FMT_NV_pid) {
      util::log::warning()
      << "Unknown file attribute in " << path.string() << ":\n"
          "  '" << k << "'='" << std::string(v) << "'!";
    }
  }
  hpcrun_fmt_hdr_free(&hdr, std::free);
  // Try to read the hierarchical tuple, failure is fatal for this Source
  {
    id_tuple_t sfTuple;
    if(hpcrun_sparse_read_id_tuple(file, &sfTuple) != SF_SUCCEED) {
      util::log::error{} << "Invalid profile identifier tuple in: "
                           << path.string();
      hpcrun_sparse_close(file);
      fileValid = false;
      return;
    }
    std::vector<pms_id_t> tuple;
    tuple.reserve(sfTuple.length);
    for(size_t i = 0; i < sfTuple.length; i++)
      tuple.push_back(sfTuple.ids[i]);
    id_tuple_free(&sfTuple);
    tattrs.idTuple(std::move(tuple));
  }
  // Try and read the dictionary for the tuple, failure is fatal for this Source
  {
    hpcrun_fmt_idtuple_dxnry_t dict;
    if(hpcrun_sparse_read_idtuple_dxnry(file, &dict) != SF_SUCCEED) {
      util::log::error{} << "Invalid profile identifier dictionary in: "
                           << path.string();
      hpcrun_sparse_close(file);
      fileValid = false;
      return;
    }
    for(int i = 0; i < dict.num_entries; i++)
      attrs.idtupleName(dict.dictionary[i].kind, dict.dictionary[i].kindStr);
    hpcrun_fmt_idtuple_dxnry_free(&dict, std::free);
  }
  // If all went well, we can pause the file here.
  hpcrun_sparse_pause(file);

  // Also check for a corrosponding tracefile. If anything goes wrong, we'll
  // just skip it.
  if(!setupTrace(traceDisorder)) tracepath.clear();
}

bool Hpcrun4::valid() const noexcept { return fileValid; }

Hpcrun4::~Hpcrun4() {
  if(fileValid) hpcrun_sparse_close(file);
}

DataClass Hpcrun4::provides() const noexcept {
  using namespace literals::data;
  Class ret = attributes + references + contexts + DataClass::metrics + threads;
  if(!tracepath.empty()) ret += ctxTimepoints;
  return ret;
}

DataClass Hpcrun4::finalizeRequest(const DataClass& d) const noexcept {
  using namespace literals::data;
  DataClass o = d;
  if(o.hasMetrics()) o += attributes + contexts + threads;
  if(o.hasCtxTimepoints()) o += contexts + threads;
  if(o.hasThreads()) o += contexts;  // In case of outlined range trees
  if(o.hasContexts()) o += references;
  return o;
}

bool Hpcrun4::setupTrace(unsigned int traceDisorder) noexcept {
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
  tattrs.ctxTimepointStats((trace_end - trace_off) / (8+4), traceDisorder);

  std::fclose(file);
  return true;
}

void Hpcrun4::read(const DataClass& needed) {
  if(!fileValid) return;  // We don't have anything more to say
  if(!realread(needed)) {
    util::log::error{} << "Error while parsing measurement profile " << path.filename().string();
    fileValid = false;
  }
}

bool Hpcrun4::realread(const DataClass& needed) try {
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
        else {
          util::log::info{} << "Invalid metric value format: " << m.flags.fields.valFmt;
          return false;
        }
        metrics.insert({id, {sink.metric(std::move(settings)), isInt}});
      }
      hpcrun_fmt_metricDesc_free(&m, std::free);
    }
    if(id < 0) {
      util::log::info{} << "Error while trying to read a metric entry";
      return false;
    }

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
            if(id.size() == 0) {
              util::log::info{} << "Invalid empty formula string";
              return false;
            }
            std::stoll(id);
          });
          id += 1;  // Adjust to sparse metric ids
          const auto it = metrics.find(id);
          if(it == metrics.end()) {
            util::log::info{} << "Value for unknown metric id: " << id;
            return false;
          }
          auto& met = it->second.first;
          es_settings.formula.emplace_back(ExtraStatistic::MetricPartialRef(
            met, met.statsAccess().requestSumPartial()));
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
    for(const auto& im: metrics) sink.metricFreeze(im.second.first);
  }
  if(needed.hasReferences()) {
    int id;
    loadmap_entry_t lm;
    while((id = hpcrun_sparse_next_lm(file, &lm)) > 0) {
      modules.emplace(id, sink.module(lm.name));
      hpcrun_fmt_loadmapEntry_free(&lm, std::free);
    }
    if(id < 0) {
      util::log::info{} << "Error while reading a load module entry";
      return false;
    }
  }
  if(needed.hasContexts()) {
    int id;
    hpcrun_fmt_cct_node_t n;
    while((id = hpcrun_sparse_next_context(file, &n)) > 0) {
      if(n.id_parent == 0) {
        // Root nodes are very limited in their forms
        if(n.lm_id == HPCRUN_PLACEHOLDER_LM) {
          if(n.lm_ip == hpcrun_placeholder_root_primary) {
            // Primary root, for normal "full" unwinds. Maps to (global).
            nodes.emplace(id, std::make_pair(nullptr, &sink.global()));
          } else {
            // It seems like we should handle root_partial here as well, but a
            // snippet in hpcrun/cct/cct.c stitches root_partial as a child of
            // the root_primary. So we handle it later.

            // All other cases are an error. Special case here for a slightly
            // nicer error message
            util::log::info{} << "Invalid root cct node: "
                              << Scope(Scope::placeholder, n.lm_ip);
            return false;
          }
        } else if(n.lm_id == HPCRUN_GPU_ROOT_NODE) {
          // Outlined range root node. No corresponding object.
          nodes.emplace(id, (int)0);
        } else {
          // All other cases are an error
          util::log::info{} << "Invalid root cct node: lm = " << n.lm_id
                            << ", ip = " << n.lm_ip;
          return false;
        }
        continue;  // No need to do any other processing
      }

      const auto outlineGpuContext = [this](uint64_t context){
        // Synthesize the id-tuple for a GPU context based on our own id-tuple.
        std::vector<pms_id_t> tuple = (tattrsValid ? tattrs : thread->thread().attributes).idTuple();
        auto node = std::find_if(tuple.rbegin(), tuple.rend(),
          [](const auto& e) -> bool {
            switch(IDTUPLE_GET_KIND(e.kind)) {
            default:
              return false;
            case IDTUPLE_NODE:
            case IDTUPLE_RANK:
              return true;
            }
          });
        tuple.erase(node.base(), tuple.end());
        tuple.push_back({IDTUPLE_COMPOSE(IDTUPLE_GPUCONTEXT, IDTUPLE_IDS_LOGIC_LOCAL),
                         context, 0});
        ThreadAttributes outlined_tattr;
        outlined_tattr.idTuple(std::move(tuple));
        return outlined_tattr;
      };

      // Determine how to interpret this node based on its parent
      auto par_it = nodes.find(n.id_parent);
      if(par_it == nodes.end()) {
        util::log::info{} << "Encountered out-of-order id_parent: " << n.id_parent;
        return false;
      }
      const auto& par = par_it->second;
      if(const auto* p_x = std::get_if<std::pair<Context*, Context*>>(&par)) {
        Context& par = *p_x->second;
        if(n.lm_id == HPCRUN_GPU_CONTEXT_NODE) {
          // This is a reference to an outlined range tree, rooted at grandpar
          // and par is the entry.
          nodes.emplace(id, std::make_pair(p_x,
              &sink.mergedThread(outlineGpuContext(n.lm_ip)) ));
          continue;
        }

        // In all other valid cases, this either represents a point or
        // placeholder Scope. Must be a point if we need reconstruction.
        Scope scope;
        if(n.lm_id == HPCRUN_PLACEHOLDER_LM && n.unwound) {
          switch(n.lm_ip) {
          case hpcrun_placeholder_unnormalized_ip:
          case hpcrun_placeholder_root_partial:  // Because hpcrun stitches here
            // Maps to (unknown) instead of a placeholder
            scope = Scope();
            break;
          case hpcrun_placeholder_root_primary:
            util::log::info{} << "Encountered <primary root> that wasn't a root!";
            return false;
          default:
            scope = Scope(Scope::placeholder, n.lm_ip);
            break;
          }
        } else {
          auto mod_it = modules.find(n.lm_id);
          if(mod_it == modules.end()) {
            util::log::info{} << "Invalid lm_id: " << n.lm_id;
            return false;
          }
          scope = Scope(mod_it->second, n.lm_ip);
        }

        if(!n.unwound) {
          // In this case, we need a Reconstruction.
          auto fg = sink.contextFlowGraph(scope);
          if(fg) {
            nodes.emplace(id, &sink.contextReconstruction(*fg, par));
          } else {
            // Failed to generate the appropriate FlowGraph, we must be missing
            // some data. Map to par -> (unknown) -> (point) and throw an error.
            util::log::error{} << "Missing required CFG data for binary: "
              << scope.point_data().first.path().string();
            nodes.emplace(id, std::make_pair(&par, &sink.context(sink.context(par, Scope()), scope)));
          }
        } else {
          // Simple straightforward Context
          nodes.emplace(id, std::make_pair(&par, &sink.context(par, scope)));
        }
      } else if(std::holds_alternative<ContextReconstruction*>(par)) {
        // This is invalid, Reconstructions cannot have children. Yet.
        util::log::info{} << "Encountered invalid child of un-unwindable cct node";
        return false;
      } else if(const auto* p_x = std::get_if<std::pair<const std::pair<Context*, Context*>*, PerThreadTemporary*>>(&par)) {
        if(n.lm_id != HPCRUN_GPU_RANGE_NODE) {
          // Children of CONTEXT nodes must be RANGE nodes.
          util::log::info{} << "Encountered invalid non-GPU_RANGE child of GPU_CONTEXT node";
          return false;
        }
        // Add this root to the proper reconstruction group
        sink.addToReconstructionGroup(*p_x->first->first, p_x->first->second->scope(), *p_x->second, n.lm_ip);
        nodes.emplace(id, std::make_pair(p_x, n.lm_ip));
      } else if(std::holds_alternative<std::pair<const std::pair<const std::pair<Context*, Context*>*, PerThreadTemporary*>*, uint64_t>>(par)) {
        // This is invalid, inline GPU_RANGE nodes cannot have children.
        util::log::info{} << "Encountered invalid child of inline GPU_RANGE node";
        return false;
      } else if(std::holds_alternative<int>(par)) {
        if(n.lm_id != HPCRUN_GPU_CONTEXT_NODE) {
          // Children of the range-root must be GPU_CONTEXT nodes.
          util::log::info{} << "Encountered invalid non-GPU_CONTEXT child of outlined range tree root";
          return false;
        }
        nodes.emplace(id, &sink.mergedThread(outlineGpuContext(n.lm_ip)));
      } else if(auto* pp_thread = std::get_if<PerThreadTemporary*>(&par)) {
        if(n.lm_id != HPCRUN_GPU_RANGE_NODE) {
          // Children of CONTEXT nodes must be RANGE nodes.
          util::log::info{} << "Encountered invalid non-GPU_RANGE child of GPU_CONTEXT node";
          return false;
        }
        nodes.emplace(id, std::make_pair(*pp_thread, n.lm_ip));
      } else if(auto* p_x = std::get_if<std::pair<PerThreadTemporary*, uint64_t>>(&par)) {
        // Sample within an outlined range tree. Always represents a point Scope.
        auto mod_it = modules.find(n.lm_id);
        if(mod_it == modules.end()) {
          util::log::info{} << "Invalid lm_id: " << n.lm_id;
          return false;
        }
        Scope scope(mod_it->second, n.lm_ip);

        auto fg = sink.contextFlowGraph(scope);
        if(fg) {
          sink.addToReconstructionGroup(*fg, *p_x->first, p_x->second);
          nodes.emplace(id, std::make_pair(p_x, &*fg));
        } else {
          // Failed to generate the appropriate FlowGraph, we must be missing
          // some data. Map to (global) -> (unknown) -> (point) and throw an error.
          util::log::error{} << "Missing required CFG data for binary: "
            << mod_it->second.path().string();
          auto& unk = sink.context(sink.global(), Scope());
          nodes.emplace(id, std::make_pair(&unk, &sink.context(unk, scope)));
        }
      } else if(std::holds_alternative<std::pair<const std::pair<PerThreadTemporary*, uint64_t>*, ContextFlowGraph*>>(par)) {
        // This is invalid, outlined range-tree sample nodes cannot have children.
        util::log::info{} << "Encountered invalid child of outlined range sample node";
        return false;
      } else {
        // It must be one of the previous, otherwise we're in trouble
        assert(false && "Invalid cct node saved storage!");
        std::abort();
      }
    }
    if(id < 0) {
      util::log::info{} << "Error while reading cct node entry";
      return false;
    }
  }
  if(needed.hasMetrics()) {
    int cid;
    while((cid = hpcrun_sparse_next_block(file)) > 0) {
      hpcrun_metricVal_t val;
      int mid = hpcrun_sparse_next_entry(file, &val);
      if(mid == 0) continue;
      assert(sink.limit().hasContexts());  // TODO: figure out what I was thinking here
      auto node_it = nodes.find(cid);
      if(node_it == nodes.end()) {
        util::log::info{} << "Encountered metric value for invalid cct node id: " << cid;
        return false;
      }
      auto accum = [&]() -> std::optional<ProfilePipeline::Source::AccumulatorsRef> {
        if(auto* p_x = std::get_if<std::pair<Context*, Context*>>(&node_it->second)) {
          return sink.accumulateTo(*thread, *p_x->second);
        } else if(auto* pp_reconst = std::get_if<ContextReconstruction*>(&node_it->second)) {
          return sink.accumulateTo(*thread, **pp_reconst);
        } else if(auto* p_x = std::get_if<std::pair<const std::pair<const std::pair<Context*, Context*>*,
            PerThreadTemporary*>*, uint64_t>>(&node_it->second)) {
          return sink.accumulateTo(*p_x->first->second, p_x->second, *p_x->first->first->second);
        } else if(auto* p_x = std::get_if<std::pair<const std::pair<PerThreadTemporary*, uint64_t>*,
                                                    ContextFlowGraph*>>(&node_it->second)) {
          return sink.accumulateTo(*p_x->first->first, p_x->first->second, *p_x->second);
        }
        return std::nullopt;
      }();
      if(!accum) {
        util::log::info{} << "Encountered metric value for cct node that should have none: " << cid;
        return false;
      }
      while(mid > 0) {
        const auto& x = metrics.at(mid);
        Metric& met = x.first;
        bool isInt = x.second;
        accum->add(met, isInt ? (double)val.i : val.r);
        mid = hpcrun_sparse_next_entry(file, &val);
      }
    }
  }

  // Pause the file, we're at a good point here
  hpcrun_sparse_pause(file);

  if(needed.hasCtxTimepoints() && !tracepath.empty()) {
    assert(thread);

    std::FILE* f = std::fopen(tracepath.c_str(), "rb");
    std::fseek(f, trace_off, SEEK_SET);
    hpctrace_fmt_datum_t tpoint;
    while(1) {
      int err = hpctrace_fmt_datum_fread(&tpoint, {0}, f);
      if(err == HPCFMT_EOF) break;
      else if(err != HPCFMT_OK) {
        util::log::info{} << "Error reading trace datum from "
                          << tracepath.filename().string();
        return false;
      }
      auto it = nodes.find(tpoint.cpId);
      if(it != nodes.end()) {
        if(auto* p_x = std::get_if<std::pair<Context*, Context*>>(&it->second)) {
          switch(sink.timepoint(*thread, *p_x->second,
              std::chrono::nanoseconds(HPCTRACE_FMT_GET_TIME(tpoint.comp)))) {
          case ProfilePipeline::Source::TimepointStatus::next:
            break;  // 'Round the loop
          case ProfilePipeline::Source::TimepointStatus::rewindStart:
            // Put the cursor back at the beginning
            std::fseek(f, trace_off, SEEK_SET);
            break;
          }
        }
      }
    }
    std::fclose(f);
  }
  return true;
} catch(std::exception& e) {
  util::log::info{} << "Exception caught while reading measurement profile "
    << path.string() << "\n"
       "  what(): " << e.what();
  return false;
}
