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

#include "../util/vgannotations.hpp"

#include "hpcmetricdb.hpp"

#include "experimentxml.hpp"
#include "../util/xml.hpp"
#include "lib/prof-lean/hpcrun-fmt.h"

#include <sstream>
#include <iterator>
#include <iomanip>
#include <algorithm>

using namespace hpctoolkit;
using namespace profilesinks;

// udMetric bits

HPCMetricDB::udMetric::udMetric(const Metric& m, HPCMetricDB& mdb) {
  auto ids = m.userdata[mdb.src.extensions().id_metric];
  std::ostringstream si;
  std::ostringstream se;
  si << "<MetricDB i=\"" << ids.second << "\" n=" << internal::xmlquoted(m.name() + " (I)")
              << " t=\"inclusive\" partner=\"" << ids.first << "\""
              << " db-glob=\"*." << HPCPROF_MetricDBSfx << "\""
              << " db-id=\"" << ids.second << "\""
              << " db-header-sz=\"" << HPCMETRICDB_FMT_HeaderLen << "\"";
  inc_open = si.str();
  se << "<MetricDB i=\"" << ids.first << "\" n=" << internal::xmlquoted(m.name() + " (E)")
              << " t=\"exclusive\" partner=\"" << ids.second << "\""
              << " db-glob=\"*." << HPCPROF_MetricDBSfx << "\""
              << " db-id=\"" << ids.first << "\""
              << " db-header-sz=\"" << HPCMETRICDB_FMT_HeaderLen << "\"";
  ex_open = se.str();
}

// HPCMetricDB bits

HPCMetricDB::HPCMetricDB(const stdshim::filesystem::path& p, bool s)
  : separated(s), dir(p), state(nullptr), threadHelp(0), threadDone(0) {};
HPCMetricDB::HPCMetricDB(stdshim::filesystem::path&& p, bool s)
  : separated(s), dir(std::move(p)), state(nullptr), threadHelp(0), threadDone(0) {};

void HPCMetricDB::notifyPipeline() {
  auto& ss = src.structs();
  ud_metric = ss.metric.add<udMetric>(std::ref(*this));
  ud_thread = ss.thread.add<udThread>();
}

void HPCMetricDB::notifyContextData(const Thread& t, const Context& c, const Metric& m, double v) {
  auto& md = t.userdata[ud_thread].data[&m];
  md[&c] += v;
  for(const Context* p = c.direct_parent(); p != nullptr; p = p->direct_parent())
    md[p] <<= v;
}

std::string HPCMetricDB::exmlTag() {
  auto nmets = src.metrics().size() * 2;
  if(nmets == 0) return "<MetricDBTable/>";
  std::ostringstream ss;
  ss << "<MetricDBTable>";

  // These have to appear in the same order as the ids, for whatever reason.
  // So we collect the string pointers into a vector first.
  std::vector<const std::string*> strs(src.metrics().size() * 2, nullptr);
  for(const auto& m: src.metrics()) {
    auto& ud = m().userdata[ud_metric];
    const auto& ids = m().userdata[src.extensions().id_metric];
    strs.at(ids.first) = &ud.ex_open;
    strs.at(ids.second) = &ud.inc_open;
  }
  for(const auto& sp: strs)
    ss << *sp << " db-num-metrics=\"" << nmets << "\"/>";

  ss << "</MetricDBTable>";
  return ss.str();
}

bool HPCMetricDB::write(std::chrono::nanoseconds) {
  if(state != nullptr) return true;
  state_t s;
  state = &s;
  {
    auto sig = ready.signal();

    // Arrange the threads
    for(const auto& tp: src.threads())
      s.threads.emplace_back(tp.get());
    s.threads.shrink_to_fit();
    std::sort(s.threads.begin(), s.threads.end(),
      [](const Thread* ta, const Thread* tb) {
        const auto& a = ta->attributes;
        const auto& b = tb->attributes;
        if(a.mpirank() != b.mpirank()) return a.mpirank() < b.mpirank();
        if(a.threadid() != b.threadid()) return a.threadid() < b.threadid();
        return ta < tb;
      });

    // Densify the metrics
    for(const auto& m: src.metrics()) {
      const auto& ids = m().userdata[src.extensions().id_metric];
      auto max = std::max(ids.first, ids.second);
      if(max >= s.metrics.size()) s.metrics.resize(max+1, nullptr);
      s.metrics.at(ids.first) = &m();
      s.metrics.at(ids.second) = &m();
    }
    s.metrics.shrink_to_fit();

    // Arrange the Contexts
    std::function<void(const Context&)> cnter = [&](const Context& c) {
      auto id = c.userdata[src.extensions().id_context];
      if(id >= s.contexts.size()) s.contexts.resize(id+1, nullptr);
      s.contexts.at(id) = &c;
      for(const auto& cc: c.children()) cnter(cc());
    };
    cnter(src.contexts());
    s.contexts.shrink_to_fit();

    // Calculate the offsets
    s.base = 2*4 + s.threads.size()*16;
    s.perline = s.metrics.size() * 8;
    s.perthread = HPCMETRICDB_FMT_HeaderLen + 2*4 + s.contexts.size()*s.perline;

    // Create the file and write the header and footer.
    if(!separated) {
      s.path = dir / "experiment-1.mdb";
      std::FILE* of = std::fopen(s.path.c_str(), "wb");
      if(!of) throw std::logic_error("Unable to open metric-db file for output!");
      hpcfmt_int4_fwrite(3, of);
      hpcfmt_int4_fwrite(s.threads.size(), of);
      for(std::size_t i = 0; i < s.threads.size(); i++) {
        const auto& t = *s.threads[i];
        hpcfmt_int4_fwrite(t.attributes.mpirank(), of);
        hpcfmt_int4_fwrite(t.attributes.threadid(), of);
        hpcfmt_int8_fwrite(s.base + s.perthread*i, of);
      }
      std::fseek(of, s.base + s.perthread*s.threads.size(), SEEK_SET);
      hpcfmt_int8_fwrite(0xFFFFFFFFDEADF00D, of);
      std::fclose(of);
    }
  }
  help();
  while(threadDone.load(std::memory_order_acquire) != s.threads.size());
  ANNOTATE_HAPPENS_AFTER(&threadDone);
  return true;
}

bool HPCMetricDB::help(std::chrono::nanoseconds) {
  ready.wait();  // Wait for the master to set stuff up.
  auto mine = threadHelp.fetch_add(1, std::memory_order_relaxed);
  while(mine < src.threads().size()) {
    auto& t = *state->threads[mine];
    std::FILE* of;
    if(separated) {
      std::ostringstream ss;
      ss << std::setfill('0') << "experiment"
            "-" << std::setw(6) << t.attributes.mpirank()
         << "-" << std::setw(3) << t.attributes.threadid()
         << "-" << std::setw(8) << std::hex << t.attributes.hostid() << std::dec
         << "-" << t.attributes.procid()
         << "-0." << HPCPROF_MetricDBSfx;
      of = std::fopen((dir / ss.str()).c_str(), "wb");
      if(!of) throw std::logic_error("Unable to open metric-db file for output!");
    } else {
      of = std::fopen(state->path.c_str(), "r+b");
      if(!of) throw std::logic_error("Unable to open metric-db file for output!");
      std::fseek(of, state->base + state->perthread*mine, SEEK_SET);
    }
    hpcmetricDB_fmt_hdr_t hdr = {
      .numNodes = (uint32_t)state->contexts.size(),
      .numMetrics = (uint32_t)state->metrics.size(),
    };
    if(hpcmetricDB_fmt_hdr_fwrite(&hdr, of) == HPCFMT_ERR)
      throw std::logic_error("Error writing to metric-db file!");

    // Since the Contexts are arranged, we can just write forward
    const auto& ud = t.userdata[ud_thread];
    for(const auto& cp: state->contexts) {
      for(std::size_t midx = 0; midx < state->metrics.size(); midx++) {
        auto& m = *state->metrics[midx];
        double v = 0;
        auto vp = ud.data.find(&m);
        if(vp != ud.data.end()) {
          auto vvp = vp->second.find_pointer(cp);
          if(vvp != nullptr) {
            auto vv = vvp->second.get();
            const auto& ids = m.userdata[src.extensions().id_metric];
            v = midx == ids.first ? vv.first :
                midx == ids.second ? vv.second :
                throw std::logic_error("Index should match the metric!");
          }
        }
        if(hpcfmt_real8_fwrite(v, of) == HPCFMT_ERR)
          throw std::logic_error("Error writing metric data!");
      }
    }

    // Clean up and close up
    t.userdata[ud_thread].data.clear();
    std::fclose(of);
    ANNOTATE_HAPPENS_BEFORE(&threadDone);
    threadDone.fetch_add(1, std::memory_order_release);
    mine = threadHelp.fetch_add(1, std::memory_order_relaxed);
  }
  threadHelp.fetch_sub(1, std::memory_order_relaxed);
  return true;
}
