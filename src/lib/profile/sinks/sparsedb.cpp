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

#include "sparsedb.hpp"

#include "../mpi/all.hpp"
#include "../util/log.hpp"

#include <lib/prof/cms-format.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/id-tuple.h>
#include <lib/prof/pms-format.h>

#include "../stdshim/numeric.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <omp.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

#include "lib/profile/sinks/FORMATS.md.inc"

using namespace hpctoolkit;
using namespace hpctoolkit::sinks;

static uint64_t align(uint64_t v, uint8_t a) {
  return (v + a - 1) / a * a;
}

static uint32_t readAsByte4(util::File::Instance& fh, uint64_t off) {
  uint32_t v = 0;
  int shift = 0, num_reads = 0;
  char input[4];

  fh.readat(off, 4, input);

  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}

static uint16_t interpretByte2(const char *input) {
  uint16_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    v |= ((uint16_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}
static uint32_t interpretByte4(const char *input) {
  uint32_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}
static uint64_t interpretByte8(const char *input) {
  uint64_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    v |= ((uint64_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}

static char* insertByte2(char* bytes, uint16_t val) {
  int shift = 0, num_writes = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes + 2;
}
static char* insertByte4(char* bytes, uint32_t val) {
  int shift = 0, num_writes = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes + 4;
}
static char* insertByte8(char* bytes, uint64_t val) {
  int shift = 0, num_writes = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes + 8;
}

static std::array<char, PMS_real_hdr_SIZE> composeProfHdr(uint32_t nProf,
    uint64_t profInfoPtr, uint64_t profInfoSize, uint64_t idTuplesPtr,
    uint64_t idTuplesSize) {
  std::array<char, PMS_real_hdr_SIZE> out;
  char* cur = out.data();
  cur = std::copy(HPCPROFILESPARSE_FMT_Magic, HPCPROFILESPARSE_FMT_Magic + HPCPROFILESPARSE_FMT_MagicLen,
                  cur);
  *(cur++) = HPCPROFILESPARSE_FMT_VersionMajor;
  *(cur++) = HPCPROFILESPARSE_FMT_VersionMinor;

  cur = insertByte4(cur, nProf);
  cur = insertByte2(cur, HPCPROFILESPARSE_FMT_NumSec);
  cur = insertByte8(cur, profInfoSize);
  cur = insertByte8(cur, profInfoPtr);
  cur = insertByte8(cur, idTuplesSize);
  cur = insertByte8(cur, idTuplesPtr);
  return out;
}

static size_t sizeofIdTuple(const id_tuple_t& tuple) {
  return PMS_id_tuple_len_SIZE + tuple.length * PMS_id_SIZE;
}

static char* insertIdTuple(char* cur, const id_tuple_t& tuple) {
  cur = insertByte2(cur, (uint16_t)tuple.length);
  for(unsigned int i = 0; i < tuple.length; i++) {
    const auto& id = tuple.ids[i];
    cur = insertByte2(cur, id.kind);
    cur = insertByte8(cur, id.physical_index);
    cur = insertByte8(cur, id.logical_index);
  }
  return cur;
}

static std::array<char, PMS_prof_info_SIZE> composeProfInfo(const pms_profile_info_t& pi) {
  std::array<char, PMS_prof_info_SIZE> buf;
  char* cur = buf.data();

  cur = insertByte8(cur, pi.id_tuple_ptr);
  cur = insertByte8(cur, pi.metadata_ptr);
  cur = insertByte8(cur, pi.spare_one);
  cur = insertByte8(cur, pi.spare_two);
  cur = insertByte8(cur, pi.num_vals);
  cur = insertByte4(cur, pi.num_nzctxs);
  cur = insertByte8(cur, pi.offset);
  return buf;
}

static char* insertCiPair(char* cur, uint32_t ctx, uint64_t idx) {
  cur = insertByte4(cur, ctx);
  cur = insertByte8(cur, idx);
  return cur;
}

static char* insertMvPair(char* cur, uint16_t metric, double value) {
  hpcrun_metricVal_t v;
  v.r = value;
  cur = insertByte8(cur, v.bits);
  cur = insertByte2(cur, metric);
  return cur;
}

static pms_profile_info_t parseProfInfo(const char *input) {
  pms_profile_info_t pi;
  pi.id_tuple_ptr = interpretByte8(input);
  pi.metadata_ptr = interpretByte8(input + PMS_id_tuple_ptr_SIZE);
  pi.spare_one    = interpretByte8(input + PMS_id_tuple_ptr_SIZE + PMS_metadata_ptr_SIZE);
  pi.spare_two    = interpretByte8(input + PMS_id_tuple_ptr_SIZE + PMS_metadata_ptr_SIZE + PMS_spare_one_SIZE);
  pi.num_vals     = interpretByte8(input + PMS_ptrs_SIZE);
  pi.num_nzctxs   = interpretByte4(input + PMS_ptrs_SIZE + PMS_num_val_SIZE);
  pi.offset       = interpretByte8(input + PMS_ptrs_SIZE + PMS_num_val_SIZE + PMS_num_nzctx_SIZE);
  return pi;
}

static std::array<char, CMS_real_hdr_SIZE> composeCtxHdr(uint32_t ctxcnt) {
  std::array<char, CMS_real_hdr_SIZE> out;
  char* cur = out.data();
  cur = std::copy(HPCCCTSPARSE_FMT_Magic, HPCCCTSPARSE_FMT_Magic + HPCCCTSPARSE_FMT_MagicLen,
                  cur);
  *(cur++) = HPCCCTSPARSE_FMT_VersionMajor;
  *(cur++) = HPCCCTSPARSE_FMT_VersionMinor;

  cur = insertByte4(cur, ctxcnt);  // num_ctx
  cur = insertByte2(cur, HPCCCTSPARSE_FMT_NumSec);  // num_sec
  cur = insertByte8(cur, ctxcnt * CMS_ctx_info_SIZE);  // ci_size
  cur = insertByte8(cur, CMS_hdr_SIZE);  // ci_ptr
  return out;
}

static char* insertCtxInfo(char* cur, cms_ctx_info_t ci) {
  cur = insertByte4(cur, ci.ctx_id);
  cur = insertByte8(cur, ci.num_vals);
  cur = insertByte2(cur, ci.num_nzmids);
  cur = insertByte8(cur, ci.offset);
  return cur;
}

//
// SparseDB common bits
//

SparseDB::SparseDB(stdshim::filesystem::path dir) {
  if(dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);

  pmf = util::File(dir / "profile.db", true);
  cmf = util::File(dir / "cct.db", true);

  // Dump the FORMATS.md file
  try {
    std::ofstream(dir / "FORMATS.md") << FORMATS_md;
  } catch(std::exception& e) {
    util::log::warning{} << "Error while writing out FORMATS.md file";
  }
}

util::WorkshareResult SparseDB::help() {
  auto res = forEachProfileInfo.contribute();
  if(!res.completed) return res;
  res = forProfilesParse.contribute();
  if(!res.completed) return res;
  return forProfilesLoad.contribute() + forEachContextRange.contribute();
}

void SparseDB::notifyPipeline() noexcept {
  src.registerOrderedWavefront();
  src.registerOrderedWrite();
  auto& ss = src.structs();
  ud.context = ss.context.add_default<udContext>();
  ud.thread = ss.thread.add_default<udThread>();
}


void SparseDB::notifyWavefront(DataClass d) noexcept {
  if(!d.hasContexts() || !d.hasThreads()) return;
  auto mpiSem = src.enterOrderedWavefront();
  auto sig = wavefrontDone.signal();

  // Fill contexts with the sorted list of Contexts
  assert(contexts.empty());
  src.contexts().citerate([&](const Context& c){
    contexts.push_back(c);
  }, nullptr);
  std::sort(contexts.begin(), contexts.end(),
    [this](const Context& a, const Context& b) -> bool{
      return a.userdata[src.identifier()] < b.userdata[src.identifier()];
    });

  // The code in this Sink depends on dense Context identifiers
  // TODO: Review this dependency and remove whenever possible
  assert(std::adjacent_find(contexts.begin(), contexts.end(),
    [this](const Context& a, const Context& b) -> bool{
      return a.userdata[src.identifier()] + 1 != b.userdata[src.identifier()];
    }) == contexts.end());

  // Synchronize the profile.db across the ranks
  pmf->synchronize();

  // Count the total number of profiles across all ranks
  size_t myNProf = src.threads().size();
  if(mpi::World::rank() == 0) myNProf++;  // Counting the summary profile
  auto nProf = mpi::allreduce(myNProf, mpi::Op::sum());

  // Allocate space for the profile infos section
  uint64_t profInfosSize = nProf * PMS_prof_info_SIZE;

  // Write out our part of the id tuples section, and figure out its final size
  uint64_t idTuplesPtr = PMS_hdr_SIZE + align(profInfosSize, 8);
  uint64_t idTuplesSize;
  {
    std::vector<char> buf;
    if(mpi::World::rank() == 0){
      // Rank 0 handles the summary profile('s id tuple)
      pms_id_t sumId = {
        .kind = IDTUPLE_SUMMARY,
        .physical_index = 0,
        .logical_index = 0,
      };
      id_tuple_t idt = {
        .length = 1,
        .ids_length = 1,
        .ids = &sumId,
      };

      // Append to the end of the blob
      auto oldsz = buf.size();
      buf.resize(oldsz + sizeofIdTuple(idt), 0);
      insertIdTuple(&buf[oldsz], idt);

      // Save the local offset in the pointer field
      summary_info.id_tuple_ptr = oldsz;
    }

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
      // Id tuple for t
      id_tuple_t idt = {
        .length = (uint16_t)t.attributes.idTuple().size(),
        .ids_length = (uint16_t)t.attributes.idTuple().size(),
        .ids = const_cast<pms_id_t*>(t.attributes.idTuple().data()),
      };

      // Append to the end of the blob
      auto oldsz = buf.size();
      buf.resize(oldsz + sizeofIdTuple(idt), 0);
      insertIdTuple(&buf[oldsz], idt);

      // Save the local offset in the pointers
      t.userdata[ud].info.id_tuple_ptr = oldsz;
    }

    // Determine the offset of our blob, update the pointers and write it out.
    uint64_t offset = mpi::exscan<uint64_t>(buf.size(), mpi::Op::sum()).value_or(0);
    summary_info.id_tuple_ptr += idTuplesPtr + offset;
    for(const auto& t: src.threads().citerate())
      t->userdata[ud].info.id_tuple_ptr += idTuplesPtr + offset;
    auto fhi = pmf->open(true, false);
    fhi.writeat(idTuplesPtr + offset, buf.size(), buf.data());

    // Set the section size based on the sizes everyone contributes
    // Rank 0 handles the file header, so only Rank 0 needs to know
    idTuplesSize = mpi::reduce(buf.size(), 0, mpi::Op::sum());
  }

  // Rank 0 writes out the final file header
  if(mpi::World::rank() == 0) {
    auto buf = composeProfHdr(nProf, PMS_hdr_SIZE, profInfosSize, idTuplesPtr,
        idTuplesSize);
    pmf->open(true, false).writeat(0, buf.size(), buf.data());
  }

  // Set up the double-buffered output for profile data
  profDataOut.initialize(*pmf, idTuplesPtr + align(idTuplesSize, 8));
}

void SparseDB::notifyThreadFinal(const PerThreadTemporary& tt) {
  const auto& t = tt.thread();
  wavefrontDone.wait();

  // Allocate the blobs needed for the final output
  std::vector<char> mvPairsBuf;
  std::vector<char> ciPairsBuf;
  ciPairsBuf.reserve((contexts.size() + 1) * PMS_ctx_pair_SIZE);

  // Helper functions to insert ctx_id/idx pairs and metric/value pairs
  const auto addCiPair = [&](uint32_t ctxId, uint64_t index) {
    auto oldsz = ciPairsBuf.size();
    ciPairsBuf.resize(oldsz + PMS_ctx_pair_SIZE, 0);
    insertCiPair(&ciPairsBuf[oldsz], ctxId, index);
  };
  const auto addMvPair = [&](uint16_t metric, double value) {
    auto oldsz = mvPairsBuf.size();
    mvPairsBuf.resize(oldsz + PMS_vm_pair_SIZE, 0);
    insertMvPair(&mvPairsBuf[oldsz], metric, value);
  };

  // Now stitch together each Context's results
  for(const Context& c: contexts) {
    if(auto accums = tt.accumulatorsFor(c)) {
      // Add the ctx_id/idx pair for this Context
      addCiPair(c.userdata[src.identifier()], mvPairsBuf.size() / PMS_vm_pair_SIZE);
      size_t nValues = 0;

      for(const auto& mx: accums->citerate()) {
        const Metric& m = mx.first;
        const auto& vv = mx.second;
        const auto& id = m.userdata[src.identifier()];
        for(MetricScope ms: m.scopes()) {
          if(auto v = vv.get(ms)) {
            addMvPair(id.getFor(ms), *v);
            nValues++;
          }
        }
      }
      c.userdata[ud].nValues.fetch_add(nValues, std::memory_order_relaxed);
    }
  }

  //Add the extra ctx id and offset pair, to mark the end of ctx
  addCiPair(LastNodeEnd, mvPairsBuf.size() / PMS_vm_pair_SIZE);

  // Build prof_info
  auto& pi = t.userdata[ud].info;
  pi.prof_info_idx = t.userdata[src.identifier()] + 1;
  pi.num_vals = mvPairsBuf.size() / PMS_vm_pair_SIZE;
  pi.num_nzctxs = ciPairsBuf.size() / PMS_ctx_pair_SIZE - 1;

  profDataOut.write(mvPairsBuf, ciPairsBuf, pi.offset);
}

SparseDB::DoubleBufferedOutput::DoubleBufferedOutput()
  : pos(mpi::Tag::SparseDB_1) {}

void SparseDB::DoubleBufferedOutput::initialize(util::File& outfile,
                                                uint64_t startOffset) {
  file = outfile;
  pos.initialize(startOffset);
}

uint64_t SparseDB::DoubleBufferedOutput::allocate(uint64_t size) {
  return pos.fetch_add(size);
}

void SparseDB::DoubleBufferedOutput::write(const std::vector<char>& mvBlob,
    const std::vector<char>& ciBlob, uint64_t& offset) {
  assert(file);

  // Lock up the top-level state and inner Buffer
  std::unique_lock<std::mutex> topl(toplock);
  Buffer& buf = bufs[currentBuf];
  std::unique_lock<std::mutex> lowl(buf.lowlock);

  // If the Buffer will be full after our addition, we'll have to flush it
  // afterwards. Let other threads progress in this case by rotating buffers.
  bool needsFlush = buf.blob.size() + mvBlob.size() + ciBlob.size() >= Buffer::bufferSize;
  if(needsFlush)
    currentBuf = (currentBuf + 1) % bufs.size();
  topl.unlock();  // Let other threads progress in the other Buffer now

  // Record the relative offset and append our new bytes to our Buffer
  offset = buf.blob.size();
  buf.toUpdate.push_back(offset);
  buf.blob.insert(buf.blob.end(), mvBlob.begin(), mvBlob.end());
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
    const util::File& pmf, const pms_profile_info_t& pi) {
  if(pi.num_nzctxs == 0) {
    // No data in this profile, skip
    return {};
  }

  // Read the whole chunk of ctx_id/idx pairs
  auto pmfi = pmf.open(false, false);
  std::vector<char> buf((pi.num_nzctxs + 1) * PMS_ctx_pair_SIZE);
  pmfi.readat(pi.offset + pi.num_vals * PMS_vm_pair_SIZE, buf.size(), buf.data());

  // Parse and save in the output
  std::vector<std::pair<uint32_t, uint64_t>> prof_ctx_pairs;
  prof_ctx_pairs.reserve(pi.num_nzctxs + 1);
  for(uint32_t i = 0; i < pi.num_nzctxs + 1; i++) {
    const char* cur = &buf[i * PMS_ctx_pair_SIZE];
    prof_ctx_pairs.emplace_back(interpretByte4(cur),
                                interpretByte8(cur + PMS_ctx_id_SIZE));
  }
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
  mvBlob.resize((last->second - first->second) * PMS_vm_pair_SIZE);
  assert(!mvBlob.empty());

  auto pmfi = pmf.open(false, false);
  pmfi.readat(offset + first->second * PMS_vm_pair_SIZE,
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
    while(heap.front().first->first == ctx_id) {
      // Pop the top entry from the heap, and increment it
      std::pop_heap(heap.begin(), heap.end(), heap_comp);
      const auto& curPair = *(heap.back().first++);

      // Fill cmb with metric/value pairs for this context, from the top profile
      const ProfileMetricData& profile = heap.back().second;
      const char* cur = &profile.mvBlob[(curPair.second - profile.first->second) * PMS_vm_pair_SIZE];
      for(uint64_t i = 0, e = heap.back().first->second - curPair.second;
          i < e; i++, cur += PMS_vm_pair_SIZE) {
        allpvs++;
        const uint16_t mid = interpretByte2(cur + PMS_val_SIZE);
        auto& subbuf = valuebufs.try_emplace(mid).first->second;

        // Write in this prof_idx/value pair, in bytes.
        // Values are represented the same so they can just be copied.
        subbuf.reserve(subbuf.size() + CMS_val_prof_idx_pair_SIZE);
        subbuf.insert(subbuf.end(), cur, cur + PMS_val_SIZE);
        insertByte4(&*subbuf.insert(subbuf.end(), CMS_prof_idx_SIZE, 0), profile.index);
      }

      // If the updated entry is still in range, push it back into the heap.
      // Otherwise pop the entry from the vector completely.
      if(heap.back().first->first < lastCtx)
        std::push_heap(heap.begin(), heap.end(), heap_comp);
      else
        heap.pop_back();
    }

    // Allocate enough space in buf for all the bits we want.
    const auto newsz = allpvs * CMS_val_prof_idx_pair_SIZE
                       + (valuebufs.size()+1) * CMS_m_pair_SIZE;
    assert(ctxOffsets[ctx_id] + newsz == ctxOffsets[ctx_id+1]
           && "Final layout doesn't match precalculated ctx_off!");
    buf.reserve(buf.size() + newsz);

    // Concatinate the prof_idx/value pairs, in bytes form, in metric order
    for(const auto& [mid, pvbuf]: valuebufs)
      buf.insert(buf.end(), pvbuf.begin(), pvbuf.end());

    // Construct the metric_id/idx pairs for this context, in bytes
    {
      char* cur = &*buf.insert(buf.end(), (valuebufs.size()+1) * CMS_m_pair_SIZE, 0);
      uint64_t pvs = 0;
      for(const auto& [mid, pvbuf]: valuebufs) {
        cur = insertByte2(cur, mid);
        cur = insertByte8(cur, pvs);
        pvs += pvbuf.size() / CMS_val_prof_idx_pair_SIZE;
      }
      assert(pvs == allpvs);

      // Last entry is always LastMidEnd
      cur = insertByte2(cur, LastMidEnd);
      cur = insertByte8(cur, pvs);
    }
  }

  // Write out the whole blob of data where it belongs in the file
  if(buf.empty()) return;
  assert(firstCtxId != LastNodeEnd);
  auto cmfi = cmf.open(true, true);
  cmfi.writeat(ctxOffsets[firstCtxId], buf.size(), buf.data());
}

void SparseDB::write() {
  auto mpiSem = src.enterOrderedWrite();

  // Make sure all the profile data is on disk and all offsets are updated
  profDataOut.flush();

  // Now that the profile infos are up-to-date, write them all in parallel
  {
    std::vector<std::reference_wrapper<const pms_profile_info_t>> prof_infos;
    prof_infos.reserve(src.threads().size());
    for(const auto& t: src.threads().citerate())
      prof_infos.push_back(t->userdata[ud].info);

    forEachProfileInfo.fill(std::move(prof_infos), [&](const pms_profile_info_t& pi){
      auto buf = composeProfInfo(pi);
      auto fhi = pmf->open(true, false);
      fhi.writeat(PMS_hdr_SIZE + pi.prof_info_idx * PMS_prof_info_SIZE,
                  buf.size(), buf.data());
    });
    forEachProfileInfo.contribute(forEachProfileInfo.wait());
  }

  // Lay out the cct.db metric data section, in terms of offsets for every context
  auto ctxcnt = mpi::bcast(contexts.size(), 0);
  std::vector<uint64_t> ctxOffsets(ctxcnt + 1, 0);
  for(const Context& c: contexts) {
    const auto i = c.userdata[src.identifier()];
    assert(&c == &contexts[i].get());
    auto& udc = c.userdata[ud];
    ctxOffsets[i] = udc.nValues.load(std::memory_order_relaxed)
                    * CMS_val_prof_idx_pair_SIZE;

    // Rank 0 has the final number of metric/idx pairs
    if(mpi::World::rank() == 0) {
      const auto& use = c.data().metricUsage();
      auto iter = use.citerate();
      bool isLine = c.scope().flat().type() == Scope::Type::line;
      udc.nMetrics = std::accumulate(iter.begin(), iter.end(), (uint16_t)0,
        [isLine](uint16_t out, const auto& mu) -> uint16_t {
          MetricScopeSet use = mu.second;
          assert((mu.first->scopes() & use) == use && "Inconsistent Metric value usage data!");
          return out + use.count();
        });
      if(udc.nMetrics > 0)
        ctxOffsets[i] += (udc.nMetrics + 1) * CMS_m_pair_SIZE;
    }
  }
  // Exclusive scan to get offsets. Rank 0 adds the initial offset for the section
  const uint64_t ctxStart = align(ctxcnt * CMS_ctx_info_SIZE, 8) + CMS_hdr_SIZE;
  stdshim::exclusive_scan(ctxOffsets.begin(), ctxOffsets.end(), ctxOffsets.begin(),
      mpi::World::rank() == 0 ? ctxStart : 0);
  // All-reduce the offsets to get global offsets incorporating everyone
  ctxOffsets = mpi::allreduce(ctxOffsets, mpi::Op::sum());

  // Divide the contexts into ranges of easily distributable sizes
  std::vector<uint32_t> ctxRanges;
  {
    ctxRanges.push_back(0);
    const uint64_t limit = std::min<uint64_t>(1024ULL*1024*1024*3,
        (ctxOffsets.back() - ctxStart) / (3 * mpi::World::size()));
    uint64_t cursize = 0;
    for(size_t i = 0; i < ctxcnt; i++) {
      const uint64_t size = ctxOffsets[i+1] - ctxOffsets[i];
      if(cursize + size > limit) {
        ctxRanges.push_back(i);
        cursize = 0;
      }
      cursize += size;
    }
    ctxRanges.push_back(ctxcnt);
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
    uint32_t nProf = readAsByte4(fi, HPCPROFILESPARSE_FMT_MagicLen + HPCPROFILESPARSE_FMT_VersionLen);

    // Read the whole section in, skipping over the index 0 summary profile
    std::vector<char> buf((nProf-1) * PMS_prof_info_SIZE);
    fi.readat(PMS_hdr_SIZE + PMS_prof_info_SIZE, buf.size(), buf.data());

    // Load the data we need from the profile.db, in parallel
    profiles = std::deque<ProfileIndexData>(nProf - 1);
    forProfilesParse.fill(profiles.size(), [this, &buf, &profiles](size_t i){
      auto pi = parseProfInfo(&buf[i * PMS_prof_info_SIZE]);
      profiles[i] = {
        .offset = pi.offset,
        .index = (uint32_t)(i + 1),
        .ctxPairs = readProfileCtxPairs(*pmf, pi),
      };
    });
    forProfilesParse.contribute(forProfilesParse.wait());
  }

  if(mpi::World::rank() == 0) {
    // Rank 0 is in charge of writing out the summary profile in profile.db
    {
      // Allocate the blobs needed for the final output
      std::vector<char> mvPairsBuf;
      std::vector<char> ciPairsBuf;
      ciPairsBuf.reserve((contexts.size() + 1) * PMS_ctx_pair_SIZE);

      // Helper functions to insert ctx_id/idx pairs and metric/value pairs
      const auto addCiPair = [&](uint32_t ctxId, uint64_t index) {
        auto oldsz = ciPairsBuf.size();
        ciPairsBuf.resize(oldsz + PMS_ctx_pair_SIZE, 0);
        insertCiPair(&ciPairsBuf[oldsz], ctxId, index);
      };
      const auto addMvPair = [&](uint16_t metric, double value) {
        auto oldsz = mvPairsBuf.size();
        mvPairsBuf.resize(oldsz + PMS_vm_pair_SIZE, 0);
        insertMvPair(&mvPairsBuf[oldsz], metric, value);
      };

      // Now stitch together each Context's results
      for(const Context& c: contexts) {
        const auto& stats = c.data().statistics();
        if(stats.size() > 0)
          addCiPair(c.userdata[src.identifier()], mvPairsBuf.size() / PMS_vm_pair_SIZE);
        for(const auto& mx: stats.citerate()) {
          const Metric& m = mx.first;
          const auto& id = m.userdata[src.identifier()];
          const auto& vv = mx.second;
          for(const auto& sp: m.partials()) {
            auto vvv = vv.get(sp);
            for(MetricScope ms: m.scopes()) {
              if(auto v = vvv.get(ms))
                addMvPair(id.getFor(sp, ms), *v);
            }
          }
        }
      }

      // Add the extra ctx id and offset pair, to mark the end of ctx
      addCiPair(LastNodeEnd, mvPairsBuf.size() / PMS_vm_pair_SIZE);

      // Build prof_info
      summary_info.num_vals = mvPairsBuf.size() / PMS_vm_pair_SIZE;
      summary_info.num_nzctxs = ciPairsBuf.size() / PMS_ctx_pair_SIZE - 1;

      // Write the summary profile out and make sure it makes it to disk
      profDataOut.write(std::move(mvPairsBuf), std::move(ciPairsBuf),
                        summary_info.offset);
      profDataOut.flush();

      // Write the summary info, which is always profile index 0
      auto pmfi = pmf->open(true, false);
      auto buf = composeProfInfo(summary_info);
      pmfi.writeat(PMS_hdr_SIZE, buf.size(), buf.data());

      // Write out the footer to indicate that profile.db is complete
      auto footer_val = PROFDBft;
      uint64_t footer_off = profDataOut.allocate(sizeof(footer_val));
      pmfi.writeat(footer_off, sizeof(footer_val), &footer_val);
    }

    // Rank 0 is also in charge of writing the header and context info sections
    // in the cct.db
    {
      auto cmfi = cmf->open(true, true);

      auto hdr = composeCtxHdr(ctxcnt);
      cmfi.writeat(0, hdr.size(), hdr.data());

      std::vector<char> buf(ctxcnt * CMS_ctx_info_SIZE);
      char* cur = buf.data();
      for(uint32_t i = 0; i < ctxcnt; i++) {
        uint16_t nMetrics = contexts[i].get().userdata[ud].nMetrics;
        uint64_t nVals = nMetrics == 0 ? 0 :
          (ctxOffsets[i+1] - ctxOffsets[i] - (nMetrics + 1) * CMS_m_pair_SIZE) / CMS_val_prof_idx_pair_SIZE;
        cur = insertCtxInfo(cur, {
          .ctx_id = i, .num_vals = nVals, .num_nzmids = nMetrics,
          .offset = ctxOffsets[i],
        });
      }
      cmfi.writeat(CMS_hdr_SIZE, buf.size(), buf.data());
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
      forProfilesLoad.reset();  // Also waits for work to complete

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
      forEachContextRange.reset();
    }

    // Fetch the next available workitem from the group
    idx = ctxRangeCounter.fetch_add(1);
  }

  // Notify our helper threads that the workshares are complete now
  forProfilesLoad.fill({});
  forProfilesLoad.complete();
  forEachContextRange.fill({});
  forEachContextRange.complete();

  // The last rank is in charge of writing the final footer, AFTER all other
  // writes have completed. If the footer isn't there, the file isn't complete.
  mpi::barrier();
  if(mpi::World::rank() + 1 == mpi::World::size()) {
    const uint64_t footer = CCTDBftr;
    auto cmfi = cmf->open(true, false);
    cmfi.writeat(ctxOffsets.back(), sizeof footer, &footer);
  }
}
