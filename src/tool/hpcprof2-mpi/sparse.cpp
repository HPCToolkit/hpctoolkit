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

#include <lib/profile/util/log.hpp>
#include <lib/profile/mpi/all.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/id-tuple.h>
#include <lib/prof/pms-format.h>
#include <lib/prof/cms-format.h>


#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <omp.h>
#include <assert.h>
#include <stdexcept> 

using namespace hpctoolkit;

SparseDB::SparseDB(stdshim::filesystem::path p, int threads)
  : dir(std::move(p)), ctxMaxId(0), fpos(0), ctxGrpId(0), accFpos(1000),
    accCtxGrp(1001), outputCnt(0), team_size(threads),
    parForPi([&](pms_profile_info_t& item){ handleItemPi(item); }),
    parForCtxs([&](ctxRange& item){ handleItemCtxs(item); }),
    parForPd([&](profData& item){ handleItemPd(item); }),
    parForCiip([&](profCtxIdIdxPairs& item){ handleItemCiip(item); }) {

  if(dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);
}

util::WorkshareResult SparseDB::help() {
  auto res = parForPi.contribute();
  if(!res.completed) return res;
  res = parForCiip.contribute();
  if(!res.completed) return res;
  res = parForPd.contribute();
  if(!res.completed) return res;
  return parForCtxs.contribute();
}

void SparseDB::notifyPipeline() noexcept {
  src.registerOrderedWavefront();
  src.registerOrderedWrite();
  auto& ss = src.structs();
  ud.context = ss.context.add<udContext>(std::ref(*this));
}

void SparseDB::notifyWavefront(DataClass d) noexcept {
  if(!d.hasContexts() || !d.hasThreads()) return;
  auto mpiSem = src.enterOrderedWavefront();
  auto sig = contextWavefront.signal();

  std::map<unsigned int, std::reference_wrapper<const Context>> cs;
  src.contexts().citerate([&](const Context& c){
    auto id = c.userdata[src.identifier()];
    ctxMaxId = std::max(ctxMaxId, id);
    if(!cs.emplace(id, c).second)
      util::log::fatal() << "Duplicate Context identifier "
                         << c.userdata[src.identifier()] << "!";
  }, nullptr);

  contexts.reserve(cs.size());
  for(const auto& ic: cs) contexts.emplace_back(ic.second);

  ctxcnt = contexts.size();

  // hdr
  int my_num_prof = src.threads().size();
  if(mpi::World::rank() == 0) my_num_prof++;
  uint32_t total_num_prof = getTotalNumProfiles(my_num_prof);
  prof_info_sec_ptr = PMS_hdr_SIZE;
  prof_info_sec_size = total_num_prof * PMS_prof_info_SIZE;
  id_tuples_sec_ptr = prof_info_sec_ptr + MULTIPLE_8(prof_info_sec_size);

  pmf = util::File(dir / "profile1.db", true);

  // write id_tuples, set id_tuples_sec_size for hdr
  workIdTuplesSection1(total_num_prof);

  // write hdr
  writePMSHdr(total_num_prof, *pmf);

  // prep for profiles writing 
  obuffers = std::vector<OutBuffer>(2);
  obuffers[0].cur_pos = 0;
  obuffers[1].cur_pos = 0;
  cur_obuf_idx = 0;
  prof_infos.resize(my_num_prof);

  // start the window to keep track of the real file cursor
  fpos += id_tuples_sec_ptr + MULTIPLE_8(id_tuples_sec_size);
  accFpos.initialize(fpos);

}

void SparseDB::notifyThreadFinal(const Thread::Temporary& tt) {
  const auto& t = tt.thread();
  contextWavefront.wait();

  // Allocate the blobs needed for the final output
  std::vector<hpcrun_metricVal_t> values;
  std::vector<uint16_t> mids;
  std::vector<uint32_t> cids;
  std::vector<uint64_t> coffsets;
  coffsets.reserve(contexts.size() + 1);
  uint64_t pre_val_size;


  // Now stitch together each Context's results
  for(const Context& c: contexts) {
    if(auto accums = tt.accumulatorsFor(c)) {
      cids.push_back(c.userdata[src.identifier()]);
      coffsets.push_back(values.size());
      pre_val_size = values.size();
      for(const auto& mx: accums->citerate()) {
        const Metric& m = mx.first;
        const auto& vv = mx.second;
        if(!m.scopes().has(MetricScope::function) || !m.scopes().has(MetricScope::execution))
          util::log::fatal{} << "Metric isn't function/execution!";
        const auto& ids = m.userdata[src.mscopeIdentifiers()];
        hpcrun_metricVal_t v;
        if(auto vex = vv.get(MetricScope::function)) {
          v.r = *vex;
          mids.push_back(ids.function);
          values.push_back(v);
        }
        if(auto vinc = vv.get(MetricScope::execution)) {
          v.r = *vinc;
          mids.push_back(ids.execution);
          values.push_back(v);
        }
      }
      c.userdata[ud].cnt += (values.size() - pre_val_size);
    }
  }

  //Add the extra ctx id and offset pair, to mark the end of ctx 
  cids.push_back(LastNodeEnd);
  coffsets.push_back(values.size());

  // Put together the sparse_metrics structure
  hpcrun_fmt_sparse_metrics_t sm;
  sm.id_tuple.length = 0; //always 0 here
  sm.num_vals = values.size();
  sm.num_cct_nodes = contexts.size();
  sm.num_nz_cct_nodes = coffsets.size() - 1; //since there is an extra end node 
  sm.values = values.data();
  sm.mids = mids.data();
  sm.cct_node_ids = cids.data();
  sm.cct_node_idxs = coffsets.data();

  // Convert the sparse_metrics structure to binary form
  auto sparse_metrics_bytes = profBytes(&sm);
  assert(sparse_metrics_bytes.size() == (values.size() * PMS_vm_pair_SIZE + coffsets.size() * PMS_ctx_pair_SIZE));

  // Build prof_info
  pms_profile_info_t pi;
  pi.prof_info_idx = t.userdata[src.identifier()] + 1;
  pi.num_vals = values.size();
  pi.num_nzctxs = coffsets.size() - 1;
  pi.id_tuple_ptr = id_tuple_ptrs[pi.prof_info_idx - min_prof_info_idx];
  pi.metadata_ptr = 0;
  pi.spare_one = 0;
  pi.spare_two = 0;

  pi.offset = writeProf(sparse_metrics_bytes, pi.prof_info_idx, mode_reg_thr);
  prof_infos[pi.prof_info_idx - min_prof_info_idx] = std::move(pi);
/*
  // Set up the output temporary file.
  stdshim::filesystem::path outfile;
  int world_rank;
  {
    world_rank = mpi::World::rank();
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
  outputs.emplace(&t, std::move(outfile));*/
}

void SparseDB::write()
{
  auto mpiSem = src.enterOrderedWrite();
  int world_rank = mpi::World::rank();

  ctxcnt = mpi::bcast(ctxcnt, 0);
  ctx_nzmids_cnts.resize(ctxcnt, 1);//one for LastNodeEnd

  if(world_rank == 0){
    // Allocate the blobs needed for the final output
    std::vector<hpcrun_metricVal_t> values;
    std::vector<uint16_t> mids;
    std::vector<uint32_t> cids;
    std::vector<uint64_t> coffsets;
    coffsets.reserve(contexts.size() + 1);

    // Now stitch together each Context's results
    for(const Context& c: contexts) {
      const auto& stats = c.statistics();
      if(stats.size() > 0) {
        cids.push_back(c.userdata[src.identifier()]);
        coffsets.push_back(values.size());
      }
      for(const auto& mx: stats.citerate()) {
        bool hasEx = false;
        bool hasInc = false;
        const Metric& m = mx.first;
        if(!m.scopes().has(MetricScope::function) || !m.scopes().has(MetricScope::execution))
          util::log::fatal{} << "Metric isn't function/execution!";
        const auto& ids = m.userdata[src.mscopeIdentifiers()];
        const auto& vv = mx.second;
        size_t idx = 0;
        for(const auto& sp: m.partials()) {
          hpcrun_metricVal_t v;
          if(auto vex = vv.get(sp).get(MetricScope::function)) {
            v.r = *vex;
            mids.push_back((ids.function << 8) + idx);
            values.push_back(v);
            hasEx = true;
          }
          if(auto vinc = vv.get(sp).get(MetricScope::execution)) {
            v.r = *vinc;
            mids.push_back((ids.execution << 8) + idx);
            values.push_back(v);
            hasInc = true;
          }
          idx++;
        }
        if(hasEx) ctx_nzmids_cnts[c.userdata[src.identifier()]]++;
        if(hasInc) ctx_nzmids_cnts[c.userdata[src.identifier()]]++;
      }
    }

  

    //Add the extra ctx id and offset pair, to mark the end of ctx
    cids.push_back(LastNodeEnd);
    coffsets.push_back(values.size());

    // Put together the sparse_metrics structure
    hpcrun_fmt_sparse_metrics_t sm;
    //sm.tid = 0;
    sm.id_tuple.length = 0;
    sm.num_vals = values.size();
    sm.num_cct_nodes = contexts.size();
    sm.num_nz_cct_nodes = coffsets.size() - 1; //since there is an extra end node 
    sm.values = values.data();
    sm.mids = mids.data();
    sm.cct_node_ids = cids.data();
    sm.cct_node_idxs = coffsets.data();
  /*
    // Set up the output temporary file.
    summaryOut = dir / "tmp-summary.sparse-db";
    std::FILE* of = std::fopen(summaryOut.c_str(), "wb");
    if(!of) util::log::fatal() << "Unable to open temporary summary sparse-db file for output!";

    // Spit it all out, and close up.
    if(hpcrun_fmt_sparse_metrics_fwrite(&sm, of) != HPCFMT_OK)
      util::log::fatal() << "Error writing out temporary summary sparse-db!";
    std::fclose(of);
  */
    // Build prof_info
    pms_profile_info_t pi;
    pi.prof_info_idx = 0;
    pi.num_vals = values.size();
    pi.num_nzctxs = coffsets.size() - 1;
    pi.id_tuple_ptr = id_tuple_ptrs[0];
    pi.metadata_ptr = 0;
    pi.spare_one = 0;
    pi.spare_two = 0;

    auto sparse_metrics_bytes = profBytes(&sm);
    assert(sparse_metrics_bytes.size() == (values.size() * PMS_vm_pair_SIZE + coffsets.size() * PMS_ctx_pair_SIZE));
    
    pi.offset = writeProf(sparse_metrics_bytes, 0, mode_wrt_root);
    prof_infos[0] = std::move(pi);

    //write prof_infos
    writeProfInfos();
  }else{// end of root rank

    std::vector<char> fake_sm_bytes;
    int ret = writeProf(fake_sm_bytes, 0, mode_wrt_nroot); // 0 won't be used 
    assert(ret == -1);

    //write prof_infos
    writeProfInfos();
  } 


  if(mpi::World::rank() == 0){
    //footer to show completeness
    auto pmfi = pmf->open(true);
    auto footer_val = PROFDBft;
    uint64_t footer_off = filePosFetchOp(sizeof(footer_val));
    pmfi.writeat(footer_off, sizeof(footer_val), &footer_val);

  }
  

  //gather cct major data
  ctx_nzval_cnts1.resize(ctxcnt, 0);
  for(const Context& c: contexts) 
    ctx_nzval_cnts1[c.userdata[src.identifier()]] = c.userdata[ud].cnt.load(std::memory_order_relaxed);
  

  //write CCT major
  writeCCTMajor1();

}

//***************************************************************************
// Work with bytes
//***************************************************************************
void SparseDB::writeAsByte4(uint32_t val, util::File::Instance& fh, uint64_t off){
  int shift = 0, num_writes = 0;
  char input[4];

  for (shift = 24; shift >= 0; shift -= 8) {
    input[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }

  fh.writeat(off, 4, input);

}

void SparseDB::writeAsByte8(uint64_t val, util::File::Instance& fh, uint64_t off){
  int shift = 0, num_writes = 0;
  char input[8];

  for (shift = 56; shift >= 0; shift -= 8) {
    input[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }

  fh.writeat(off, 8, input);

}

uint32_t SparseDB::readAsByte4(util::File::Instance& fh, uint64_t off){
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

uint64_t SparseDB::readAsByte8(util::File::Instance& fh, uint64_t off){
  uint32_t v = 0;
  int shift = 0, num_reads = 0;
  char input[8];

  fh.readat(off, 8, input);
  
  for (shift = 56; shift >= 0; shift -= 8) {
    v |= ((uint64_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  return v;
}

uint16_t SparseDB::interpretByte2(const char *input){
  uint16_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    v |= ((uint16_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  return v;
}

uint32_t SparseDB::interpretByte4(const char *input){
  uint32_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  return v;
}

uint64_t SparseDB::interpretByte8(const char *input){
  uint64_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    v |= ((uint64_t)(input[num_reads] & 0xff) << shift); 
    num_reads++;
  }

  return v;
}

std::vector<char> SparseDB::convertToByte2(uint16_t val){
  std::vector<char> bytes(2);
  int shift = 0, num_writes = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes;
}

std::vector<char> SparseDB::convertToByte4(uint32_t val){
  std::vector<char> bytes(4);
  int shift = 0, num_writes = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes;
}

std::vector<char> SparseDB::convertToByte8(uint64_t val){
  std::vector<char> bytes(8);
  int shift = 0, num_writes = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes;
}



//***************************************************************************
// profile.db  - YUMENG
//
///EXAMPLE
///HPCPROF-tmsdb_____
///[hdr:
///  (version: 0)
///]
///[Id tuples for 121 profiles
///  0[(SUMMARY: 0) ]
///  1[(RANK: 0) (THREAD: 0) ]
///  2[(RANK: 0) (THREAD: 1) ]
///  ...
///]
///[Profile informations for 72 profiles
///  0[(id_tuple_ptr: 23) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 12984) (num_nzctxs: 8353) (starting location: 489559)]
///  1[(id_tuple_ptr: 31) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 4422) (num_nzctxs: 3117) (starting location: 886225)]
///  ...
///]
///[thread 36
///  [metrics:
///  (NOTES: printed in file order, help checking if the file is correct)
///    (value: 2.8167, metric id: 1)
///    (value: 2.8167, metric id: 1)
///    ...
///  ]
///  [ctx indices:
///    (ctx id: 1, index: 0)
///    (ctx id: 7, index: 1)
///    ...
///  ]
///]
///...
//***************************************************************************
void SparseDB::assignSparseInputs(int world_rank)
{
  for(const auto& tp : outputs.citerate()){
    if(tp.first->attributes.idTuple().size() == 0) continue; //skip this profile
    //regular prof_info_idx starts with 1, 0 is for summary
    sparseInputs.emplace_back(tp.first->userdata[src.identifier()] + 1, tp.second.string());
  }

  if(world_rank == 0)
    sparseInputs.emplace_back(IDTUPLE_SUMMARY_PROF_INFO_IDX, summaryOut.string());
  
}

uint32_t SparseDB::getTotalNumProfiles(const uint32_t my_num_prof)
{
  return mpi::allreduce(my_num_prof, mpi::Op::sum());
}

//---------------------------------------------------------------------------
// header
//---------------------------------------------------------------------------
void SparseDB::writePMSHdr(const uint32_t total_num_prof, const util::File& fh)
{
  if(mpi::World::rank() != 0) return;

  std::vector<char> hdr;
  
  hdr.insert(hdr.end(), HPCPROFILESPARSE_FMT_Magic, HPCPROFILESPARSE_FMT_Magic + HPCPROFILESPARSE_FMT_MagicLen);

  hdr.emplace_back(HPCPROFILESPARSE_FMT_VersionMajor);
  hdr.emplace_back(HPCPROFILESPARSE_FMT_VersionMinor);

  auto b = convertToByte4(total_num_prof);
  hdr.insert(hdr.end(), b.begin(), b.end());

  b = convertToByte2(HPCPROFILESPARSE_FMT_NumSec); 
  hdr.insert(hdr.end(), b.begin(), b.end());

  b = convertToByte8(prof_info_sec_size);
  hdr.insert(hdr.end(), b.begin(), b.end());
  b = convertToByte8(prof_info_sec_ptr);
  hdr.insert(hdr.end(), b.begin(), b.end());

  b = convertToByte8(id_tuples_sec_size);
  hdr.insert(hdr.end(), b.begin(), b.end());
  b = convertToByte8(id_tuples_sec_ptr);
  hdr.insert(hdr.end(), b.begin(), b.end());
  
  assert(hdr.size() == PMS_real_hdr_SIZE);
  auto fhi = fh.open(true);
  fhi.writeat(0, PMS_real_hdr_SIZE, hdr.data());
  
}

//---------------------------------------------------------------------------
// profile offsets
//---------------------------------------------------------------------------
uint64_t SparseDB::getProfileSizes()
{
  uint64_t my_size = 0;
  for(const auto& tp: sparseInputs){
    struct stat buf;
    stat(tp.second.c_str(),&buf);
    my_size += (buf.st_size - PMS_prof_skip_SIZE);
    profile_sizes.emplace_back(buf.st_size - PMS_prof_skip_SIZE);    
  }
  return my_size;
}


uint64_t SparseDB::getMyOffset(const uint64_t my_size, const int rank)
{
  return mpi::exscan(my_size, mpi::Op::sum()).value_or(0); ;
}


void SparseDB::getMyProfOffset(const uint32_t total_prof, const uint64_t my_offset, 
                               const int threads)
{
  prof_offsets.resize(profile_sizes.size());

  std::vector<uint64_t> tmp (profile_sizes.size());
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < tmp.size();i++)
    tmp[i] = profile_sizes[i];
  

  exscan<uint64_t>(tmp,threads);

  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < tmp.size();i++){
    if(i < tmp.size() - 1) assert(tmp[i] + profile_sizes[i] == tmp[i+1]);
    prof_offsets[i] = tmp[i] + my_offset + PMS_hdr_SIZE
      + (MULTIPLE_8(prof_info_sec_size)) + (MULTIPLE_8(id_tuples_sec_size)); 
  }

}


void SparseDB::workProfSizesOffsets(const int world_rank, const int total_prof, 
                                    const int threads)
{
  //work on class private variable profile_sizes
  uint64_t my_size = getProfileSizes();
  uint64_t my_off = getMyOffset(my_size, world_rank);
  //work on class private variable prof_offsets
  getMyProfOffset(total_prof, my_off, threads);
}


//---------------------------------------------------------------------------
// profile id tuples 
//---------------------------------------------------------------------------

std::vector<std::pair<uint16_t, uint64_t>>  SparseDB::getMyIdTuplesPairs()
{
  std::vector<std::pair<uint16_t, uint64_t>> pairs;
  
  uint64_t rank = mpi::World::rank();

  //each rank's pairs are led by a pair: {RANK_SPOT, rank number}
  pairs.emplace_back(RANK_SPOT, rank);

  //go through my rank's profiles' tuples and save them as pairs
  for(const auto& tp : outputs.citerate()){
    auto& ta = tp.first->attributes;
    uint16_t tuple_length = ta.idTuple().size();
    uint64_t prof_info_idx = tp.first->userdata[src.identifier()] + 1;
    if(tuple_length == 0) continue; //skip this profile

    pairs.emplace_back(tuple_length, prof_info_idx);
    for(auto id : ta.idTuple())
      pairs.emplace_back(id.kind, id.index);
  }

  if(rank != 0) return pairs;

  //don't forget summary file for rank 0
  pairs.emplace_back(IDTUPLE_SUMMARY_LENGTH, IDTUPLE_SUMMARY_PROF_INFO_IDX); 
  pairs.emplace_back(IDTUPLE_SUMMARY, IDTUPLE_SUMMARY_IDX); 
  return pairs;
}


std::vector<pms_id_tuple_t>
SparseDB::intPairs2Tuples(const std::vector<std::pair<uint16_t, uint64_t>>& all_pairs)
{
  std::vector<pms_id_tuple_t> tuples;
  uint i = 0;   //idx in all_pairs
  uint idx = 0; //idx in tuples
  uint cur_rank = -1; 
  while(i < all_pairs.size()){
    std::pair<uint16_t, uint64_t> p = all_pairs[i];

    //check if the following pairs belong to a new rank 
    if(p.first == RANK_SPOT){
      assert(p.second != cur_rank);
      cur_rank = p.second;
      i++;
      continue;
    }

    //create a new tuple
    id_tuple_t it;
    it.length = p.first;
    it.ids = (pms_id_t*)malloc(it.length * sizeof(pms_id_t));
    for(uint j = 0; j < it.length; j++){
      it.ids[j].kind = all_pairs[i+j+1].first;
      it.ids[j].index = all_pairs[i+j+1].second;
    }

    pms_id_tuple_t t;
    t.rank = cur_rank;
    t.all_at_root_idx = idx;
    t.prof_info_idx = p.second;
    t.idtuple = it;
    
    //update tuples, next idx in tuples, and next idx in all_pairs
    tuples.emplace_back(t);
    idx += 1;
    i += (1 + t.idtuple.length);
  }

  return tuples;
}



std::vector<std::pair<uint16_t, uint64_t>> SparseDB::gatherIdTuplesPairs(const int world_rank, const int world_size,
                                                                         const int threads, MPI_Datatype IntPairType, 
                                                                         const std::vector<std::pair<uint16_t, uint64_t>>& rank_pairs)
{
  //get the size of each ranks' pairs
  int rank_pairs_size = rank_pairs.size();
  auto all_rank_pairs_sizes = mpi::gather(rank_pairs_size, 0);

  //get the displacement of all ranks' pairs
  std::vector<int> all_rank_pairs_disps; 
  std::vector<std::pair<uint16_t, uint64_t>> all_rank_pairs;
  if(world_rank == 0){
    all_rank_pairs_disps.resize(world_size);

    #pragma omp parallel for num_threads(threads)
    for(int i = 0; i<world_size; i++) all_rank_pairs_disps[i] = all_rank_pairs_sizes->at(i);
    exscan<int>(all_rank_pairs_disps,threads); 

    int total_size = all_rank_pairs_disps.back() + all_rank_pairs_sizes->back();
    all_rank_pairs.resize(total_size);
  }

  MPI_Gatherv(rank_pairs.data(),rank_pairs_size, IntPairType, \
    all_rank_pairs.data(), all_rank_pairs_sizes->data(), all_rank_pairs_disps.data(), IntPairType, 0, MPI_COMM_WORLD);

  return all_rank_pairs;

}


void SparseDB::scatterIdxPtrs(const std::vector<std::pair<uint32_t, uint64_t>>& idx_ptr_buffer, 
                              const int num_prof,const int world_size, const int world_rank,
                              const int threads)
{
  rank_idx_ptr_pairs.resize(num_prof);

  auto all_rank_tuples_sizes = mpi::gather(num_prof, 0);

  //get the displacement of all ranks' tuples
  std::vector<int> all_rank_tuples_disps; 
  if(world_rank == 0){
    all_rank_tuples_disps.resize(world_size);

    #pragma omp parallel for num_threads(threads)
    for(int i = 0; i<world_size; i++) all_rank_tuples_disps[i] = all_rank_tuples_sizes->at(i);
    exscan<int>(all_rank_tuples_disps,threads); 
  }

  //create a new Datatype for prof_info_idx and offset
  MPI_Datatype IdxPtrType = createPairType<uint32_t, uint64_t>(MPI_UINT32_T, MPI_UINT64_T);

  MPI_Scatterv(idx_ptr_buffer.data(), all_rank_tuples_sizes->data(), all_rank_tuples_disps.data(), \
    IdxPtrType, rank_idx_ptr_pairs.data(), num_prof, IdxPtrType, 0, MPI_COMM_WORLD);
  MPI_Type_free(&IdxPtrType);

}


void SparseDB::sortIdTuples(std::vector<pms_id_tuple_t>& all_tuples)
{
  struct {
    bool operator()(pms_id_tuple_t a, 
                    pms_id_tuple_t b) const
    {   
      id_tuple_t& a_tuple = a.idtuple;
      id_tuple_t& b_tuple = b.idtuple;
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


void SparseDB::sortIdTuplesOnProfInfoIdx(std::vector<pms_id_tuple_t>& all_tuples)
{
  struct {
    bool operator()(pms_id_tuple_t a, 
                    pms_id_tuple_t b) const
    {   
      return a.prof_info_idx < b.prof_info_idx;
    }   
  }tupleComp;
  
  std::sort(all_tuples.begin(), all_tuples.end(), tupleComp);
}


void SparseDB::fillIdxPtrBuffer(std::vector<pms_id_tuple_t>& all_tuples,
                                std::vector<std::pair<uint32_t, uint64_t>>& buffer,
                                const int threads)
{
  assert(buffer.size() == all_tuples.size());
  std::vector<uint64_t> tupleSizes (all_tuples.size()+1,0);

  //notice the last entry in tupleSizes is still 0
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < all_tuples.size(); i++)
    tupleSizes[i] = PMS_id_tuple_len_SIZE + all_tuples[i].idtuple.length * PMS_id_SIZE;
  

  exscan<uint64_t>(tupleSizes,threads);

  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < all_tuples.size();i++){
    auto& t = all_tuples[i];
    buffer[t.all_at_root_idx] = {t.prof_info_idx, (tupleSizes[i] + id_tuples_sec_ptr)};
  }

}


void SparseDB::freeIdTuples(std::vector<pms_id_tuple_t>& all_tuples, const int threads)
{
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < all_tuples.size(); i++){
    free(all_tuples[i].idtuple.ids);
    all_tuples[i].idtuple.ids = NULL;
  }
}

 
std::vector<char> SparseDB::convertTuple2Bytes(const pms_id_tuple_t& tuple)
{
  std::vector<char> bytes;

  uint16_t len = tuple.idtuple.length;
  auto b = convertToByte2(len);
  bytes.insert(bytes.end(), b.begin(), b.end());

  for(uint i = 0; i < len; i++){
    auto& id = tuple.idtuple.ids[i];

    b = convertToByte2(id.kind);
    bytes.insert(bytes.end(), b.begin(), b.end());
    b = convertToByte8(id.index);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }

  assert(bytes.size() == (size_t)PMS_id_tuple_len_SIZE + len * PMS_id_SIZE);
  return bytes;
}


void SparseDB::writeAllIdTuples(const std::vector<pms_id_tuple_t>& all_tuples, const util::File& fh)
{
  std::vector<char> bytes;
  for(auto& tuple : all_tuples)
  {
    auto b = convertTuple2Bytes(tuple);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }

  auto fhi = fh.open(true);
  fhi.writeat(id_tuples_sec_ptr, bytes.size(), bytes.data());

  //set class private variable 
  id_tuples_sec_size = bytes.size();
}

//void SparseDB::workIdTuplesSection1(const int total_num_prof)
void SparseDB::workIdTuplesSection1(const int total_num_prof)
{
  int rank = mpi::World::rank();
  int local_num_prof = src.threads().size();
  if(rank == 0) local_num_prof++;

  std::vector<id_tuple_t> id_tuples(local_num_prof);
  id_tuple_ptrs.resize(local_num_prof);
  uint64_t local_tuples_size = 0; 

  // find the minimum prof_info_idx of this rank
  min_prof_info_idx = 0;
  if(rank != 0){
    min_prof_info_idx = total_num_prof;
    for(const auto& t : src.threads().iterate()) {
      uint32_t prof_info_idx = t->userdata[src.identifier()] + 1;
      if(prof_info_idx < min_prof_info_idx) min_prof_info_idx = prof_info_idx;
    }
  }

  for(const auto& t : src.threads().iterate()) {
    // get the idx in the id_tuples and id_tuple_ptrs
    uint32_t idx = t->userdata[src.identifier()] + 1 - min_prof_info_idx;

    // build the id_tuple
    id_tuple_t idt;
    idt.length = t->attributes.idTuple().size();
    idt.ids = (pms_id_t*)malloc(idt.length * sizeof(pms_id_t));
    for(uint i = 0; i < idt.length; i++)
      idt.ids[i] = t->attributes.idTuple()[i];
    

    id_tuples[idx] = std::move(idt);
    id_tuple_ptrs[idx] = PMS_id_tuple_len_SIZE + idt.length * PMS_id_SIZE; 
    local_tuples_size += id_tuple_ptrs[idx]; 
  }

  // don't forget the summary id_tuple
  if(rank == 0){
    id_tuple_t idt;
    idt.length = IDTUPLE_SUMMARY_LENGTH;
    idt.ids = (pms_id_t*)malloc(idt.length * sizeof(pms_id_t));
    idt.ids[0].kind = IDTUPLE_SUMMARY;
    idt.ids[0].index= IDTUPLE_SUMMARY_IDX;

    id_tuples[0] = std::move(idt);
    id_tuple_ptrs[0] = PMS_id_tuple_len_SIZE + idt.length * PMS_id_SIZE;
    local_tuples_size += id_tuple_ptrs[0]; 
  }

  // find where to write as a rank
  uint64_t my_offset = 0;
  my_offset = mpi::exscan(local_tuples_size, mpi::Op::sum()).value_or(0); 


  // write out id_tuples
  std::vector<char> bytes;
  for(auto& tuple : id_tuples)
  {
    pms_id_tuple_t temp;
    temp.idtuple = tuple;
    auto b = convertTuple2Bytes(temp);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }

  auto fhi = pmf->open(true);
  fhi.writeat(id_tuples_sec_ptr + my_offset, bytes.size(), bytes.data());

  //set class private variable 
  id_tuples_sec_size = mpi::allreduce(local_tuples_size, mpi::Op::sum());

  //id_tuple_ptrs now store the number of bytes for each idtuple, exscan to get ptr
  exscan<uint64_t>(id_tuple_ptrs, 1); // temp use 1 thread, try multiple later with Jonathon's new stuff
  for(auto& ptr : id_tuple_ptrs){
    ptr += (my_offset + id_tuples_sec_ptr);
  }

  // free all the tuples
  for(auto tuple : id_tuples){
    free(tuple.ids);
    tuple.ids = NULL;
  }

 
}


void SparseDB::workIdTuplesSection(const int world_rank, const int world_size, const int threads,
                                   const int num_prof, const util::File& fh)
{
  MPI_Datatype IntPairType = createPairType<uint16_t, uint64_t>(MPI_UINT16_T, MPI_UINT64_T);

  //will be used as a send buffer for {prof_info_idx, id_tuple_ptr}s
  std::vector<std::pair<uint32_t, uint64_t>> idx_ptr_buffer;

  //each rank collect its own pairs
  auto pairs = getMyIdTuplesPairs();

  //rank 0 gather all the pairs
  auto all_rank_pairs = gatherIdTuplesPairs(world_rank, world_size, threads, IntPairType, pairs);
  MPI_Type_free(&IntPairType);

  //rank 0 convert pairs to tuples and sort them
  if(world_rank == 0) {
    auto all_rank_tuples = intPairs2Tuples(all_rank_pairs);

    sortIdTuplesOnProfInfoIdx(all_rank_tuples);

    //write all the tuples
    writeAllIdTuples(all_rank_tuples, fh);
  
    //calculate the tuples' offset as ptrs
    idx_ptr_buffer.resize(all_rank_tuples.size());
    fillIdxPtrBuffer(all_rank_tuples, idx_ptr_buffer, threads);

    freeIdTuples(all_rank_tuples, threads);
  }

  //rank 0 sends back necessary info
  mpi::bcast(id_tuples_sec_size, 0);
  scatterIdxPtrs(idx_ptr_buffer, num_prof, world_size, world_rank, threads);

}


//---------------------------------------------------------------------------
// get profile's real data (bytes)
//---------------------------------------------------------------------------
std::vector<char> SparseDB::profBytes(hpcrun_fmt_sparse_metrics_t* sm)
{
  std::vector<char> out;
  std::vector<char> b;

  for (uint i = 0; i < sm->num_vals; ++i) {
    b = convertToByte8(sm->values[i].bits);
    out.insert(out.end(), b.begin(), b.end());
    b = convertToByte2(sm->mids[i]);
    out.insert(out.end(), b.begin(), b.end());
  }

  for (uint i = 0; i < sm->num_nz_cct_nodes + 1; ++i) {
    b = convertToByte4(sm->cct_node_ids[i]);
    out.insert(out.end(), b.begin(), b.end());
    b = convertToByte8(sm->cct_node_idxs[i]);
    out.insert(out.end(), b.begin(), b.end());
  }

  return out;

}

uint64_t SparseDB::writeProf(const std::vector<char>& prof_bytes, uint32_t prof_info_idx, int mode)
{
  uint64_t rel_off = -1; // only mode_wrt_nroot will return -1
                         // it doesn't have any data to add, so no position for it 
  uint64_t wrt_off = 0;  // only when it is going to write to the file, i.e write == true

  // add summary data to the current buffer
  if(mode == mode_wrt_root){
    obuffers[cur_obuf_idx].buf.insert(obuffers[cur_obuf_idx].buf.end(), prof_bytes.begin(), prof_bytes.end());
    rel_off = obuffers[cur_obuf_idx].cur_pos;
  }

  if(mode != mode_reg_thr){ // called by write(), no need for lock, just communicate between ranks
    OutBuffer& ob = obuffers[cur_obuf_idx];

    size_t my_size = ob.buf.size();
    if(my_size == 0) return rel_off;
    wrt_off = filePosFetchOp(my_size);


    //write
    auto pmfi = pmf->open(true);
    pmfi.writeat(wrt_off, ob.buf.size(), ob.buf.data());
    for(auto pi : ob.buffered_pidxs)
      prof_infos[pi-min_prof_info_idx].offset += wrt_off;
    ob.cur_pos = 0;
    ob.buf.clear();
    ob.buffered_pidxs.clear();

    return (mode == mode_wrt_nroot) ? rel_off : (wrt_off + rel_off);

  }else{
    std::unique_lock<std::mutex> olck (outputs_l);
    OutBuffer& ob = obuffers[cur_obuf_idx];

    std::unique_lock<std::mutex> lck (ob.mtx);

    // take care all previous profs in buffer
    bool write = false;
    if((prof_bytes.size() + ob.cur_pos) >= (1024 * 1024 * 1024)){ 
      cur_obuf_idx = 1 - cur_obuf_idx;
      write = true;

      size_t my_size = ob.buf.size() + prof_bytes.size(); 
      wrt_off = filePosFetchOp(my_size);

    }
    olck.unlock();

    //add bytes to the current buffer
    ob.buf.insert(ob.buf.end(), prof_bytes.begin(), prof_bytes.end());

    //record the prof_info_idx of the profile being added to the buffer
    ob.buffered_pidxs.emplace_back(prof_info_idx);

    //update current position
    rel_off = ob.cur_pos;
    ob.cur_pos += prof_bytes.size();

    if(write){
      auto pmfi = pmf->open(true);
      pmfi.writeat(wrt_off, ob.buf.size(), ob.buf.data());
      for(auto pi : ob.buffered_pidxs)
        prof_infos[pi-min_prof_info_idx].offset += wrt_off;
      
      ob.cur_pos = 0;
      ob.buf.clear();
      ob.buffered_pidxs.clear();
      return (rel_off + wrt_off);
    }

    return rel_off;
  }




}

uint64_t SparseDB::filePosFetchOp(uint64_t val)
{ 
  uint64_t r;
  if(mpi::World::size() == 1){ // just talk to its own data
    r = fpos;
    fpos += val;
  }else{ // More than one rank, use RMA
    r = accFpos.fetch_add(val);
  }
  return r;
}


void SparseDB::updateCtxMids(const char* input, const uint64_t ctx_nzval_cnt,
                             std::set<uint16_t>& ctx_nzmids)
{
  for(uint m = 0; m < ctx_nzval_cnt; m++){
    uint16_t mid = interpretByte2(input);
    ctx_nzmids.insert(mid); 
    input += (PMS_mid_SIZE + PMS_val_SIZE);
  }
}


void SparseDB::interpretOneCtxValCntMids(const char* val_cnt_input, const char* mids_input,
                                         std::vector<std::set<uint16_t>>& ctx_nzmids,
                                         std::vector<uint64_t>& ctx_nzval_cnts)
{
  uint32_t ctx_id = interpretByte4(val_cnt_input);
  
  // nzval_cnt
  uint64_t ctx_idx       = interpretByte8(val_cnt_input + PMS_ctx_id_SIZE);
  uint64_t next_ctx_idx  = interpretByte8(val_cnt_input + PMS_ctx_id_SIZE + PMS_ctx_pair_SIZE);
  uint64_t ctx_nzval_cnt = next_ctx_idx - ctx_idx;
  ctx_nzval_cnts[CTX_VEC_IDX(ctx_id)] += ctx_nzval_cnt;

  // nz-mids
  mids_input += ctx_idx * (PMS_mid_SIZE + PMS_val_SIZE) + PMS_val_SIZE;
  updateCtxMids(mids_input, ctx_nzval_cnt, ctx_nzmids[CTX_VEC_IDX(ctx_id)]);

}


void SparseDB::collectCctMajorData(const uint32_t prof_info_idx, std::vector<char>& bytes,
                                   std::vector<uint64_t>& ctx_nzval_cnts, 
                                   std::vector<std::set<uint16_t>>& ctx_nzmids)
{ 
  assert(ctx_nzval_cnts.size() == ctx_nzmids.size());
  assert(ctx_nzval_cnts.size() > 0);

  uint64_t num_vals    = interpretByte8(bytes.data() + PMS_fake_id_tuple_SIZE);
  uint32_t num_nzctxs  = interpretByte4(bytes.data() + PMS_fake_id_tuple_SIZE + PMS_num_val_SIZE);
  uint64_t ctx_end_idx = interpretByte8(bytes.data() + bytes.size() - PMS_ctx_idx_SIZE);
  if(num_vals != ctx_end_idx) 
    util::log::fatal() << "tmpDB file for thread " << prof_info_idx << " is corrupted!";
  
  char* val_cnt_input = bytes.data() + PMS_prof_skip_SIZE + num_vals * (PMS_val_SIZE + PMS_mid_SIZE);
  char* mids_input = bytes.data() + PMS_prof_skip_SIZE;
  for(uint i = 0; i < num_nzctxs; i++){
    interpretOneCtxValCntMids(val_cnt_input, mids_input, ctx_nzmids, ctx_nzval_cnts);
    val_cnt_input += PMS_ctx_pair_SIZE;  
  }

}

//---------------------------------------------------------------------------
// write profiles 
//---------------------------------------------------------------------------
void SparseDB::handleItemPi(pms_profile_info_t& pi)
{
  std::vector<char> info_bytes;

  auto b = convertToByte8(pi.id_tuple_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.metadata_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.spare_one);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.spare_two);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.num_vals);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte4(pi.num_nzctxs);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.offset);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  auto fhi = pmf->open(true);
  fhi.writeat(prof_info_sec_ptr + pi.prof_info_idx * PMS_prof_info_SIZE, PMS_prof_info_SIZE, info_bytes.data());
}


void SparseDB::writeProfInfos()
{

  parForPi.fill(std::move(prof_infos));
  parForPi.contribute(parForPi.wait());

}


std::vector<char> SparseDB::profInfoBytes(const std::vector<char>& partial_info_bytes, 
                                          const uint64_t id_tuple_ptr, const uint64_t metadata_ptr,
                                          const uint64_t spare_one_ptr, const uint64_t spare_two_ptr,
                                          const uint64_t prof_offset)
{
  std::vector<char> info_bytes; 

  auto b = convertToByte8(id_tuple_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(metadata_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(spare_one_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(spare_two_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  info_bytes.insert(info_bytes.end(), partial_info_bytes.begin(), partial_info_bytes.end());

  b = convertToByte8(prof_offset);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  assert(info_bytes.size() == PMS_prof_info_SIZE);
  return info_bytes;
}


void SparseDB::writeOneProfile(const std::pair<uint32_t, std::string>& tupleFn,
                               const uint64_t my_prof_offset, 
                               const std::pair<uint32_t,uint64_t>& prof_idx_tuple_ptr_pair,
                               std::vector<uint64_t>& ctx_nzval_cnts,
                               std::vector<std::set<uint16_t>>& ctx_nzmids,
                               util::File::Instance& fh)
{
  //get file name
  const std::string fn = tupleFn.second;

  //get all bytes from a profile
  std::ifstream input(fn.c_str(), std::ios::binary);
  std::vector<char> bytes(
      (std::istreambuf_iterator<char>(input)),
      (std::istreambuf_iterator<char>()));
  input.close();

  if(!keepTemps)
    stdshim::filesystem::remove(fn);

  //collect context local nonzero value counts and nz_mids from this profile
  if(tupleFn.first != IDTUPLE_SUMMARY_PROF_INFO_IDX)
    collectCctMajorData(prof_idx_tuple_ptr_pair.first, bytes, ctx_nzval_cnts, ctx_nzmids);
   
  //write profile info
  std::vector<char> partial_info (PMS_num_val_SIZE + PMS_num_nzctx_SIZE);
  std::copy(bytes.begin() + PMS_fake_id_tuple_SIZE, bytes.begin() + PMS_prof_skip_SIZE, partial_info.begin());
  // metadata_ptr, sparse_one/two are empty now, so 0,0,0
  std::vector<char> info = profInfoBytes(partial_info, prof_idx_tuple_ptr_pair.second, 0, 0, 0, my_prof_offset);
  uint64_t info_off = PMS_hdr_SIZE + prof_idx_tuple_ptr_pair.first * PMS_prof_info_SIZE;
  fh.writeat(info_off, PMS_prof_info_SIZE, info.data());

  //write profile data
  fh.writeat(my_prof_offset, bytes.size() - PMS_prof_skip_SIZE, bytes.data() + PMS_prof_skip_SIZE);
}

// write all the profiles at the correct place, and collect data needed for cct.db 
// input: calculated prof_offsets, calculated profile_sizes, file handle, number of available threads
void SparseDB::writeProfiles(const util::File& fh,const int threads,
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
    auto fhi = fh.open(true);

    std::set<uint16_t> empty;
    std::vector<std::set<uint16_t>> thread_ctx_nzmids (ctx_nzmids.size(), empty);
    threads_ctx_nzmids[omp_get_thread_num()] = &thread_ctx_nzmids;

    #pragma omp for reduction(vectorSum : ctx_nzval_cnts)
    for(uint i = 0; i<prof_offsets.size();i++){
      const std::pair<uint32_t, std::string>& tupleFn = sparseInputs[i];
      uint64_t my_prof_offset = prof_offsets[i];
      writeOneProfile(tupleFn, my_prof_offset, rank_idx_ptr_pairs[i], ctx_nzval_cnts, thread_ctx_nzmids, fhi);
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


void SparseDB::writeProfileMajor(const int threads, const int world_rank, 
                                 const int world_size, std::vector<uint64_t>& ctx_nzval_cnts,
                                 std::vector<std::set<uint16_t>>& ctx_nzmids)
{
  //
  // some private variables:
  // profile_sizes: vector of profile's own size
  // prof_offsets:  vector of final global offset
  // rank_idx_ptr_pairs: vector of (profile's idx at prof_info section : the ptr to its id_tuple)  
  //

  //set basic info
  util::File profile_major_f(dir / "profile.db", true);

  assignSparseInputs(world_rank);
  int my_num_prof = sparseInputs.size();
  uint32_t total_num_prof = getTotalNumProfiles(my_num_prof);

  //set hdr info, id_tuples_sec_size will be set in wordIdTuplesSection
  prof_info_sec_ptr = PMS_hdr_SIZE;
  prof_info_sec_size = total_num_prof * PMS_prof_info_SIZE;
  id_tuples_sec_ptr = prof_info_sec_ptr + (MULTIPLE_8(prof_info_sec_size));


  //write id_tuples, set id_tuples_sec_size 
  workIdTuplesSection(world_rank, world_size, threads, my_num_prof, profile_major_f);

  //write hdr
  writePMSHdr(total_num_prof,profile_major_f);
    
  //write rest profiles and corresponding prof_info
  workProfSizesOffsets(world_rank, total_num_prof, threads);
  writeProfiles(profile_major_f, threads, ctx_nzval_cnts, ctx_nzmids);

  //footer to show completeness
  mpi::barrier();
  if(world_rank != world_size - 1) return;

  auto pmfi = profile_major_f.open(true);
  auto footer_off = prof_offsets.back() + profile_sizes.back();
  uint64_t footer_val = PROFDBft;
  pmfi.writeat(footer_off, sizeof(footer_val), &footer_val);
  

}

//***************************************************************************
// cct.db 
//
///EXAMPLE
///[Context informations for 220 Contexts
///  [(context id: 1) (num_vals: 72) (num_nzmids: 1) (starting location: 4844)]
///  [(context id: 3) (num_vals: 0) (num_nzmids: 0) (starting location: 5728)]
///  ...
///]
///[context 1
///  [metrics easy grep version:
///  (NOTES: printed in file order, help checking if the file is correct)
///    (value: 2.64331, thread id: 0)
///    (value: 2.62104, thread id: 1)
///    ...
///  ]
///  [metric indices:
///    (metric id: 1, index: 0)
///    (metric id: END, index: 72)
///  ]
///]
///...same [sparse metrics] for all rest ctxs 
//***************************************************************************

//---------------------------------------------------------------------------
// header
//---------------------------------------------------------------------------
void SparseDB::writeCMSHdr(util::File::Instance& cct_major_fi)
{
  std::vector<char> hdr;
  hdr.insert(hdr.end(), HPCCCTSPARSE_FMT_Magic, HPCCCTSPARSE_FMT_Magic + HPCCCTSPARSE_FMT_MagicLen);
  hdr.emplace_back(HPCCCTSPARSE_FMT_VersionMajor);
  hdr.emplace_back(HPCCCTSPARSE_FMT_VersionMinor);
  
  auto b = convertToByte4(ctxcnt);
  hdr.insert(hdr.end(), b.begin(), b.end()); 

  b = convertToByte2(HPCCCTSPARSE_FMT_NumSec); 
  hdr.insert(hdr.end(), b.begin(), b.end());

  b = convertToByte8(ctxcnt * CMS_ctx_info_SIZE); // ctx_info_sec_size
  hdr.insert(hdr.end(), b.begin(), b.end());
  b = convertToByte8(CMS_hdr_SIZE); //ctx_info_sec_ptr
  hdr.insert(hdr.end(), b.begin(), b.end());
  
  assert(hdr.size() == CMS_real_hdr_SIZE);

  cct_major_fi.writeat(0, CMS_real_hdr_SIZE, hdr.data());
}


//---------------------------------------------------------------------------
// ctx info
//---------------------------------------------------------------------------
std::vector<char> SparseDB::ctxInfoBytes(const cms_ctx_info_t& ctx_info)
{
  std::vector<char> bytes;

  auto b = convertToByte4(ctx_info.ctx_id);
  bytes.insert(bytes.end(), b.begin(), b.end());
  b = convertToByte8(ctx_info.num_vals);
  bytes.insert(bytes.end(), b.begin(), b.end());
  b = convertToByte2(ctx_info.num_nzmids);
  bytes.insert(bytes.end(), b.begin(), b.end());
  b = convertToByte8(ctx_info.offset);
  bytes.insert(bytes.end(), b.begin(), b.end());
  
  return bytes;
}

void SparseDB::writeCtxInfoSec(const std::vector<std::set<uint16_t>>& ctx_nzmids,
                               const std::vector<uint64_t>& ctx_off,
                               util::File::Instance& ofh)
{
  assert(ctxcnt == ctx_nzmids.size());

  std::vector<char> info_bytes;

  for(uint i = 0; i < ctxcnt; i++){
    uint16_t num_nzmids = (uint16_t)(ctx_nzmids[i].size() - 1);
    uint64_t num_vals = (num_nzmids == 0) ? 0 \
      : ((ctx_off[i+1] - ctx_off[i]) - (num_nzmids + 1) * CMS_m_pair_SIZE) / CMS_val_prof_idx_pair_SIZE;
    cms_ctx_info_t ci = {CTXID(i), num_vals, num_nzmids, ctx_off[i]};
    
    auto bytes = std::move(ctxInfoBytes(ci));
    info_bytes.insert(info_bytes.end(), bytes.begin(), bytes.end());
  }

  assert(info_bytes.size() == CMS_ctx_info_SIZE * ctxcnt);
  ofh.writeat(CMS_hdr_SIZE, info_bytes.size(), info_bytes.data());
}

void SparseDB::writeCtxInfoSec1(util::File::Instance& ofh)
{
  assert(ctx_nzmids_cnts.size() == ctxcnt);
  std::vector<char> info_bytes;

  for(uint i = 0; i < ctxcnt; i++){
    uint16_t num_nzmids = (uint16_t)(ctx_nzmids_cnts[i] - 1);
    uint64_t num_vals = (num_nzmids == 0) ? 0 \
      : ((ctx_off1[i+1] - ctx_off1[i]) - (num_nzmids + 1) * CMS_m_pair_SIZE) / CMS_val_prof_idx_pair_SIZE;
    cms_ctx_info_t ci = {CTXID(i), num_vals, num_nzmids, ctx_off1[i]};
    
    auto bytes = std::move(ctxInfoBytes(ci));
    info_bytes.insert(info_bytes.end(), bytes.begin(), bytes.end());
  }

  assert(info_bytes.size() == CMS_ctx_info_SIZE * ctxcnt);
  ofh.writeat(CMS_hdr_SIZE, info_bytes.size(), info_bytes.data());
}

//---------------------------------------------------------------------------
// ctx offsets
//---------------------------------------------------------------------------
void SparseDB::unionMids(std::vector<std::set<uint16_t>>& ctx_nzmids, const int rank, 
                         const int num_proc, const int threads)
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
    for(auto mid: ctx)
      rank_all_mids.emplace_back(mid);
    rank_all_mids.emplace_back(stopper);
  }

  //STEP 2: gather all rank_all_mids to rank 0
  auto global_all_mids = mpi::gather(rank_all_mids, 0);
  size_t total_size = global_all_mids->size();

  //STEP 3: turn the long vector global_all_mids back to rank 0's ctx_nzmids
  if(rank == 0){
    int num_stopper = 0;
    int num_ctx     = ctx_nzmids.size();
    for(uint i = 0; i< total_size; i++) {
      auto g = global_all_mids->at(i);
      for(uint j = 0; j < g.size(); j++){
        uint16_t mid = g[j];
        if(mid == stopper)
          num_stopper++;
        else
          ctx_nzmids[num_stopper % num_ctx].insert(mid); 
      }
    }

    //  Add extra space for marking end location for the last mid
    #pragma omp parallel for num_threads(threads)
    for(uint i = 0; i<ctx_nzmids.size(); i++)
      ctx_nzmids[i].insert(LastMidEnd);
  }

}


std::vector<uint64_t> SparseDB::ctxOffsets(const std::vector<uint64_t>& ctx_val_cnts, 
                                           const std::vector<std::set<uint16_t>>& ctx_nzmids,
                                           const int threads, const int rank)
{

  assert(ctx_val_cnts.size() == ctx_nzmids.size());

  std::vector<uint64_t> ctx_off (ctxcnt + 1);
  std::vector<uint64_t> local_ctx_off (ctxcnt + 1, 0);

  //get local sizes
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < ctx_val_cnts.size(); i++){
    local_ctx_off[i] = ctx_val_cnts[i] * CMS_val_prof_idx_pair_SIZE;
    //empty context also has LastMidEnd in ctx_nzmids, so if the size is 1, offet should not count that pair for calculation
    if(rank == 0 && ctx_nzmids[i].size() > 1) local_ctx_off[i] += ctx_nzmids[i].size() * CMS_m_pair_SIZE; 
  }

  //get local offsets
  exscan<uint64_t>(local_ctx_off,threads); 

  //sum up local offsets to get global offsets
  return mpi::allreduce(local_ctx_off, mpi::Op::sum());


}


std::vector<uint64_t> SparseDB::ctxOffsets1()
{
  std::vector<uint64_t> ctx_off (ctxcnt + 1);
  std::vector<uint64_t> local_ctx_off (ctxcnt + 1, 0);

  //get local sizes
  for(uint i = 0; i < ctx_nzval_cnts1.size(); i++){
    local_ctx_off[i] = ctx_nzval_cnts1[i] * CMS_val_prof_idx_pair_SIZE;
    //empty context also has LastMidEnd in ctx_nzmids_cnts, so if the size is 1, offet should not count that pair for calculation
    if(mpi::World::rank() == 0 && ctx_nzmids_cnts[i] > 1) local_ctx_off[i] += ctx_nzmids_cnts[i] * CMS_m_pair_SIZE; 
  }

  //get local offsets
  exscan<uint64_t>(local_ctx_off,1); 

  //sum up local offsets to get global offsets
  return mpi::allreduce(local_ctx_off, mpi::Op::sum());


}

//each rank is responsible for a group of ctxs
std::vector<uint32_t> SparseDB::myCtxs(const std::vector<uint64_t>& ctx_off,
                                       const int num_ranks, const int rank)
{
  assert(ctx_off.size() > 0);

  std::vector<uint32_t> my_ctxs;

  //split work among ranks by volume of ctxs
  uint64_t total_size = ctx_off.back();
  uint64_t max_size_per_rank = round(total_size/num_ranks);
  uint64_t my_start = rank * max_size_per_rank;
  uint64_t my_end = (rank == num_ranks - 1) ? total_size : (rank + 1) * max_size_per_rank;

  for(uint i = 1; i<ctx_off.size(); i++)
    if(ctx_off[i] > my_start && ctx_off[i] <= my_end) my_ctxs.emplace_back(CTXID((i-1)));

  return my_ctxs;
}

void SparseDB::buildCtxGroupList()
{
  uint64_t cur_size = 0;
  uint64_t total_size = ctx_off1.back();
  uint64_t size_limit = std::min<uint64_t>((uint64_t)1024*1024*1024*3,\
                        round(total_size/(5 * mpi::World::size())));

  ctx_group_list.emplace_back(0);
  for(uint i = 0; i < ctx_off1.size() - 1; i++){
    uint64_t cur_ctx_size = ctx_off1[CTX_VEC_IDX(i) + 1] - ctx_off1[CTX_VEC_IDX(i)];

    if((cur_size + cur_ctx_size) > size_limit){
      ctx_group_list.emplace_back(i);
      cur_size = 0;
    }

    cur_size += cur_ctx_size;
  }
  ctx_group_list.emplace_back(ctx_off1.size() - 1);

}

void SparseDB::updateCtxOffsets(const int threads, std::vector<uint64_t>& ctx_off)
{
  assert(ctx_off.size() == ctxcnt + 1);

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < ctxcnt + 1; i++)
    ctx_off[i] += (MULTIPLE_8(ctxcnt * CMS_ctx_info_SIZE)) + CMS_hdr_SIZE;
  
}


//---------------------------------------------------------------------------
// get a list of profile info
//---------------------------------------------------------------------------
pms_profile_info_t SparseDB::profInfo(const char *input)
{
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


std::vector<pms_profile_info_t> SparseDB::profInfoList(const int threads, const util::File& fh)
{
  std::vector<pms_profile_info_t> prof_info;
  auto fhi = fh.open(false);

  //read the number of profiles
  uint32_t num_prof = readAsByte4(fhi, (HPCPROFILESPARSE_FMT_MagicLen + HPCPROFILESPARSE_FMT_VersionLen));
  num_prof--; //minus the summary profile

  //read the whole Profile Information section
  int count = num_prof * PMS_prof_info_SIZE; 
  char input[count];
  fhi.readat(PMS_hdr_SIZE + PMS_prof_info_SIZE, count, input); //skip one prof_info (summary)

  //interpret the section and store in a vector of pms_profile_info_t
  prof_info.resize(num_prof);
  #pragma omp parallel for num_threads(threads)
  for(int i = 0; i<count; i += PMS_prof_info_SIZE){
    auto pi = profInfo(input + i);
    pi.prof_info_idx = i/PMS_prof_info_SIZE + 1; // the first one is summary profile
    prof_info[i/PMS_prof_info_SIZE] = std::move(pi);
  }

  return prof_info;
}

//---------------------------------------------------------------------------
// get all profiles' CtxIdIdxPairs
//---------------------------------------------------------------------------
SparseDB::PMS_CtxIdIdxPair SparseDB::ctxIdIdxPair(const char *input)
{
  PMS_CtxIdIdxPair ctx_pair;
  ctx_pair.ctx_id  = interpretByte4(input);
  ctx_pair.ctx_idx = interpretByte8(input + PMS_ctx_id_SIZE);
  return ctx_pair;
}

std::vector<SparseDB::PMS_CtxIdIdxPair> SparseDB::ctxIdIdxPairs(util::File::Instance& fh, const pms_profile_info_t pi)
{
  std::vector<PMS_CtxIdIdxPair> ctx_id_idx_pairs(pi.num_nzctxs + 1);
  if(pi.num_nzctxs == 0) return ctx_id_idx_pairs;

  //read the whole ctx_id_idx_pairs chunk
  int count = (pi.num_nzctxs + 1) * PMS_ctx_pair_SIZE; 
  char input[count];
  uint64_t ctx_pairs_offset = pi.offset + pi.num_vals * (PMS_val_SIZE + PMS_mid_SIZE);
  fh.readat(ctx_pairs_offset, count, input);

  //interpret the chunk and store accordingly
  for(int i = 0; i<count; i += PMS_ctx_pair_SIZE)
    ctx_id_idx_pairs[i/PMS_ctx_pair_SIZE] = std::move(ctxIdIdxPair(input + i));

  return ctx_id_idx_pairs;
}

std::vector<std::vector<SparseDB::PMS_CtxIdIdxPair>> 
SparseDB::allProfileCtxIdIdxPairs(const util::File& fh, const int threads,
                                  const std::vector<pms_profile_info_t>& prof_info)
{
  std::vector<std::vector<PMS_CtxIdIdxPair>> all_prof_ctx_pairs(prof_info.size());

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < prof_info.size(); i++){
    auto fhi = fh.open(false);
    all_prof_ctx_pairs[i] = std::move(ctxIdIdxPairs(fhi, prof_info[i]));
  }

  return all_prof_ctx_pairs;
}


void SparseDB::handleItemCiip(profCtxIdIdxPairs& ciip)
{
  auto pmfi = pmf->open(false);
  *(ciip.prof_ctx_pairs) = std::move(ctxIdIdxPairs(pmfi, *(ciip.pi)));
}

std::vector<std::vector<SparseDB::PMS_CtxIdIdxPair>> 
SparseDB::allProfileCtxIdIdxPairs1( std::vector<pms_profile_info_t>& prof_info)
{
  std::vector<std::vector<PMS_CtxIdIdxPair>> all_prof_ctx_pairs(prof_info.size());
  std::vector<profCtxIdIdxPairs> ciips(prof_info.size());

  for(uint i = 0; i < prof_info.size(); i++){
    profCtxIdIdxPairs ciip;
    ciip.prof_ctx_pairs = &all_prof_ctx_pairs[i];
    ciip.pi = &prof_info[i];
    ciips[i] = std::move(ciip);
  }

  parForCiip.fill(std::move(ciips));
  parForCiip.contribute(parForCiip.wait());

  return all_prof_ctx_pairs;
}
//---------------------------------------------------------------------------
// read/extract profiles data - my_ctx_id_idx_pairs and my val&mid bytes
//---------------------------------------------------------------------------
int SparseDB::findOneCtxIdIdxPair(const uint32_t target_ctx_id,
                                  const std::vector<PMS_CtxIdIdxPair>& profile_ctx_pairs,
                                  const uint length,const int round,const int found_ctx_idx, 
                                  std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs)
{
  int idx;

  if(round != 0){
    idx = 1 + found_ctx_idx;

    //the profile_ctx_pairs has been searched through
    if(idx == (int)length) return SPARSE_END;
    
    //the ctx_id at idx
    uint32_t prof_ctx_id = profile_ctx_pairs[idx].ctx_id;
    if(prof_ctx_id == target_ctx_id){
      my_ctx_pairs.emplace_back(profile_ctx_pairs[idx].ctx_id, profile_ctx_pairs[idx].ctx_idx);
      return idx;
    }else if(prof_ctx_id > target_ctx_id){
      return SPARSE_NOT_FOUND; //back to original since this might be next target
    }else{ //prof_ctx_id < target_ctx_id, should not happen
      util::log::fatal() << __FUNCTION__ << ": ctx id " << prof_ctx_id << " in a profile does not exist in the full ctx list while searching for " 
        << target_ctx_id << " with index " << idx << "!";
      return SPARSE_NOT_FOUND; //this should not be called if exit, but function requires return value
    }

  }else{
    PMS_CtxIdIdxPair target_ciip;
    target_ciip.ctx_id = target_ctx_id;
    idx = struct_member_binary_search(profile_ctx_pairs, target_ciip, &PMS_CtxIdIdxPair::ctx_id, length);
    if(idx >= 0)
      my_ctx_pairs.emplace_back(profile_ctx_pairs[idx].ctx_id, profile_ctx_pairs[idx].ctx_idx);
    else if(idx == -1) //target < the first value, so start future searches at 0
      idx = -1;  
    else if(idx < -1) //target > some value at (-2-idx), so start future searches at (-2-idx) + 1
      idx = -2 - idx;
    return idx;
  }
}

std::vector<std::pair<uint32_t, uint64_t>> 
SparseDB::myCtxPairs(const std::vector<uint32_t>& ctx_ids,const std::vector<PMS_CtxIdIdxPair>& profile_ctx_pairs)
{
  std::vector<std::pair<uint32_t, uint64_t>> my_ctx_pairs;
  if(profile_ctx_pairs.size() == 1) return my_ctx_pairs;

  uint n = profile_ctx_pairs.size() - 1; //since the last is LastNodeEnd
  int idx = -1; //index of current pair in profile_ctx_pairs
  uint32_t target;

  for(uint i = 0; i<ctx_ids.size(); i++){
    target = ctx_ids[i];
    int ret = findOneCtxIdIdxPair(target, profile_ctx_pairs, n, i, idx, my_ctx_pairs);
    if(ret == SPARSE_END) break;
    if(ret == SPARSE_NOT_FOUND) continue;

    idx = ret;
  }

  //add one extra context pair for later use
  my_ctx_pairs.emplace_back(LastNodeEnd, profile_ctx_pairs[idx + 1].ctx_idx);
  
  assert(my_ctx_pairs.size() <= ctx_ids.size() + 1);
  return my_ctx_pairs;
}


std::vector<char> SparseDB::valMidsBytes(std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs,
                                         const uint64_t& off, util::File::Instance& fh)
{
  std::vector<char> bytes;
  if(my_ctx_pairs.size() <= 1) return bytes;

  uint64_t first_ctx_idx = my_ctx_pairs.front().second;
  uint64_t last_ctx_idx  = my_ctx_pairs.back().second;
  int val_mid_count = (last_ctx_idx - first_ctx_idx) * (PMS_val_SIZE + PMS_mid_SIZE);
  if(val_mid_count == 0) return bytes;

  bytes.resize(val_mid_count);
  uint64_t val_mid_start_pos = off + first_ctx_idx * (PMS_val_SIZE + PMS_mid_SIZE);
  fh.readat(val_mid_start_pos, val_mid_count, bytes.data());
  return bytes;
}

std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>>
SparseDB::profilesData(const std::vector<uint32_t>& ctx_ids, const std::vector<pms_profile_info_t>& prof_info_list,
                       int threads, const std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                       const util::File& fh)
{

  std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>> profiles_data (prof_info_list.size());

  //read all profiles for this ctx_ids group
  #pragma omp parallel for num_threads(threads) 
  for(uint i = 0; i < prof_info_list.size(); i++){
    auto fhi = fh.open(false);

    pms_profile_info_t pi = prof_info_list[i];
    std::vector<PMS_CtxIdIdxPair> prof_ctx_pairs = all_prof_ctx_pairs[i];

    auto my_ctx_pairs = std::move(myCtxPairs(ctx_ids, prof_ctx_pairs));
    auto vmbytes = std::move(valMidsBytes(my_ctx_pairs, pi.offset, fhi));
    
    profiles_data[i] = {std::move(my_ctx_pairs), std::move(vmbytes)};
  }
  
  return profiles_data;
}

std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>>
SparseDB::profilesData1(std::vector<uint32_t>& ctx_ids, std::vector<pms_profile_info_t>& prof_info_list,
                       std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs)
{

  std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>> profiles_data (prof_info_list.size());
  std::vector<profData> pds (prof_info_list.size());

  //read all profiles for this ctx_ids group
  for(uint i = 0; i < prof_info_list.size(); i++){
   profData pd;
   pd.i = i;
   pd.pi_list = &prof_info_list;
   pd.all_prof_ctx_pairs = &all_prof_ctx_pairs;
   pd.ctx_ids = &ctx_ids;
   pd.profiles_data = &profiles_data;
   pds[i] = std::move(pd);
  }

  parForPd.fill(std::move(pds));
  parForPd.reset();  // Also waits for work to complete
  
  return profiles_data;
}

void SparseDB::handleItemPd(profData& pd)
{
  uint i = pd.i;
  auto pmfi = pmf->open(false);

  auto poff = pd.pi_list->at(i).offset;

  auto my_ctx_pairs = std::move(myCtxPairs(*(pd.ctx_ids), pd.all_prof_ctx_pairs->at(i)));
  auto vmbytes = std::move(valMidsBytes(my_ctx_pairs, poff, pmfi));
  
  pd.profiles_data->at(i) = {std::move(my_ctx_pairs), std::move(vmbytes)};

}

//---------------------------------------------------------------------------
// interpret the data bytes and convert to CtxMetricBlock
//---------------------------------------------------------------------------
SparseDB::MetricValBlock SparseDB::metValBloc(const hpcrun_metricVal_t val, 
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

void SparseDB::updateCtxMetBloc(const hpcrun_metricVal_t val, const uint16_t mid,
                                const uint32_t prof_idx, CtxMetricBlock& cmb)
{
  //find if this mid exists
  auto& metric_blocks = cmb.metrics; 
  auto it = metric_blocks.find(mid);

  if(it != metric_blocks.end()) //found mid
    it->second.values_prof_idxs.emplace_back(val, prof_idx);
  else
    metric_blocks.emplace(mid, std::move(metValBloc(val, mid, prof_idx)));
  
}

void SparseDB::interpretValMidsBytes(char *vminput, const uint32_t prof_idx,
                                     const std::pair<uint32_t,uint64_t>& ctx_pair,
                                     const uint64_t next_ctx_idx, const uint64_t first_ctx_idx,
                                     CtxMetricBlock& cmb)
{
  uint32_t ctx_id = ctx_pair.first;
  uint64_t ctx_idx = ctx_pair.second;
  uint64_t num_val_this_ctx = next_ctx_idx - ctx_idx;
  assert(cmb.ctx_id == ctx_id);

  char* ctx_met_input = vminput + (PMS_val_SIZE + PMS_mid_SIZE) * (ctx_idx - first_ctx_idx);
  for(uint i = 0; i < num_val_this_ctx; i++){
    hpcrun_metricVal_t val;
    val.bits = interpretByte8(ctx_met_input);
    uint16_t mid = interpretByte2(ctx_met_input + PMS_val_SIZE);

    updateCtxMetBloc(val, mid, prof_idx, cmb);
    ctx_met_input += (PMS_val_SIZE + PMS_mid_SIZE);
  }

}


//---------------------------------------------------------------------------
// convert CtxMetricBlocks to correct bytes for writing
//---------------------------------------------------------------------------
std::vector<char> SparseDB::mvbBytes(const MetricValBlock& mvb)
{
  uint64_t num_vals = mvb.values_prof_idxs.size();
  std::vector<char> bytes;
  std::vector<char> b;

  for(uint i = 0; i < num_vals; i++){
    auto& pair = mvb.values_prof_idxs[i];
    b = convertToByte8(pair.first.bits);
    bytes.insert(bytes.end(), b.begin(), b.end());
    b = convertToByte4(pair.second);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }

  assert(bytes.size() == num_vals * CMS_val_prof_idx_pair_SIZE);
  return bytes;
}

std::vector<char> SparseDB::mvbsBytes(std::map<uint16_t, MetricValBlock>& metrics)
{
  uint64_t num_vals = 0;
  std::vector<char> bytes;

  for(auto i = metrics.begin(); i != metrics.end(); i++){
    MetricValBlock mvb = i->second;
    i->second.num_values = mvb.values_prof_idxs.size(); 
    num_vals += mvb.values_prof_idxs.size();

    auto mvb_bytes = mvbBytes(mvb);
    bytes.insert(bytes.end(), mvb_bytes.begin(),mvb_bytes.end());
  }

  assert(num_vals == (uint)bytes.size()/CMS_val_prof_idx_pair_SIZE);

  return bytes;
}

std::vector<char> SparseDB::metIdIdxPairsBytes(const std::map<uint16_t, MetricValBlock>& metrics)
{
  std::vector<char> bytes;
  std::vector<char> b;

  uint64_t m_idx = 0;
  for(auto i = metrics.begin(); i != metrics.end(); i++){
    uint16_t mid = i->first;
    b = convertToByte2(mid);
    bytes.insert(bytes.end(), b.begin(), b.end());
    b = convertToByte8(m_idx);
    bytes.insert(bytes.end(), b.begin(), b.end());
    m_idx += i->second.num_values;
  }

   uint16_t mid = LastMidEnd;
   b = convertToByte2(mid);
   bytes.insert(bytes.end(), b.begin(), b.end());
   b = convertToByte8(m_idx);
   bytes.insert(bytes.end(), b.begin(), b.end());

  assert(bytes.size() == ((metrics.size() + 1) * CMS_m_pair_SIZE));
  return bytes;
}

std::vector<char> SparseDB::cmbBytes(const CtxMetricBlock& cmb, const std::vector<uint64_t>& ctx_off, 
                                     const uint32_t& ctx_id)
{
  assert(cmb.metrics.size() > 0);
  assert(ctx_off.size() > 0);

  //convert MetricValBlocks to val & prof pairs bytes
  std::map<uint16_t, MetricValBlock> metrics = cmb.metrics;
  auto bytes = std::move(mvbsBytes(metrics));

  //convert MetricValBlocks to metric Id & Idx pairs bytes
  auto miip_bytes = std::move(metIdIdxPairsBytes(metrics));
  bytes.insert(bytes.end(), miip_bytes.begin(), miip_bytes.end());

  //check if the previous calculations for offsets and newly collected data match
  if(ctx_off[CTX_VEC_IDX(ctx_id)] + bytes.size() !=  ctx_off[CTX_VEC_IDX(ctx_id)+1]){
    util::log::fatal() << __FUNCTION__ << ": (ctx id: " << ctx_id 
       << ") (num_vals: " << bytes.size()/CMS_val_prof_idx_pair_SIZE
       << ") (num_nzmids: " << metrics.size()
       << ") (ctx_off: " << ctx_off[CTX_VEC_IDX(ctx_id)]
       << ") (next_ctx_off: " << ctx_off[CTX_VEC_IDX(ctx_id) + 1] << ")";
  }

  return bytes;

}

//---------------------------------------------------------------------------
// read and write for all contexts in this rank's list
//---------------------------------------------------------------------------
void SparseDB::writeOneCtx(const uint32_t& ctx_id, const std::vector<uint64_t>& ctx_off,
                           const CtxMetricBlock& cmb, util::File::Instance& ofh)
{
  auto metrics_bytes = std::move(cmbBytes(cmb, ctx_off, ctx_id));

  uint64_t metrics_off = ctx_off[CTX_VEC_IDX(ctx_id)];
  ofh.writeat(metrics_off, metrics_bytes.size(), metrics_bytes.data());
}

void SparseDB::handleItemCtxs(ctxRange& cr)
{
  auto ofhi = cmf->open(true);

  uint my_start = cr.start;
  uint my_end = cr.end;
  auto& profiles_data = *cr.pd;
  auto& ctx_ids = *cr.ctx_ids;
  auto& prof_info = *cr.pis;

  if(my_start < my_end) {     
    //each thread sets up a heap to store <ctx_id, profile_idx, profile_cursor> for each profile
    std::vector<nextCtx> heap;
    heap.reserve(profiles_data.size());
    for(uint i = 0; i < profiles_data.size(); i++){
      uint32_t ctx_id = ctx_ids[my_start];
      auto& ctx_id_idx_pairs = profiles_data[i].first;
      if(ctx_id_idx_pairs.empty()) continue;
      //find the smallest ctx_id this profile has and I am responsible for
      size_t cursor =lower_bound(ctx_id_idx_pairs.begin(),ctx_id_idx_pairs.end(), 
                std::make_pair(ctx_id,std::numeric_limits<uint64_t>::min()), //Value to compare
                [](const std::pair<uint32_t, uint64_t>& lhs, const std::pair<uint32_t, uint64_t>& rhs)      
                { return lhs.first < rhs.first ;}) - ctx_id_idx_pairs.begin();
      heap.push_back({ctx_id_idx_pairs[cursor].first, i, cursor});
    }
    heap.shrink_to_fit();
    std::make_heap(heap.begin(), heap.end());

    while(1){
      //get the min ctx_id in the heap
      uint32_t ctx_id = heap.front().ctx_id;
      if(ctx_id > ctx_ids[my_end-1]) break;

      //a new CtxMetricBlock
      CtxMetricBlock cmb;
      cmb.ctx_id = ctx_id;

      while(heap.front().ctx_id == ctx_id){
        uint32_t prof_idx = heap.front().prof_idx;

        //find data corresponding to this profile
        auto& vmbytes = profiles_data[prof_idx].second;
        auto& ctx_id_idx_pairs = profiles_data[prof_idx].first;
        
        //interpret the bytes of this profile related to ctx_id
        uint64_t next_ctx_idx = ctx_id_idx_pairs[heap.front().cursor+1].second;
        uint64_t first_ctx_idx = ctx_id_idx_pairs[0].second;
        interpretValMidsBytes(vmbytes.data(), prof_info[prof_idx].prof_info_idx, \
          ctx_id_idx_pairs[heap.front().cursor], next_ctx_idx, first_ctx_idx, cmb);
        
        //update the heap
        std::pop_heap(heap.begin(), heap.end()); //the front one will be at the back()
        heap.back().cursor++;
        heap.back().ctx_id = ctx_id_idx_pairs[heap.back().cursor].first;
        std::push_heap(heap.begin(), heap.end());//the back() will go to the right place

      }

      writeOneCtx(ctx_id, ctx_off1, cmb, ofhi);

    }//END of while

  } //END of if my_start < my_end
}

void SparseDB::rwOneCtxGroup(const std::vector<uint32_t>& ctx_ids, 
                             const std::vector<pms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                             const std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                             const util::File& fh,
                             const util::File& ofh)
{
  if(ctx_ids.size() == 0) return;

  //----------------------------------
  //read corresponding ctx_id_idx pairs and relevant val&mids bytes
  //----------------------------------
  auto profiles_data = std::move(profilesData(ctx_ids, prof_info, threads, all_prof_ctx_pairs, fh));

  //----------------------------------
  //assign ctx_ids to diffrent threads based on size
  //----------------------------------
  uint first_ctx_off = ctx_off[CTX_VEC_IDX(ctx_ids.front())];
  uint total_ctx_ids_size = ctx_off[CTX_VEC_IDX(ctx_ids.back()) + 1] - first_ctx_off;
  uint thread_ctx_ids_size = round(total_ctx_ids_size/threads);

  //record the start and end position of each thread's ctxs
  std::vector<uint64_t> t_starts (threads, 0);
  std::vector<uint64_t> t_ends (threads, 0);
  int cur_thread = 0;

  //make sure first thread at least gets one ctx
  size_t cur_size = ctx_off[CTX_VEC_IDX(ctx_ids.front()) + 1] - first_ctx_off; //size of first ctx
  t_starts[cur_thread] = 0; //the first ctx goes to t_starts[0]
  if(threads > 1){
    for(uint i = 2; i <= ctx_ids.size(); i++){
      auto cid = (i == ctx_ids.size()) ? ctx_ids[i-1] + 1 : ctx_ids[i];
      auto cid_size = (ctx_off[CTX_VEC_IDX(cid)] - ctx_off[CTX_VEC_IDX(ctx_ids[i-1])]);

      if(cur_size > thread_ctx_ids_size){
        t_ends[cur_thread] = i-1;
        cur_thread++;
        t_starts[cur_thread] = i-1;
        cur_size = cid_size;
      }else{
        cur_size += cid_size;
      }
      if(cur_thread == threads - 1) break;
    }  
  }
  t_ends[cur_thread] = ctx_ids.size();


  //----------------------------------
  //each thread uses a heap to go over ctx_ids needed to be processed
  //----------------------------------
  #pragma omp parallel num_threads(threads)
  {
    auto ofhi = ofh.open(true);

    //each thread is responsible for a group of ctx_ids, idx from [my_start, my_end)
    int thread_num = omp_get_thread_num();
    uint my_start = t_starts[thread_num];
    uint my_end = t_ends[thread_num];

    if(my_start < my_end) {     
      //each thread sets up a heap to store <ctx_id, profile_idx, profile_cursor> for each profile
      std::vector<nextCtx> heap;
      heap.reserve(profiles_data.size());
      for(uint i = 0; i < profiles_data.size(); i++){
        uint32_t ctx_id = ctx_ids[my_start];
        auto& ctx_id_idx_pairs = profiles_data[i].first;
        if(ctx_id_idx_pairs.empty()) continue;
        //find the smallest ctx_id this profile has and I am responsible for
        size_t cursor =lower_bound(ctx_id_idx_pairs.begin(),ctx_id_idx_pairs.end(), 
                  std::make_pair(ctx_id,std::numeric_limits<uint64_t>::min()), //Value to compare
                  [](const std::pair<uint32_t, uint64_t>& lhs, const std::pair<uint32_t, uint64_t>& rhs)      
                  { return lhs.first < rhs.first ;}) - ctx_id_idx_pairs.begin();
        heap.push_back({ctx_id_idx_pairs[cursor].first, i, cursor});
      }
      heap.shrink_to_fit();
      std::make_heap(heap.begin(), heap.end());

      while(1){
        //get the min ctx_id in the heap
        uint32_t ctx_id = heap.front().ctx_id;
        if(ctx_id > ctx_ids[my_end-1]) break;

        //a new CtxMetricBlock
        CtxMetricBlock cmb;
        cmb.ctx_id = ctx_id;

        while(heap.front().ctx_id == ctx_id){
          uint32_t prof_idx = heap.front().prof_idx;

          //find data corresponding to this profile
          auto& vmbytes = profiles_data[prof_idx].second;
          auto& ctx_id_idx_pairs = profiles_data[prof_idx].first;
          
          //interpret the bytes of this profile related to ctx_id
          uint64_t next_ctx_idx = ctx_id_idx_pairs[heap.front().cursor+1].second;
          uint64_t first_ctx_idx = ctx_id_idx_pairs[0].second;
          interpretValMidsBytes(vmbytes.data(), prof_info[prof_idx].prof_info_idx, \
            ctx_id_idx_pairs[heap.front().cursor], next_ctx_idx, first_ctx_idx, cmb);
          
          //update the heap
          std::pop_heap(heap.begin(), heap.end()); //the front one will be at the back()
          heap.back().cursor++;
          heap.back().ctx_id = ctx_id_idx_pairs[heap.back().cursor].first;
          std::push_heap(heap.begin(), heap.end());//the back() will go to the right place

        }

        writeOneCtx(ctx_id, ctx_off, cmb, ofhi);

      }//END of while

    } //END of if my_start < my_end

  }//END of parallel region
  
   
}

void SparseDB::rwOneCtxGroup1(std::vector<uint32_t>& ctx_ids, 
                              std::vector<pms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                              std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs)
{
  if(ctx_ids.size() == 0) return;

  //----------------------------------
  //read corresponding ctx_id_idx pairs and relevant val&mids bytes
  //----------------------------------
  auto profiles_data = std::move(profilesData1(ctx_ids, prof_info, all_prof_ctx_pairs));

  //----------------------------------
  //assign ctx_ids to diffrent threads based on size
  //----------------------------------
  uint first_ctx_off = ctx_off[CTX_VEC_IDX(ctx_ids.front())];
  uint total_ctx_ids_size = ctx_off[CTX_VEC_IDX(ctx_ids.back()) + 1] - first_ctx_off;
  uint thread_ctx_ids_size = round(total_ctx_ids_size/threads);

  //record the start and end position of each thread's ctxs
  std::vector<uint64_t> t_starts (threads, 0);
  std::vector<uint64_t> t_ends (threads, 0);
  int cur_thread = 0;

  //make sure first thread at least gets one ctx
  size_t cur_size = ctx_off[CTX_VEC_IDX(ctx_ids.front()) + 1] - first_ctx_off; //size of first ctx
  t_starts[cur_thread] = 0; //the first ctx goes to t_starts[0]
  if(threads > 1){
    for(uint i = 2; i <= ctx_ids.size(); i++){
      auto cid = (i == ctx_ids.size()) ? ctx_ids[i-1] + 1 : ctx_ids[i];
      auto cid_size = (ctx_off[CTX_VEC_IDX(cid)] - ctx_off[CTX_VEC_IDX(ctx_ids[i-1])]);

      if(cur_size > thread_ctx_ids_size){
        t_ends[cur_thread] = i-1;
        cur_thread++;
        t_starts[cur_thread] = i-1;
        cur_size = cid_size;
      }else{
        cur_size += cid_size;
      }
      if(cur_thread == threads - 1) break;
    }  
  }
  t_ends[cur_thread] = ctx_ids.size();


  std::vector<ctxRange> crs(threads);
  auto pdptr = &profiles_data;
  auto cidsptr = &ctx_ids;
  auto pisptr = &prof_info;
  for(int i = 0; i < threads; i++){
    ctxRange cr;
    cr.start = t_starts[i];
    cr.end = t_ends[i];
    cr.pd = pdptr;
    cr.ctx_ids = cidsptr;
    cr.pis = pisptr;
    crs[i] = std::move(cr);
  }
  parForCtxs.fill(std::move(crs));
  parForCtxs.contribute(parForCtxs.wait());
  
}

void SparseDB::rwAllCtxGroup(const std::vector<uint32_t>& my_ctxs, 
                             const std::vector<pms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                             const std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                             const util::File& fh,
                             const util::File& ofh)
{
  if(my_ctxs.size() == 0) return;
  //For each ctx group (< memory limit) this rank is in charge of, read and write
  std::vector<uint32_t> ctx_ids;
  size_t cur_size = 0;
  int cur_cnt = 0;
  uint64_t size_limit = std::min<uint64_t>((uint64_t)1024*1024*1024*3, \
    ctx_off[CTX_VEC_IDX(my_ctxs.back()) + 1] - ctx_off[CTX_VEC_IDX(my_ctxs.front())]);

  for(uint i =0; i<my_ctxs.size(); i++){
    uint32_t ctx_id = my_ctxs[i];
    size_t cur_ctx_size = ctx_off[CTX_VEC_IDX(ctx_id) + 1] - ctx_off[CTX_VEC_IDX(ctx_id)];

    if((cur_size + cur_ctx_size) <= size_limit){
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
    if((i == my_ctxs.size() - 1) && (ctx_ids.size() != 0)) 
      rwOneCtxGroup(ctx_ids, prof_info, ctx_off, threads, all_prof_ctx_pairs, fh, ofh);
    
  }

}

uint32_t SparseDB::ctxGrpIdFetch()
{ 
  uint32_t r;
  if(mpi::World::size() == 1){ // just talk to its own data
    r = ctxGrpId;
    ctxGrpId++;
  }else{ // More than one rank, use RMA
    r = accCtxGrp.fetch_add(1); //just increment the index by 1
  }
  return r;
}


void SparseDB::rwAllCtxGroup1(std::vector<pms_profile_info_t>& prof_info, 
                             const std::vector<uint64_t>& ctx_off, 
                             const int threads, 
                              std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs)
{
  uint32_t idx = mpi::World::rank();
  uint32_t num_groups = ctx_group_list.size();
  std::vector<uint32_t> ctx_ids;

  while(idx < num_groups - 1){ // the last one is to record the ending idx
    ctx_ids.clear();
    auto& start_id = ctx_group_list[idx];
    auto& end_id = ctx_group_list[idx + 1];
    for(uint i = start_id; i < end_id; i++) ctx_ids.emplace_back(i);
    rwOneCtxGroup1(ctx_ids, prof_info, ctx_off, threads, all_prof_ctx_pairs);
    idx = ctxGrpIdFetch();
  }
  parForPd.fill({});  // Make sure the workshare is non-empty
  parForPd.complete();

/*
  if(my_ctxs.size() == 0) return;
  //For each ctx group (< memory limit) this rank is in charge of, read and write
  std::vector<uint32_t> ctx_ids;
  size_t cur_size = 0;
  int cur_cnt = 0;
  uint64_t size_limit = std::min<uint64_t>((uint64_t)1024*1024*1024*3, \
    ctx_off[CTX_VEC_IDX(my_ctxs.back()) + 1] - ctx_off[CTX_VEC_IDX(my_ctxs.front())]);

  for(uint i =0; i<my_ctxs.size(); i++){
    uint32_t ctx_id = my_ctxs[i];
    size_t cur_ctx_size = ctx_off[CTX_VEC_IDX(ctx_id) + 1] - ctx_off[CTX_VEC_IDX(ctx_id)];

    if((cur_size + cur_ctx_size) <= size_limit){
      ctx_ids.emplace_back(ctx_id);
      cur_size += cur_ctx_size;
      cur_cnt++;
    }else{
      rwOneCtxGroup1(ctx_ids, prof_info, ctx_off, threads, all_prof_ctx_pairs);

      ctx_ids.clear();
      ctx_ids.emplace_back(ctx_id);
      cur_size = cur_ctx_size;
      cur_cnt = 1;
    }   

    // final ctx group
    if((i == my_ctxs.size() - 1) && (ctx_ids.size() != 0)) 
      rwOneCtxGroup1(ctx_ids, prof_info, ctx_off, threads, all_prof_ctx_pairs);
    
  }
  */
}


void SparseDB::writeCCTMajor(const std::vector<uint64_t>& ctx_nzval_cnts, 
                             std::vector<std::set<uint16_t>>& ctx_nzmids,
                             const int world_rank, 
                             const int world_size, 
                             const int threads)
{
  //Prepare a union ctx_nzmids, only rank 0's ctx_nzmids is global
  unionMids(ctx_nzmids,world_rank,world_size, threads);
  
  if(!world_rank){
     for(uint i = 0; i<ctxcnt; i++){
      if(ctx_nzmids_cnts[i] != ctx_nzmids[i].size())
        printf("nzmids not equal on %d ctx, size %d != %ld\n", i, ctx_nzmids_cnts[i], ctx_nzmids[i].size());
     }
  }
 
  //Get context global final offsets for cct.db
  auto ctx_offs = std::move(ctxOffsets(ctx_nzval_cnts, ctx_nzmids, threads, world_rank));
  auto my_ctxs = std::move(myCtxs(ctx_offs, world_size, world_rank));
  updateCtxOffsets(threads, ctx_offs);

  //Prepare files to read and write, get the list of profiles
  util::File profile_major_f(dir / "profile.db", false);
  util::File cct_major_f(dir / "cct.db", true);
  
  if(world_rank == 0){
    auto cct_major_fi = cct_major_f.open(true);
    // Write hdr
    writeCMSHdr(cct_major_fi);
    // Write ctx info section
    writeCtxInfoSec(ctx_nzmids, ctx_offs, cct_major_fi);
  }

  //get the list of prof_info
  auto prof_info_list = std::move(profInfoList(threads, profile_major_f));

  //get the ctx_id & ctx_idx pairs for all profiles
  auto all_prof_ctx_pairs = std::move(allProfileCtxIdIdxPairs(profile_major_f, threads, prof_info_list));
  
  //read and write all the context groups I(rank) am responsible for
  rwAllCtxGroup(my_ctxs, prof_info_list, ctx_offs, threads, all_prof_ctx_pairs, profile_major_f, cct_major_f);

  //footer
  mpi::barrier();
  if(world_rank != world_size - 1) return;

  auto cmfi = cct_major_f.open(true);
  auto footer_off = ctx_offs.back();
  uint64_t footer_val = CCTDBftr;
  cmfi.writeat(footer_off, sizeof(footer_val), &footer_val);
  
}

void SparseDB::writeCCTMajor1()
{
  int world_rank = mpi::World::rank();
  int world_size = mpi::World::size();

  //Get context global final offsets for cct.db
  ctx_off1 = std::move(ctxOffsets1());
  buildCtxGroupList();
  //auto my_ctxs = std::move(myCtxs(ctx_off1, world_size, world_rank));
  updateCtxOffsets(team_size, ctx_off1);

  ctxGrpId = world_size;
  accCtxGrp.initialize(ctxGrpId);

  //Prepare files to read and write, get the list of profiles
  cmf = util::File(dir / "cct1.db", true);
  
  if(world_rank == 0){
    auto cct_major_fi = cmf->open(true);
    // Write hdr
    writeCMSHdr(cct_major_fi);
    // Write ctx info section
    writeCtxInfoSec1(cct_major_fi);
  }

  //get the list of prof_info
  auto prof_info_list = std::move(profInfoList(team_size, *pmf));

  //get the ctx_id & ctx_idx pairs for all profiles
  auto all_prof_ctx_pairs = std::move(allProfileCtxIdIdxPairs1(prof_info_list));
  
  //read and write all the context groups I(rank) am responsible for
  rwAllCtxGroup1(prof_info_list, ctx_off1, team_size, all_prof_ctx_pairs);

  //footer
  mpi::barrier();
  if(world_rank != world_size - 1) return;

  auto cmfi = cmf->open(true);
  auto footer_off = ctx_off1.back();
  uint64_t footer_val = CCTDBftr;
  cmfi.writeat(footer_off, sizeof(footer_val), &footer_val);

}



//***************************************************************************
// others
//***************************************************************************

void SparseDB::merge(int threads, bool debug) {
 /* 
  int world_rank = mpi::World::rank();
  int world_size = mpi::World::size();

  ctxcnt = mpi::bcast(ctxcnt, 0);

  {
    util::log::debug msg{false};  // Switch to true for CTX id printouts
    msg << "CTXs (" << world_rank << ":" << sparseInputs.size() << "): "
        << ctxcnt;
  }

  std::vector<uint64_t> ctx_nzval_cnts (ctxcnt,0);
  std::set<uint16_t> empty;
  std::vector<std::set<uint16_t>> ctx_nzmids(ctxcnt,empty);
  keepTemps = debug;

  writeProfileMajor(threads,world_rank,world_size, ctx_nzval_cnts, ctx_nzmids);

  for(uint i = 0; i<ctxcnt; i++){
    if(ctx_nzval_cnts1[i] != ctx_nzval_cnts[i]) 
      printf("%d: %ld != %ld\n", i, ctx_nzval_cnts1[i], ctx_nzval_cnts[i]);
  }
  writeCCTMajor(ctx_nzval_cnts,ctx_nzmids, world_rank, world_size, threads);
  */
}



template <typename T>
void SparseDB::exscan(std::vector<T>& data, int threads) {
  int n = data.size();
  int rounds = ceil(std::log2(n));
  std::vector<T> tmp (n);
  int p;

  for(int i = 0; i<rounds; i++){
    p = (int)pow(2.0,i);
    #pragma omp parallel for num_threads(threads)
    for(int j = 0; j < n; j++){
      tmp.at(j) = (j<p) ?  data.at(j) : data.at(j) + data.at(j-p);
    }
    if(i<rounds-1) data = tmp;
  }

  if(n>0) data[0] = 0;
  #pragma omp parallel for num_threads(threads)
  for(int i = 1; i < n; i++)
    data[i] = tmp[i-1];
  
}


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

  return (R == -1) ? R : (-2 - R); //make it negative to differentiate it from found
}


template<class A, class B>
MPI_Datatype SparseDB::createPairType(MPI_Datatype aty, MPI_Datatype bty) {
  using realtype = std::pair<A, B>;
  std::array<int, 2> cnts = {1, 1};
  std::array<MPI_Datatype, 2> types = {aty, bty};
  std::array<MPI_Aint, 2> offsets = {offsetof(realtype, first), offsetof(realtype, second)};
  MPI_Datatype outtype;
  MPI_Type_create_struct(2, cnts.data(), offsets.data(), types.data(), &outtype);
  MPI_Type_commit(&outtype);
  return outtype;
}
  


