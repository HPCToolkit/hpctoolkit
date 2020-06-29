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
// Copyright ((c)) 2020, Rice University
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

#include "sparse.hpp"

#include "mpi-strings.h"
#include <lib/profile/util/log.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof/tms-format.h>
#include <lib/prof/cms-format.h>

#include <sstream>
#include <iomanip>

#include <sys/types.h>
#include <sys/stat.h>
#include <omp.h>
#include <assert.h>
#include <stdexcept> 

using namespace hpctoolkit;

SparseDB::SparseDB(const stdshim::filesystem::path& p) : dir(p), ctxMaxId(0), outputCnt(0) {
  if(dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);
}

SparseDB::SparseDB(stdshim::filesystem::path&& p) : dir(std::move(p)), ctxMaxId(0), outputCnt(0) {
  if(dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);
}

void SparseDB::notifyWavefront(DataClass::singleton_t ds) noexcept {
  if(((DataClass)ds).hasContexts())
    contextPrep.call_nowait([this]{ prepContexts(); });
}

void SparseDB::prepContexts() noexcept {
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

void SparseDB::notifyThreadFinal(const Thread::Temporary& tt) {
  const auto& t = tt.thread;

  // Make sure the Context list is ready to go
  contextPrep.call([this]{ prepContexts(); });

  // Prep a quick-access Metric list, so we know what to ping.
  // TODO: Do this better with the Statistics update.
  std::vector<std::reference_wrapper<const Metric>> metrics;
  for(const Metric& m: src.metrics().iterate()) metrics.emplace_back(m);

  // Allocate the blobs needed for the final output
  std::vector<hpcrun_metricVal_t> values;
  std::vector<uint16_t> mids;
  std::vector<uint32_t> cids;
  std::vector<uint64_t> coffsets;
  coffsets.reserve(1 + (ctxMaxId+1)*2 + 1);  // To match up with EXML ids.

  // Now stitch together each Context's results
  for(const Context& c: contexts) {
    bool any = false;
    std::size_t offset = values.size();
    for(const Metric& m: metrics) {
      const auto& ids = m.userdata[src.identifier()];
      auto vv = m.getFor(tt, c);
      hpcrun_metricVal_t v;
      if(vv.first != 0) {
        v.r = vv.first;
        any = true;
        mids.push_back(ids.first);
        values.push_back(v);
      }
      if(vv.second != 0) {
        v.r = vv.second;
        any = true;
        mids.push_back(ids.second);
        values.push_back(v);
      }
    }
    if(any) {
      cids.push_back(c.userdata[src.identifier()]*2 + 1);  // Convert to EXML id
      coffsets.push_back(offset);
    }
  }

  //Add the extra ctx id and offset pair, to mark the end of ctx  - YUMENG
  cids.push_back(LastNodeEnd);
  coffsets.push_back(values.size());

  // Put together the sparse_metrics structure
  hpcrun_fmt_sparse_metrics_t sm;
  sm.tid = t.attributes.has_threadid() ? t.attributes.threadid() : 0;
  sm.num_vals = values.size();
  sm.num_cct_nodes = contexts.size();
  sm.num_nz_cct_nodes = coffsets.size() - 1; //since there is an extra end node YUMENG
  sm.values = values.data();
  sm.mids = mids.data();
  sm.cct_node_ids = cids.data();
  sm.cct_node_idxs = coffsets.data();

  // Set up the output temporary file.
  stdshim::filesystem::path outfile;
  {
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    std::ostringstream ss;
    ss << "tmp-" << world_rank << "."
       << outputCnt.fetch_add(1, std::memory_order_relaxed) << ".sparse-db";
    outfile = dir / ss.str();
  }
  std::FILE* of = std::fopen(outfile.c_str(), "wb");
  if(!of) util::log::fatal() << "Unable to open temporary sparse-db file for output!";

  // Spit it all out, and close up.
  if(hpcrun_fmt_sparse_metrics_fwrite(&sm, of) != HPCFMT_OK)
    util::log::fatal() << "Error writing out temporary sparse-db!";
  std::fclose(of);

  // Log the output for posterity
  outputs.emplace(&t, std::move(outfile));
}

void SparseDB::write() {};

//***************************************************************************
// Work with bytes - YUMENG
//***************************************************************************
int SparseDB::writeAsByte4(uint32_t val, MPI_File fh, MPI_Offset off){
  int shift = 0, num_writes = 0;
  char input[4];

  for (shift = 24; shift >= 0; shift -= 8) {
    input[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }

  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_write_at(fh,off,&input,4,MPI_BYTE,&stat));
  return SPARSE_OK;
}

int SparseDB::writeAsByte8(uint64_t val, MPI_File fh, MPI_Offset off){
  int shift = 0, num_writes = 0;
  char input[8];

  for (shift = 56; shift >= 0; shift -= 8) {
    input[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }

  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_write_at(fh,off,&input,8,MPI_BYTE,&stat));
  return SPARSE_OK;
}

int SparseDB::writeAsByteX(std::vector<char>& val, size_t size, MPI_File fh, MPI_Offset off){
  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_write_at(fh,off,val.data(),size,MPI_BYTE,&stat));
  return SPARSE_OK;
}

int SparseDB::readAsByte4(uint32_t& val, MPI_File fh, MPI_Offset off){
  uint32_t v = 0;
  int shift = 0, num_reads = 0;
  char input[4];

  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_read_at(fh,off,&input,4,MPI_BYTE,&stat));
  
  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  val = v;
  return SPARSE_OK;

}

int SparseDB::readAsByte8(uint64_t& val, MPI_File fh, MPI_Offset off){
  uint32_t v = 0;
  int shift = 0, num_reads = 0;
  char input[8];

  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_read_at(fh,off,&input,8,MPI_BYTE,&stat));
  
  for (shift = 56; shift >= 0; shift -= 8) {
    v |= ((uint64_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  val = v;
  return SPARSE_OK;

}

void SparseDB::interpretByte2(uint16_t& val, const char *input){
  uint16_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    v |= ((uint16_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  val = v;
}

void SparseDB::interpretByte4(uint32_t& val, const char *input){
  uint32_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  val = v;
}

void SparseDB::interpretByte8(uint64_t& val, const char *input){
  uint64_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    v |= ((uint64_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  val = v;
}

void SparseDB::convertToByte2(uint16_t val, char* bytes){
  int shift = 0, num_writes = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
}

void SparseDB::convertToByte4(uint32_t val, char* bytes){
  int shift = 0, num_writes = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
}

void SparseDB::convertToByte8(uint64_t val, char* bytes){
  int shift = 0, num_writes = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
}



//***************************************************************************
// thread_major_sparse.db  - YUMENG
//
/*EXAMPLE
[Profile informations for 72 profiles
  [(thread id: 0) (num_vals: 170) (num_nzctxs: 98) (starting location: 78144)]
  [(thread id: 1) (num_vals: 117) (num_nzctxs: 64) (starting location: 83270)]
  ...
]
[thread 36
  [metrics:
  (NOTES: printed in file order, help checking if the file is correct)
    (value: 2.8167, metric id: 1)
    (value: 2.8167, metric id: 1)
    ...
  ]
  [ctx indices:
    (ctx id: 1, index: 0)
    (ctx id: 7, index: 1)
    ...
  ]
]
...
*/
//***************************************************************************

//---------------------------------------------------------------------------
// get profile's size and corresponding offsets
//---------------------------------------------------------------------------

// iterate through rank's profile list, assign profile_sizes, a vector of (profile Thread object : size) pair
// also assign value to my_size, which is the total size of rank's profiles
void SparseDB::getProfileSizes(std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes, 
                               uint64_t& my_size)
{
  for(const auto& tp: outputs.citerate()){
    struct stat buf;
    stat(tp.second.string().c_str(),&buf);
    my_size += (buf.st_size - TMS_prof_skip_SIZE);
    profile_sizes.emplace_back(tp.first,(buf.st_size - TMS_prof_skip_SIZE));    
  }
}


// get total number of profiles across all ranks
// input: rank's number of profiles, output: assigned total_num_prof
void SparseDB::getTotalNumProfiles(const uint32_t my_num_prof, 
                                   uint32_t& total_num_prof)
{
  MPI_Allreduce(&my_num_prof, &total_num_prof, 1, mpi_data<uint32_t>::type, MPI_SUM, MPI_COMM_WORLD);
}


// get the offset for this rank's start in thread_major_sparse.db
// input: my_size as the total size of all profiles belong to this rank, rank number
// output: calculated my_offset
void SparseDB::getMyOffset(const uint64_t my_size, 
                           const int rank, 
                           uint64_t& my_offset)
{
  MPI_Exscan(&my_size, &my_offset, 1, mpi_data<uint64_t>::type, MPI_SUM, MPI_COMM_WORLD);
  if(rank == 0) my_offset = 0;
}


// get the final global offsets for each profile in this rank
// input: profile_sizes (size of each profile), total number of profiles across all ranks, rank's offset, number of threads
// output: (last argument) assigned prof_offsets
void SparseDB::getMyProfOffset(const std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes,
                               const uint32_t total_prof, 
                               const uint64_t my_offset, 
                               const int threads, 
                               std::vector<uint64_t>& prof_offsets)
{
  assert(prof_offsets.size() == profile_sizes.size());

  std::vector<uint64_t> tmp (profile_sizes.size());
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < tmp.size();i++){
    tmp[i] = profile_sizes[i].second;
  }

  exscan<uint64_t>(tmp,threads);

  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < tmp.size();i++){
    if(i < tmp.size() - 1) assert(tmp[i] + profile_sizes[i].second == tmp[i+1]);
    prof_offsets[i] = tmp[i] + my_offset + (total_prof * TMS_prof_info_SIZE) + TMS_total_prof_SIZE; 
  }

}


//---------------------------------------------------------------------------
// get profile's real data (bytes)
//---------------------------------------------------------------------------

// interpret one context's metric ids
// input: input bytes that start from the context's first non-zero metric id, ctx_nzval_cnt
// output: assigned ctx_nzmids
void SparseDB::interpretOneCtxMids(const char* input,
                                   const uint64_t ctx_nzval_cnt,
                                   std::set<uint16_t>& ctx_nzmids)
{
  for(uint m = 0; m < ctx_nzval_cnt; m++){
    uint16_t mid;
    interpretByte2(mid, input);
    ctx_nzmids.insert(mid); 
    input += (TMS_mid_SIZE + TMS_val_SIZE);
  }
}

// interpret one context's number of non-zero values
// input: input bytes that start from the context's index
// output: assigned ctx_nzval_cnt and ctx_idx
void SparseDB::interpretOneCtxNzvalCnt(const char* input,
                                       uint64_t& ctx_nzval_cnt,
                                       uint64_t& ctx_idx)
{
  uint64_t next_ctx_idx;

  interpretByte8(ctx_idx,      input);
  interpretByte8(next_ctx_idx, input + TMS_ctx_pair_SIZE);
  ctx_nzval_cnt = next_ctx_idx - ctx_idx;
}


// interpret one context for its number of non-zero values and non-zero metric ids
// input: input bytes that start from the context's index, 
//        input bytes that start from the context's first non-zero metric id,
// output: assigned ctx_nzval_cnts and ctx_nzmids
void SparseDB::interpretOneCtxValCntMids(const char* val_cnt_input, 
                                         const char* mids_input,
                                         std::vector<std::set<uint16_t>>& ctx_nzmids,
                                         std::vector<uint64_t>& ctx_nzval_cnts)
{
  uint32_t ctx_id;
  interpretByte4(ctx_id, val_cnt_input);
  
  // nzval_cnt
  uint64_t ctx_idx;
  uint64_t ctx_nzval_cnt;
  interpretOneCtxNzvalCnt(val_cnt_input + TMS_ctx_id_SIZE, ctx_nzval_cnt, ctx_idx);
  ctx_nzval_cnts[CTX_VEC_IDX(ctx_id)] += ctx_nzval_cnt;

  // nz-mids
  mids_input += ctx_idx * (TMS_mid_SIZE + TMS_val_SIZE) + TMS_val_SIZE;
  interpretOneCtxMids(mids_input, ctx_nzval_cnt, ctx_nzmids[CTX_VEC_IDX(ctx_id)]);

}


//---------------------------------------------------------------------------
// write profiles 
//---------------------------------------------------------------------------

// collect ctx_nzval_cnts and ctx_nzmids
// input: bytes collected from each individual profile
// output: (last two arguments) assigned ctx_nzval_cnts, ctx_nzmids
void SparseDB::collectCctMajorData(std::vector<char>& bytes,
                                   std::vector<uint64_t>& ctx_nzval_cnts, 
                                   std::vector<std::set<uint16_t>>& ctx_nzmids)
{ 
  assert(ctx_nzval_cnts.size() == ctx_nzmids.size());
  assert(ctx_nzval_cnts.size() > 0);

  uint64_t num_vals;
  uint32_t num_nzctxs;
  interpretByte8(num_vals,   bytes.data() + TMS_tid_SIZE);
  interpretByte4(num_nzctxs, bytes.data() + TMS_tid_SIZE + TMS_num_val_SIZE);
  uint64_t ctx_end_idx;
  interpretByte8(ctx_end_idx, bytes.data() + bytes.size() - TMS_ctx_idx_SIZE);
  if(num_vals != ctx_end_idx) {
    uint32_t tid;
    interpretByte4(tid, bytes.data());
    exitError("tmpDB file for thread " + std::to_string(tid) + " is corrupted!");
  }

  char* val_cnt_input = bytes.data() + TMS_prof_skip_SIZE + num_vals * (TMS_val_SIZE + TMS_mid_SIZE);
  char* mids_input = bytes.data() + TMS_prof_skip_SIZE;
  for(uint i = 0; i < num_nzctxs; i++){
    interpretOneCtxValCntMids(val_cnt_input, mids_input, ctx_nzmids, ctx_nzval_cnts);

    val_cnt_input += TMS_ctx_pair_SIZE;  
  }

}



// write one profile information at the top of thread_major_sparse.db
// input: bytes of the profile, calculated offset, thread id, file handle
void SparseDB::writeOneProfInfo(const std::vector<char>& info_bytes, 
                                const uint32_t tid,
                                const MPI_File fh)
{

  MPI_Offset info_off = TMS_total_prof_SIZE + tid * TMS_prof_info_SIZE;
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_write_at(fh, info_off, info_bytes.data(), TMS_prof_info_SIZE, MPI_BYTE, &stat), 
    __FUNCTION__ +  std::string(": profile ") + std::to_string(tid));
}

// write one profile data at the <offset> of thread_major_sparse.db
// input: bytes of the profile, calculated offset, thread id, file handle
void SparseDB::writeOneProfileData(const std::vector<char>& bytes, 
                                   const uint64_t offset, 
                                   const uint32_t tid,
                                   const MPI_File fh)
{
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_write_at(fh, offset, bytes.data()+TMS_prof_skip_SIZE, bytes.size()-TMS_prof_skip_SIZE, MPI_BYTE, &stat),
    __FUNCTION__ +  std::string(": profile ") + std::to_string(tid));
}

// write one profile in thread_major_sparse.db
// input: one profile's thread attributes, profile's offset, file handle
// output: assigned ctx_nzval_cnts and ctx_nzmids
void SparseDB::writeOneProfile(const hpctoolkit::Thread* threadp,
                               const MPI_Offset my_prof_offset, 
                               std::vector<uint64_t>& ctx_nzval_cnts,
                               std::vector<std::set<uint16_t>>& ctx_nzmids,
                               MPI_File fh)
{
  //to read and write: get tid, file name
  uint32_t tid = (uint32_t)threadp->attributes.threadid();
  std::string fn = outputs.at(threadp).string();

  //get all bytes from a profile
  std::ifstream input(fn.c_str(), std::ios::binary);
  std::vector<char> bytes(
      (std::istreambuf_iterator<char>(input)),
      (std::istreambuf_iterator<char>()));
  input.close();

  //collect context local nonzero value counts and nz_mids from this profile
  collectCctMajorData(bytes, ctx_nzval_cnts, ctx_nzmids);

  //write profile info
  std::vector<char> info (TMS_prof_info_SIZE);
  std::copy(bytes.begin(), bytes.begin() + TMS_prof_skip_SIZE, info.begin());
  convertToByte8(my_prof_offset, info.data() + TMS_prof_skip_SIZE);
  writeOneProfInfo(info, tid, fh);

  //write profile data
  writeOneProfileData(bytes, my_prof_offset, tid, fh);
}

// write all the profiles at the correct place, and collect data needed for cct_major_sparse.db 
// input: calculated prof_offsets, calculated profile_sizes, file handle, number of available threads
void SparseDB::writeProfiles(const std::vector<uint64_t>& prof_offsets, 
                             const std::vector<std::pair<const hpctoolkit::Thread*,uint64_t>>& profile_sizes,
                             const MPI_File fh, 
                             const int threads,
                             std::vector<uint64_t>& ctx_nzval_cnts,
                             std::vector<std::set<uint16_t>>& ctx_nzmids)
{

  assert(ctx_nzval_cnts.size() == ctx_nzmids.size());
  assert(ctx_nzval_cnts.size() > 0);
  assert(prof_offsets.size() == profile_sizes.size());

  std::vector<std::vector<std::set<uint16_t>> *> threads_ctx_nzmids(threads);

  #pragma omp declare reduction (vectorSum : std::vector<uint64_t> : \
      std::transform(omp_out.begin(), omp_out.end(), omp_in.begin(), omp_out.begin(), std::plus<uint64_t>()))\
      initializer(omp_priv = decltype(omp_orig)(omp_orig.size()))

  #pragma omp parallel num_threads(threads)
  {
    std::set<uint16_t> empty;
    std::vector<std::set<uint16_t>> thread_ctx_nzmids (ctx_nzmids.size(), empty);
    threads_ctx_nzmids[omp_get_thread_num()] = &thread_ctx_nzmids;

    #pragma omp for reduction(vectorSum : ctx_nzval_cnts)
    for(uint i = 0; i<profile_sizes.size();i++){
      const hpctoolkit::Thread* threadp = profile_sizes[i].first;
      MPI_Offset my_prof_offset = prof_offsets[i];
      writeOneProfile(threadp, my_prof_offset, ctx_nzval_cnts, thread_ctx_nzmids, fh);
    }

    // union non-zero metric ids collected from different threads
    #pragma omp for
    for(uint j = 0; j<ctx_nzmids.size(); j++){
      for(int t = 0; t < threads; t++){
        std::set_union(ctx_nzmids[j].begin(), ctx_nzmids[j].end(),
              (*threads_ctx_nzmids[t])[j].begin(), (*threads_ctx_nzmids[t])[j].end(),
              std::inserter(ctx_nzmids[j], ctx_nzmids[j].begin()));
      }
    }

  }//END of parallel region

}


// input: number of available threads, world_rank, world_size
// output: (last two arguments) assigned ctx_nzval_cnts and ctx_nzmids
void SparseDB::writeThreadMajor(const int threads, 
                                const int world_rank, 
                                const int world_size, 
                                std::vector<uint64_t>& ctx_nzval_cnts,
                                std::vector<std::set<uint16_t>>& ctx_nzmids)
{
  //
  // profile_sizes: vector of (thread attributes: its own size)
  // prof_offsets:  vector of final global offset
  // my_size:       the total size of this rank's all profiles  
  // total_prof:    total number of profiles across all ranks
  // my_off:        starting offset for all of the profiles of this rank as a chunk 
  //

  std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>> profile_sizes;
  uint64_t my_size = 0;
  getProfileSizes(profile_sizes, my_size);
  std::vector<uint64_t> prof_offsets (profile_sizes.size());
  uint32_t total_prof;
  getTotalNumProfiles(profile_sizes.size(), total_prof);
  uint64_t my_off;
  getMyOffset(my_size, world_rank, my_off);
  getMyProfOffset(profile_sizes, total_prof, my_off, threads/world_size, prof_offsets);


  MPI_File thread_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "thread_major_sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &thread_major_f);


  if(world_rank == 0) SPARSE_exitIfMPIError(writeAsByte4(total_prof, thread_major_f, 0), __FUNCTION__ + std::string(": write the number of profiles wrong"));
  writeProfiles(prof_offsets, profile_sizes, thread_major_f, threads/world_size, ctx_nzval_cnts, ctx_nzmids);

  MPI_File_close(&thread_major_f);
}

//***************************************************************************
// cct_major_sparse.db  - YUMENG
//
/*EXAMPLE
[Context informations for 220 Contexts
  [(context id: 1) (num_vals: 72) (num_nzmids: 1) (starting location: 4844)]
  [(context id: 3) (num_vals: 0) (num_nzmids: 0) (starting location: 5728)]
  ...
]
[context 1
  [metrics easy grep version:
  (NOTES: printed in file order, help checking if the file is correct)
    (value: 2.64331, thread id: 0)
    (value: 2.62104, thread id: 1)
    ...
  ]
  [metric indices:
    (metric id: 1, index: 0)
    (metric id: END, index: 72)
  ]
]
...same [sparse metrics] for all rest ctxs 
*/
//***************************************************************************

//---------------------------------------------------------------------------
// calculate the offset for each context's section in cct_major_sparse.db
// assign contexts to different ranks
//---------------------------------------------------------------------------

// union ctx_nzmids from all ranks to root, in order to avoid double counting for offset calculation
// input: each rank's own ctx_nzmids (collected while writing thread_major_sparse.db), rank number, number of processes/ranks, available threads
// output: rank 0's ctx_nzmids becomes global version
void SparseDB::unionMids(std::vector<std::set<uint16_t>>& ctx_nzmids, 
                         const int rank, 
                         const int num_proc, 
                         const int threads)
{
  assert(ctx_nzmids.size() > 0);

  /*
  LOGIC:                                      
  NOTE: ctx_nzmids is a vector of set, each set represents the metric ids for one context
  Step 1: turn ctx_nzmids to a long vector of metric ids, and use a stopper to differentiate ids for one contexts group
  Step 2: MPI_Gatherv to gather all long vectors to rank 0
    - get the size of the long vector and MPI_Gather to rank 0
    - rank 0 local exscan to get displacement for each rank's long vector
  Step 3: convert long vector (with all mids from all ranks) to rank 0's ctx_nzmids (global version now)
    - add the extra LastMidEnd to help marking the end location later for mid&offset pairs

    |----------|----------|       EACH RANK --\
    |context 0 | {0, 1, 2}|                   | 
    |----------|----------|      |---------------------------------------|
    |context 1 | {0, 2}   | ---> |0|1|2|stopper|0|2|stopper|0|1|3|stopper| --- \
    |----------|----------|      |---------------------------------------|     |
    |context 2 | {0, 1, 3}|                                                    |
    |----------|----------|                                                    |
                                                                               |
                                                                               |                                                                     
    RANK 0 --\                                                                 |
             |                                                                 |
    |-------------------------------------------------------------------|      |  
    |0|1|2|stopper|0|2|stopper|0|1|3|stopper|3|stopper|stopper|4|stopper|<--- /
    |-------------------------------------------------------------------|     
                                   |
                                   |
                                   |
                                  \|/
                       |----------|-------------------------|
                       |context 0 | {0, 1, 2, 3, LastMidEnd}|
        RANK 0 ----    |----------|-------------------------|
                       |context 1 | {0, 2, LastMidEnd}      | 
                       |----------|-------------------------|
                       |context 2 | {0, 1, 3, 4, LastMidEnd}|
                       |----------|-------------------------|                          
  */

  //STEP 1: convert to a long vector with stopper between mids for different contexts
  uint16_t stopper = -1;
  std::vector<uint16_t> rank_all_mids;
  for(auto ctx : ctx_nzmids){
    for(auto mid: ctx){
      rank_all_mids.emplace_back(mid);
    }
    rank_all_mids.emplace_back(stopper);
  }

  //STEP 2: gather all rank_all_mids to rank 0
  //  prepare for later gatherv: get the size of each rank's rank_all_mids
  int rank_all_mids_size = rank_all_mids.size();
  std::vector<int> all_rank_mids_sizes;
  if(rank == 0) all_rank_mids_sizes.resize(num_proc);
  MPI_Gather(&rank_all_mids_size, 1, MPI_INT, all_rank_mids_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

  //  prepare for later gatherv: get the displacement of each rank's rank_all_mids
  int total_size = 0;
  std::vector<int> all_rank_mids_disps; 
  std::vector<uint16_t> global_all_mids;
  if(rank == 0){
    all_rank_mids_disps.resize(num_proc);

    #pragma omp parallel for num_threads(threads)
    for(int i = 0; i<num_proc; i++) all_rank_mids_disps[i] = all_rank_mids_sizes[i];
    exscan<int>(all_rank_mids_disps,threads); 

    total_size = all_rank_mids_disps.back() + all_rank_mids_sizes.back();
    global_all_mids.resize(total_size);
  }

  //  gather all the rank_all_mids (i.e. ctx_nzmids) to root
  MPI_Gatherv(rank_all_mids.data(),rank_all_mids_size, mpi_data<uint16_t>::type, \
    global_all_mids.data(), all_rank_mids_sizes.data(), all_rank_mids_disps.data(), mpi_data<uint16_t>::type, 0, MPI_COMM_WORLD);


  //STEP 3: turn the long vector global_all_mids back to rank 0's ctx_nzmids
  if(rank == 0){
    int num_stopper = 0;
    int num_ctx     = ctx_nzmids.size();
    for(int i = 0; i< total_size; i++) {
      uint16_t mid = global_all_mids[i];
      if(mid == stopper){
        num_stopper++;
      }else{
        ctx_nzmids[num_stopper % num_ctx].insert(mid); 
      }
    }

    //  Add extra space for marking end location for the last mid
    #pragma omp parallel for num_threads(threads)
    for(uint i = 0; i<ctx_nzmids.size(); i++){
      ctx_nzmids[i].insert(LastMidEnd);
    }
  }


}


// helper functions to help sum reduce a vector of things
void vSum ( uint64_t *, uint64_t *, int *, MPI_Datatype * );
void vSum(uint64_t *invec, uint64_t *inoutvec, int *len, MPI_Datatype *dtype)
{
    int i;
    for ( i=0; i<*len; i++ )
        inoutvec[i] += invec[i];
}


// each rank gets the local offsets for each context
// input: local value counts for all contexts, non-zero metric ids for all contexts, available threads, rank number
// output: (last argument) assigned local offsets for all contexts
void SparseDB::getLocalCtxOffset(const std::vector<uint64_t>& ctx_val_cnts, 
                                 const std::vector<std::set<uint16_t>>& ctx_nzmids,
                                 const int threads, 
                                 const int rank,
                                 std::vector<uint64_t>& ctx_off)
{
  assert(ctx_val_cnts.size() == ctx_nzmids.size());
  assert(ctx_val_cnts.size() == ctx_off.size() - 1);

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < ctx_val_cnts.size(); i++){
    ctx_off[i] = ctx_val_cnts[i] * CMS_val_tid_pair_SIZE;
    //empty context also has LastMidEnd in ctx_nzmids, so if the size is 1, offet should not count that pair for calculation
    if(rank == 0 && ctx_nzmids[i].size() > 1) ctx_off[i] += ctx_nzmids[i].size() * CMS_m_pair_SIZE; 
  }

  //get local offsets
  exscan<uint64_t>(ctx_off,threads); 

}


// allreduce the local offsets for each context to global
// input: rank's local offsets for each context
// output: (last argument) assigned global offsets for each context
void SparseDB::getGlobalCtxOffset(const std::vector<uint64_t>& local_ctx_off,
                                  std::vector<uint64_t>& global_ctx_off)
{
  assert(local_ctx_off.size() == global_ctx_off.size());

  //sum up local offsets to get global offsets
  MPI_Op vectorSum;
  MPI_Op_create((MPI_User_function *)vSum, 1, &vectorSum);
  MPI_Allreduce(local_ctx_off.data(), global_ctx_off.data(), local_ctx_off.size(), mpi_data<uint64_t>::type, vectorSum, MPI_COMM_WORLD);
  MPI_Op_free(&vectorSum);

}


// get the final global offsets for each context
// input: local value counts for all contexts, non-zero metric ids for all contexts, available threads, rank number
// output: global offsets for all contexts
void SparseDB::getCtxOffset(const std::vector<uint64_t>& ctx_val_cnts, 
                            const std::vector<std::set<uint16_t>>& ctx_nzmids,
                            const int threads, 
                            const int rank,
                            std::vector<uint64_t>& ctx_off)
{
  assert(ctx_val_cnts.size() == ctx_nzmids.size());
  assert(ctx_val_cnts.size() == ctx_off.size() - 1);

  std::vector<uint64_t> local_ctx_off (ctx_off.size(), 0);
  getLocalCtxOffset(ctx_val_cnts, ctx_nzmids, threads, rank, local_ctx_off);
  getGlobalCtxOffset(local_ctx_off, ctx_off);

}


//based on ctx offsets, assign ctx ids to different ranks
//input: global offsets for all contexts, number of processes, rank number
//output: rank's own responsible ctx list
void SparseDB::getMyCtxs(const std::vector<uint64_t>& ctx_off,
                         const int num_ranks, 
                         const int rank,
                         std::vector<uint32_t>& my_ctxs)
{
  assert(ctx_off.size() > 0);

  uint64_t total_size = ctx_off.back();
  uint64_t max_size_per_rank = round(total_size/num_ranks);
  uint64_t my_start = rank * max_size_per_rank;
  uint64_t my_end = (rank == num_ranks - 1) ? total_size : (rank + 1) * max_size_per_rank;

  for(uint i = 1; i<ctx_off.size(); i++){
    if(ctx_off[i] > my_start && ctx_off[i] <= my_end) my_ctxs.emplace_back(CTXID((i-1)));
  }

}


//update the global offsets with calculated Context info section size
//input: ctxcnt, available threads
//output: (last argument) updated ctx_off
void SparseDB::updateCtxOffset(const size_t ctxcnt, 
                               const int threads, 
                               std::vector<uint64_t>& ctx_off)
{
  assert(ctx_off.size() == ctxcnt + 1);

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < ctxcnt + 1; i++){
    ctx_off[i] += ctxcnt * CMS_ctx_info_SIZE + CMS_num_ctx_SIZE;
  }
}


//---------------------------------------------------------------------------
// get a list of profile info
//---------------------------------------------------------------------------

//given the bytes of the profile information, interpret it and return a tms_profile_info_t
//input: input bytes (the first byte of this prof info)
//output: return a tms_profile_info_t
void SparseDB::interpretOneProfInfo(const char *input,
                                    tms_profile_info_t& pi)
{
  interpretByte4(pi.tid,       input);
  interpretByte8(pi.num_vals,   input + TMS_tid_SIZE);
  interpretByte4(pi.num_nzctxs, input + TMS_tid_SIZE + TMS_num_val_SIZE);
  interpretByte8(pi.offset,    input + TMS_tid_SIZE + TMS_num_val_SIZE + TMS_num_nzctx_SIZE);

}


//read the Profile Information section of thread_major_sparse.db to get the list of profiles 
//input: available threads, MPI_File handle
//output: (last argument) assigned prof_info (a vector of tms_profile_info_t)
void SparseDB::readProfileInfo(const int threads, 
                               const MPI_File fh,
                               std::vector<tms_profile_info_t>& prof_info)
{
  //read the number of profiles
  uint32_t num_prof;
  SPARSE_exitIfMPIError(readAsByte4(num_prof,fh,0), __FUNCTION__ + std::string(": cannot read the number of profiles"));

  //read the whole Profile Information section
  int count = num_prof * TMS_prof_info_SIZE; 
  char input[count];
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_read_at(fh, TMS_total_prof_SIZE, &input, count, MPI_BYTE, &stat), 
    __FUNCTION__ + std::string(": cannot read the whole Profile Information section"));

  //interpret the section and store in a vector of tms_profile_info_t
  prof_info.resize(num_prof);
  #pragma omp parallel for num_threads(threads)
  for(int i = 0; i<count; i += TMS_prof_info_SIZE){
    tms_profile_info_t pi;
    interpretOneProfInfo(input + i, pi);
    prof_info[i/TMS_prof_info_SIZE] = pi;
  }

}


//---------------------------------------------------------------------------
// read and interpret one profie - CtxIdIdxPairs
//---------------------------------------------------------------------------
//interpret the input bytes and assign values to a TMS_CtxIdIdxPair
//input: input bytes
//output: (last argument) assigned TMS_CtxIdIdxPair
void SparseDB::interpretOneCtxIdIdxPair(const char *input,
                                        TMS_CtxIdIdxPair& ctx_pair)
{
  interpretByte4(ctx_pair.ctx_id,  input);
  interpretByte8(ctx_pair.ctx_idx, input + TMS_ctx_id_SIZE);
}


//read from ONE profile (at off of fh) and fill up ctx_id_idx_pairs, with known ctx_id_idx_pairs size
//input: file handle, offset where the data starts in the file
//output: (last argument) filled ctx_id_idx_pairs (a vector of TMS_CtxIdIdxPair)
int SparseDB::readCtxIdIdxPairs(const MPI_File fh, 
                                const MPI_Offset off, 
                                std::vector<TMS_CtxIdIdxPair>& ctx_id_idx_pairs)
{
  assert(ctx_id_idx_pairs.size() > 1);

  //read the whole ctx_id_idx_pairs chunk
  int count = ctx_id_idx_pairs.size() * TMS_ctx_pair_SIZE; 
  char input[count];
  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_read_at(fh,off,&input,count,MPI_BYTE,&stat));

  //interpret the chunk and store accordingly
  for(int i = 0; i<count; i += TMS_ctx_pair_SIZE){
    TMS_CtxIdIdxPair ciip;
    interpretOneCtxIdIdxPair(input + i, ciip);
    ctx_id_idx_pairs[i/TMS_ctx_pair_SIZE] = ciip;
  }

  return SPARSE_OK;
    
}


//in a vector of TMS_CtxIdIdxPair, find one with target context id
//input: target context id, the vector we are searching through, 
//       length to search for (searching range index will be 0..length-1), 
//       whether already found previous one, the previous found context id (it no previously found, this var will not be used)
//output: found idx / SPARSE_END(already end of the vector, not found) / SPARSE_NOT_FOUND
//        (last argument) found ctx_id_idx_pair will be inserted if found 
int SparseDB::findOneCtxIdIdxPair(const uint32_t target_ctx_id,
                                  const std::vector<TMS_CtxIdIdxPair>& profile_ctx_pairs,
                                  const uint length, 
                                  const bool found,
                                  const int found_ctx_idx, 
                                  std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs)
{
  int idx;

  if(found){
    idx = 1 + found_ctx_idx;

    //the profile_ctx_pairs has been searched through
    if(idx == (int)length) return SPARSE_END;
    
    //the ctx_id at idx
    uint32_t prof_ctx_id = profile_ctx_pairs[idx].ctx_id;
    if(prof_ctx_id == target_ctx_id){
      my_ctx_pairs.emplace_back(profile_ctx_pairs[idx]);
      return idx;
    }else if(prof_ctx_id > target_ctx_id){
      return SPARSE_NOT_FOUND; //back to original since this might be next target
    }else{ //prof_ctx_id < target_ctx_id, should not happen
      std::ostringstream ss;
      ss << __FUNCTION__ << "ctx id " << prof_ctx_id << " in a profile does not exist in the full ctx list while searching for " 
        << target_ctx_id << " with index " << idx;
      exitError(ss.str());
      return SPARSE_NOT_FOUND; //this should not be called if exit, but function requires return value
    }

  }else{
    TMS_CtxIdIdxPair target_ciip;
    target_ciip.ctx_id = target_ctx_id;
    idx = struct_member_binary_search(profile_ctx_pairs, target_ciip, &TMS_CtxIdIdxPair::ctx_id, length);
    if(idx != SPARSE_NOT_FOUND) my_ctx_pairs.emplace_back(profile_ctx_pairs[idx]);
    return idx;
  }
}


//from a group of TMS_CtxIdIdxPair of one profile, get the pairs related to a group of ctx_ids
//input: a vector of ctx_ids we are searching for, a vector of profile TMS_CtxIdIdxPairs we are searching through
//output: (last argument) a filled vector of TMS_CtxIdIdxPairs related to that group of ctx_ids
void SparseDB::findCtxIdIdxPairs(const std::vector<uint32_t>& ctx_ids,
                                 const std::vector<TMS_CtxIdIdxPair>& profile_ctx_pairs,
                                 std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs)
{
  assert(profile_ctx_pairs.size() > 1);

  uint n = profile_ctx_pairs.size() - 1; //since the last is LastNodeEnd
  int idx = -1; //index of current pair in profile_ctx_pairs
  bool found = false;
  uint32_t target;

  for(uint i = 0; i<ctx_ids.size(); i++){
    target = ctx_ids[i];
    int ret = findOneCtxIdIdxPair(target, profile_ctx_pairs, n, found, idx, my_ctx_pairs);
    if(ret == SPARSE_END) break;
    if(ret != SPARSE_NOT_FOUND){
      idx = ret;
      found = true;
    }    
  }

  //add one extra context pair for later use
  TMS_CtxIdIdxPair end_pair;
  end_pair.ctx_id = LastNodeEnd;
  end_pair.ctx_idx = profile_ctx_pairs[idx + 1].ctx_idx;
  my_ctx_pairs.emplace_back(end_pair);

  
  assert(my_ctx_pairs.size() <= ctx_ids.size() + 1);
}


// get the context id and index pairs for the group of contexts from one profile 
// input: prof_info for this profile, context ids we want, file handle
// output: found context id and idx pairs
int SparseDB::getMyCtxIdIdxPairs(const tms_profile_info_t& prof_info,
                                 const std::vector<uint32_t>& ctx_ids,
                                 const MPI_File fh,
                                 std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs)
{
  MPI_Offset ctx_pairs_offset = prof_info.offset + prof_info.num_vals * (TMS_val_SIZE + TMS_mid_SIZE);
  std::vector<TMS_CtxIdIdxPair> prof_ctx_pairs (prof_info.num_nzctxs + 1);
  if(prof_ctx_pairs.size() == 1) return SPARSE_ERR;

  SPARSE_exitIfMPIError(readCtxIdIdxPairs(fh, ctx_pairs_offset, prof_ctx_pairs), 
                          __FUNCTION__ + std::string(": cannot read context pairs for profile ") + std::to_string(prof_info.tid));

  findCtxIdIdxPairs(ctx_ids, prof_ctx_pairs, my_ctx_pairs);
  return SPARSE_OK;
}

//---------------------------------------------------------------------------
// read and interpret one profie - ValMid
//---------------------------------------------------------------------------
// read all bytes for a group of contexts from one profile
void SparseDB::readValMidsBytes(const std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs,
                                const tms_profile_info_t& prof_info,
                                const MPI_File fh,
                                std::vector<char>& bytes)
{

  uint64_t first_ctx_idx = my_ctx_pairs.front().ctx_idx;
  uint64_t last_ctx_idx  = my_ctx_pairs.back().ctx_idx;
  MPI_Offset val_mid_start_pos = prof_info.offset + first_ctx_idx * (TMS_val_SIZE + TMS_mid_SIZE);
  int val_mid_count = (last_ctx_idx - first_ctx_idx) * (TMS_val_SIZE + TMS_mid_SIZE);
  bytes.resize(val_mid_count);
  MPI_Status stat;
  if(val_mid_count != 0) {
    SPARSE_exitIfMPIError(MPI_File_read_at(fh, val_mid_start_pos, bytes.data(), val_mid_count, MPI_BYTE, &stat),
                        __FUNCTION__ + std::string(": cannot read real data for profile ") + std::to_string(prof_info.tid));
  }

}

//create and return a new MetricValBlock
SparseDB::MetricValBlock SparseDB::createNewMetricValBlock(const hpcrun_metricVal_t val, 
                                                           const uint16_t mid,
                                                           const uint32_t tid)
{
  MetricValBlock mvb;
  mvb.mid = mid;
  std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_tids;
  values_tids.emplace_back(val, tid);
  mvb.values_tids = values_tids;
  return mvb;
}

//create and return a new CtxMetricBlock
SparseDB::CtxMetricBlock SparseDB::createNewCtxMetricBlock(const hpcrun_metricVal_t val, 
                                                 const uint16_t mid,
                                                 const uint32_t tid,
                                                 const uint32_t ctx_id)
{
  //create a new MetricValBlock for this mid, val, tid
  MetricValBlock mvb = createNewMetricValBlock(val, mid, tid);

  //create a vector of MetricValBlock for this context
  std::vector<MetricValBlock> mvbs;
  mvbs.emplace_back(mvb);

  //store it
  CtxMetricBlock cmb = {ctx_id, mvbs};
  return cmb;
}

//insert a pair of val and metric id to a CtxMetBlock they belong to (ctx id matches)
void SparseDB::insertValMidPair2OneCtxMetBlock(const hpcrun_metricVal_t val, 
                                               const uint16_t mid,
                                               const uint32_t tid,
                                               CtxMetricBlock& cmb)
{
  //find if this mid exists
  std::vector<MetricValBlock>& metric_blocks = cmb.metrics;
  std::vector<MetricValBlock>::iterator it = std::find_if(metric_blocks.begin(), metric_blocks.end(), 
                  [&mid] (const MetricValBlock& mvb) { 
                    return mvb.mid == mid; 
                  });

  if(it != metric_blocks.end()){ //found mid
    it->values_tids.emplace_back(val, tid);
  }else{ 
    metric_blocks.emplace_back(createNewMetricValBlock(val, mid, tid));
  }
}


//insert a triple of val, metric id and ctx_id to the correct place of ctx_met_blocks
void SparseDB::insertValMidCtxId2CtxMetBlocks(const hpcrun_metricVal_t val, 
                                              const uint16_t mid,
                                              const uint32_t tid,
                                              const uint32_t ctx_id,
                                              std::vector<CtxMetricBlock>& ctx_met_blocks)
{

  std::vector<CtxMetricBlock>::iterator got = std::find_if(ctx_met_blocks.begin(), ctx_met_blocks.end(), 
                    [&ctx_id] (const CtxMetricBlock& cmb) { 
                      return cmb.ctx_id == ctx_id; 
                    });

  if (got != ctx_met_blocks.end()){ //ctx_id found
    insertValMidPair2OneCtxMetBlock(val, mid, tid, *got);
  }else{
    CtxMetricBlock cmb = createNewCtxMetricBlock(val, mid, tid, ctx_id);
    ctx_met_blocks.emplace_back(cmb);
  }
}

//interpret the input bytes, assign value to val and metric id
void SparseDB::interpretOneValMidPair(const char *input,
                                      hpcrun_metricVal_t& val,
                                      uint16_t& mid)
{
  interpretByte8(val.bits, input);
  interpretByte2(mid,      input + TMS_val_SIZE);
}       

//interpret the bytes that has all val_mids for a group of contexts from one profile
//assign values accordingly to ctx_met_blocks
void SparseDB::interpretValMidsBytes(char *vminput,
                                     const uint32_t tid,
                                     const std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs,
                                     std::vector<CtxMetricBlock>& ctx_met_blocks)
{
  char* ctx_met_input = vminput;
  //for each context, keep track of the values, metric ids, and thread ids
  for(uint c = 0; c < my_ctx_pairs.size() - 1; c++) 
  {
    uint32_t ctx_id  = my_ctx_pairs[c].ctx_id;
    uint64_t ctx_idx = my_ctx_pairs[c].ctx_idx;
    uint64_t num_val_this_ctx = my_ctx_pairs[c + 1].ctx_idx - ctx_idx;

    //char* ctx_met_input = vminput + (ctx_idx - my_ctx_pairs.front().ctx_idx) * (TMS_val_SIZE + TMS_mid_SIZE);
    for(uint i = 0; i < num_val_this_ctx; i++){
      hpcrun_metricVal_t val;
      uint16_t mid;
      interpretOneValMidPair(ctx_met_input, val, mid);
      insertValMidCtxId2CtxMetBlocks(val, mid, tid, ctx_id, ctx_met_blocks);
      ctx_met_input += (TMS_val_SIZE + TMS_mid_SIZE);
    }

  }
}

//read all the data for a group of contexts, from one profile with its prof_info, store data to ctx_met_blocks 
void SparseDB::readOneProfile(const std::vector<uint32_t>& ctx_ids, 
                              const tms_profile_info_t& prof_info,
                              const MPI_File fh,
                              std::vector<CtxMetricBlock>& ctx_met_blocks)
{
  //get the id and idx pairs for this group of ctx_ids from this profile
  std::vector<TMS_CtxIdIdxPair> my_ctx_pairs;
  int ret = getMyCtxIdIdxPairs(prof_info, ctx_ids, fh, my_ctx_pairs);
  if(ret != SPARSE_OK) return;

  //read all values and metric ids for this group of ctx at once
  std::vector<char> vmbytes;
  readValMidsBytes(my_ctx_pairs, prof_info, fh, vmbytes);
  //interpret the bytes and store them in ctx_met_blocks
  interpretValMidsBytes(vmbytes.data(), prof_info.tid, my_ctx_pairs, ctx_met_blocks);

  assert(my_ctx_pairs.size() - 1 <= ctx_met_blocks.size());
}


//---------------------------------------------------------------------------
// read and interpret ALL profies 
//---------------------------------------------------------------------------
//merge source CtxMetBlock to destination CtxMetBlock
//it has to be used for two blocks with the same ctx_id
void SparseDB::mergeCtxMetBlocks(const CtxMetricBlock& source,
                                 CtxMetricBlock& dest)
{
  assert(source.ctx_id == dest.ctx_id);

  std::vector<MetricValBlock>& dest_metrics = dest.metrics;
  for(MetricValBlock source_m : source.metrics){
    const uint16_t& mid = source_m.mid;
    std::vector<MetricValBlock>::iterator mvb_i = std::find_if(dest_metrics.begin(), dest_metrics.end(), 
                [&mid] (const MetricValBlock& mvb) { 
                  return mvb.mid == mid; 
                });

    if(mvb_i == dest_metrics.end()){
      dest_metrics.emplace_back(source_m);
    }else{
      mvb_i->values_tids.reserve(mvb_i->values_tids.size() + source_m.values_tids.size());
      mvb_i->values_tids.insert(mvb_i->values_tids.end(), source_m.values_tids.begin(), source_m.values_tids.end());
    }
  }
}


//merge all the CtxMetricBlocks from all the threads for one Ctx (one ctx_id) to dest
void SparseDB::mergeOneCtxAllThreadBlocks(const std::vector<std::vector<CtxMetricBlock> *>& threads_ctx_met_blocks,
                                          CtxMetricBlock& dest)
{
  uint32_t ctx_id = dest.ctx_id;
  for(uint i = 0; i < threads_ctx_met_blocks.size(); i++){
    std::vector<CtxMetricBlock> thread_cmb = *threads_ctx_met_blocks[i];
    std::vector<CtxMetricBlock>::iterator cmb_i = std::find_if(thread_cmb.begin(), thread_cmb.end(), 
                    [&ctx_id] (const CtxMetricBlock& cmb) { 
                      return cmb.ctx_id == ctx_id; 
                    });
    
    if(cmb_i != thread_cmb.end()){
      mergeCtxMetBlocks(*cmb_i,dest);
    }
  }
}

//within each CtxMetricBlock sort based on metric id, within each MetricValBlock, sort based on thread id
void SparseDB::sortCtxMetBlocks(std::vector<CtxMetricBlock>& ctx_met_blocks)
{

  #pragma omp for
  for(uint i = 0; i < ctx_met_blocks.size(); i++){   
    CtxMetricBlock& hcmb = ctx_met_blocks[i];

    for(uint j = 0; j < hcmb.metrics.size(); j++){
      MetricValBlock& mvb = hcmb.metrics[j];
      std::sort(mvb.values_tids.begin(), mvb.values_tids.end(), 
        [](const std::pair<hpcrun_metricVal_t,uint32_t>& lhs, const std::pair<hpcrun_metricVal_t,uint32_t>& rhs) {
          return lhs.second < rhs.second;
        });  
      mvb.num_values = mvb.values_tids.size(); //update each mid's num_values
    }

    std::sort(hcmb.metrics.begin(), hcmb.metrics.end(), [](const MetricValBlock& lhs, const MetricValBlock& rhs) {
        return lhs.mid < rhs.mid;
    });    
    
  }

}

//read all the profiles and convert data to appropriate bytes for a group of contexts
void SparseDB::readProfiles(const std::vector<uint32_t>& ctx_ids, 
                            const std::vector<tms_profile_info_t>& prof_info,
                            int threads,
                            MPI_File fh,
                            std::vector<CtxMetricBlock>& ctx_met_blocks)
{
  std::vector<CtxMetricBlock> empty;
  std::vector<std::vector<CtxMetricBlock> * > threads_ctx_met_blocks(threads, &empty);
  ctx_met_blocks.resize(ctx_ids.size());

  #pragma omp parallel num_threads(threads) 
  {
    std::vector<CtxMetricBlock> thread_cmb;
    threads_ctx_met_blocks[omp_get_thread_num()] = &thread_cmb;

    //read all profiles for this ctx_ids group
    #pragma omp for 
    for(uint i = 0; i < prof_info.size(); i++){
      tms_profile_info_t pi = prof_info[i];
      readOneProfile(ctx_ids, pi, fh, thread_cmb);
    }

    //merge all threads_ctx_met_blocks to global transpose_helper_cmbs
    #pragma omp for
    for(uint c = 0; c < ctx_ids.size(); c++){
      ctx_met_blocks[c].ctx_id = ctx_ids[c];
      mergeOneCtxAllThreadBlocks(threads_ctx_met_blocks,ctx_met_blocks[c]);
    }

    //sort ctx_met_blocks
    sortCtxMetBlocks(ctx_met_blocks);
  }


  ctx_met_blocks.erase(std::remove_if(ctx_met_blocks.begin(),
                                      ctx_met_blocks.end(),
                                      [](const CtxMetricBlock cmb){ return cmb.metrics.size() == 0;}), 
                        ctx_met_blocks.end());
                        
}


//---------------------------------------------------------------------------
// convert ctx_met_blocks to correct bytes for writing
//---------------------------------------------------------------------------
//convert one MetricValBlock to bytes at bytes location
//return number of bytes converted
int SparseDB::convertOneMetricValBlock(const MetricValBlock& mvb,                                        
                                       char *bytes)
{
  char* bytes_pos = bytes;
  for(uint i = 0; i < mvb.values_tids.size(); i++){
    auto& pair = mvb.values_tids[i];
    convertToByte8(pair.first.bits, bytes_pos);
    convertToByte4(pair.second,     bytes_pos + CMS_val_SIZE);
    bytes_pos += CMS_val_tid_pair_SIZE;
  }

  return (bytes_pos - bytes);
}

//convert ALL MetricValBlock of one CtxMetricBlock to bytes at bytes location
//return number of bytes converted
int SparseDB::convertCtxMetrics(const std::vector<MetricValBlock>& metrics,                                        
                                char *bytes)
{
  uint64_t num_vals = 0;

  char* mvb_pos = bytes; 
  for(uint i = 0; i < metrics.size(); i++){
    MetricValBlock mvb = metrics[i];
    num_vals += mvb.num_values ;

    int bytes_converted = convertOneMetricValBlock(mvb, mvb_pos);
    mvb_pos += bytes_converted;
  }

  assert(num_vals == (uint)(mvb_pos - bytes)/CMS_val_tid_pair_SIZE);

  return (mvb_pos - bytes);
}

//build metric id and idx pairs for one context as bytes to bytes location
//return number of bytes built
int SparseDB::buildCtxMetIdIdxPairsBytes(const std::vector<MetricValBlock>& metrics,                                        
                                         char *bytes)
{
  char* bytes_pos = bytes;
  uint64_t m_idx = 0;
  for(uint i =0; i < metrics.size() + 1; i++){   
    uint16_t mid = (i == metrics.size()) ? LastMidEnd : metrics[i].mid;
    convertToByte2(mid,   bytes_pos);
    convertToByte8(m_idx, bytes_pos + CMS_mid_SIZE);
    bytes_pos += CMS_m_pair_SIZE;
    if(i < metrics.size()) m_idx += metrics[i].num_values;
  }

  return (bytes_pos - bytes);
}

//convert one CtxMetricBlock to bytes at bytes location
//also assigne value to num_nzmids and num_vals of this context
//return number of bytes converted
int SparseDB::convertOneCtxMetricBlock(const CtxMetricBlock& cmb,                                        
                                       char *bytes,
                                       uint16_t& num_nzmids,
                                       uint64_t& num_vals)
{
  std::vector<MetricValBlock> metrics = cmb.metrics;
  num_nzmids = metrics.size();

  int bytes_converted = convertCtxMetrics(metrics, bytes);
  num_vals = bytes_converted/CMS_val_tid_pair_SIZE;

  bytes_converted += buildCtxMetIdIdxPairsBytes(metrics, bytes + num_vals * CMS_val_tid_pair_SIZE);

  return bytes_converted;
}

//convert one cms_ctx_info_t to bytes at bytes location
int SparseDB::convertOneCtxInfo(const cms_ctx_info_t& ctx_info,                                        
                                char *bytes)
{
  convertToByte4(ctx_info.ctx_id,   bytes);
  convertToByte8(ctx_info.num_vals,  bytes + CMS_ctx_id_SIZE);
  convertToByte2(ctx_info.num_nzmids,bytes + CMS_ctx_id_SIZE + CMS_num_val_SIZE);
  convertToByte8(ctx_info.offset,   bytes + CMS_ctx_id_SIZE + CMS_num_val_SIZE + CMS_num_nzmid_SIZE);
  
  return CMS_ctx_info_SIZE;
}


//convert one ctx (whose id is ctx_id), including info and metrics to bytes
//info will be converted to info_bytes, metrics will be converted to met_bytes
//info_byte_cnt and met_byte_cnt will be assigned number of bytes converted
void SparseDB::convertOneCtx(const uint32_t ctx_id, 
                             const std::vector<uint64_t>& ctx_off,    
                             const std::vector<CtxMetricBlock>& ctx_met_blocks, 
                             const uint64_t first_ctx_off,
                             uint& met_byte_cnt,
                             uint& info_byte_cnt, 
                             char* met_bytes,                                 
                             char* info_bytes)
{
  //INFO_BYTES
  uint64_t num_vals = 0;
  uint16_t num_nzmids = 0;
  uint64_t offset = ctx_off[CTX_VEC_IDX(ctx_id)]; 

  //METRIC_BYTES (also update num_vals, num_nzmids)
  auto cmb_i = std::find_if(ctx_met_blocks.begin(), ctx_met_blocks.end(), 
                      [&ctx_id] (const CtxMetricBlock& cmb) { 
                        return cmb.ctx_id == ctx_id; 
                      });

  if(cmb_i != ctx_met_blocks.end()){ //ctx_met_blocks has the ctx_id
    met_byte_cnt += convertOneCtxMetricBlock(*cmb_i, met_bytes + offset - first_ctx_off, num_nzmids, num_vals);
  }

  if(num_nzmids != 0)
    if(offset + num_vals * CMS_val_tid_pair_SIZE + (num_nzmids+1) * CMS_m_pair_SIZE !=  ctx_off[CTX_VEC_IDX(ctx_id+2)])
      exitError("collected cct data (num_vals/num_nzmids) were wrong !");

  //INFO_BYTES
  cms_ctx_info_t ci = {ctx_id, num_vals, num_nzmids, offset};
  info_byte_cnt += convertOneCtxInfo(ci, info_bytes);
}

//convert a group of contexts to appropriate bytes 
void SparseDB::ctxBlocks2Bytes(const std::vector<CtxMetricBlock>& ctx_met_blocks, 
                               const std::vector<uint64_t>& ctx_off, 
                               const std::vector<uint32_t>& ctx_ids,
                               int threads,
                               std::vector<char>& info_bytes,
                               std::vector<char>& metrics_bytes)
{
  assert(ctx_met_blocks.size() > 0);
  assert(ctx_met_blocks.size() <= ctx_ids.size());
  assert(ctx_off.size() > 0);

  uint64_t first_ctx_off =  ctx_off[CTX_VEC_IDX(ctx_ids[0])]; 
  uint info_byte_cnt = 0;
  uint met_byte_cnt  = 0;

  #pragma omp parallel for num_threads(threads) reduction(+:info_byte_cnt, met_byte_cnt)
  for(uint i = 0; i<ctx_ids.size(); i++ ){
    uint32_t ctx_id = ctx_ids[i];
    convertOneCtx(ctx_id, ctx_off, ctx_met_blocks, first_ctx_off, met_byte_cnt, info_byte_cnt, metrics_bytes.data(), info_bytes.data() + i * CMS_ctx_info_SIZE );
  } 

  if(info_byte_cnt != info_bytes.size())    exitError("the count of info_bytes converted is not as expected"
    + std::to_string(info_byte_cnt) + " != " + std::to_string(info_bytes.size()));
 
  if(met_byte_cnt  != metrics_bytes.size()) exitError("the count of metrics_bytes converted is not as expected" 
    + std::to_string(met_byte_cnt) + " != " + std::to_string(metrics_bytes.size()));

}


//given ctx_met_blocks, convert all and write everything for the group of contexts, to the ofh file 
void SparseDB::writeCtxGroup(const std::vector<uint32_t>& ctx_ids,
                             const std::vector<uint64_t>& ctx_off,
                             const std::vector<CtxMetricBlock>& ctx_met_blocks,
                             const int threads,
                             MPI_File ofh)
{
  assert(ctx_met_blocks.size() <= ctx_ids.size());
  
  std::vector<char> info_bytes (CMS_ctx_info_SIZE * ctx_ids.size());

  uint32_t first_ctx_id = ctx_ids.front();
  uint32_t last_ctx_id  = ctx_ids.back();
  int metric_bytes_size = ctx_off[CTX_VEC_IDX(last_ctx_id) + 1] - ctx_off[CTX_VEC_IDX(first_ctx_id)];
  std::vector<char> metrics_bytes (metric_bytes_size);
  
  ctxBlocks2Bytes(ctx_met_blocks, ctx_off, ctx_ids, threads, info_bytes, metrics_bytes);

  MPI_Status stat;
  MPI_Offset info_off = CMS_num_ctx_SIZE + CTX_VEC_IDX(first_ctx_id) * CMS_ctx_info_SIZE;
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, info_off, info_bytes.data(), info_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Info Bytes for ctx ") + std::to_string(first_ctx_id) + " to ctx " + std::to_string(last_ctx_id));

  MPI_Offset metrics_off = ctx_off[CTX_VEC_IDX(first_ctx_id)];
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, metrics_off, metrics_bytes.data(), metrics_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Metrics Bytes for ctx ") + std::to_string(first_ctx_id) + " to ctx " + std::to_string(last_ctx_id));
}


//---------------------------------------------------------------------------
// read and write for all contexts in this rank's list
//---------------------------------------------------------------------------
//read a context group's data and write them out
void SparseDB::rwOneCtxGroup(const std::vector<uint32_t>& ctx_ids, 
                             const std::vector<tms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                             const MPI_File fh, 
                             MPI_File ofh)
{
  std::vector<CtxMetricBlock> ctx_met_blocks;
  readProfiles(ctx_ids, prof_info, threads, fh, ctx_met_blocks);

  writeCtxGroup(ctx_ids, ctx_off, ctx_met_blocks, threads, ofh);
}

//read ALL context groups' data and write them out
void SparseDB::rwAllCtxGroup(const std::vector<uint32_t>& my_ctxs, 
                             const std::vector<tms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                             const MPI_File fh, 
                             MPI_File ofh)
{
  //For each ctx group (< memory limit) this rank is in charge of, read and write
  std::vector<uint32_t> ctx_ids;
  size_t cur_size = 0;
  for(uint i =0; i<my_ctxs.size(); i++){
    uint32_t ctx_id = my_ctxs[i];
    size_t cur_ctx_size = ctx_off[CTX_VEC_IDX(ctx_id) + 1] - ctx_off[CTX_VEC_IDX(ctx_id)];

    if((cur_size + cur_ctx_size) <= pow(10,4)) { //temp 10^4 TODO: user-defined memory limit
      ctx_ids.emplace_back(ctx_id);
      cur_size += cur_ctx_size;
    }else{
      rwOneCtxGroup(ctx_ids, prof_info, ctx_off, threads, fh, ofh);
      ctx_ids.clear();
      ctx_ids.emplace_back(ctx_id);
      cur_size = cur_ctx_size;
    }   

    // final ctx group
    if((i == my_ctxs.size() - 1) && (ctx_ids.size() != 0)) rwOneCtxGroup(ctx_ids, prof_info, ctx_off, threads, fh, ofh);
  }
}


void SparseDB::writeCCTMajor(const std::vector<uint64_t>& ctx_nzval_cnts, 
                             std::vector<std::set<uint16_t>>& ctx_nzmids,
                             const int ctxcnt, 
                             const int world_rank, 
                             const int world_size, 
                             const int threads)
{
  //Prepare a union ctx_nzmids
  unionMids(ctx_nzmids,world_rank,world_size, threads/world_size);

  //Get context global final offsets for cct_major_sparse.db
  std::vector<uint64_t> ctx_off (ctxcnt + 1);
  getCtxOffset(ctx_nzval_cnts, ctx_nzmids, threads/world_size, world_rank, ctx_off);
  std::vector<uint32_t> my_ctxs;
  getMyCtxs(ctx_off, world_size, world_rank, my_ctxs);
  updateCtxOffset(ctxcnt, threads/world_size, ctx_off);


  //Prepare files to read and write, get the list of profiles
  MPI_File thread_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "thread_major_sparse.db").c_str(),
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &thread_major_f);
  MPI_File cct_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "cct_major_sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &cct_major_f);

  std::vector<tms_profile_info_t> prof_info;
  readProfileInfo(threads/world_size, thread_major_f, prof_info);

  SPARSE_exitIfMPIError(writeAsByte4(ctxcnt,cct_major_f,0), __FUNCTION__ + std::string(": write the number of contexts"));
  rwAllCtxGroup(my_ctxs, prof_info, ctx_off, threads/world_size, thread_major_f, cct_major_f);

  //Close both files
  MPI_File_close(&thread_major_f);
  MPI_File_close(&cct_major_f);


}


//***************************************************************************
// general - YUMENG
//***************************************************************************

void SparseDB::merge(int threads, std::size_t ctxcnt) {
  int world_rank;
  int world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  {
    util::log::debug msg{false};  // Switch to true for CTX id printouts
    msg << "CTXs (" << world_rank << ":" << outputs.size() << "): "
        << ctxcnt;
  }

  std::vector<uint64_t> ctx_nzval_cnts (ctxcnt,0);
  std::set<uint16_t> empty;
  std::vector<std::set<uint16_t>> ctx_nzmids(ctxcnt,empty);
  writeThreadMajor(threads,world_rank,world_size, ctx_nzval_cnts,ctx_nzmids);
  writeCCTMajor(ctx_nzval_cnts,ctx_nzmids, ctxcnt, world_rank, world_size, threads);
  
}


#if 0
//Jonathon's original code
void SparseDB::merge(int threads) {
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  if(world_rank != 0) {
    // Tell rank 0 all about our data
    {
      Gather<uint32_t> g;
      GatherStrings gs;
      for(const auto& tp: outputs.citerate()) {
        auto& attr = tp.first->attributes;
        g.add(attr.has_hostid() ? attr.hostid() : 0);
        g.add(attr.has_mpirank() ? attr.mpirank() : 0);
        g.add(attr.has_threadid() ? attr.threadid() : 0);
        g.add(attr.has_procid() ? attr.procid() : 0);
        gs.add(tp.second.string());
      }
      g.gatherN(6);
      gs.gatherN(7);
    }

    // Open up the output file. We use MPI's I/O substrate to make sure things
    // work in the end. Probably.
    MPI_File of;
    MPI_File_open(MPI_COMM_WORLD, (dir / "sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &of);

    // Make sure the file is truncated before we start writing stuff
    MPI_File_set_size(of, 0);

    // Shift into the worker code
    mergeN(threads, of);

    // Close up
    MPI_File_close(&of);

  } else {
    // Gather the data from all the workers, build a big attributes table
    std::vector<std::pair<ThreadAttributes, stdshim::filesystem::path>> woutputs;
    {
      auto g = Gather<uint32_t>::gather0(6);
      auto gs = GatherStrings::gather0(7);
      for(std::size_t peer = 1; peer < gs.size(); peer++) {
        auto& a = g[peer];
        auto& s = gs[peer];
        for(std::size_t i = 0; i < s.size(); i++) {
          ThreadAttributes attr;
          attr.hostid(a.at(i*4));
          attr.mpirank(a.at(i*4 + 1));
          attr.threadid(a.at(i*4 + 2));
          attr.procid(a.at(i*4 + 3));
          woutputs.emplace_back(std::move(attr), std::move(s.at(i)));
        }
      }
    }

    // Copy our bits in too.
    for(const auto& tp: outputs.citerate()){
      auto& attr = tp.first->attributes;
       woutputs.emplace_back(tp.first->attributes, tp.second);
    }

    // Open up the output file. We use MPI's I/O substrate to make sure things
    // work in the end. Probably.
    MPI_File of;
    MPI_File_open(MPI_COMM_WORLD, (dir / "sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &of);

    // Make sure the file is truncated before we start writing stuff
    MPI_File_set_size(of, 0);

    // Shift into the worker code
    merge0(threads, of, woutputs);

    // Close up and clean up
    MPI_File_close(&of);
    /* for(const auto& tp: woutputs) */
    /*   stdshim::filesystem::remove(tp.second); */
  }
}
#endif

//local exscan over a vector of T, value after exscan will be stored in the original vector
template <typename T>
void SparseDB::exscan(std::vector<T>& data, int threads) {
  int n = data.size();
  int rounds = ceil(std::log2(n));
  std::vector<T> tmp (n);

  for(int i = 0; i<rounds; i++){
    #pragma omp parallel for num_threads(threads)
    for(int j = 0; j < n; j++){
      int p = (int)pow(2.0,i);
      tmp.at(j) = (j<p) ?  data.at(j) : data.at(j) + data.at(j-p);
    }
    if(i<rounds-1) data = tmp;
  }

  if(n>0) data[0] = 0;
  #pragma omp parallel for num_threads(threads)
  for(int i = 1; i < n; i++){
    data[i] = tmp[i-1];
  }
}


//binary search over a vector of T, unlike std::binary_search, which only returns true/false, 
//this returns the idx of found one, SPARSE_ERR as NOT FOUND
template <typename T, typename MemberT>
int SparseDB::struct_member_binary_search(const std::vector<T>& datas, const T target, const MemberT target_type, const int length) {
  int m;
  int L = 0;
  int R = length - 1;
  while(L <= R){
    m = (L + R) / 2;

    auto target_val = target.*target_type;
    auto comp_val   = datas[m].*target_type;

    if(comp_val < target_val){
      L = m + 1;
    }else if(comp_val > target_val){
      R = m - 1;
    }else{ //find match
      return m;
    }
  }
  return SPARSE_NOT_FOUND; 
}



//use for MPI error 
void SparseDB::exitMPIError(int error_code, std::string info)
{
  char estring[MPI_MAX_ERROR_STRING];
  int len;
  MPI_Error_string(error_code, estring, &len);
  //util::log::fatal() << info << ": " << estring;
  std::cerr << "FATAL: " << info << ": " << estring << "\n";
  MPI_Abort(MPI_COMM_WORLD, error_code);
}

//use for regular error
void SparseDB::exitError(std::string info)
{
  //TODO: consider how to terminate mpi processes gracefully
  //util::log::fatal() << info << "\n";
  std::cerr << "FATAL: " << info << "\n";
  MPI_Abort(MPI_COMM_WORLD, MPI_ERR_OTHER);
}


