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

#include "hpctracedb.hpp"

#include "lib/prof-lean/hpcrun-fmt.h"

#include <iomanip>
#include <sstream>

#include <iostream>

using namespace hpctoolkit;
using namespace profilesinks;

HPCTraceDB::HPCTraceDB(const stdshim::filesystem::path& p, bool omni)
  : ProfileSink(), is_omniocular(omni), dir(p), has_traces(false),
  min(std::chrono::nanoseconds::max()), max(std::chrono::nanoseconds::min()) {
  stdshim::filesystem::create_directory(dir);
}

HPCTraceDB::HPCTraceDB(stdshim::filesystem::path&& p, bool omni)
  : ProfileSink(), is_omniocular(omni), dir(std::move(p)), has_traces(false),
  min(std::chrono::nanoseconds::max()), max(std::chrono::nanoseconds::min()) {
  stdshim::filesystem::create_directory(dir);
}

HPCTraceDB::udThread::udThread(const Thread& t, HPCTraceDB& tdb)
  : uds(tdb.uds), file(nullptr), minTime(std::chrono::nanoseconds::max()),
    maxTime(std::chrono::nanoseconds::min()) {
  std::ostringstream ss;
  ss << std::setfill('0') << "experiment"
        "-" << std::setw(6) << t.attributes.mpirank()
     << "-" << std::setw(3) << t.attributes.threadid()
     << "-" << std::setw(8) << std::hex << t.attributes.hostid() << std::dec
     << "-" << t.attributes.procid()
     << "-0." << HPCRUN_TraceFnmSfx;
  path = tdb.dir / ss.str();
}

void HPCTraceDB::traceStart(const Thread& t, unsigned long, std::chrono::nanoseconds) {
  auto& ud = t.userdata[uds.thread];
  has_traces.exchange(true, std::memory_order_relaxed);
  ud.has_trace = true;
  if(ud.file == nullptr) {
    ud.file = std::fopen(ud.path.c_str(), "wb");
    if(!ud.file) throw std::logic_error("Unable to open trace file for output!");
    hpctrace_fmt_hdr_fwrite({0}, ud.file);
  }
}

void HPCTraceDB::trace(const Thread& t, unsigned long, std::chrono::nanoseconds tm, const Context& c) {
  auto& ud = t.userdata[uds.thread];
  if(tm < ud.minTime) ud.minTime = tm;
  if(ud.maxTime < tm) ud.maxTime = tm;
  hpctrace_fmt_datum_t datum = {
    static_cast<uint64_t>(tm.count()),  // Point in time
    c.userdata[src.extensions().id_context],  // Point in the CCT
    0  // MetricID (for datacentric, I guess)
  };
  hpctrace_fmt_datum_fwrite(&datum, {0}, ud.file);

  if(is_omniocular) {
    auto& udc = c.userdata[uds.context];
    udc.used.exchange(true, std::memory_order_relaxed);
  }
}

void HPCTraceDB::traceEnd(const Thread& t, unsigned long) {
  auto& ud = t.userdata[uds.thread];
  std::fclose(ud.file);
  ud.file = nullptr;
}

void HPCTraceDB::notifyPipeline() {
  auto& ss = src.structs();
  uds.thread = ss.thread.add<udThread>(std::ref(*this));
  if(is_omniocular)
    uds.context = ss.context.add<udContext>(std::ref(*this));
}

bool HPCTraceDB::seen(const Context& c) {
  if(is_omniocular)
    return c.userdata[uds.context].used.load(std::memory_order_relaxed);
  return true;
}

void HPCTraceDB::extra(std::chrono::nanoseconds beg, std::chrono::nanoseconds end) {
  has_traces.exchange(true, std::memory_order_relaxed);
  if(beg < min) min = beg;
  if(max < end) max = end;
}

std::string HPCTraceDB::exmlTag() {
  if(!has_traces.load(std::memory_order_relaxed)) return "";
  for(const auto& t: src.threads()) {
    auto& ud = t->userdata[uds.thread];
    if(ud.has_trace) {
      if(ud.minTime < min) min = ud.minTime;
      if(ud.maxTime > max) max = ud.maxTime;
    }
  }
  std::ostringstream ss;
  ss << "<TraceDB"
        " i=\"0\""
        " db-glob=\"*." << HPCRUN_TraceFnmSfx << "\""
        " db-min-time=\"" << min.count() << "\""
        " db-max-time=\"" << max.count() << "\""
        " db-header-sz=\"" << HPCTRACE_FMT_HeaderLen << "\""
        " u=\"1000000000\"/>";
  return ss.str();
}

bool HPCTraceDB::write(std::chrono::nanoseconds) {
  return true;
}
