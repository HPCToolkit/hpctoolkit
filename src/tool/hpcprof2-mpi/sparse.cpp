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
#include <lib/profile/mpi/all.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof/tms-format.h>
#include <lib/prof/cms-format.h>

#include <sstream>
#include <iomanip>
#include <iostream>

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

  ctxcnt = contexts.size();
}

void SparseDB::notifyThreadFinal(const Thread::Temporary& tt) {
  const auto& t = tt.thread();

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
  sm.tid = t.attributes.threadid().value_or(0);
  sm.num_vals = values.size();
  sm.num_cct_nodes = contexts.size();
  sm.num_nz_cct_nodes = coffsets.size() - 1; //since there is an extra end node YUMENG
  sm.values = values.data();
  sm.mids = mids.data();
  sm.cct_node_ids = cids.data();
  sm.cct_node_idxs = coffsets.data();

  // Set up the output temporary file.
  stdshim::filesystem::path outfile;
  int world_rank;
  {
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

void SparseDB::write()
{
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  if(world_rank != 0) return;

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
      auto vv = m.getFor(c);
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

  //Add the extra ctx id and offset pair, to mark the end of ctx
  cids.push_back(LastNodeEnd);
  coffsets.push_back(values.size());

  // Put together the sparse_metrics structure
  hpcrun_fmt_sparse_metrics_t sm;
  sm.tid = 0;
  sm.num_vals = values.size();
  sm.num_cct_nodes = contexts.size();
  sm.num_nz_cct_nodes = coffsets.size() - 1; //since there is an extra end node 
  sm.values = values.data();
  sm.mids = mids.data();
  sm.cct_node_ids = cids.data();
  sm.cct_node_idxs = coffsets.data();

  // Set up the output temporary file.
  summaryOut = dir / "tmp-summary.sparse-db";
  std::FILE* of = std::fopen(summaryOut.c_str(), "wb");
  if(!of) util::log::fatal() << "Unable to open temporary summary sparse-db file for output!";

  // Spit it all out, and close up.
  if(hpcrun_fmt_sparse_metrics_fwrite(&sm, of) != HPCFMT_OK)
    util::log::fatal() << "Error writing out temporary summary sparse-db!";
  std::fclose(of);
}

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
HPCPROF-tmsdb_____
[hdr:
  (version: 0)
]
[Id tuples for 121 profiles
  0[(SUMMARY: 0) ]
  1[(RANK: 0) (THREAD: 0) ]
  2[(RANK: 0) (THREAD: 1) ]
  ...
]
[Profile informations for 72 profiles
  0[(id_tuple_ptr: 23) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 12984) (num_nzctxs: 8353) (starting location: 489559)]
  1[(id_tuple_ptr: 31) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 4422) (num_nzctxs: 3117) (starting location: 886225)]
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
// profile id tuples - format conversion with ThreadAttribute and IntPair
//---------------------------------------------------------------------------
//builds a new ID tuple from ThreadAttributes
//TODO: we will build Id Tuple in hpcrun in the future, this is temporary
tms_id_tuple_t SparseDB::buildIdTuple(const hpctoolkit::ThreadAttributes& ta,
                                      const int rank)
{
  tms_id_t rank_idx;
  rank_idx.kind = RANK;
  rank_idx.index = (uint64_t)ta.mpirank().value();

  tms_id_t thread_idx;
  thread_idx.kind = THREAD;
  thread_idx.index = (uint64_t)ta.threadid().value();

  tms_id_t* ids = (tms_id_t*)malloc(2 * sizeof(tms_id_t));
  ids[0] = rank_idx;
  ids[1] = thread_idx;

  tms_id_tuple_t tuple;
  tuple.length = 2;
  tuple.rank = rank;
  tuple.ids = ids;
  tuple.prof_info_idx = not_assigned; 
  tuple.all_at_root_idx = not_assigned;

  return tuple;

}

//builds an ID tuple for summary file
tms_id_tuple_t SparseDB::buildSmryIdTuple()
{
  tms_id_t summary_idx;
  summary_idx.kind = SUMMARY;
  summary_idx.index = (uint64_t)0;

  tms_id_t* ids = (tms_id_t*)malloc(1 * sizeof(tms_id_t));
  ids[0] = summary_idx;

  tms_id_tuple_t tuple;
  tuple.length = 1;
  tuple.rank = 0; //summary tuple is always at root rank
  tuple.ids = ids;
  tuple.prof_info_idx = not_assigned; 
  tuple.all_at_root_idx = not_assigned;

  return tuple;
}

//transfer from outputs(ThreadAttributes:filename) to sparseInputs(id_tuple:filename)
void SparseDB::assignSparseInputs(int world_rank)
{
  for(const auto& tp : outputs.citerate()){
    tms_id_tuple_t tuple = buildIdTuple(tp.first->attributes, world_rank);
    sparseInputs.emplace_back(tuple, tp.second.string());
  }

  if(world_rank == 0){
    tms_id_tuple_t tuple = buildSmryIdTuple();
    sparseInputs.emplace_back(tuple, summaryOut.string());
  }

}


//get my list of tms_id_tuple_t from sparseInputs
std::vector<tms_id_tuple_t> SparseDB::getMyIdTuples()
{
  std::vector<tms_id_tuple_t> all_tuples;
  for(const auto& tp : sparseInputs){
    all_tuples.emplace_back(tp.first);
  }

  assert(all_tuples.size() == sparseInputs.size());
  return all_tuples;
}


//convert a vector of tuples to a vector of uint16 and uint32 pairs
std::vector<std::pair<uint16_t, uint64_t>> 
SparseDB::tuples2IntPairs(const std::vector<tms_id_tuple_t>& all_tuples)
{
  std::vector<std::pair<uint16_t, uint64_t>> pairs;
  for(auto& tuple : all_tuples){
    pairs.emplace_back(tuple.length, (uint64_t)tuple.rank);
    for(uint i = 0; i < tuple.length; i++){
      pairs.emplace_back(tuple.ids[i].kind,tuple.ids[i].index);
    }
  }

  return pairs;
}


//convert a vector of uint16 and uint32 pairs to a vector of tuples 
std::vector<tms_id_tuple_t>
SparseDB::intPairs2Tuples(const std::vector<std::pair<uint16_t, uint64_t>>& all_pairs)
{
  std::vector<tms_id_tuple_t> tuples;
  uint i = 0;
  uint idx = 0;
  uint cur_rank = 0; //root rank
  while(i < all_pairs.size()){
    std::pair<uint16_t, uint64_t> p = all_pairs[i];
    tms_id_tuple_t t;
    t.length = p.first;
    t.rank = p.second;
    if(t.rank != cur_rank){
      cur_rank = t.rank;
    }

    t.ids = (tms_id_t*)malloc(t.length * sizeof(tms_id_t));
    for(uint j = 0; j < t.length; j++){
      t.ids[j].kind = all_pairs[i+j+1].first;
      t.ids[j].index = all_pairs[i+j+1].second;
    }

    t.all_at_root_idx = idx;
    t.prof_info_idx = not_assigned;
    
    tuples.emplace_back(t);
    idx += 1;
    i += (1 + t.length);
  }

  return tuples;
}



//---------------------------------------------------------------------------
// profile id tuples - organize(communication,sorting,etc)
//---------------------------------------------------------------------------
//rank 0 gather all the tuples as pairs format from other ranks
std::vector<std::pair<uint16_t, uint64_t>> SparseDB::gatherIdTuplesData(const int world_rank, 
                                                                        const int world_size,
                                                                        const int threads,
                                                                        MPI_Datatype IntPairType, 
                                                                        const std::vector<std::pair<uint16_t, uint64_t>>& rank_pairs)
{
  //get the size of each ranks' pairs
  int rank_pairs_size = rank_pairs.size();
  std::vector<int> all_rank_pairs_sizes;
  if(world_rank == 0) all_rank_pairs_sizes.resize(world_size);
  MPI_Gather(&rank_pairs_size, 1, MPI_INT, all_rank_pairs_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

  //get the displacement of all ranks' pairs
  int total_size = 0;
  std::vector<int> all_rank_pairs_disps; 
  std::vector<std::pair<uint16_t, uint64_t>> all_rank_pairs;
  if(world_rank == 0){
    all_rank_pairs_disps.resize(world_size);

    #pragma omp parallel for num_threads(threads)
    for(int i = 0; i<world_size; i++) all_rank_pairs_disps[i] = all_rank_pairs_sizes[i];
    exscan<int>(all_rank_pairs_disps,threads); 

    total_size = all_rank_pairs_disps.back() + all_rank_pairs_sizes.back();
    all_rank_pairs.resize(total_size);
  }

  MPI_Gatherv(rank_pairs.data(),rank_pairs_size, IntPairType, \
    all_rank_pairs.data(), all_rank_pairs_sizes.data(), all_rank_pairs_disps.data(), IntPairType, 0, MPI_COMM_WORLD);

  return all_rank_pairs;

}


//rank 0 scatter the calculated offsets and prof_info_idx of each profile back to their coming ranks
void SparseDB::scatterProfIdxOffset(const std::vector<tms_id_tuple_t>& tuples,
                                    const std::vector<uint64_t>& all_tuple_ptrs,
                                    const size_t num_prof,
                                    const int world_size,
                                    const int world_rank,
                                    const int threads)
{
  prof_idx_off_pairs.resize(num_prof);

  //create send buffer
  std::vector<std::pair<uint32_t, uint64_t>> idx_off_buffer(tuples.size());
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i<tuples.size(); i++)
  {
    tms_id_tuple_t tuple = tuples[i];
    idx_off_buffer[tuple.all_at_root_idx] = {tuple.prof_info_idx, all_tuple_ptrs[i]};
  }

  //get the size of each ranks' tuples
  std::vector<int> all_rank_tuples_sizes;
  if(world_rank == 0) all_rank_tuples_sizes.resize(world_size);
  MPI_Gather(&num_prof, 1, MPI_INT, all_rank_tuples_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

  //get the displacement of all ranks' tuples
  std::vector<int> all_rank_tuples_disps; 
  if(world_rank == 0){
    all_rank_tuples_disps.resize(world_size);

    #pragma omp parallel for num_threads(threads)
    for(int i = 0; i<world_size; i++) all_rank_tuples_disps[i] = all_rank_tuples_sizes[i];
    exscan<int>(all_rank_tuples_disps,threads); 
  }

  //create a new Datatype for prof_info_idx and offset
  std::vector<MPI_Datatype> types{MPI_UINT32_T, MPI_UINT64_T};
  MPI_Datatype IdxOffType = createTupleType(types);

  MPI_Scatterv(idx_off_buffer.data(), all_rank_tuples_sizes.data(), all_rank_tuples_disps.data(), \
    IdxOffType, prof_idx_off_pairs.data(), num_prof, IdxOffType, 0, MPI_COMM_WORLD);

}



//sort a vector of id_tuples
//compare the kind first and then index for the same kind, if all previous kind-index pairs are same, compare length
void SparseDB::sortIdTuples(std::vector<tms_id_tuple_t>& all_tuples)
{
  struct {
    bool operator()(tms_id_tuple_t a_tuple, 
                    tms_id_tuple_t b_tuple) const
    {   
      uint16_t len = std::min(a_tuple.length, b_tuple.length);
      for(uint i = 0; i<len; i++){
        if(a_tuple.ids[i].kind != b_tuple.ids[i].kind){
          return a_tuple.ids[i].kind < b_tuple.ids[i].kind;
        }else{
          if(a_tuple.ids[i].index != b_tuple.ids[i].index){
            return a_tuple.ids[i].index < b_tuple.ids[i].index;
          }
        }
      }
      return a_tuple.length < b_tuple.length;
    }   
  }tupleComp;
  
  std::sort(all_tuples.begin(), all_tuples.end(), tupleComp);
}


//assign the prof_info_idx of each tuple (index in prof_info section of thread_major_sparse.db)
void SparseDB::assignIdTuplesIdx(std::vector<tms_id_tuple_t>& all_tuples,
                                 const int threads)
{
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < all_tuples.size(); i++){
    assert(all_tuples[i].prof_info_idx == not_assigned);
    all_tuples[i].prof_info_idx = i;
  }
}


//exscan to get the writing offset for each tuple in the thread_major_sparse.db
std::vector<uint64_t> SparseDB::getIdTuplesOff(std::vector<tms_id_tuple_t>& all_tuples,
                                               const int threads)
{
  std::vector<uint64_t> tupleSizes (all_tuples.size()+1,0);
  std::vector<uint64_t> tupleOffsets (all_tuples.size()+1);

  //notice the last entry in tupleSizes is still 0
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < all_tuples.size(); i++){
    tupleSizes[i] = TMS_id_tuple_len_SIZE + all_tuples[i].length * TMS_id_SIZE;
  }

  exscan<uint64_t>(tupleSizes,threads);

  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < tupleSizes.size();i++){
    tupleOffsets[i] = tupleSizes[i] + HPCTHREADSPARSE_FMT_IdTupleOff; 
  }

  return tupleOffsets;

}


//free the kind-index pairs in tms_id_tuple_t
void SparseDB::freeIdTuples(std::vector<tms_id_tuple_t>& all_tuples,
                            const int threads)
{
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < all_tuples.size(); i++){
    free(all_tuples[i].ids);
    all_tuples[i].ids = NULL;
  }
}


//---------------------------------------------------------------------------
// profile id tuples -  write it down 
//---------------------------------------------------------------------------  
//convert one tms_id_tuple_t to a vector of bytes
std::vector<char> SparseDB::convertTuple2Bytes(const tms_id_tuple_t& tuple)
{
  std::vector<char> bytes(TMS_id_tuple_len_SIZE + tuple.length * TMS_id_SIZE);
  convertToByte2(tuple.length, bytes.data());
  char* byte_pos = bytes.data() + 2;
  for(uint i = 0; i < tuple.length; i++){
    auto& id = tuple.ids[i];
    convertToByte2(id.kind, byte_pos);
    convertToByte8(id.index, byte_pos+2);
    byte_pos += TMS_id_SIZE;
  }
  return bytes;
}

//write the vector of tms_id_tuple_t in fh
size_t SparseDB::writeAllIdTuples(const std::vector<tms_id_tuple_t>& all_tuples,
                                MPI_File fh)
{
  std::vector<char> bytes;
  for(auto& tuple : all_tuples)
  {
    std::vector<char> b = convertTuple2Bytes(tuple);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }
  SPARSE_exitIfMPIError(writeAsByteX(bytes, bytes.size(), fh, HPCTHREADSPARSE_FMT_IdTupleOff),"error writing all tuples");
  
  return bytes.size();
}


//all work related to IdTuples Section, other sections only need the vector of prof_info_idx and id_tuple_ptr pairs
uint64_t SparseDB::workIdTuplesSection(const int world_rank,
                                       const int world_size,
                                       const int threads,
                                       MPI_File fh)
{
  //assign sparseInputs based on outputs
  assignSparseInputs(world_rank);

  std::vector<MPI_Datatype> types {MPI_UINT16_T, MPI_UINT64_T};
  MPI_Datatype IntPairType = createTupleType(types);

  std::vector<tms_id_tuple_t> tuples = getMyIdTuples();
  std::vector<std::pair<uint16_t, uint64_t>> pairs = tuples2IntPairs(tuples);
  std::vector<tms_id_tuple_t> all_rank_tuples;
  std::vector<std::pair<uint16_t, uint64_t>> all_rank_pairs = gatherIdTuplesData(world_rank, world_size, threads, IntPairType, pairs);
  std::vector<uint64_t> all_tuple_ptrs;

  uint64_t all_id_tuples_size;
  if(world_rank == 0) {
    all_rank_tuples = intPairs2Tuples(all_rank_pairs);
    sortIdTuples(all_rank_tuples);
    assignIdTuplesIdx(all_rank_tuples, threads);
    all_tuple_ptrs = getIdTuplesOff(all_rank_tuples, threads);

    all_id_tuples_size = writeAllIdTuples(all_rank_tuples, fh);
    assert(HPCTHREADSPARSE_FMT_IdTupleOff + all_id_tuples_size == all_tuple_ptrs.back());
  }

  MPI_Bcast(&all_id_tuples_size, 1, mpi_data<uint64_t>::type, 0, MPI_COMM_WORLD);
  scatterProfIdxOffset(all_rank_tuples, all_tuple_ptrs, tuples.size(), world_size, world_rank, threads);

  freeIdTuples(all_rank_tuples, threads);
  return all_id_tuples_size;
}


//---------------------------------------------------------------------------
// get profile's size and corresponding offsets
//---------------------------------------------------------------------------

// iterate through rank's profile list, assign profile_sizes, a vector of (profile Thread object : size) pair
// also assign value to my_size, which is the total size of rank's profiles
uint64_t SparseDB::getProfileSizes()
{
  uint64_t my_size = 0;
  for(const auto& tp: sparseInputs){
    struct stat buf;
    stat(tp.second.c_str(),&buf);
    my_size += (buf.st_size - TMS_prof_skip_SIZE);
    profile_sizes.emplace_back(buf.st_size - TMS_prof_skip_SIZE);    
  }
  return my_size;
}


// get total number of profiles across all ranks
// input: rank's number of profiles, output: total_num_prof
uint32_t SparseDB::getTotalNumProfiles(const uint32_t my_num_prof)
{
  uint32_t total_num_prof;
  MPI_Allreduce(&my_num_prof, &total_num_prof, 1, mpi_data<uint32_t>::type, MPI_SUM, MPI_COMM_WORLD);
  return total_num_prof;
}


// get the offset for this rank's start in thread_major_sparse.db
// input: my_size as the total size of all profiles belong to this rank, rank number
// output: calculated my_offset
uint64_t SparseDB::getMyOffset(const uint64_t my_size, 
                               const int rank)
{
  uint64_t my_offset;
  MPI_Exscan(&my_size, &my_offset, 1, mpi_data<uint64_t>::type, MPI_SUM, MPI_COMM_WORLD);
  if(rank == 0) my_offset = 0;
  return my_offset;
}


// get the final global offsets for each profile in this rank
// input: total number of profiles across all ranks, rank's offset, number of threads
// output: assigned prof_offsets
// profile_sizes and prof_offsets are private variables of the class
void SparseDB::getMyProfOffset(const uint32_t total_prof, 
                               const uint64_t my_offset, 
                               const int threads, 
                               const uint64_t id_tuples_size)
{
  prof_offsets.resize(profile_sizes.size());

  std::vector<uint64_t> tmp (profile_sizes.size());
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < tmp.size();i++){
    tmp[i] = profile_sizes[i];
  }

  exscan<uint64_t>(tmp,threads);

  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < tmp.size();i++){
    if(i < tmp.size() - 1) assert(tmp[i] + profile_sizes[i] == tmp[i+1]);
    prof_offsets[i] = tmp[i] + my_offset + (total_prof * TMS_prof_info_SIZE)  
      + id_tuples_size + HPCTHREADSPARSE_FMT_IdTupleOff; 
  }

}


//work on profile_sizes and prof_offsets (two private variables) used later for writing profiles
uint32_t SparseDB::workProfSizesOffsets(const int world_rank, const int threads, const uint64_t id_tuples_size)
{
  uint64_t my_size = getProfileSizes();
  uint32_t total_prof = getTotalNumProfiles(profile_sizes.size());
  uint64_t my_off = getMyOffset(my_size, world_rank);
  getMyProfOffset(total_prof, my_off, threads, id_tuples_size);
  return total_prof; 
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
void SparseDB::collectCctMajorData(const uint32_t prof_info_idx,
                                   std::vector<char>& bytes,
                                   std::vector<uint64_t>& ctx_nzval_cnts, 
                                   std::vector<std::set<uint16_t>>& ctx_nzmids)
{ 
  assert(ctx_nzval_cnts.size() == ctx_nzmids.size());
  assert(ctx_nzval_cnts.size() > 0);

  uint64_t num_vals;
  uint32_t num_nzctxs;
  interpretByte8(num_vals,   bytes.data() + TMS_prof_info_idx_SIZE);
  interpretByte4(num_nzctxs, bytes.data() + TMS_prof_info_idx_SIZE + TMS_num_val_SIZE);
  uint64_t ctx_end_idx;
  interpretByte8(ctx_end_idx, bytes.data() + bytes.size() - TMS_ctx_idx_SIZE);
  if(num_vals != ctx_end_idx) {
    exitError("tmpDB file for thread " + std::to_string(prof_info_idx) + " is corrupted!");
  }

  char* val_cnt_input = bytes.data() + TMS_prof_skip_SIZE + num_vals * (TMS_val_SIZE + TMS_mid_SIZE);
  char* mids_input = bytes.data() + TMS_prof_skip_SIZE;
  for(uint i = 0; i < num_nzctxs; i++){
    interpretOneCtxValCntMids(val_cnt_input, mids_input, ctx_nzmids, ctx_nzval_cnts);

    val_cnt_input += TMS_ctx_pair_SIZE;  
  }

}


// build prof_info bytes
std::vector<char> SparseDB::buildOneProfInfoBytes(const std::vector<char>& partial_info_bytes, 
                                                  const uint64_t id_tuple_ptr,
                                                  const uint64_t metadata_ptr,
                                                  const uint64_t spare_one_ptr,
                                                  const uint64_t spare_two_ptr,
                                                  const uint64_t prof_offset)
{
  std::vector<char> info_bytes(TMS_prof_info_SIZE); 
  convertToByte8(id_tuple_ptr, info_bytes.data());
  convertToByte8(metadata_ptr, info_bytes.data() + TMS_id_tuple_ptr_SIZE);
  convertToByte8(spare_one_ptr, info_bytes.data() + TMS_id_tuple_ptr_SIZE + TMS_metadata_ptr_SIZE);
  convertToByte8(spare_two_ptr, info_bytes.data() + TMS_id_tuple_ptr_SIZE + TMS_metadata_ptr_SIZE + TMS_spare_one_SIZE);
  std::copy(partial_info_bytes.begin(), partial_info_bytes.end(),info_bytes.begin()+TMS_ptrs_SIZE);
  convertToByte8(prof_offset, info_bytes.data() + TMS_prof_info_SIZE - TMS_prof_offset_SIZE);
  return info_bytes;
}

// write one profile information at the top of thread_major_sparse.db
// input: bytes of the profile, calculated offset, thread id, file handle
void SparseDB::writeOneProfInfo(const std::vector<char>& info_bytes, 
                                const uint32_t prof_info_idx,
                                const uint64_t id_tuples_size,
                                const MPI_File fh)
{
  MPI_Offset info_off = HPCTHREADSPARSE_FMT_IdTupleOff + id_tuples_size + prof_info_idx * TMS_prof_info_SIZE;
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_write_at(fh, info_off, info_bytes.data(), TMS_prof_info_SIZE, MPI_BYTE, &stat), 
    __FUNCTION__ +  std::to_string(prof_info_idx) + std::string("th profile"));
}

// write one profile data at the <offset> of thread_major_sparse.db
// input: bytes of the profile, calculated offset, thread id, file handle
void SparseDB::writeOneProfileData(const std::vector<char>& bytes, 
                                   const uint64_t offset, 
                                   const uint32_t prof_info_idx,
                                   const MPI_File fh)
{
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_write_at(fh, offset, bytes.data()+TMS_prof_skip_SIZE, bytes.size()-TMS_prof_skip_SIZE, MPI_BYTE, &stat),
    __FUNCTION__ + std::to_string(prof_info_idx) + std::string("th profile"));
}

// write one profile in thread_major_sparse.db
// input: one profile's thread attributes, profile's offset, file handle
// output: assigned ctx_nzval_cnts and ctx_nzmids
void SparseDB::writeOneProfile(const std::pair<tms_id_tuple_t, std::string>& tupleFn,
                               const MPI_Offset my_prof_offset, 
                               const std::pair<uint32_t,uint64_t>& prof_idx_off_pair,
                               const uint64_t id_tuples_size,
                               std::vector<uint64_t>& ctx_nzval_cnts,
                               std::vector<std::set<uint16_t>>& ctx_nzmids,
                               MPI_File fh)
{
  //get file name
  const std::string fn = tupleFn.second;

  //get all bytes from a profile
  std::ifstream input(fn.c_str(), std::ios::binary);
  std::vector<char> bytes(
      (std::istreambuf_iterator<char>(input)),
      (std::istreambuf_iterator<char>()));
  input.close();

  //collect context local nonzero value counts and nz_mids from this profile
  if(tupleFn.first.ids[0].kind != SUMMARY){
    collectCctMajorData(prof_idx_off_pair.first, bytes, ctx_nzval_cnts, ctx_nzmids);
  }
   

  //write profile info
  std::vector<char> partial_info (TMS_num_val_SIZE + TMS_num_nzctx_SIZE);
  std::copy(bytes.begin()+TMS_prof_info_idx_SIZE, bytes.begin() + TMS_prof_skip_SIZE, partial_info.begin());
  std::vector<char> info = buildOneProfInfoBytes(partial_info, prof_idx_off_pair.second, 0, 0, 0, my_prof_offset);
  writeOneProfInfo(info, prof_idx_off_pair.first, id_tuples_size,fh);

  //write profile data
  writeOneProfileData(bytes, my_prof_offset, prof_idx_off_pair.first, fh);
}

// write all the profiles at the correct place, and collect data needed for cct_major_sparse.db 
// input: calculated prof_offsets, calculated profile_sizes, file handle, number of available threads
void SparseDB::writeProfiles(const uint64_t id_tuples_size,
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
    for(uint i = 0; i<prof_offsets.size();i++){
      const std::pair<tms_id_tuple_t, std::string> tupleFn = sparseInputs[i];
      MPI_Offset my_prof_offset = prof_offsets[i];
      writeOneProfile(tupleFn, my_prof_offset, prof_idx_off_pairs[i], id_tuples_size, ctx_nzval_cnts, thread_ctx_nzmids, fh);
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
  // private variables:
  // profile_sizes: vector of profile's own size
  // prof_offsets:  vector of final global offset
  // prof_idx_off_pairs:       vector of (profile's idx at prof_info section : the ptr to its id_tuple)  
  // id_tuples_size: number of bytes id_tuples section occupied
  //

  MPI_File thread_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "thread_major_sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &thread_major_f);

  id_tuples_size = workIdTuplesSection(world_rank, world_size, threads, thread_major_f);
  uint32_t total_prof = workProfSizesOffsets(world_rank, threads, id_tuples_size);

  if(world_rank == 0){
    std::vector<char> hdr; 
    hdr.insert(hdr.end(), HPCTHREADSPARSE_FMT_Magic, HPCTHREADSPARSE_FMT_Magic + HPCTHREADSPARSE_FMT_MagicLen);
    hdr.emplace_back(HPCTHREADSPARSE_FMT_Version);
    convertToByte4(total_prof, hdr.data() + HPCTHREADSPARSE_FMT_HeaderLen);
    
    SPARSE_exitIfMPIError(writeAsByteX(hdr, HPCTHREADSPARSE_FMT_IdTupleOff, thread_major_f, 0),
     __FUNCTION__ + std::string(": write the hdr wrong"));
  }
    
  writeProfiles(id_tuples_size, thread_major_f, threads, ctx_nzval_cnts, ctx_nzmids);

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
    ctx_off[i] = ctx_val_cnts[i] * CMS_val_prof_idx_pair_SIZE;
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

  /* 
  //split work among ranks by volume of ctxs
  uint64_t total_size = ctx_off.back();
  uint64_t max_size_per_rank = round(total_size/num_ranks);
  uint64_t my_start = rank * max_size_per_rank;
  uint64_t my_end = (rank == num_ranks - 1) ? total_size : (rank + 1) * max_size_per_rank;

  for(uint i = 1; i<ctx_off.size(); i++){
    if(ctx_off[i] > my_start && ctx_off[i] <= my_end) my_ctxs.emplace_back(CTXID((i-1)));
  }
  */

  //split work among ranks by number of ctxs
  size_t num_ctxs_per_rank = round(ctx_off.size()/num_ranks);
  uint64_t my_start = rank * num_ctxs_per_rank;
  uint64_t my_end = (rank == num_ranks - 1) ? ctx_off.size()-1 : (rank + 1) * num_ctxs_per_rank;

  for(uint i = my_start; i<my_end; i++){
    my_ctxs.emplace_back(CTXID((i)));
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
    ctx_off[i] += ctxcnt * CMS_ctx_info_SIZE + HPCCCTSPARSE_FMT_CtxInfoOff;
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
  interpretByte8(pi.id_tuple_ptr, input);
  interpretByte8(pi.metadata_ptr, input + TMS_id_tuple_ptr_SIZE);
  interpretByte8(pi.spare_one,    input + TMS_id_tuple_ptr_SIZE + TMS_metadata_ptr_SIZE);
  interpretByte8(pi.spare_two,    input + TMS_id_tuple_ptr_SIZE + TMS_metadata_ptr_SIZE + TMS_spare_one_SIZE);
  interpretByte8(pi.num_vals,     input + TMS_ptrs_SIZE);
  interpretByte4(pi.num_nzctxs,   input + TMS_ptrs_SIZE + TMS_num_val_SIZE);
  interpretByte8(pi.offset,       input + TMS_ptrs_SIZE + TMS_num_val_SIZE + TMS_num_nzctx_SIZE);

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
  SPARSE_exitIfMPIError(readAsByte4(num_prof,fh,HPCTHREADSPARSE_FMT_HeaderLen), __FUNCTION__ + std::string(": cannot read the number of profiles"));
  num_prof--; //minus the summary profile

  //read the whole Profile Information section
  int count = num_prof * TMS_prof_info_SIZE; 
  char input[count];
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_read_at(fh, HPCTHREADSPARSE_FMT_IdTupleOff + id_tuples_size + TMS_prof_info_SIZE, &input, count, MPI_BYTE, &stat), 
    __FUNCTION__ + std::string(": cannot read the whole Profile Information section"));

  //interpret the section and store in a vector of tms_profile_info_t
  prof_info.resize(num_prof);
  #pragma omp parallel for num_threads(threads)
  for(int i = 0; i<count; i += TMS_prof_info_SIZE){
    tms_profile_info_t pi;
    interpretOneProfInfo(input + i, pi);
    pi.prof_info_idx = i/TMS_prof_info_SIZE + 1; // the first one is summary profile
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
                                  const bool notfirst,
                                  const int found_ctx_idx, 
                                  //std::map<uint32_t, uint64_t>& my_ctx_pairs)
                                  std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs)
{
  int idx;

  if(notfirst){
    idx = 1 + found_ctx_idx;

    //the profile_ctx_pairs has been searched through
    if(idx == (int)length) return SPARSE_END;
    
    //the ctx_id at idx
    uint32_t prof_ctx_id = profile_ctx_pairs[idx].ctx_id;
    if(prof_ctx_id == target_ctx_id){
      //my_ctx_pairs.emplace(profile_ctx_pairs[idx].ctx_id,profile_ctx_pairs[idx].ctx_idx);
      my_ctx_pairs.emplace_back(profile_ctx_pairs[idx].ctx_id, profile_ctx_pairs[idx].ctx_idx);
      return idx;
    }else if(prof_ctx_id > target_ctx_id){
      return SPARSE_NOT_FOUND; //back to original since this might be next target
    }else{ //prof_ctx_id < target_ctx_id, should not happen
      std::ostringstream ss;
      ss << __FUNCTION__ << ": ctx id " << prof_ctx_id << " in a profile does not exist in the full ctx list while searching for " 
        << target_ctx_id << " with index " << idx;
      exitError(ss.str());
      return SPARSE_NOT_FOUND; //this should not be called if exit, but function requires return value
    }

  }else{
    TMS_CtxIdIdxPair target_ciip;
    target_ciip.ctx_id = target_ctx_id;
    idx = struct_member_binary_search(profile_ctx_pairs, target_ciip, &TMS_CtxIdIdxPair::ctx_id, length);
    //if(idx != SPARSE_NOT_FOUND) my_ctx_pairs.emplace_back(profile_ctx_pairs[idx]);
    if(idx >= 0){
      //my_ctx_pairs.emplace(profile_ctx_pairs[idx].ctx_id, profile_ctx_pairs[idx].ctx_idx);
      my_ctx_pairs.emplace_back(profile_ctx_pairs[idx].ctx_id, profile_ctx_pairs[idx].ctx_idx);
    }
    else if(idx == -1){
      idx = 0;
    }else if(idx < -1){
      idx = -2 - idx;
    }
    return idx;
  }
}


//from a group of TMS_CtxIdIdxPair of one profile, get the pairs related to a group of ctx_ids
//input: a vector of ctx_ids we are searching for, a vector of profile TMS_CtxIdIdxPairs we are searching through
//output: (last argument) a filled vector of TMS_CtxIdIdxPairs related to that group of ctx_ids
void SparseDB::findCtxIdIdxPairs(const std::vector<uint32_t>& ctx_ids,
                                 const std::vector<TMS_CtxIdIdxPair>& profile_ctx_pairs,
                                 std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs)
{
  assert(profile_ctx_pairs.size() > 1);

  uint n = profile_ctx_pairs.size() - 1; //since the last is LastNodeEnd
  int idx = -1; //index of current pair in profile_ctx_pairs
  bool notfirst = false;
  uint32_t target;

  for(uint i = 0; i<ctx_ids.size(); i++){
    target = ctx_ids[i];
    int ret = findOneCtxIdIdxPair(target, profile_ctx_pairs, n, notfirst, idx, my_ctx_pairs);
    if(ret == SPARSE_END) break;
    if(ret != SPARSE_NOT_FOUND){
      idx = ret;
      notfirst = true;
    } 
  }

  //add one extra context pair for later use
  //my_ctx_pairs.emplace(LastNodeEnd,profile_ctx_pairs[idx + 1].ctx_idx);
  my_ctx_pairs.emplace_back(LastNodeEnd, profile_ctx_pairs[idx + 1].ctx_idx);
  
  assert(my_ctx_pairs.size() <= ctx_ids.size() + 1);
}


// get the context id and index pairs for the group of contexts from one profile 
// input: prof_info for this profile, context ids we want, file handle
// output: found context id and idx pairs
int SparseDB::getMyCtxIdIdxPairs(const tms_profile_info_t& prof_info,
                                 const std::vector<uint32_t>& ctx_ids,
                                 const std::vector<TMS_CtxIdIdxPair>& prof_ctx_pairs,
                                 const MPI_File fh,
                                 //std::map<uint32_t, uint64_t>& my_ctx_pairs)
                                 std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs)
{
  if(prof_ctx_pairs.size() == 1) return SPARSE_ERR;

  findCtxIdIdxPairs(ctx_ids, prof_ctx_pairs, my_ctx_pairs);
  if(my_ctx_pairs.size() == 1) return SPARSE_ERR;

  return SPARSE_OK;
}


std::vector<std::vector<SparseDB::TMS_CtxIdIdxPair>> 
SparseDB::getProfileCtxIdIdxPairs(const MPI_File fh,  
                                  const int threads,
                                  const std::vector<tms_profile_info_t>& prof_info)
{
  std::vector<std::vector<TMS_CtxIdIdxPair>> all_prof_ctx_pairs(prof_info.size());

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < prof_info.size(); i++){
    tms_profile_info_t pi = prof_info[i];
    MPI_Offset ctx_pairs_offset = pi.offset + pi.num_vals * (TMS_val_SIZE + TMS_mid_SIZE);
    std::vector<TMS_CtxIdIdxPair> prof_ctx_pairs (pi.num_nzctxs + 1);

    if(prof_ctx_pairs.size() != 1)
      SPARSE_exitIfMPIError(readCtxIdIdxPairs(fh, ctx_pairs_offset, prof_ctx_pairs), 
                          __FUNCTION__ + std::string(": cannot read context pairs for profile ") + std::to_string(pi.prof_info_idx));

    all_prof_ctx_pairs[i] = prof_ctx_pairs;

  }

  return all_prof_ctx_pairs;
}



//---------------------------------------------------------------------------
// read and interpret one profie - ValMid
//---------------------------------------------------------------------------
// read all bytes for a group of contexts from one profile
void SparseDB::readValMidsBytes(const std::vector<uint32_t>& ctx_ids,
                                //std::map<uint32_t, uint64_t>& my_ctx_pairs,
                                std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs,
                                const tms_profile_info_t& prof_info,
                                const MPI_File fh,
                                std::vector<char>& bytes)
{

  //uint64_t first_ctx_idx = my_ctx_pairs.begin()->second;
  //uint64_t last_ctx_idx  = my_ctx_pairs.rbegin()->second;
  uint64_t first_ctx_idx = my_ctx_pairs[0].second;
  uint64_t last_ctx_idx  = my_ctx_pairs.back().second;

  MPI_Offset val_mid_start_pos = prof_info.offset + first_ctx_idx * (TMS_val_SIZE + TMS_mid_SIZE);
  int val_mid_count = (last_ctx_idx - first_ctx_idx) * (TMS_val_SIZE + TMS_mid_SIZE);
  bytes.resize(val_mid_count);
  MPI_Status stat;
  if(val_mid_count != 0) {
    SPARSE_exitIfMPIError(MPI_File_read_at(fh, val_mid_start_pos, bytes.data(), val_mid_count, MPI_BYTE, &stat),
                        __FUNCTION__ + std::string(": cannot read real data for profile ") + std::to_string(prof_info.prof_info_idx));
  }

}

//create and return a new MetricValBlock
SparseDB::MetricValBlock SparseDB::createNewMetricValBlock(const hpcrun_metricVal_t val, 
                                                           const uint16_t mid,
                                                           const uint32_t prof_idx)
{
  MetricValBlock mvb;
  mvb.mid = mid;
  std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_prof_idxs;
  values_prof_idxs.emplace_back(val, prof_idx);
  mvb.values_prof_idxs = values_prof_idxs;
  return mvb;
}

//create and return a new CtxMetricBlock
SparseDB::CtxMetricBlock SparseDB::createNewCtxMetricBlock(const hpcrun_metricVal_t val, 
                                                 const uint16_t mid,
                                                 const uint32_t prof_idx,
                                                 const uint32_t ctx_id)
{
  //create a new MetricValBlock for this mid, val, prof_idx
  MetricValBlock mvb = createNewMetricValBlock(val, mid, prof_idx);

  //create a vector of MetricValBlock for this context
  std::map<uint16_t, MetricValBlock> mvbs;
  mvbs.emplace(mid,mvb);

  //store it
  CtxMetricBlock cmb = {ctx_id, mvbs};
  return cmb;
}

//insert a pair of val and metric id to a CtxMetBlock they belong to (ctx id matches)
void SparseDB::insertValMidPair2OneCtxMetBlock(const hpcrun_metricVal_t val, 
                                               const uint16_t mid,
                                               const uint32_t prof_idx,
                                               CtxMetricBlock& cmb)
{
  //find if this mid exists
  std::map<uint16_t, MetricValBlock>& metric_blocks = cmb.metrics; 
  auto it = metric_blocks.find(mid);

  if(it != metric_blocks.end()){ //found mid
    it->second.values_prof_idxs.emplace_back(val, prof_idx);
  }else{ 
    metric_blocks.emplace(mid, createNewMetricValBlock(val, mid, prof_idx));
  }
}


//insert a triple of val, metric id and ctx_id to the correct place of ctx_met_blocks
void SparseDB::insertValMidCtxId2CtxMetBlocks(const hpcrun_metricVal_t val, 
                                              const uint16_t mid,
                                              const uint32_t prof_idx,
                                              const uint32_t ctx_id,
                                              CtxMetricBlock& cmb)
{
  
  /*
  // for ctx_met_blocks not single CtxMextricBlock
  std::map<uint32_t, CtxMetricBlock>::iterator got = ctx_met_blocks.find(ctx_id);

  
  if (got != ctx_met_blocks.end()){ //ctx_id found
    insertValMidPair2OneCtxMetBlock(val, mid, prof_idx, got->second);
  }else{
    CtxMetricBlock newcmb = createNewCtxMetricBlock(val, mid, prof_idx, ctx_id);
    ctx_met_blocks.emplace(ctx_id, newcmb);
  }
  */

  //for single CtxMextricBlock
  assert(cmb.ctx_id == ctx_id);
  insertValMidPair2OneCtxMetBlock(val, mid, prof_idx, cmb);
  

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
                                     const uint32_t prof_idx,
                                     const std::pair<uint32_t,uint64_t>& ctx_pair,
                                     const uint64_t next_ctx_idx,
                                     const uint64_t first_ctx_idx,
                                     //std::map<uint32_t, uint64_t>& my_ctx_pairs,
                                     std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs,
                                     CtxMetricBlock& cmb)
{
  // for ctx_met_blocks not single CtxMetricBlock
  /*
  char* ctx_met_input = vminput;
  //for each context, keep track of the values, metric ids, and thread ids
  for(uint c = 0; c < my_ctx_pairs.size() - 1; c++) //change this to map iterator
  {
    uint32_t ctx_id  = my_ctx_pairs[c].ctx_id;
    uint64_t ctx_idx = my_ctx_pairs[c].ctx_idx;
    uint64_t num_val_this_ctx = my_ctx_pairs[c + 1].ctx_idx - ctx_idx;

    for(uint i = 0; i < num_val_this_ctx; i++){
      hpcrun_metricVal_t val;
      uint16_t mid;
      interpretOneValMidPair(ctx_met_input, val, mid);

      insertValMidCtxId2CtxMetBlocks(val, mid, prof_idx, ctx_id, ctx_met_blocks);
      ctx_met_input += (TMS_val_SIZE + TMS_mid_SIZE);
    }

  }
  */

  // for single CtxMetricBlock
  //uint64_t ctx_idx = my_ctx_pairs[ctx_id];
  uint32_t ctx_id = ctx_pair.first;
  uint64_t ctx_idx = ctx_pair.second;
  uint64_t num_val_this_ctx = next_ctx_idx - ctx_idx;

  char* ctx_met_input = vminput + (TMS_val_SIZE + TMS_mid_SIZE) * (ctx_idx - first_ctx_idx);
  for(uint i = 0; i < num_val_this_ctx; i++){
    hpcrun_metricVal_t val;
    uint16_t mid;
    interpretOneValMidPair(ctx_met_input, val, mid);

    insertValMidCtxId2CtxMetBlocks(val, mid, prof_idx, ctx_id, cmb);
    ctx_met_input += (TMS_val_SIZE + TMS_mid_SIZE);
  }

}

//read all the data for a group of contexts, from one profile with its prof_info, store data to ctx_met_blocks 
void SparseDB::readOneProfile(const std::vector<uint32_t>& ctx_ids, 
                              const tms_profile_info_t& prof_info,
                              const std::vector<TMS_CtxIdIdxPair>& prof_ctx_pairs,
                              const MPI_File fh,
                              std::map<uint32_t, CtxMetricBlock>& ctx_met_blocks)
{
  /*
  //get the id and idx pairs for this group of ctx_ids from this profile
  std::map<uint32_t, uint64_t> my_ctx_pairs;
  int ret = getMyCtxIdIdxPairs(prof_info, ctx_ids, prof_ctx_pairs, fh, my_ctx_pairs);
  if(ret != SPARSE_OK) return;

  //read all values and metric ids for this group of ctx at once
  std::vector<char> vmbytes;
  readValMidsBytes(my_ctx_pairs, prof_info, fh, vmbytes);

  //interpret the bytes and store them in ctx_met_blocks
  interpretValMidsBytes(vmbytes.data(), prof_info.prof_info_idx, my_ctx_pairs, ctx_met_blocks);

  //assert(my_ctx_pairs.size() - 1 <= ctx_met_blocks.size());
  */
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

  std::map<uint16_t, MetricValBlock>& dest_metrics = dest.metrics;

  for(auto it = source.metrics.begin(); it != source.metrics.end(); it++){
    MetricValBlock source_m = it->second;
    const uint16_t& mid = source_m.mid;
    
    auto mvb_i = dest_metrics.find(mid);
    if(mvb_i == dest_metrics.end()){
      dest_metrics.emplace(mid,source_m);
    }else{
      mvb_i->second.values_prof_idxs.reserve(mvb_i->second.values_prof_idxs.size() + source_m.values_prof_idxs.size());
      mvb_i->second.values_prof_idxs.insert(mvb_i->second.values_prof_idxs.end(), source_m.values_prof_idxs.begin(), source_m.values_prof_idxs.end());
    }
  }
}


//merge all the CtxMetricBlocks from all the threads for one Ctx (one ctx_id) to dest
void SparseDB::mergeOneCtxAllThreadBlocks(const std::vector<std::map<uint32_t, CtxMetricBlock> *>& threads_ctx_met_blocks,
                                          CtxMetricBlock& dest)
{
  uint32_t ctx_id = dest.ctx_id;
  for(uint i = 0; i < threads_ctx_met_blocks.size(); i++){
    std::map<uint32_t, CtxMetricBlock>& thread_cmb = *threads_ctx_met_blocks[i];
    
    std::map<uint32_t, CtxMetricBlock>::iterator cmb_i = thread_cmb.find(ctx_id);   
    if(cmb_i != thread_cmb.end()){
      mergeCtxMetBlocks(cmb_i->second, dest);
    }
  }
}

//within each CtxMetricBlock sort based on metric id, within each MetricValBlock, sort based on thread id
void SparseDB::sortCtxMetBlocks(std::map<uint32_t, CtxMetricBlock>& ctx_met_blocks)
{

  //#pragma omp for  
  for(auto i = ctx_met_blocks.begin(); i != ctx_met_blocks.end(); i++){
    CtxMetricBlock& hcmb = i->second;

    for(auto j = hcmb.metrics.begin(); j != hcmb.metrics.end(); j++){
      MetricValBlock& mvb = j->second;

      std::sort(mvb.values_prof_idxs.begin(), mvb.values_prof_idxs.end(), 
        [](const std::pair<hpcrun_metricVal_t,uint32_t>& lhs, const std::pair<hpcrun_metricVal_t,uint32_t>& rhs) {
          return lhs.second < rhs.second;
        });  
      mvb.num_values = mvb.values_prof_idxs.size(); //update each mid's num_values
    }
  }

}

//read all the profiles and convert data to appropriate bytes for a group of contexts
//std::vector<std::pair<std::map<uint32_t, uint64_t>, std::vector<char>>>
std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>>
SparseDB::readProfiles(const std::vector<uint32_t>& ctx_ids, 
                            const std::vector<tms_profile_info_t>& prof_info,
                            int threads,
                            const std::vector<std::vector<TMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                            MPI_File fh,
                            std::map<uint32_t, CtxMetricBlock>& ctx_met_blocks)
{
  
  /*
  //old design
  std::map<uint32_t, CtxMetricBlock> empty;
  std::vector<std::map<uint32_t, CtxMetricBlock> * > threads_ctx_met_blocks(threads, &empty);

  for(auto id : ctx_ids){
    CtxMetricBlock cmb;
    cmb.ctx_id = id;
    ctx_met_blocks.emplace(id, cmb);
  }

  #pragma omp parallel num_threads(threads) 
  {
    std::map<uint32_t, CtxMetricBlock> thread_cmb;
    threads_ctx_met_blocks[omp_get_thread_num()] = &thread_cmb;

    #pragma omp for 
    for(uint i = 0; i < prof_info.size(); i++){
      tms_profile_info_t pi = prof_info[i];
      std::vector<TMS_CtxIdIdxPair> prof_ctx_pairs = all_prof_ctx_pairs[i];
      readOneProfile(ctx_ids, pi, prof_ctx_pairs, fh, thread_cmb);
    }

    //merge all threads_ctx_met_blocks to global transpose_helper_cmbs
    #pragma omp for
    for(uint c = 0; c < ctx_ids.size(); c++){
      mergeOneCtxAllThreadBlocks(threads_ctx_met_blocks,ctx_met_blocks[ctx_ids[c]]);
    }

    //sort ctx_met_blocks(if it's still serial, should be outside of parallel region)
    sortCtxMetBlocks(ctx_met_blocks);
  }

  for (auto i = ctx_met_blocks.begin(); i != ctx_met_blocks.end(); ) {
    if (i->second.metrics.size() == 0) {
      i = ctx_met_blocks.erase(i);
    } else {
      i++;
    }
  }
  */

  //new design
  //std::vector<std::pair<std::map<uint32_t, uint64_t>, std::vector<char>>> profiles_data (prof_info.size());
  std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>> profiles_data (prof_info.size());

  //read all profiles for this ctx_ids group
  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < prof_info.size(); i++){
    tms_profile_info_t pi = prof_info[i];
    std::vector<TMS_CtxIdIdxPair> prof_ctx_pairs = all_prof_ctx_pairs[i];
    
    //std::map<uint32_t, uint64_t> my_ctx_pairs;
    std::vector<std::pair<uint32_t,uint64_t>> my_ctx_pairs;
    int ret = getMyCtxIdIdxPairs(pi, ctx_ids, prof_ctx_pairs, fh, my_ctx_pairs);
  
    std::vector<char> vmbytes;
    if(ret == SPARSE_OK){
      readValMidsBytes(ctx_ids, my_ctx_pairs, pi, fh, vmbytes);
    }

    profiles_data[i] = {my_ctx_pairs, vmbytes};
  }
  
  return profiles_data;
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
  for(uint i = 0; i < mvb.values_prof_idxs.size(); i++){
    auto& pair = mvb.values_prof_idxs[i];
    convertToByte8(pair.first.bits, bytes_pos);
    convertToByte4(pair.second,     bytes_pos + CMS_val_SIZE);
    bytes_pos += CMS_val_prof_idx_pair_SIZE;
  }

  return (bytes_pos - bytes);
}

//convert ALL MetricValBlock of one CtxMetricBlock to bytes at bytes location
//return number of bytes converted
int SparseDB::convertCtxMetrics(std::map<uint16_t, MetricValBlock>& metrics,                                        
                                char *bytes)
{
  uint64_t num_vals = 0;

  char* mvb_pos = bytes; 
  for(auto i = metrics.begin(); i != metrics.end(); i++){
    MetricValBlock mvb = i->second;
    i->second.num_values = mvb.values_prof_idxs.size(); //if we use sort in readProfiles, we don't need this
    num_vals += mvb.values_prof_idxs.size();

    int bytes_converted = convertOneMetricValBlock(mvb, mvb_pos);
    mvb_pos += bytes_converted;
  }

  assert(num_vals == (uint)(mvb_pos - bytes)/CMS_val_prof_idx_pair_SIZE);

  return (mvb_pos - bytes);
}

//build metric id and idx pairs for one context as bytes to bytes location
//return number of bytes built
int SparseDB::buildCtxMetIdIdxPairsBytes(const std::map<uint16_t, MetricValBlock>& metrics,                                        
                                         char *bytes)
{
  char* bytes_pos = bytes;
  uint64_t m_idx = 0;
  for(auto i = metrics.begin(); i != metrics.end(); i++){
    uint16_t mid = i->first;
    convertToByte2(mid,   bytes_pos);
    convertToByte8(m_idx, bytes_pos + CMS_mid_SIZE);
    bytes_pos += CMS_m_pair_SIZE;
    m_idx += i->second.num_values;
  }

   uint16_t mid = LastMidEnd;
   convertToByte2(mid,   bytes_pos);
   convertToByte8(m_idx, bytes_pos + CMS_mid_SIZE);
   bytes_pos += CMS_m_pair_SIZE;

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
  std::map<uint16_t, MetricValBlock> metrics = cmb.metrics;
  num_nzmids = metrics.size();

  int bytes_converted = convertCtxMetrics(metrics, bytes);
  num_vals = bytes_converted/CMS_val_prof_idx_pair_SIZE;

  bytes_converted += buildCtxMetIdIdxPairsBytes(metrics, bytes + num_vals * CMS_val_prof_idx_pair_SIZE);

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
/*
void SparseDB::convertOneCtx(const uint32_t ctx_id, 
                             const std::vector<uint64_t>& ctx_off,    
                             const std::vector<CtxMetricBlock>& ctx_met_blocks, 
                             const uint64_t first_ctx_off,
                             uint& met_byte_cnt,
                             uint& info_byte_cnt, 
                             char* met_bytes,                                 
                             char* info_bytes)
                             */
void SparseDB::convertOneCtx(const uint32_t ctx_id, 
                             const uint64_t next_ctx_off,    
                             const CtxMetricBlock& cmb,                          
                             const uint64_t first_ctx_off,
                             cms_ctx_info_t& ci,
                             uint& met_byte_cnt,
                             char* met_bytes)
{

  met_byte_cnt += convertOneCtxMetricBlock(cmb, met_bytes + ci.offset - first_ctx_off, ci.num_nzmids, ci.num_vals);

  if(ci.num_nzmids != 0)
    if(ci.offset + ci.num_vals * CMS_val_prof_idx_pair_SIZE + (ci.num_nzmids+1) * CMS_m_pair_SIZE !=  next_ctx_off){
      printf("ctx_id %d, offset: %ld, num_vals: %ld, num_nzmids %d, next off %ld\n", ctx_id, ci.offset, ci.num_vals, ci.num_nzmids, next_ctx_off );
      exitError("collected cct data (num_vals:" + std::to_string(ci.num_vals) + " /num_nzmids:" + std::to_string(ci.num_nzmids) + ") were wrong !");
    }

}

//convert a group of contexts to appropriate bytes 
void SparseDB::ctxBlocks2Bytes(const CtxMetricBlock& cmb, 
                               const std::vector<uint64_t>& ctx_off, 
                               const uint32_t& ctx_id,
                               int threads,
                               std::vector<char>& info_bytes,
                               std::vector<char>& metrics_bytes)
{
  //assert(ctx_met_blocks.size() > 0);
  //assert(ctx_met_blocks.size() <= ctx_ids.size());
  assert(ctx_off.size() > 0);

  //uint64_t first_ctx_off =  ctx_off[CTX_VEC_IDX(ctx_ids[0])]; 
  uint64_t first_ctx_off =  ctx_off[CTX_VEC_IDX(ctx_id)]; 
  uint info_byte_cnt = 0;
  uint met_byte_cnt  = 0;

  /*
  //for ctx_met_blocks, not single cmb
  auto it = ctx_met_blocks.begin();
  for(uint i = 0; i<ctx_ids.size(); i++){
    //INFO BYTES
    uint32_t ctx_id = ctx_ids[i];
    cms_ctx_info_t ci = {ctx_id, 0, 0, ctx_off[CTX_VEC_IDX(ctx_id)]};

    //METRIC BYTES
    if(it != ctx_met_blocks.end()){
      uint32_t cmb_ctx_id = it->second.ctx_id;
      
      if(ctx_id == cmb_ctx_id){ // this ctx_id does have values in it
        convertOneCtx(ctx_id, ctx_off[CTX_VEC_IDX(ctx_id+2)], it->second, first_ctx_off, ci, met_byte_cnt, metrics_bytes.data());
        it++;
      }else if (ctx_id > cmb_ctx_id){ // this should not happen
        exitError("ctxBlocks2Bytes: either we missed ctxs from CtxMetricBlocks or there exists some invalid ctxs in CtxMetricBlocks\n");
      }
    }

    //INFO BYTES
    info_byte_cnt += convertOneCtxInfo(ci, info_bytes.data() + i * CMS_ctx_info_SIZE );

  }
  */

  //for single cmb
  cms_ctx_info_t ci = {ctx_id, 0, 0, ctx_off[CTX_VEC_IDX(ctx_id)]};

  if(cmb.metrics.size() > 0)
    convertOneCtx(ctx_id, ctx_off[CTX_VEC_IDX(ctx_id+2)], cmb, first_ctx_off, ci, met_byte_cnt, metrics_bytes.data());

  info_byte_cnt += convertOneCtxInfo(ci, info_bytes.data());


  //same for both versions
  if(info_byte_cnt != info_bytes.size())    exitError("the count of info_bytes converted is not as expected"
    + std::to_string(info_byte_cnt) + " != " + std::to_string(info_bytes.size()));
 
  if(met_byte_cnt  != metrics_bytes.size()) exitError("the count of metrics_bytes converted is not as expected" 
    + std::to_string(met_byte_cnt) + " != " + std::to_string(metrics_bytes.size()));

}


//given ctx_met_blocks, convert all and write everything for the group of contexts, to the ofh file 
void SparseDB::writeCtxGroup(const uint32_t& ctx_id,
                             const std::vector<uint64_t>& ctx_off,
                             const CtxMetricBlock& cmb,
                             const int threads,
                             MPI_File ofh)
{
  //assert(ctx_met_blocks.size() <= ctx_ids.size());
  
  //std::vector<char> info_bytes (CMS_ctx_info_SIZE * ctx_ids.size());
  std::vector<char> info_bytes (CMS_ctx_info_SIZE);

  /*
  uint32_t first_ctx_id = ctx_ids.front();
  uint32_t last_ctx_id  = ctx_ids.back();
  int metric_bytes_size = ctx_off[CTX_VEC_IDX(last_ctx_id) + 1] - ctx_off[CTX_VEC_IDX(first_ctx_id)];
  std::vector<char> metrics_bytes (metric_bytes_size);
  */
  int metric_bytes_size = ctx_off[CTX_VEC_IDX(ctx_id) + 1] - ctx_off[CTX_VEC_IDX(ctx_id)];
  std::vector<char> metrics_bytes (metric_bytes_size);
  
  ctxBlocks2Bytes(cmb, ctx_off, ctx_id, threads, info_bytes, metrics_bytes);

  /*
  MPI_Status stat;
  MPI_Offset info_off = HPCCCTSPARSE_FMT_CtxInfoOff + CTX_VEC_IDX(first_ctx_id) * CMS_ctx_info_SIZE;
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, info_off, info_bytes.data(), info_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Info Bytes for ctx ") + std::to_string(first_ctx_id) + " to ctx " + std::to_string(last_ctx_id));

  MPI_Offset metrics_off = ctx_off[CTX_VEC_IDX(first_ctx_id)];
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, metrics_off, metrics_bytes.data(), metrics_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Metrics Bytes for ctx ") + std::to_string(first_ctx_id) + " to ctx " + std::to_string(last_ctx_id));
  */
  MPI_Status stat;
  MPI_Offset info_off = HPCCCTSPARSE_FMT_CtxInfoOff + CTX_VEC_IDX(ctx_id) * CMS_ctx_info_SIZE;
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, info_off, info_bytes.data(), info_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Info Bytes for ctx ") + std::to_string(ctx_id));

  MPI_Offset metrics_off = ctx_off[CTX_VEC_IDX(ctx_id)];
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, metrics_off, metrics_bytes.data(), metrics_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Metrics Bytes for ctx ") + std::to_string(ctx_id));
}


//---------------------------------------------------------------------------
// read and write for all contexts in this rank's list
//---------------------------------------------------------------------------
//read a context group's data and write them out
void SparseDB::rwOneCtxGroup(const std::vector<uint32_t>& ctx_ids, 
                             const std::vector<tms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                             const std::vector<std::vector<TMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                             const MPI_File fh, 
                             MPI_File ofh)
{
  if(ctx_ids.size() == 0) return;
  std::map<uint32_t, CtxMetricBlock> fake_ctx_met_blocks;

  //read corresponding ctx_id_idx pairs and relevant ValMidsBytes
  //std::vector<std::pair<std::map<uint32_t, uint64_t>, std::vector<char>>> profiles_data =
  std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>> profiles_data =
    readProfiles(ctx_ids, prof_info, threads, all_prof_ctx_pairs, fh, fake_ctx_met_blocks);

  //find a small unit (number of ctxs) to be one task
  //int ctx_unit = round(ctx_ids.size()/(threads*5));

  //for each ctx, find corresponding ctx_id_idx and bytes, and interpret
  #pragma omp parallel num_threads(threads)
  {
    std::vector<int> profiles_cursor (profiles_data.size(), SPARSE_NOT_FOUND);
    uint ctx_ids_size = round(ctx_ids.size()/threads);
    int thread_num = omp_get_thread_num();
    uint my_start = ctx_ids_size * thread_num;
    uint my_end = (thread_num != threads -1) ? my_start + ctx_ids_size : ctx_ids.size();

    for(uint i = my_start; i < my_end; i++){
      uint32_t ctx_id = ctx_ids[i];

      //a new CtxMetricBlock
      CtxMetricBlock cmb;
      cmb.ctx_id = ctx_id;
    
      //interpret all profiles' data about this ctx_id
      for(uint j = 0; j < profiles_data.size(); j++){
        std::vector<char>& vmbytes = profiles_data[j].second;
        //std::map<uint32_t, uint64_t>& ctx_id_idx_pairs = profiles_data[j].first;
        std::vector<std::pair<uint32_t, uint64_t>>& ctx_id_idx_pairs = profiles_data[j].first;
        
        //set up the profile cursor if first time for this thread, otherwise just use it
        if(profiles_cursor[j] == SPARSE_NOT_FOUND){
          profiles_cursor[j]=(int)(lower_bound(ctx_id_idx_pairs.begin(),ctx_id_idx_pairs.end(), 
                std::make_pair(ctx_id,std::numeric_limits<uint64_t>::min()), //Value to compare
                [](const std::pair<uint32_t, uint64_t>& lhs, const std::pair<uint32_t, uint64_t>& rhs)      
                { return lhs.first < rhs.first ;}) - ctx_id_idx_pairs.begin());     
        }
  
        if(ctx_id_idx_pairs[profiles_cursor[j]].first == ctx_id){
        //auto ciipi = ctx_id_idx_pairs.find(ctx_id);
        //if(ciipi != ctx_id_idx_pairs.end()){
          //uint64_t next_ctx_idx = std::next(ciipi, 1)->second;
          //uint64_t first_ctx_idx = ctx_id_idx_pairs.begin()->second;
          uint64_t next_ctx_idx = ctx_id_idx_pairs[profiles_cursor[j]+1].second;
          uint64_t first_ctx_idx = ctx_id_idx_pairs[0].second;
          interpretValMidsBytes(vmbytes.data(), prof_info[j].prof_info_idx, ctx_id_idx_pairs[profiles_cursor[j]], next_ctx_idx, first_ctx_idx, ctx_id_idx_pairs, cmb);
          profiles_cursor[j]++;
        }

      }
      //write for ctx_met_blocks     
      //if(ctx_met_blocks.size() > 0){
        writeCtxGroup(ctx_id, ctx_off, cmb, threads, ofh);
      //}

    } // END of parallel for loop

  }//END of parallel region
  
   
}

//read ALL context groups' data and write them out
void SparseDB::rwAllCtxGroup(const std::vector<uint32_t>& my_ctxs, 
                             const std::vector<tms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                             const std::vector<std::vector<TMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                             const MPI_File fh, 
                             MPI_File ofh)
{
  //For each ctx group (< memory limit) this rank is in charge of, read and write
  std::vector<uint32_t> ctx_ids;
  size_t cur_size = 0;
  int cur_cnt = 0;
  for(uint i =0; i<my_ctxs.size(); i++){
    uint32_t ctx_id = my_ctxs[i];
    size_t cur_ctx_size = ctx_off[CTX_VEC_IDX(ctx_id) + 1] - ctx_off[CTX_VEC_IDX(ctx_id)];

    if((cur_size + cur_ctx_size) <= pow(10,6)) { //temp 10^4 TODO: user-defined memory limit
    //if(cur_cnt <= 1000){
      ctx_ids.emplace_back(ctx_id);
      cur_size += cur_ctx_size;
      cur_cnt++;
    }else{
      rwOneCtxGroup(ctx_ids, prof_info, ctx_off, threads, all_prof_ctx_pairs, fh, ofh);
      ctx_ids.clear();
      ctx_ids.emplace_back(ctx_id);
      cur_size = cur_ctx_size;
      cur_cnt = 1;
    }   

    // final ctx group
    if((i == my_ctxs.size() - 1) && (ctx_ids.size() != 0)) rwOneCtxGroup(ctx_ids, prof_info, ctx_off, threads, all_prof_ctx_pairs, fh, ofh);
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
  unionMids(ctx_nzmids,world_rank,world_size, threads);

  //Get context global final offsets for cct_major_sparse.db
  std::vector<uint64_t> ctx_off (ctxcnt + 1);
  getCtxOffset(ctx_nzval_cnts, ctx_nzmids, threads, world_rank, ctx_off);
  std::vector<uint32_t> my_ctxs;
  getMyCtxs(ctx_off, world_size, world_rank, my_ctxs);
  updateCtxOffset(ctxcnt, threads, ctx_off);


  //Prepare files to read and write, get the list of profiles
  MPI_File thread_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "thread_major_sparse.db").c_str(),
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &thread_major_f);
  MPI_File cct_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "cct_major_sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &cct_major_f);
  
  if(world_rank == 0){
    // Write hdr
    std::vector<char> hdr;
    hdr.insert(hdr.end(), HPCCCTSPARSE_FMT_Magic, HPCCCTSPARSE_FMT_Magic + HPCCCTSPARSE_FMT_MagicLen);
    hdr.emplace_back(HPCCCTSPARSE_FMT_Version);
    convertToByte4(ctxcnt, hdr.data() + HPCCCTSPARSE_FMT_HeaderLen); 
    SPARSE_exitIfMPIError(writeAsByteX(hdr, HPCCCTSPARSE_FMT_CtxInfoOff, cct_major_f, 0),
      __FUNCTION__ + std::string(": write the hdr wrong"));
  }
  
  std::vector<tms_profile_info_t> prof_info;
  readProfileInfo(threads, thread_major_f, prof_info);

  std::vector<std::vector<TMS_CtxIdIdxPair>> all_prof_ctx_pairs = getProfileCtxIdIdxPairs(thread_major_f, threads,prof_info);
  
  rwAllCtxGroup(my_ctxs, prof_info, ctx_off, threads, all_prof_ctx_pairs, thread_major_f, cct_major_f);

  //Close both files
  MPI_File_close(&thread_major_f);
  MPI_File_close(&cct_major_f);


}


//***************************************************************************
// general - YUMENG
//***************************************************************************

void SparseDB::merge(int threads) {
  int world_rank;
  int world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  ctxcnt = mpi::bcast(ctxcnt, 0);

  {
    util::log::debug msg{false};  // Switch to true for CTX id printouts
    msg << "CTXs (" << world_rank << ":" << sparseInputs.size() << "): "
        << ctxcnt;
  }

  std::vector<uint64_t> ctx_nzval_cnts (ctxcnt,0);
  std::set<uint16_t> empty;
  std::vector<std::set<uint16_t>> ctx_nzmids(ctxcnt,empty);
  writeThreadMajor(threads,world_rank,world_size, ctx_nzval_cnts,ctx_nzmids);
  writeCCTMajor(ctx_nzval_cnts,ctx_nzmids, ctxcnt, world_rank, world_size, threads);
  
}


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
  //return SPARSE_NOT_FOUND; 
  return (R == -1) ? R : (-2 - R); //make it negative to differentiate it from found
}

MPI_Datatype SparseDB::createTupleType(const std::vector<MPI_Datatype>& types)
{
  MPI_Datatype TupleType;
  int num_elem = types.size();
  int blocklen[num_elem];
  MPI_Aint disp[num_elem];
  MPI_Aint cur_disp = 0;
  for(int i = 0; i<num_elem; i++){
    blocklen[i] = 1;
    disp[i] = cur_disp;
    int this_type_size;
    MPI_Type_size(types[i], &this_type_size);
    cur_disp += this_type_size;
  }

  MPI_Type_create_struct(num_elem, blocklen, disp, types.data(), &TupleType);
  MPI_Type_commit(&TupleType);

  return TupleType;
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


