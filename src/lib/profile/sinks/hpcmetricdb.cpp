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

#include "../util/log.hpp"
#include "../util/xml.hpp"
#include "experimentxml.hpp"
#include "lib/prof-lean/hpcrun-fmt.h"

#include <sstream>
#include <iterator>
#include <iomanip>
#include <algorithm>

using namespace hpctoolkit;
using namespace sinks;

// udMetric bits

HPCMetricDB::udMetric::udMetric(const Metric& m, HPCMetricDB& mdb) {
  auto ids = m.userdata[mdb.src.identifier()];
  std::ostringstream si;
  std::ostringstream se;
  si << "<MetricDB i=\"" << ids.second << "\" n=" << util::xmlquoted(m.name() + " (I)")
              << " t=\"inclusive\" partner=\"" << ids.first << "\""
              << " db-glob=\"*." << HPCPROF_MetricDBSfx << "\""
              << " db-id=\"" << ids.second << "\""
              << " db-header-sz=\"" << HPCMETRICDB_FMT_HeaderLen << "\"";
  inc_open = si.str();
  se << "<MetricDB i=\"" << ids.first << "\" n=" << util::xmlquoted(m.name() + " (E)")
              << " t=\"exclusive\" partner=\"" << ids.second << "\""
              << " db-glob=\"*." << HPCPROF_MetricDBSfx << "\""
              << " db-id=\"" << ids.first << "\""
              << " db-header-sz=\"" << HPCMETRICDB_FMT_HeaderLen << "\"";
  ex_open = se.str();
}

// HPCMetricDB bits

HPCMetricDB::HPCMetricDB(const stdshim::filesystem::path& p) : dir(p), ctxMaxId(0) {
  if(dir.empty()) {
    util::log::info() << "MetricDB issuing a dry run!";
  } else {
    stdshim::filesystem::create_directory(dir);
  }
}
HPCMetricDB::HPCMetricDB(stdshim::filesystem::path&& p) : dir(std::move(p)), ctxMaxId(0) {
  if(dir.empty()) {
    util::log::info() << "MetricDB issuing a dry run!";
  } else {
    stdshim::filesystem::create_directory(dir);
  }
}

void HPCMetricDB::notifyPipeline() noexcept {
  auto& ss = src.structs();
  ud_metric = ss.metric.add<udMetric>(std::ref(*this));
  ud_thread = ss.thread.add<udThread>();
}

std::string HPCMetricDB::exmlTag() {
  auto nmets = src.metrics().size() * 2;
  if(nmets == 0) return "<MetricDBTable/>";
  std::ostringstream ss;
  ss << "<MetricDBTable>";

  // These have to appear in the same order as the ids, for whatever reason.
  // So we collect the string pointers into a vector first.
  std::vector<const std::string*> strs(src.metrics().size() * 2, nullptr);
  for(const auto& m: src.metrics().iterate()) {
    auto& ud = m().userdata[ud_metric];
    const auto& ids = m().userdata[src.identifier()];
    strs.at(ids.first) = &ud.ex_open;
    strs.at(ids.second) = &ud.inc_open;
  }
  for(const auto& sp: strs)
    ss << *sp << " db-num-metrics=\"" << nmets << "\"/>";

  ss << "</MetricDBTable>";
  return ss.str();
}

void HPCMetricDB::notifyWavefront(DataClass::singleton_t ds) noexcept {
  if(((DataClass)ds).hasAttributes())
    metricPrep.call_nowait([this]{ prepMetrics(); });
  if(((DataClass)ds).hasContexts())
    contextPrep.call_nowait([this]{ prepContexts(); });
}

void HPCMetricDB::prepMetrics() noexcept {
  for(const Metric& m: src.metrics().iterate()) {
    const auto& ids = m.userdata[src.identifier()];
    auto max = std::max(ids.first, ids.second);
    if(max >= metrics.size()) metrics.resize(max+1, {false, nullptr});
    metrics.at(ids.first) = {true, &m};
    metrics.at(ids.second) = {false, &m};
  }
  metrics.shrink_to_fit();
}

void HPCMetricDB::prepContexts() noexcept {
  std::map<unsigned int, std::reference_wrapper<const Context>> cs;
  std::function<void(const Context&)> ctx = [&](const Context& c) {
    auto id = c.userdata[src.identifier()];
    ctxMaxId = std::max(ctxMaxId, id);
    if(!cs.emplace(id, c).second)
      util::log::fatal() << "Duplicate Context identifier "
                         << c.userdata[src.identifier()] << "!";
    for(const Context& cc: c.children().iterate()) ctx(cc);
  };
  ctx(src.contexts());

  contexts.reserve(cs.size());
  for(const auto& ic: cs) contexts.emplace_back(ic.second);
}

void HPCMetricDB::notifyThreadFinal(const Thread::Temporary& tt) {
  const auto& t = tt.thread;

  bool dry = dir.empty();

  std::FILE* of = nullptr;
  {
    std::ostringstream ss;
    ss << std::setfill('0') << "experiment"
          "-" << std::setw(6) << t.attributes.mpirank()
        << "-" << std::setw(3) << t.attributes.threadid()
        << "-" << std::setw(8) << std::hex << t.attributes.hostid() << std::dec
        << "-" << t.attributes.procid()
        << "-0." << HPCPROF_MetricDBSfx;
    t.userdata[ud_thread].outfile = dir / ss.str();
  }
  if(!dry) {
    of = std::fopen(t.userdata[ud_thread].outfile.c_str(), "wb");
    if(!of) util::log::fatal() << "Unable to open metric-db file for output!";
  }

  // Make sure we have the Metric and Context lists sorted out
  metricPrep.call([this]{ prepMetrics(); });
  contextPrep.call([this]{ prepContexts(); });

  // Write the header, so we know where to start. Each Context takes up two
  // "nodes," and then node 0 is skipped to match EXML ids.
  hpcmetricDB_fmt_hdr_t hdr;
  hdr.numNodes = (ctxMaxId+1)*2 + 1;
  hdr.numMetrics = (uint32_t)metrics.size();
  if(!dry)
    if(hpcmetricDB_fmt_hdr_fwrite(&hdr, of) == HPCFMT_ERR)
      util::log::fatal() << "Error writing to metric-db file!";

  // Now go through the Contexts and write them out, in order.
  std::size_t skip = metrics.size()*8;  // Node 0 is skipped.
  unsigned int nextid = 0;
  for(const Context& c: contexts) {
    // There may be gaps in the ids. Make sure we properly skip over any.
    auto id = c.userdata[src.identifier()];
    if(nextid < id) skip += (id - nextid) * 2 * metrics.size()*8;
    nextid = id + 1;

    // Helper union for messing about with all the bits.
    union byte8 {
      byte8(double x) : d(x) {};
      double d;
      uint64_t i;
    };
    static_assert(sizeof(double) == 8 && sizeof(uint64_t) == 8);

    // First cook the output strip. That way we only need one write.
    std::vector<char> strip(metrics.size()*8*2);
    std::size_t idx = 0;
    bool any = false;
    for(const auto& fm: metrics) {
      byte8 v(0);
      if(fm.second) {
        auto vv = fm.second->getFor(tt, c);
        v.d = fm.first ? vv.first : vv.second;
      }
      if(v.d != 0) any = true;

      // Convert to big-endian and write into the strip
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x38) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x30) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x28) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x20) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x18) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x10) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x08) & 0xff; idx++;
      strip[idx] = strip[metrics.size()*8 + idx] = (v.i >> 0x00) & 0xff; idx++;
    }

    if(any) {
      // Seek up to the right location and write out the strip
      if(!dry && skip > 0) std::fseek(of, skip, SEEK_CUR);
      skip = 0;
      if(!dry)
        std::fwrite(strip.data(), 1, strip.size(), of);
    } else {
      // Mark that this entry is skipped
      skip += strip.size();
    }
  }

  // Seek past the last bit and write a byte to extend the file
  if(!dry && skip > 0) {
    std::fseek(of, skip-1, SEEK_CUR);
    std::fputc(0, of);
  }

  // Clean up and close up
  if(!dry)
    std::fclose(of);
}

void HPCMetricDB::write() {};
