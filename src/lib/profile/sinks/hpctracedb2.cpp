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

#include "hpctracedb2.hpp"

#include "../util/log.hpp"
#include "lib/prof-lean/tracedb.h"
#include "lib/prof-lean/hpcrun-fmt.h"
#include "lib/prof-lean/hpcfmt.h"
#include "../mpi/all.hpp"


#include <iomanip>
#include <sstream>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <unistd.h>


using namespace hpctoolkit;
using namespace sinks;

HPCTraceDB2::HPCTraceDB2(const stdshim::filesystem::path& dir) {
  if(!dir.empty()) {
    stdshim::filesystem::create_directory(dir);
    tracefile = util::File(dir / "trace.db", true);
  } else {
    util::log::info() << "TraceDB issuing a dry run!";
  }
}

HPCTraceDB2::udThread::udThread(const Thread& t, HPCTraceDB2& tdb)
  : uds(tdb.uds), hdr(t, tdb) {}

void HPCTraceDB2::notifyWavefront(DataClass d){
  if(!d.hasThreads()) return;
  auto wd_sem = threadsReady.signal();
  util::File::Instance traceinst;
  {
    auto mpiSem = src.enterOrderedWavefront();
    if(tracefile) {
      tracefile->synchronize();
      traceinst = tracefile->open(true, true);
    }

    //initialize the hdr to write
    tracedb_hdr_t hdr;

    //assign value to trace_hdrs_size
    uint32_t num_traces = getTotalNumTraces();
    trace_hdrs_size = num_traces * trace_hdr_SIZE;
    hdr.num_trace = num_traces;
    hdr.trace_hdr_sec_size = trace_hdrs_size;

    //calculate the offsets for later stored in start and end
    //assign the values of the hdrs
    assignHdrs(calcStartEnd());

    //open the trace.db for writing, and write magic string, version number and number of tracelines
    if(mpi::World::rank() == 0) {
      std::array<char, HPCTRACEDB_FMT_Real_HeaderLen> buf;
      tracedb_hdr_swrite(&hdr, buf.data());
      if(tracefile) traceinst.writeat(0, buf);
    }

    // Ensure the file is truncated by rank 0 before proceeding.
    mpi::barrier();
  }

  // Write out the headers for threads that have no timepoints
  for(const auto& t : src.threads().iterate()) {
    const auto& hdr = t->userdata[uds.thread].hdr;
    if(hdr.start == hdr.end && tracefile) {
      trace_hdr_t thdr = {
        hdr.prof_info_idx, hdr.trace_idx, hdr.start, hdr.end
      };
      std::array<char, trace_hdr_SIZE> buf;
      trace_hdr_swrite(thdr, buf.data());
      traceinst.writeat((hdr.prof_info_idx - 1) * trace_hdr_SIZE + HPCTRACEDB_FMT_HeaderLen, buf);
    }
  }
}

void HPCTraceDB2::notifyThread(const Thread& t) {
  t.userdata[uds.thread].has_trace = false; 
}

void HPCTraceDB2::notifyTimepoint(const Thread& t, ContextRef::const_t cr, std::chrono::nanoseconds tm) {
  // Skip timepoints at locations we can't represent currently.
  if(!std::holds_alternative<const Context>(cr)) return;

  const Context& c = std::get<const Context>(cr);
  threadsReady.wait();
  auto& ud = t.userdata[uds.thread];
  if(!ud.has_trace) {
    has_traces.exchange(true, std::memory_order_relaxed);
    ud.has_trace = true;
    if(tracefile) {
      ud.inst = tracefile->open(true, true);

      //write the hdr
      trace_hdr_t hdr = {
        ud.hdr.prof_info_idx, ud.hdr.trace_idx, ud.hdr.start, ud.hdr.end
      };
      assert((hdr.start != (uint64_t)INVALID_HDR) | (hdr.end != (uint64_t)INVALID_HDR));
      std::array<char, trace_hdr_SIZE> buf;
      trace_hdr_swrite(hdr, buf.data());
      ud.inst->writeat((ud.hdr.prof_info_idx - 1) * trace_hdr_SIZE + HPCTRACEDB_FMT_HeaderLen, buf);
    }
  }

  if(tm < ud.minTime) ud.minTime = tm;
  if(ud.maxTime < tm) ud.maxTime = tm;
  hpctrace_fmt_datum_t datum = {
    static_cast<uint64_t>(tm.count()),  // Point in time
    c.userdata[src.identifier()],  // Point in the CCT
    0  // MetricID (for datacentric, I guess)
  };
  if(ud.inst) {
    if(ud.cursor == ud.buffer.data())
      ud.off = ud.hdr.start + ud.tmcntr * timepoint_SIZE;
    assert(ud.hdr.start + ud.tmcntr * timepoint_SIZE < ud.hdr.end);
    ud.cursor = hpctrace_fmt_datum_swrite(&datum, {0}, ud.cursor);
    if(ud.cursor == &ud.buffer[ud.buffer.size()]) {
      ud.inst->writeat(ud.off, ud.buffer);
      ud.cursor = ud.buffer.data();
    }
    ud.tmcntr++;
  }
}

void HPCTraceDB2::notifyThreadFinal(const Thread::Temporary& tt) {
  auto& ud = tt.thread().userdata[uds.thread];
  if(ud.inst) {
    if(ud.cursor != ud.buffer.data())
      ud.inst->writeat(ud.off, ud.cursor - ud.buffer.data(), ud.buffer.data());
    ud.inst = std::nullopt;
  }
}

void HPCTraceDB2::notifyPipeline() noexcept {
  auto& ss = src.structs();
  uds.thread = ss.thread.add<udThread>(std::ref(*this));
  src.registerOrderedWavefront();
}

bool HPCTraceDB2::seen(const Context& c) {
  return true;
}

void HPCTraceDB2::mmupdate(std::chrono::nanoseconds tmin, std::chrono::nanoseconds tmax) {
  while(1) {
    auto val = min.load(std::memory_order_relaxed);
    if(min.compare_exchange_weak(val, std::min(val, tmin), std::memory_order_relaxed))
      break;
  }
  while(1) {
    auto val = max.load(std::memory_order_relaxed);
    if(max.compare_exchange_weak(val, std::max(val, tmax), std::memory_order_relaxed))
      break;
  }
}

void HPCTraceDB2::notifyTimepoint(std::chrono::nanoseconds tm) {
  has_traces.exchange(true, std::memory_order_relaxed);
  mmupdate(tm, tm);
}

std::string HPCTraceDB2::exmlTag() {
  if(!has_traces.load(std::memory_order_relaxed)) return "";
  for(const auto& t: src.threads().iterate()) {
    auto& ud = t->userdata[uds.thread];
    if(ud.has_trace) mmupdate(ud.minTime, ud.maxTime);
  }
  std::ostringstream ss;
  ss << "<TraceDB"
        " i=\"0\""
        " db-min-time=\"" << min.load(std::memory_order_relaxed).count() << "\""
        " db-max-time=\"" << max.load(std::memory_order_relaxed).count() << "\""
        " u=\"1000000000\"/>\n";
  return ss.str();
}

void HPCTraceDB2::write() {}


//***************************************************************************
// trace_hdr
//***************************************************************************
HPCTraceDB2::traceHdr::traceHdr(const Thread& t, HPCTraceDB2& tdb)
  : prof_info_idx(t.userdata[tdb.src.identifier()] + 1) , 
    trace_idx(0), start(INVALID_HDR), end(INVALID_HDR) {}

uint64_t HPCTraceDB2::getTotalNumTraces() {
  uint32_t rank_num_traces = src.threads().size();
  return mpi::allreduce<uint32_t>(rank_num_traces, mpi::Op::sum());
}

std::vector<uint64_t> HPCTraceDB2::calcStartEnd() {
  //get the size of all traces
  std::vector<uint64_t> trace_sizes;
  uint64_t total_size = 0;
  for(const auto& t : src.threads().iterate()){
    uint64_t trace_sz = t->attributes.timepointCnt().value_or(0) * timepoint_SIZE;
    trace_sizes.emplace_back(trace_sz);
    total_size += trace_sz;
  }

  //get the offset of this rank's traces section
  uint64_t my_off = mpi::exscan(total_size, mpi::Op::sum()).value_or(0);

  //get the individual offsets of this rank's traces
  std::vector<uint64_t> trace_offs(trace_sizes.size() + 1);
  trace_sizes.emplace_back(0);
  exscan<uint64_t>(trace_sizes);
  for(uint i = 0; i < trace_sizes.size();i++){
    trace_offs[i] = trace_sizes[i] + my_off + HPCTRACEDB_FMT_HeaderLen + (MULTIPLE_8(trace_hdrs_size)); 
  }

  return trace_offs;

}

void HPCTraceDB2::assignHdrs(const std::vector<uint64_t>& trace_offs) {
  int i = 0;
  for(const auto& t : src.threads().iterate()){
    auto& hdr = t->userdata[uds.thread].hdr;
    hdr.start = trace_offs[i];
    hdr.end = trace_offs[i+1];
    i++;
  }

}

template <typename T>
void HPCTraceDB2::exscan(std::vector<T>& data) {
  int n = data.size();
  int rounds = ceil(std::log2(n));
  std::vector<T> tmp (n);

  for(int i = 0; i<rounds; i++){
    for(int j = 0; j < n; j++){
      int p = (int)pow(2.0,i);
      tmp.at(j) = (j<p) ?  data.at(j) : data.at(j) + data.at(j-p);
    }
    if(i<rounds-1) data = tmp;
  }

  if(n>0) data[0] = 0;
  for(int i = 1; i < n; i++){
    data[i] = tmp[i-1];
  }
}
