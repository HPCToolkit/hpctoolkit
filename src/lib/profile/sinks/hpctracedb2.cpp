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

#include "../util/vgannotations.hpp"

#include "hpctracedb2.hpp"

#include "../util/log.hpp"
#include "../util/cache.hpp"
#include "lib/prof-lean/tracedb.h"
#include "lib/prof-lean/hpcrun-fmt.h"
#include "lib/prof-lean/hpcfmt.h"
#include "../mpi/all.hpp"

#include <iomanip>
#include <sstream>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cmath>
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

void HPCTraceDB2::notifyTimepoints(const Thread& t, const std::vector<
    std::pair<std::chrono::nanoseconds, std::reference_wrapper<const Context>>>& tps) {
  assert(!tps.empty());

  threadsReady.wait();
  auto& ud = t.userdata[uds.thread];
  if(!ud.has_trace) {
    has_traces.exchange(true, std::memory_order_relaxed);
    ud.has_trace = true;
    if(tracefile) ud.inst = tracefile->open(true, true);
  }

  util::linear_lru_cache<util::reference_index<const Context>, unsigned int,
                         2> cache;

  for(const auto& [tm, cr]: tps) {
    const Context& c = cr;
    // Try to cache our work as much as possible
    auto id = cache.lookup(c, [&](util::reference_index<const Context> c){
      // HACK to work around experiment.xml. If this Context is a point and
      // its parent is a line, emit a trace point for the line instead.
      if(c->scope().flat().type() == Scope::Type::point) {
        if(c->scope().relation() == Relation::enclosure) {
          if(auto pc = c->direct_parent()) {
            if(pc->scope().flat().type() == Scope::Type::line)
              c = *pc;
          }
        }
      }
      // HACK to work around experiment.xml. If this Context is not a line,
      // emit a trace point for a child line instead, if available.
      else if(c->scope().flat().type() != Scope::Type::line) {
        for(const Context& cc: c->children().citerate()) {
          if(cc.scope().relation() == Relation::enclosure
             && cc.scope().flat().type() == Scope::Type::line) {
            c = cc;
            break;
          }
        }
      }
      return c.get().userdata[src.identifier()];
    });

    hpctrace_fmt_datum_t datum = {
      static_cast<uint64_t>(tm.count()),  // Point in time
      id,  // Point in the CCT
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
}

void HPCTraceDB2::notifyCtxTimepointRewindStart(const Thread& t) {
  auto& ud = t.userdata[uds.thread];
  ud.cursor = ud.buffer.data();
  ud.off = -1;
  ud.tmcntr = 0;
}

void HPCTraceDB2::notifyThreadFinal(const PerThreadTemporary& tt) {
  auto& ud = tt.thread().userdata[uds.thread];
  if(ud.inst) {
    if(ud.cursor != ud.buffer.data())
      ud.inst->writeat(ud.off, ud.cursor - ud.buffer.data(), ud.buffer.data());

    //write the hdr
    auto new_end = ud.hdr.start + ud.tmcntr * timepoint_SIZE;
    assert(new_end <= ud.hdr.end);
    ud.hdr.end = new_end;
    trace_hdr_t hdr = {
      ud.hdr.prof_info_idx, ud.hdr.trace_idx, ud.hdr.start, ud.hdr.end
    };
    assert((hdr.start != (uint64_t)INVALID_HDR) | (hdr.end != (uint64_t)INVALID_HDR));
    std::array<char, trace_hdr_SIZE> buf;
    trace_hdr_swrite(hdr, buf.data());
    ud.inst->writeat((ud.hdr.prof_info_idx - 1) * trace_hdr_SIZE + HPCTRACEDB_FMT_HeaderLen, buf);

    ud.inst = std::nullopt;
  }
}

void HPCTraceDB2::notifyPipeline() noexcept {
  auto& ss = src.structs();
  uds.thread = ss.thread.add<udThread>(std::ref(*this));
  src.registerOrderedWavefront();
}

std::string HPCTraceDB2::exmlTag() {
  if(!has_traces.load(std::memory_order_relaxed)) return "";
  auto [min, max] = src.timepointBounds().value_or(std::make_pair(
      std::chrono::nanoseconds::zero(), std::chrono::nanoseconds::zero()));
  std::ostringstream ss;
  ss << "<TraceDB"
        " i=\"0\""
        " db-min-time=\"" << min.count() << "\""
        " db-max-time=\"" << max.count() << "\""
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
    uint64_t trace_sz = t->attributes.ctxTimepointMaxCount() * timepoint_SIZE;
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
