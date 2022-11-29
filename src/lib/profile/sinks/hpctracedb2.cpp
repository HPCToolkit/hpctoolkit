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

#include "metadb.hpp"

#include "../util/log.hpp"
#include "../util/cache.hpp"
#include "../mpi/all.hpp"

#include "lib/prof-lean/formats/tracedb.h"

#include <iomanip>
#include <sstream>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <unistd.h>

using namespace hpctoolkit;
using namespace sinks;

static constexpr uint64_t align(uint64_t v, uint8_t a) {
  return (v + a - 1) / a * a;
}

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

static constexpr uint64_t pCtxTraces = align(FMT_TRACEDB_SZ_FHdr, 8);
static constexpr uint64_t ctx_pTraces = align(pCtxTraces + FMT_TRACEDB_SZ_CtxTraceSHdr, 8);

void HPCTraceDB2::notifyWavefront(DataClass d){
  if(!d.hasThreads()) return;

  util::File::Instance traceinst;
  {
    auto mpiSem = src.enterOrderedWavefront();
    if(tracefile)
      traceinst = tracefile->open(true, true);

    // Determine the total number of Threads
    totalNumTraces = mpi::allreduce<uint32_t>(src.threads().size(), mpi::Op::sum());

    // Determine the total number of Threads with any timepoints
    uint32_t myRealTraces = 0;
    for(const auto& t: src.threads().citerate()) {
      if(t->attributes.ctxTimepointMaxCount() > 0)
        myRealTraces += 1;
    }
    has_traces = mpi::allreduce<uint32_t>(myRealTraces, mpi::Op::sum()) > 0;

    //calculate the offsets for later stored in start and end
    //assign the values of the hdrs
    assignHdrs(calcStartEnd());
  }

  // Update all the Threads, if we have data for them already
  for(const auto& t : src.threads().citerate()) {
    auto& ud = t->userdata[uds.thread];

    // Drain the prebuffer. We extract the prebuffer first to minimize the
    // critical section and allow notifyTimepoints to continue.
    std::unique_lock<std::shared_mutex> l(ud.prebuffer_lock);
    auto prebuffer = std::move(ud.prebuffer);
    ud.prebuffer_done = true;
    l.unlock();

    traceinst.writeat(ud.hdr.start, prebuffer);
    if(ud.hdr_prebuffered)
      writeHdrFor(ud, traceinst);
  }
}

void HPCTraceDB2::notifyThread(const Thread& t) {
  (void)t.userdata[uds.thread];
}

void HPCTraceDB2::notifyTimepoints(const Thread& t, const std::vector<
    std::pair<std::chrono::nanoseconds, std::reference_wrapper<const Context>>>& tps) {
  assert(!tps.empty());

  auto& ud = t.userdata[uds.thread];
  if(!ud.has_trace) {
    ud.has_trace = true;
    if(tracefile) ud.inst = tracefile->open(true, true);
  }

  // If we're getting timepoints before the Threads wavefront, we don't know
  // where we need to write yet. So buffer in the "prebuffer" until we know.
  std::unique_lock<std::shared_mutex> l;
  char* prebuffer_cursor = nullptr;
  {
    bool done;
    {
      std::shared_lock<std::shared_mutex> l(ud.prebuffer_lock);
      done = ud.prebuffer_done;
    }
    if(!done) {
      l = std::unique_lock<std::shared_mutex>(ud.prebuffer_lock);
      if(ud.prebuffer_done) {
        l.unlock();
      } else {
        auto oldsz = ud.prebuffer.size();
        ud.prebuffer.resize(oldsz + tps.size() * FMT_TRACEDB_SZ_CtxSample);
        prebuffer_cursor = &ud.prebuffer[oldsz];
      }
    }
  }

  util::linear_lru_cache<util::reference_index<const Context>, unsigned int,
                         2> cache;

  for(const auto& [tm, cr]: tps) {
    const Context& c = cr;

    // Never repeat "blank" samples
    const bool isBlank = c.scope().flat().type() == Scope::Type::global;
    if(ud.lastWasBlank && isBlank) continue;
    ud.lastWasBlank = isBlank;

    // Try to cache our work as much as possible
    auto id = cache.lookup(c, [&](util::reference_index<const Context> c){
      if(MetaDB::elide(c))
        c = *c->direct_parent();
      return c->userdata[src.identifier()];
    });

    fmt_tracedb_ctxSample_t datum = {
      .timestamp = static_cast<uint64_t>(tm.count()),
      .ctxId = id,
    };
    if(ud.inst) {
      if(prebuffer_cursor != nullptr) {
        fmt_tracedb_ctxSample_write(prebuffer_cursor, &datum);
        prebuffer_cursor += FMT_TRACEDB_SZ_CtxSample;
      } else {
        if(ud.cursor == ud.buffer.data())
          ud.off = ud.hdr.start + ud.tmcntr * FMT_TRACEDB_SZ_CtxSample;
        assert(ud.hdr.start + ud.tmcntr * FMT_TRACEDB_SZ_CtxSample < ud.hdr.end);
        fmt_tracedb_ctxSample_write(ud.cursor, &datum);
        ud.cursor += FMT_TRACEDB_SZ_CtxSample;
        if(ud.cursor == &ud.buffer[ud.buffer.size()]) {
          ud.inst->writeat(ud.off, ud.buffer);
          ud.cursor = ud.buffer.data();
        }
      }
      ud.tmcntr++;
    }
  }

  // Trim the prebuffer back down to the cursor value
  if(prebuffer_cursor != nullptr)
    ud.prebuffer.resize(std::distance(ud.prebuffer.data(), prebuffer_cursor));
}

void HPCTraceDB2::notifyCtxTimepointRewindStart(const Thread& t) {
  auto& ud = t.userdata[uds.thread];
  ud.cursor = ud.buffer.data();
  ud.off = -1;
  ud.tmcntr = 0;

  std::unique_lock<std::shared_mutex> l(ud.prebuffer_lock);
  if(!ud.prebuffer_done)
    ud.prebuffer.clear();
}

void HPCTraceDB2::notifyThreadFinal(std::shared_ptr<const PerThreadTemporary> tt) {
  auto& ud = tt->thread().userdata[uds.thread];
  util::File::Instance inst;
  if(ud.inst) {
    inst = std::move(ud.inst.value());
  } else if(tracefile) {
    inst = tracefile->open(true, true);
  }

  // Write any timepoints that are still in the buffer
  //
  // NB: If the timepoints were prebuffered, they are in prebuffer instead of
  // buffer, so this condition evaluates false.
  if(ud.cursor != ud.buffer.data())
    inst.writeat(ud.off, ud.cursor - ud.buffer.data(), ud.buffer.data());

  // Check if the prebuffer is done. If it isn't, defer the header write until then
  {
    bool prebuffer_done;
    {
      std::shared_lock<std::shared_mutex> l(ud.prebuffer_lock);
      prebuffer_done = ud.prebuffer_done;
    }
    if(!prebuffer_done) {
      std::unique_lock<std::shared_mutex> l(ud.prebuffer_lock);
      if(!ud.prebuffer_done) {
        ud.hdr_prebuffered = true;
        return;
      }
    }
  }

  // Write the trace header for this thread
  writeHdrFor(ud, inst);
}

void HPCTraceDB2::writeHdrFor(udThread& ud, util::File::Instance& inst) {
  auto new_end = ud.hdr.start + ud.tmcntr * FMT_TRACEDB_SZ_CtxSample;
  assert(new_end <= ud.hdr.end);
  ud.hdr.end = new_end;
  fmt_tracedb_ctxTrace_t hdr = {
    .profIndex = ud.hdr.prof_info_idx,
    .pStart = ud.hdr.start,
    .pEnd = ud.hdr.end,
  };
  assert((hdr.pStart != (uint64_t)INVALID_HDR) | (hdr.pEnd != (uint64_t)INVALID_HDR));
  char buf[FMT_TRACEDB_SZ_CtxTrace];
  fmt_tracedb_ctxTrace_write(buf, &hdr);
  inst.writeat(ctx_pTraces + (ud.hdr.prof_info_idx - 1) * FMT_TRACEDB_SZ_CtxTrace,
               sizeof buf, buf);
}

void HPCTraceDB2::notifyPipeline() noexcept {
  auto& ss = src.structs();
  uds.thread = ss.thread.add<udThread>(std::ref(*this));
  src.registerOrderedWavefront();

  if(tracefile)
    tracefile->synchronize();
}

void HPCTraceDB2::write() {
  if(!tracefile) return;

  // If there are no traces, delete the trace.db file outright, and do nothing more.
  if(!has_traces) {
    if(mpi::World::rank() == 0)
      tracefile->remove();
    return;
  }

  auto traceinst = tracefile->open(true, true);
  if(mpi::World::rank() + 1 == mpi::World::size())
    traceinst.writeat(footerPos, sizeof fmt_tracedb_footer, fmt_tracedb_footer);
  if(mpi::World::rank() != 0) return;

  auto [min, max] = src.timepointBounds().value_or(std::make_pair(
      std::chrono::nanoseconds::zero(), std::chrono::nanoseconds::zero()));

  // Write out the static headers
  {
    fmt_tracedb_fHdr_t fhdr = {
      .szCtxTraces = ctx_pTraces + totalNumTraces * FMT_TRACEDB_SZ_CtxTrace - pCtxTraces,
      .pCtxTraces = pCtxTraces,
    };
    char buf[FMT_TRACEDB_SZ_FHdr];
    fmt_tracedb_fHdr_write(buf, &fhdr);
    traceinst.writeat(0, sizeof buf, buf);
  }
  {
    fmt_tracedb_ctxTraceSHdr_t shdr = {
      .pTraces = ctx_pTraces,
      .nTraces = (uint32_t)totalNumTraces,
      .szTrace = 0,
      .minTimestamp = (uint64_t)min.count(), .maxTimestamp = (uint64_t)max.count(),
    };
    char buf[FMT_TRACEDB_SZ_CtxTraceSHdr];
    fmt_tracedb_ctxTraceSHdr_write(buf, &shdr);
    traceinst.writeat(pCtxTraces, sizeof buf, buf);
  }

}


//***************************************************************************
// trace_hdr
//***************************************************************************
HPCTraceDB2::traceHdr::traceHdr(const Thread& t, HPCTraceDB2& tdb)
  : prof_info_idx(t.userdata[tdb.src.identifier()] + 1),
   start(INVALID_HDR), end(INVALID_HDR) {}

std::vector<uint64_t> HPCTraceDB2::calcStartEnd() {
  //get the size of all traces
  std::vector<uint64_t> trace_sizes;
  uint64_t total_size = 0;
  for(const auto& t : src.threads().iterate()){
    uint64_t trace_sz = align(t->attributes.ctxTimepointMaxCount() * FMT_TRACEDB_SZ_CtxSample, 8);
    trace_sizes.emplace_back(trace_sz);
    total_size += trace_sz;
  }

  //get the offset of this rank's traces section
  uint64_t my_off = mpi::exscan(total_size, mpi::Op::sum()).value_or(0);
  my_off += align(ctx_pTraces + totalNumTraces * FMT_TRACEDB_SZ_CtxTrace, 8);

  //get the individual offsets of this rank's traces
  std::vector<uint64_t> trace_offs(trace_sizes.size() + 1);
  trace_sizes.emplace_back(0);
  exscan<uint64_t>(trace_sizes);
  for(unsigned int i = 0; i < trace_sizes.size();i++){
    trace_offs[i] = trace_sizes[i] + my_off;
  }

  return trace_offs;

}

void HPCTraceDB2::assignHdrs(const std::vector<uint64_t>& trace_offs) {
  int i = 0;
  for(const auto& t : src.threads().iterate()){
    auto& hdr = t->userdata[uds.thread].hdr;
    hdr.start = trace_offs[i];
    hdr.end = trace_offs[i] + t->attributes.ctxTimepointMaxCount() * FMT_TRACEDB_SZ_CtxSample;
    i++;
  }
  footerPos = trace_offs.back();
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
