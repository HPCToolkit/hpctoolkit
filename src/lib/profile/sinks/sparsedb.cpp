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
// Copyright ((c)) 2002-2024, Rice University
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

#include "sparsedb.hpp"

#include "metadb.hpp"

#include "../mpi/all.hpp"
#include "../util/log.hpp"

#include "../../prof-lean/id-tuple.h"
#include "../../prof-lean/formats/profiledb.h"
#include "../../prof-lean/formats/cctdb.h"

#include "../stdshim/numeric.hpp"
#include "../stdshim/filesystem.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <omp.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

using namespace hpctoolkit;
using namespace hpctoolkit::sinks;

static constexpr uint64_t align(uint64_t v, uint8_t a) {
  return (v + a - 1) / a * a;
}

//
// SparseDB common bits
//

namespace hpctoolkit::staticdata {
extern const char formats_md[];
}

SparseDB::SparseDB(stdshim::filesystem::path dir) {
  if(dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);

  pmf = util::File(dir / "profile.db", true);
  cmf = util::File(dir / "cct.db", true);

  // Dump the FORMATS.md file
  try {
    std::ofstream out(dir / "FORMATS.md", std::ios::out | std::ios::ate);
    if(out.tellp() > 0) {
      util::log::warning w;
      w << "Error while writing out FORMATS.md: file exists";
    } else {
      out << staticdata::formats_md;
    }
  } catch(std::exception& ex) {
      util::log::warning w;
      w << "Error while writing out FORMATS.md: " << ex.what();
  }
}

util::WorkshareResult SparseDB::help() {
  return forEachThread.contributeWhileAble()
         + forProfilesParse.contributeWhileAble()
         + forProfilesLoad.contributeWhileAble()
         + forEachContextRange.contributeWhileAble();
}

void SparseDB::notifyPipeline() noexcept {
  src.registerOrderedWavefront();
  src.registerOrderedWrite();
  auto& ss = src.structs();
  ud.context = ss.context.add_default<udContext>();
  ud.thread = ss.thread.add_default<udThread>();
}

static constexpr auto pProfileInfos = align(FMT_PROFILEDB_SZ_FHdr, 8);
static constexpr auto pProfiles = pProfileInfos + align(FMT_PROFILEDB_SZ_ProfInfoSHdr, 8);

void SparseDB::notifyWavefront(DataClass d) noexcept {
  if(!d.hasContexts() || !d.hasThreads()) return;
  auto mpiSem = src.enterOrderedWavefront();

  // Fill contexts with the sorted list of Contexts
  assert(contexts.empty());
  src.contexts().citerate([&](const Context& c){
    contexts.push_back(c);
  }, nullptr);
  std::sort(contexts.begin(), contexts.end(),
    [this](const Context& a, const Context& b) -> bool{
      return a.userdata[src.identifier()] < b.userdata[src.identifier()];
    });

  // Synchronize the profile.db across the ranks
  pmf->synchronize();

  // Count the total number of profiles across all ranks
  size_t myNProf = src.threads().size();
  if(mpi::World::rank() == 0) myNProf++;  // Counting the summary profile
  auto nProf = mpi::allreduce(myNProf, mpi::Op::sum());

  // Start laying out the profile.db file format
  fmt_profiledb_fHdr_t fhdr;
  fmt_profiledb_profInfoSHdr_t pi_sHdr;
  fhdr.pProfileInfos = pProfileInfos;
  pi_sHdr.pProfiles = pProfiles;
  pi_sHdr.nProfiles = nProf;
  fhdr.szProfileInfos = pi_sHdr.pProfiles + pi_sHdr.nProfiles * FMT_PROFILEDB_SZ_ProfInfo
                        - fhdr.pProfileInfos;
  fhdr.pIdTuples = align(fhdr.pProfileInfos + fhdr.szProfileInfos, 8);

  // Write out our part of the id tuples section, and figure out its final size
  {
    std::vector<char> buf;
    // Threads within each block are sorted by identifier. TODO: Remove this
    std::vector<std::reference_wrapper<const Thread>> threads;
    threads.reserve(src.threads().size());
    for(const auto& t: src.threads().citerate()) threads.push_back(*t);
    std::sort(threads.begin(), threads.end(),
      [&](const Thread& a, const Thread& b){
        return a.userdata[src.identifier()] < b.userdata[src.identifier()];
      });

    // Construct id tuples for each Thread in turn
    for(const Thread& t: threads) {
      // Convert and append the id tuple to the end of this here
      const auto oldsz = buf.size();
      buf.resize(oldsz + FMT_PROFILEDB_SZ_IdTuple(t.attributes.idTuple().size()), 0);
      fmt_profiledb_idTupleHdr_t hdr = {
        .nIds = (uint16_t)t.attributes.idTuple().size(),
      };
      fmt_profiledb_idTupleHdr_write(&buf[oldsz], &hdr);

      char* cur = &buf[oldsz + FMT_PROFILEDB_SZ_IdTupleHdr];
      for(const auto& id: t.attributes.idTuple()) {
        fmt_profiledb_idTupleElem_t elem = {
          .kind = (uint8_t)IDTUPLE_GET_KIND(id.kind),
          .isPhysical = IDTUPLE_GET_INTERPRET(id.kind) == IDTUPLE_IDS_BOTH_VALID,
          .logicalId = (uint32_t)id.logical_index,
          .physicalId = id.physical_index,
        };
        fmt_profiledb_idTupleElem_write(cur, &elem);
        cur += FMT_PROFILEDB_SZ_IdTupleElem;
      }

      // Save the local offset in the pointers
      t.userdata[ud].info.pIdTuple = oldsz;
    }

    // Determine the offset of our blob, update the pointers and write it out.
    uint64_t offset = mpi::exscan<uint64_t>(buf.size(), mpi::Op::sum()).value_or(0);
    for(const auto& t: src.threads().citerate())
      t->userdata[ud].info.pIdTuple += fhdr.pIdTuples + offset;
    auto fhi = pmf->open(true, false);
    fhi.writeat(fhdr.pIdTuples + offset, buf.size(), buf.data());

    // Set the section size based on the sizes everyone contributes
    // Rank 0 handles the file header, so only Rank 0 needs to know
    fhdr.szIdTuples = mpi::reduce(buf.size(), 0, mpi::Op::sum());
  }

  // Rank 0 writes out the final file header and section headers
  if(mpi::World::rank() == 0) {
    auto pmfi = pmf->open(true, false);
    {
      char buf[FMT_PROFILEDB_SZ_FHdr];
      fmt_profiledb_fHdr_write(buf, &fhdr);
      pmfi.writeat(0, sizeof buf, buf);
    }
    {
      char buf[FMT_PROFILEDB_SZ_ProfInfoSHdr];
      fmt_profiledb_profInfoSHdr_write(buf, &pi_sHdr);
      pmfi.writeat(fhdr.pProfileInfos, sizeof buf, buf);
    }
  }

  // Set up the double-buffered output for profile data
  profDataOut.initialize(*pmf, fhdr.pIdTuples + fhdr.szIdTuples);

  // Drain the prebuffer and process the waiting Threads
  std::unique_lock<std::shared_mutex> l(prebuffer_lock);
  auto prebuffer_l = std::move(prebuffer);
  prebuffer_done = true;
  l.unlock();

  for(auto& tt: prebuffer_l) process(std::move(tt));
}

void SparseDB::notifyThreadFinal(std::shared_ptr<const PerThreadTemporary> tt) {
  {
    std::shared_lock<std::shared_mutex> l(prebuffer_lock);
    if(prebuffer_done)
      return process(std::move(tt));
  }

  std::unique_lock<std::shared_mutex> l(prebuffer_lock);
  if(prebuffer_done)
    return process(std::move(tt));

  prebuffer.emplace_back(std::move(tt));
}

void SparseDB::process(std::shared_ptr<const PerThreadTemporary> tt) {
  const auto& t = tt->thread();

  // Allocate the blobs needed for the final output
  std::vector<char> mvalsBuf;
  std::vector<char> cidxsBuf;
  cidxsBuf.reserve(contexts.size() * FMT_PROFILEDB_SZ_CIdx);

  // Helper functions to insert ctx_id/idx pairs and metric/value pairs
  const auto addCIdx = [&](const fmt_profiledb_cIdx_t idx) {
    auto oldsz = cidxsBuf.size();
    cidxsBuf.resize(oldsz + FMT_PROFILEDB_SZ_CIdx, 0);
    fmt_profiledb_cIdx_write(&cidxsBuf[oldsz], &idx);
  };
  const auto addMVal = [&](const fmt_profiledb_mVal_t val) {
    auto oldsz = mvalsBuf.size();
    mvalsBuf.resize(oldsz + FMT_PROFILEDB_SZ_MVal, 0);
    fmt_profiledb_mVal_write(&mvalsBuf[oldsz], &val);
  };

  // Now stitch together each Context's results
  for(const Context& c: contexts) {
    if(auto accums = tt->accumulatorsFor(c)) {
      // Add the ctx_id/idx pair for this Context
      addCIdx({
        .ctxId = c.userdata[src.identifier()],
        .startIndex = mvalsBuf.size() / FMT_PROFILEDB_SZ_MVal,
      });
      size_t nValues = 0;

      auto iter = accums->citerate();
      std::vector<std::reference_wrapper<const
          std::pair<const util::reference_index<const Metric>, MetricAccumulator>>> pairs;
      pairs.reserve(accums->size());
      for(const auto& mx: iter) pairs.push_back(std::cref(mx));
      std::sort(pairs.begin(), pairs.end(), [=](const auto& a, const auto& b){
        return a.get().first->userdata[src.identifier()].base()
               < b.get().first->userdata[src.identifier()].base();
      });
      for(const auto& mx: pairs) {
        const Metric& m = mx.get().first;
        const auto& vv = mx.get().second;
        const auto& id = m.userdata[src.identifier()];
        for(MetricScope ms: m.scopes()) {
          if(auto v = vv.get(ms)) {
            addMVal({
              .metricId = (uint16_t)id.getFor(ms),
              .value = *v,
            });
            nValues++;
          }
        }
      }
      c.userdata[ud].nValues.fetch_add(nValues, std::memory_order_relaxed);
    }
  }

  // Build prof_info
  auto& pi = t.userdata[ud].info;
  pi.isSummary = false;
  pi.valueBlock.nValues = mvalsBuf.size() / FMT_PROFILEDB_SZ_MVal;
  pi.valueBlock.nCtxs = cidxsBuf.size() / FMT_PROFILEDB_SZ_CIdx;

  profDataOut.write(std::move(mvalsBuf), pi.valueBlock.pValues,
                    std::move(cidxsBuf), pi.valueBlock.pCtxIndices);
}

SparseDB::DoubleBufferedOutput::DoubleBufferedOutput()
  : pos(mpi::Tag::SparseDB_1) {}

void SparseDB::DoubleBufferedOutput::initialize(util::File& outfile,
                                                uint64_t startOffset) {
  file = outfile;
  // We ensure all blobs are 4-aligned in the final output.
  pos.initialize(align(startOffset, 4));
}

uint64_t SparseDB::DoubleBufferedOutput::allocate(uint64_t size) {
  // We ensure all blobs are 4-aligned in the final output.
  return pos.fetch_add(align(size, 4));
}

void SparseDB::DoubleBufferedOutput::write(
    const std::vector<char>& mvBlob, uint64_t& mvOffset,
    const std::vector<char>& ciBlob, uint64_t& ciOffset) {
  assert(file);

  // Lock up the top-level state and inner Buffer
  std::unique_lock<std::mutex> topl(toplock);
  Buffer& buf = bufs[currentBuf];
  std::unique_lock<std::mutex> lowl(buf.lowlock);

  // Lay out the Buffer first, recording relative offsets to be updated later
  mvOffset = align(buf.blob.size(), 2);
  ciOffset = align(mvOffset + mvBlob.size(), 4);
  buf.toUpdate.push_back(mvOffset);
  buf.toUpdate.push_back(ciOffset);

  // If the Buffer will be full after our addition, we'll have to flush it
  // afterwards. Let other threads progress in this case by rotating buffers.
  bool needsFlush = ciOffset + ciBlob.size() >= Buffer::bufferSize;
  if(needsFlush)
    currentBuf = (currentBuf + 1) % bufs.size();
  topl.unlock();  // Let other threads progress in the other Buffer now

  // Append our new bytes to our Buffer
  buf.blob.resize(mvOffset, 0);
  buf.blob.insert(buf.blob.end(), mvBlob.begin(), mvBlob.end());
  buf.blob.resize(ciOffset, 0);
  buf.blob.insert(buf.blob.end(), ciBlob.begin(), ciBlob.end());

  if(needsFlush) buf.flush(*file, allocate(buf.blob.size()));
}

void SparseDB::DoubleBufferedOutput::Buffer::flush(util::File& file,
                                                   uint64_t offset) {
  if(!blob.empty())
    file.open(true, true).writeat(offset, blob.size(), blob.data());

  // Update the saved offsets with the final answers
  for(uint64_t& target: toUpdate) target += offset;

  // Reset this Buffer for the next time around
  blob.clear();
  blob.reserve(bufferSize);
  toUpdate.clear();
}

void SparseDB::DoubleBufferedOutput::flush() {
  assert(file);
  for(Buffer& buf: bufs) {
    std::unique_lock<std::mutex> l(buf.lowlock);
    buf.flush(*file, allocate(buf.blob.size()));
  }
}


// Read all the ctx_id/idx pairs for a profile from the profile.db
static std::vector<std::pair<uint32_t, uint64_t>> readProfileCtxPairs(
    const util::File& pmf, const fmt_profiledb_profInfo_t& pi) {
  if(pi.valueBlock.nCtxs == 0) {
    // No data in this profile, skip
    return {};
  }

  // Read the whole chunk of ctx_id/idx pairs
  auto pmfi = pmf.open(false, false);
  std::vector<char> buf(pi.valueBlock.nCtxs * FMT_PROFILEDB_SZ_CIdx);
  pmfi.readat(pi.valueBlock.pCtxIndices, buf.size(), buf.data());

  // Parse and save in the output
  std::vector<std::pair<uint32_t, uint64_t>> prof_ctx_pairs;
  prof_ctx_pairs.reserve(pi.valueBlock.nCtxs + 1);
  for(uint32_t i = 0; i < pi.valueBlock.nCtxs; i++) {
    fmt_profiledb_cIdx_t idx;
    fmt_profiledb_cIdx_read(&idx, &buf[i * FMT_PROFILEDB_SZ_CIdx]);
    prof_ctx_pairs.emplace_back(idx.ctxId, idx.startIndex);
  }
  prof_ctx_pairs.push_back({std::numeric_limits<uint32_t>::max(), pi.valueBlock.nValues});
  return prof_ctx_pairs;
}

namespace {
// Helper structure for the indices loaded for a profile
struct ProfileIndexData {
  // Offset of the data block for this profile in the file
  uint64_t offset;
  // Absolute index of this profile
  uint32_t index;
  // Preparsed ctx_id/idx pairs
  std::vector<std::pair<uint32_t, uint64_t>> ctxPairs;
};

// Helper structure for the metric data loaded for a profile, which is always
// limited by a context range
struct ProfileMetricData {
  // First loaded ctx_id/idx pair
  std::vector<std::pair<uint32_t,uint64_t>>::const_iterator first;
  // Last (one-after-end) loaded ctx_id/idx pair
  std::vector<std::pair<uint32_t,uint64_t>>::const_iterator last;
  // Absolute index of the profile
  uint32_t index;
  // Loaded metric/value blob for the profile, in the [first, last) ctx range
  std::vector<char> mvBlob;

  ProfileMetricData() = default;

  ProfileMetricData(uint32_t firstCtx, uint32_t lastCtx, const util::File& pmf,
      uint64_t offset, uint32_t index,
      const std::vector<std::pair<uint32_t, uint64_t>>& ctxPairs);
};
}

// Load a profile's metric data from the given File and data
ProfileMetricData::ProfileMetricData(uint32_t firstCtx, uint32_t lastCtx,
    const util::File& pmf, const uint64_t offset, const uint32_t index,
    const std::vector<std::pair<uint32_t, uint64_t>>& ctxPairs)
  : first(ctxPairs.begin()), last(ctxPairs.begin()), index(index) {
  if(ctxPairs.size() <= 1 || firstCtx >= lastCtx) {
    // Empty range, we don't have any data to add.
    return;
  }

  // Compare a range of ctx_ids [first, second) to a ctx_id/idx pair.
  // ctx_ids within [first, second) compare "equal".
  using cipair = std::pair<uint32_t, uint64_t>;
  using range = std::pair<uint32_t, uint32_t>;
  static_assert(!std::is_same_v<cipair, range>, "ADL will fail here!");
  struct compare {
    bool operator()(const cipair& a, const range& b) {
      return a.first < b.first;
    }
    bool operator()(const range& a, const cipair& b) {
      return a.second <= b.first;
    }
  };

  // Binary search for the [first, last) range among this profile's pairs.
  // Skip the last pair, which is always LastNodeEnd.
  auto ctxRange = std::equal_range(ctxPairs.begin(), --ctxPairs.end(),
      range(firstCtx, lastCtx), compare{});
  first = ctxRange.first;
  last = ctxRange.second;
  if(first == last) {
    // Still an empty range, we don't have any data to add.
    return;
  }

  // Read the blob of data containing all our pairs
  mvBlob.resize((last->second - first->second) * FMT_PROFILEDB_SZ_MVal);
  assert(!mvBlob.empty());

  auto pmfi = pmf.open(false, false);
  pmfi.readat(offset + first->second * FMT_PROFILEDB_SZ_MVal,
              mvBlob.size(), mvBlob.data());
}

// Transpose and write the metric data for a range of contexts
static void writeContexts(uint32_t firstCtx, uint32_t lastCtx,
    const util::File& cmf,
    const std::deque<ProfileMetricData>& metricData,
    const std::vector<uint64_t>& ctxOffsets) {
  // Set up a heap with cursors into each profile's data blob
  std::vector<std::pair<
    std::vector<std::pair<uint32_t, uint64_t>>::const_iterator,  // ctx_id/idx pair in a profile
    std::reference_wrapper<const ProfileMetricData>  // Profile to get data from
  >> heap;
  const auto heap_comp = [](const auto& a, const auto& b){
    // The "largest" heap entry has the smallest ctx id or profile index
    return a.first->first != b.first->first ? a.first->first > b.first->first
        : a.second.get().index > b.second.get().index;
  };
  heap.reserve(metricData.size());
  for(const auto& profile: metricData) {
    if(profile.first == profile.last)
      continue;  // Profile is empty, skip

    auto start = std::lower_bound(profile.first, profile.last,
      std::make_pair(firstCtx, 0), [](const auto& a, const auto& b){
        return a.first < b.first;
      });
    if(start->first >= lastCtx) continue;  // Profile has no data for us, skip

    heap.push_back({start, profile});
  }
  heap.shrink_to_fit();
  std::make_heap(heap.begin(), heap.end(), heap_comp);
  if(heap.empty()) return;  // No data for us!

  // Start copying data over, one context at a time. The heap efficiently sorts
  // our search so we can jump straight to the next context we want.
  const auto firstCtxId = heap.front().first->first;
  std::vector<char> buf;
  while(!heap.empty() && heap.front().first->first < lastCtx) {
    const uint32_t ctx_id = heap.front().first->first;
    std::map<uint16_t, std::vector<char>> valuebufs;
    uint64_t allpvs = 0;

    // Pull the data out for one context and save it to cmb
    while(!heap.empty() && heap.front().first->first == ctx_id) {
      // Pop the top entry from the heap, and increment it
      std::pop_heap(heap.begin(), heap.end(), heap_comp);
      const auto& curPair = *(heap.back().first++);

      // Fill cmb with metric/value pairs for this context, from the top profile
      const ProfileMetricData& profile = heap.back().second;
      const char* cur = &profile.mvBlob[(curPair.second - profile.first->second) * FMT_PROFILEDB_SZ_MVal];
      for(uint64_t i = 0, e = heap.back().first->second - curPair.second;
          i < e; i++, cur += FMT_PROFILEDB_SZ_MVal) {
        allpvs++;
        fmt_profiledb_mVal_t val;
        fmt_profiledb_mVal_read(&val, cur);
        auto& subbuf = valuebufs.try_emplace(val.metricId).first->second;
        fmt_cctdb_pVal_t outval = {
          .profIndex = profile.index,
          .value = val.value,
        };

        // Write in this prof_idx/value pair, in bytes.
        auto oldsz = subbuf.size();
        subbuf.resize(oldsz + FMT_CCTDB_SZ_PVal, 0);
        fmt_cctdb_pVal_write(&subbuf[oldsz], &outval);
      }

      // If the updated entry is still in range, push it back into the heap.
      // Otherwise pop the entry from the vector completely.
      if(heap.back().first->first < lastCtx)
        std::push_heap(heap.begin(), heap.end(), heap_comp);
      else
        heap.pop_back();
    }

    // Allocate enough space in buf for all the bits we want.
    buf.resize(align(buf.size(), 4));
    assert(align(ctxOffsets[ctx_id], 4) == ctxOffsets[ctx_id]
           && "Final layout is not sufficiently aligned!");
    const auto newsz = allpvs * FMT_CCTDB_SZ_PVal + valuebufs.size() * FMT_CCTDB_SZ_MIdx;
    assert(align(ctxOffsets[ctx_id] + newsz, 4) == ctxOffsets[ctx_id+1]
           && "Final layout doesn't match precalculated ctx_off!");
    buf.reserve(buf.size() + newsz);

    // Concatenate the prof_idx/value pairs, in bytes form, in metric order
    for(const auto& [mid, pvbuf]: valuebufs)
      buf.insert(buf.end(), pvbuf.begin(), pvbuf.end());

    // Construct the metric_id/idx pairs for this context, in bytes
    {
      auto oldsz = buf.size();
      buf.resize(oldsz + valuebufs.size() * FMT_CCTDB_SZ_MIdx);
      char* cur = &buf[oldsz];
      uint64_t pvs = 0;
      for(const auto& [mid, pvbuf]: valuebufs) {
        fmt_cctdb_mIdx_t idx = {
          .metricId = mid,
          .startIndex = pvs,
        };
        fmt_cctdb_mIdx_write(cur, &idx);
        cur += FMT_CCTDB_SZ_MIdx;
        pvs += pvbuf.size() / FMT_CCTDB_SZ_PVal;
      }
      assert(pvs == allpvs);
    }
  }

  // Write out the whole blob of data where it belongs in the file
  if(buf.empty()) return;
  auto cmfi = cmf.open(true, true);
  cmfi.writeat(ctxOffsets[firstCtxId], buf.size(), buf.data());
}

void SparseDB::write() {
  auto mpiSem = src.enterOrderedWrite();

  // Make sure all the profile data is on disk and all offsets are updated
  profDataOut.flush();
  mpi::barrier();

  // Now that the profile infos are up-to-date, write them all in parallel
  {
    std::vector<std::reference_wrapper<const Thread>> threads;
    threads.reserve(src.threads().size());
    for(const auto& t: src.threads().citerate())
      threads.push_back(*t);

    forEachThread.fill(std::move(threads), [&](const Thread& t){
      auto& pi = t.userdata[ud].info;
      const auto idx = t.userdata[src.identifier()] + 1;

      char buf[FMT_PROFILEDB_SZ_ProfInfo];
      fmt_profiledb_profInfo_write(buf, &pi);

      pmf->open(true, false).writeat(pProfiles + FMT_PROFILEDB_SZ_ProfInfo * idx,
                                     sizeof buf, buf);
    });
    forEachThread.contributeUntilComplete();
  }

  // Lay out the main parts of the cct.db file
  fmt_cctdb_fHdr_t fHdr;
  fHdr.pCtxInfo = align(FMT_CCTDB_SZ_FHdr, 8);
  fmt_cctdb_ctxInfoSHdr_t ci_sHdr;
  ci_sHdr.pCtxs = align(fHdr.pCtxInfo + FMT_CCTDB_SZ_CtxInfoSHdr, 8);
  ci_sHdr.nCtxs = mpi::bcast((contexts.back().get().userdata[src.identifier()] + 1), 0);
  fHdr.szCtxInfo = ci_sHdr.pCtxs + ci_sHdr.nCtxs * FMT_CCTDB_SZ_CtxInfo - fHdr.pCtxInfo;

  // Lay out the cct.db metric data, in terms of offsets for every context
  // First figure out the byte counts for each potential context's blob
  std::vector<uint64_t> ctxOffsets(ci_sHdr.nCtxs + 1, 0);
  for(const Context& c: contexts) {
    const auto i = c.userdata[src.identifier()];
    auto& udc = c.userdata[ud];
    ctxOffsets[i] = udc.nValues.load(std::memory_order_relaxed) * FMT_CCTDB_SZ_PVal;

    // Rank 0 has the final number of metric/idx pairs
    if(mpi::World::rank() == 0) {
      const auto& use = c.data().metricUsage();
      auto iter = use.citerate();
      udc.nMetrics = std::accumulate(iter.begin(), iter.end(), (uint16_t)0,
        [](uint16_t out, const auto& mu) -> uint16_t {
          MetricScopeSet use = mu.second;
          assert((mu.first->scopes() & use) == use && "Inconsistent Metric value usage data!");
          return out + use.count();
        });
      ctxOffsets[i] += udc.nMetrics * FMT_CCTDB_SZ_MIdx;
    }
  }
  // All-reduce to get the total size for every context
  ctxOffsets = mpi::allreduce(ctxOffsets, mpi::Op::sum());
  // Exclusive-scan the sizes to get the offsets, adjusting for 4-alignment
  const auto ctxStart = align(fHdr.pCtxInfo + fHdr.szCtxInfo, 4);
  stdshim::transform_exclusive_scan(ctxOffsets.begin(), ctxOffsets.end(), ctxOffsets.begin(),
    ctxStart, std::plus<>{},
    [](uint64_t sz){ return align(sz, 4); });

  // Divide the contexts into ranges of easily distributable sizes
  std::vector<uint32_t> ctxRanges;
  {
    ctxRanges.push_back(0);
    const uint64_t limit = std::min<uint64_t>(1024ULL*1024*1024*3,
        (ctxOffsets.back() - ctxStart) / (3 * mpi::World::size()));
    uint64_t cursize = 0;
    for(size_t i = 0; i < ci_sHdr.nCtxs; i++) {
      const uint64_t size = ctxOffsets[i+1] - ctxOffsets[i];
      if(cursize + size > limit) {
        ctxRanges.push_back(i);
        cursize = 0;
      }
      cursize += size;
    }
    ctxRanges.push_back(ci_sHdr.nCtxs);
  }

  // We use a SharedAccumulator to dynamically distribute context ranges across
  // the ranks. All ranks other than rank 0 get a pre-allocated context range
  mpi::SharedAccumulator ctxRangeCounter(mpi::Tag::SparseDB_2);
  ctxRangeCounter.initialize(mpi::World::size() - 1);

  // Synchronize cct.db across the ranks
  cmf->synchronize();

  // Read and parse the Profile Info section of the final profile.db
  std::deque<ProfileIndexData> profiles;
  {
    auto fi = pmf->open(false, false);
    fmt_profiledb_profInfoSHdr_t piSHdr;
    {
      char buf[FMT_PROFILEDB_SZ_ProfInfoSHdr];
      fi.readat(pProfileInfos, sizeof buf, buf);
      fmt_profiledb_profInfoSHdr_read(&piSHdr, buf);
    }

    // Read the whole section in
    std::vector<char> buf(piSHdr.nProfiles * FMT_PROFILEDB_SZ_ProfInfo);
    fi.readat(piSHdr.pProfiles, buf.size(), buf.data());

    // Load the data we need from the profile.db, in parallel
    profiles = std::deque<ProfileIndexData>(piSHdr.nProfiles);
    std::atomic<size_t> next(0);
    forProfilesParse.fill(profiles.size(), [this, &buf, &profiles, &next](size_t i){
      fmt_profiledb_profInfo_t pi;
      fmt_profiledb_profInfo_read(&pi, &buf[i * FMT_PROFILEDB_SZ_ProfInfo]);
      if(pi.isSummary)
        return;
      profiles[next.fetch_add(1, std::memory_order_relaxed)] = {
        .offset = pi.valueBlock.pValues,
        .index = (uint32_t)i,
        .ctxPairs = readProfileCtxPairs(*pmf, pi),
      };
    });
    forProfilesParse.contributeUntilComplete();
    profiles.resize(next.load(std::memory_order_relaxed));
  }

  if(mpi::World::rank() == 0) {
    // Rank 0 is in charge of writing out the summary profile in profile.db
    {
      // Allocate the blobs needed for the final output
      std::vector<char> mvalsBuf;
      std::vector<char> cidxsBuf;
      cidxsBuf.reserve(contexts.size() * FMT_PROFILEDB_SZ_CIdx);

      // Helper functions to insert ctx_id/idx pairs and metric/value pairs
      const auto addCIdx = [&](const fmt_profiledb_cIdx_t idx) {
        auto oldsz = cidxsBuf.size();
        cidxsBuf.resize(oldsz + FMT_PROFILEDB_SZ_CIdx, 0);
        fmt_profiledb_cIdx_write(&cidxsBuf[oldsz], &idx);
      };
      const auto addMVal = [&](const fmt_profiledb_mVal_t val) {
        auto oldsz = mvalsBuf.size();
        mvalsBuf.resize(oldsz + FMT_PROFILEDB_SZ_MVal, 0);
        fmt_profiledb_mVal_write(&mvalsBuf[oldsz], &val);
      };

      // Now stitch together each Context's results
      for(const Context& c: contexts) {
        const auto& stats = c.data().statistics();
        if(stats.size() > 0) {
          addCIdx({
            .ctxId = c.userdata[src.identifier()],
            .startIndex = mvalsBuf.size() / FMT_PROFILEDB_SZ_MVal,
          });
        }

        auto iter = stats.citerate();
        std::vector<std::reference_wrapper<const
            std::pair<const util::reference_index<const Metric>, StatisticAccumulator>>> pairs;
        pairs.reserve(stats.size());
        for(const auto& mx: iter) pairs.push_back(std::cref(mx));
        std::sort(pairs.begin(), pairs.end(), [=](const auto& a, const auto& b){
          return a.get().first->userdata[src.identifier()].base()
                 < b.get().first->userdata[src.identifier()].base();
        });
        for(const auto& mx: pairs) {
          const Metric& m = mx.get().first;
          const auto& id = m.userdata[src.identifier()];
          const auto& vv = mx.get().second;
          for(const auto& sp: m.partials()) {
            auto vvv = vv.get(sp);
            for(MetricScope ms: m.scopes()) {
              if(auto v = vvv.get(ms)) {
                addMVal({
                  .metricId = (uint16_t)id.getFor(sp, ms),
                  .value = *v,
                });
              }
            }
          }
        }
      }

      // Build prof_info
      fmt_profiledb_profInfo_t summary_info;
      summary_info.isSummary = true;
      summary_info.pIdTuple = 0;
      summary_info.valueBlock.nValues = mvalsBuf.size() / FMT_PROFILEDB_SZ_MVal;
      summary_info.valueBlock.nCtxs = cidxsBuf.size() / FMT_PROFILEDB_SZ_CIdx;

      // Write the summary profile out and make sure it makes it to disk
      profDataOut.write(std::move(mvalsBuf), summary_info.valueBlock.pValues,
                        std::move(cidxsBuf), summary_info.valueBlock.pCtxIndices);
      profDataOut.flush();

      // Write the summary info, which is always profile index 0
      auto pmfi = pmf->open(true, false);
      {
        char buf[FMT_PROFILEDB_SZ_ProfInfo];
        fmt_profiledb_profInfo_write(buf, &summary_info);
        pmfi.writeat(pProfiles, sizeof buf, buf);
      }

      // Write out the footer to indicate that profile.db is complete
      pmfi.writeat(profDataOut.allocate(sizeof fmt_profiledb_footer),
                   sizeof fmt_profiledb_footer, fmt_profiledb_footer);
    }

    // Rank 0 is also in charge of writing the header and context info sections
    // in the cct.db
    {
      auto cmfi = cmf->open(true, true);

      {
        char buf[FMT_CCTDB_SZ_FHdr];
        fmt_cctdb_fHdr_write(buf, &fHdr);
        cmfi.writeat(0, sizeof buf, buf);
      }
      {
        char buf[FMT_CCTDB_SZ_CtxInfoSHdr];
        fmt_cctdb_ctxInfoSHdr_write(buf, &ci_sHdr);
        cmfi.writeat(fHdr.pCtxInfo, sizeof buf, buf);
      }

      std::vector<char> buf(ci_sHdr.nCtxs * FMT_CCTDB_SZ_CtxInfo);
      char* cur = buf.data();
      unsigned int ctxid = 0;
      for(const Context& c: contexts) {
        const auto i = c.userdata[src.identifier()];

        // write context info for all the never-exist contexts before context i
        while(ctxid < i){
          fmt_cctdb_ctxInfo_t ci;
          ci.valueBlock.nMetrics = 0;
          ci.valueBlock.nValues = 0;
          ci.valueBlock.pValues = ctxOffsets[ctxid];
          ci.valueBlock.pMetricIndices = ci.valueBlock.pValues;
          fmt_cctdb_ctxInfo_write(cur, &ci);
          cur += FMT_CCTDB_SZ_CtxInfo;
          ctxid++;
        }

        // write context info for context i
        fmt_cctdb_ctxInfo_t cii;
        cii.valueBlock.nMetrics = c.userdata[ud].nMetrics;
        cii.valueBlock.nValues = (ctxOffsets[i+1] - ctxOffsets[i] - cii.valueBlock.nMetrics * FMT_CCTDB_SZ_MIdx) / FMT_CCTDB_SZ_PVal;
        cii.valueBlock.pValues = ctxOffsets[i];
        cii.valueBlock.pMetricIndices = cii.valueBlock.pValues + cii.valueBlock.nValues * FMT_CCTDB_SZ_PVal;
        fmt_cctdb_ctxInfo_write(cur, &cii);
        cur += FMT_CCTDB_SZ_CtxInfo;
        ctxid++;
      }
      cmfi.writeat(ci_sHdr.pCtxs, buf.size(), buf.data());
    }
  }

  // Transpose and copy context data until we're done
  uint32_t idx = mpi::World::rank() > 0 ? mpi::World::rank() - 1  // Pre-allocation
                                        : ctxRangeCounter.fetch_add(1);
  while(idx < ctxRanges.size() - 1) {
    // Process the next range of contexts allocated to us
    auto firstCtx = ctxRanges[idx];
    auto lastCtx = ctxRanges[idx + 1];
    if(firstCtx < lastCtx) {
      // Read the blob of data we need from each profile, in parallel
      std::deque<ProfileMetricData> metricData(profiles.size());
      forProfilesLoad.fill(metricData.size(),
        [this, &metricData, firstCtx, lastCtx, &profiles](size_t i){
          const auto& p = profiles[i];
          metricData[i] = {firstCtx, lastCtx, *pmf, p.offset, p.index, p.ctxPairs};
        });
      forProfilesLoad.contributeUntilEmpty();

      // Divide up this ctx group into ranges suitable for distributing to threads.
      std::vector<std::pair<uint32_t, uint32_t>> ctxRanges;
      {
        ctxRanges.reserve(src.teamSize());
        const size_t target = (ctxOffsets[lastCtx] - ctxOffsets[firstCtx])
                              / src.teamSize();
        size_t cursize = 0;
        for(uint32_t id = firstCtx; id < lastCtx; id++) {
          // Stop allocating once we have nearly enough ranges
          if(ctxRanges.size() + 1 == src.teamSize()) break;

          cursize += ctxOffsets[id+1] - ctxOffsets[id];
          if(cursize > target) {
            ctxRanges.push_back({!ctxRanges.empty() ? ctxRanges.back().second
                                                    : firstCtx, id+1});
            cursize = 0;
          }
        }
        if(ctxRanges.empty() || ctxRanges.back().second < lastCtx) {
          // Last range takes whatever remains
          ctxRanges.push_back({!ctxRanges.empty() ? ctxRanges.back().second
                                                  : firstCtx, lastCtx});
        }
      }

      // Handle the individual ctx copies
      forEachContextRange.fill(std::move(ctxRanges),
        [this, &metricData, &ctxOffsets](const auto& range){
          writeContexts(range.first, range.second, *cmf, metricData, ctxOffsets);
        });
      forEachContextRange.contributeUntilEmpty();
    }

    // Fetch the next available workitem from the group
    idx = ctxRangeCounter.fetch_add(1);
  }

  // Notify our helper threads that the workshares are complete now
  forProfilesLoad.complete();
  forEachContextRange.complete();

  // The last rank is in charge of writing the final footer, AFTER all other
  // writes have completed. If the footer isn't there, the file isn't complete.
  mpi::barrier();
  if(mpi::World::rank() + 1 == mpi::World::size()) {
    cmf->open(true, false).writeat(ctxOffsets.back(),
                                   sizeof fmt_cctdb_footer, fmt_cctdb_footer);
  }
}
