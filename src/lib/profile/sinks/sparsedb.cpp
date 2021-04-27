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

#include "sparsedb.hpp"

#include "../mpi/all.hpp"
#include "../util/log.hpp"

#include <lib/prof/cms-format.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/id-tuple.h>
#include <lib/prof/pms-format.h>

#include <cassert>
#include <cmath>
#include <fstream>
#include <omp.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

using namespace hpctoolkit;
using namespace hpctoolkit::sinks;

SparseDB::SparseDB(stdshim::filesystem::path p, int threads)
  : dir(std::move(p)), team_size(threads), rank(mpi::World::rank()), ctxMaxId(0),
    parForPi([&](pms_profile_info_t& item){ handleItemPi(item); }),
    fpos(0), accFpos(1000),
    parForCiip([&](profCtxIdIdxPairs& item){ handleItemCiip(item); }),
    parForPd([&](profData& item){ handleItemPd(item); }),
    ctxGrpId(0), accCtxGrp(1001),
    parForCtxs([&](ctxRange& item){ handleItemCtxs(item); }) {

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
  return parForPd.contribute() + parForCtxs.contribute();
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
    auto x = cs.emplace(id, c);
    assert(x.second && "Context identifiers not unique!");
  }, nullptr);

  contexts.reserve(cs.size());
  for(const auto& ic: cs) contexts.emplace_back(ic.second);

  ctxcnt = contexts.size();

  // initialize profile.db
  pmf = util::File(dir / "profile.db", true);
  pmf->synchronize();

  // hdr + id_tuples section
  int my_num_prof = src.threads().size();
  if(rank == 0) my_num_prof++;
  uint32_t total_num_prof = mpi::allreduce(my_num_prof, mpi::Op::sum());
  prof_info_sec_ptr = PMS_hdr_SIZE;
  prof_info_sec_size = total_num_prof * PMS_prof_info_SIZE;
  id_tuples_sec_ptr = prof_info_sec_ptr + MULTIPLE_8(prof_info_sec_size);

  setMinProfInfoIdx(total_num_prof); //set min_prof_info_idx, help later functions
  workIdTuplesSection(); // write id_tuples, set id_tuples_sec_size for hdr
  writePMSHdr(total_num_prof, *pmf);   // write hdr

  // prep for profiles writing
  prof_infos.resize(my_num_prof); // prepare for prof infos

  obuffers = std::vector<OutBuffer>(2); //prepare for prof data
  obuffers[0].cur_pos = 0;
  obuffers[1].cur_pos = 0;
  cur_obuf_idx = 0;

  fpos += id_tuples_sec_ptr + MULTIPLE_8(id_tuples_sec_size);
  accFpos.initialize(fpos); // start the window to keep track of the real file cursor

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

  pi.offset = writeProf(sparse_metrics_bytes, pi.prof_info_idx);
  prof_infos[pi.prof_info_idx - min_prof_info_idx] = std::move(pi);
}

void SparseDB::write()
{
  auto mpiSem = src.enterOrderedWrite();

  ctxcnt = mpi::bcast(ctxcnt, 0);
  ctx_nzmids_cnts.resize(ctxcnt, 1);//one for LastNodeEnd

  OutBuffer& ob = obuffers[cur_obuf_idx];

  if(rank == 0){
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

    //add summary data into the current buffer
    ob.buf.insert(ob.buf.end(), sparse_metrics_bytes.begin(), sparse_metrics_bytes.end());
    ob.buffered_pidxs.emplace_back(0);
    pi.offset = ob.cur_pos;
    prof_infos[0] = std::move(pi);
  }

  //flush out the remaining buffer
  if(ob.buf.size() != 0){
    uint64_t wrt_off = filePosFetchOp(ob.buf.size());
    flushOutBuffer(wrt_off, ob);
  }

  //write prof_infos
  writeProfInfos();

  //footer to show completeness
  if(rank == 0){
    auto pmfi = pmf->open(true, false);
    auto footer_val = PROFDBft;
    uint64_t footer_off = filePosFetchOp(sizeof(footer_val));
    pmfi.writeat(footer_off, sizeof(footer_val), &footer_val);
  }

  //gather cct major data
  ctx_nzval_cnts1.resize(ctxcnt, 0);
  for(const Context& c: contexts)
    ctx_nzval_cnts1[c.userdata[src.identifier()]] = c.userdata[ud].cnt.load(std::memory_order_relaxed);


  //write CCT major
  writeCCTMajor();

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
///  (version: 1.0)
///  (num_prof: 73)
///  (num_sec: 2)
///  (prof_info_sec_size: 3796)
///  (prof_info_sec_ptr: 128)
///  (id_tuples_sec_size: 1596)
///  (id_tuples_sec_ptr: 3928)
///]
///[Profile informations for 72 profiles
///  0[(id_tuple_ptr: 3928) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 182) (num_nzctxs: 120) (starting location: 70594))]
///  1[(id_tuple_ptr: 3940) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 52) (num_nzctxs: 32) (starting location: 59170)]
///  ...
///]
///[Id tuples for 121 profiles
///  0[(SUMMARY: 0) ]
///  1[(NODE: 713053824) (THREAD: 5) ]
///  2[(NODE: 713053824) (THREAD: 53) ]
///  ...
///]
///[thread 39
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
///PROFILEDB FOOTER CORRECT, FILE COMPLETE
//***************************************************************************

//
// hdr
//
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
  auto fhi = fh.open(true, false);
  fhi.writeat(0, PMS_real_hdr_SIZE, hdr.data());

}

//
// id tuples
//
std::vector<char> SparseDB::convertTuple2Bytes(const id_tuple_t& tuple)
{
  std::vector<char> bytes;

  uint16_t len = tuple.length;
  auto b = convertToByte2(len);
  bytes.insert(bytes.end(), b.begin(), b.end());

  for(uint i = 0; i < len; i++){
    auto& id = tuple.ids[i];

    b = convertToByte2(id.kind);
    bytes.insert(bytes.end(), b.begin(), b.end());
    b = convertToByte8(id.index);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }

  assert(bytes.size() == (size_t)PMS_id_tuple_len_SIZE + len * PMS_id_SIZE);
  return bytes;
}

void SparseDB::writeIdTuples(std::vector<id_tuple_t>& id_tuples, uint64_t my_offset)
{
  // convert to bytes
  std::vector<char> bytes;
  for(auto& tuple : id_tuples)
  {
    auto b = convertTuple2Bytes(tuple);
    bytes.insert(bytes.end(), b.begin(), b.end());
  }

  auto fhi = pmf->open(true, false);
  fhi.writeat(id_tuples_sec_ptr + my_offset, bytes.size(), bytes.data());
}


void SparseDB::workIdTuplesSection()
{
  int local_num_prof = src.threads().size();
  if(rank == 0) local_num_prof++;

  std::vector<id_tuple_t> id_tuples(local_num_prof);
  id_tuple_ptrs.resize(local_num_prof);
  uint64_t local_tuples_size = 0;

  // fill the id_tuples and id_tuple_ptrs
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
  writeIdTuples(id_tuples, my_offset);

  //set class variable, will be output in hdr
  id_tuples_sec_size = mpi::allreduce(local_tuples_size, mpi::Op::sum());

  //id_tuple_ptrs now store the number of bytes for each idtuple, exscan to get ptr
  exscan<uint64_t>(id_tuple_ptrs);
  for(auto& ptr : id_tuple_ptrs){
    ptr += (my_offset + id_tuples_sec_ptr);
  }

  // free all the tuples
  for(auto tuple : id_tuples){
    free(tuple.ids);
    tuple.ids = NULL;
  }


}

//
// prof infos
//
void SparseDB::setMinProfInfoIdx(const int total_num_prof)
{
  // find the minimum prof_info_idx of this rank
  min_prof_info_idx = 0;
  if(rank != 0){
    min_prof_info_idx = total_num_prof;
    for(const auto& t : src.threads().iterate()) {
      uint32_t prof_info_idx = t->userdata[src.identifier()] + 1;
      if(prof_info_idx < min_prof_info_idx) min_prof_info_idx = prof_info_idx;
    }
  }
}

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

  auto fhi = pmf->open(true, false);
  fhi.writeat(prof_info_sec_ptr + pi.prof_info_idx * PMS_prof_info_SIZE, PMS_prof_info_SIZE, info_bytes.data());
}


void SparseDB::writeProfInfos()
{

  parForPi.fill(std::move(prof_infos));
  parForPi.contribute(parForPi.wait());

}

//
// write profiles
//
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

void SparseDB::flushOutBuffer(uint64_t wrt_off, OutBuffer& ob)
{
  auto pmfi = pmf->open(true, true);
  pmfi.writeat(wrt_off, ob.buf.size(), ob.buf.data());
  for(auto pi : ob.buffered_pidxs)
    prof_infos[pi-min_prof_info_idx].offset += wrt_off;
  ob.cur_pos = 0;
  ob.buf.clear();
  ob.buffered_pidxs.clear();
}

uint64_t SparseDB::writeProf(const std::vector<char>& prof_bytes, uint32_t prof_info_idx)
{
  uint64_t wrt_off = 0;

  std::unique_lock<std::mutex> olck (outputs_l);
  OutBuffer& ob = obuffers[cur_obuf_idx];

  std::unique_lock<std::mutex> lck (ob.mtx);

  bool flush = false;
  if((prof_bytes.size() + ob.cur_pos) >= (64 * 1024 * 1024)){
    cur_obuf_idx = 1 - cur_obuf_idx;
    flush = true;

    size_t my_size = ob.buf.size() + prof_bytes.size();
    wrt_off = filePosFetchOp(my_size);
  }
  olck.unlock();

  //add bytes to the current buffer
  ob.buf.insert(ob.buf.end(), prof_bytes.begin(), prof_bytes.end());

  //record the prof_info_idx of the profile being added to the buffer
  ob.buffered_pidxs.emplace_back(prof_info_idx);

  //update current position
  uint64_t rel_off = ob.cur_pos;
  ob.cur_pos += prof_bytes.size();

  if(flush) flushOutBuffer(wrt_off, ob);

  return rel_off + wrt_off;
}






//***************************************************************************
// cct.db
//
///EXAMPLE
///HPCPROF-cmsdb___
///[hdr:
///  (version: 1.0)
///  (num_ctx: 137)
///  (num_sec: 1)
/// (ctx_info_sec_size: 3014)
///  (ctx_info_sec_ptr: 128)
///]
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
///CCTDB FOOTER CORRECT, FILE COMPLETE
//***************************************************************************
void SparseDB::writeCCTMajor()
{
  size_t world_size = mpi::World::size();

  // get context global final offsets for cct.db
  setCtxOffsets();
  buildCtxGroupList();
  updateCtxOffsets();

  // each processor gets its first ctx group
  ctxGrpId = world_size;
  accCtxGrp.initialize(ctxGrpId);

  cmf = util::File(dir / "cct.db", true);
  cmf->synchronize();

  // hdr + ctx info
  if(rank == 0){
    writeCMSHdr();
    writeCtxInfoSec();
  }

  // get the list of prof_info
  auto prof_info_list = std::move(profInfoList());

  // get the ctx_id & ctx_idx pairs for all profiles
  auto all_prof_ctx_pairs = std::move(allProfileCtxIdIdxPairs(prof_info_list));

  // read and write all the context groups I(rank) am responsible for
  rwAllCtxGroup(prof_info_list, all_prof_ctx_pairs);

  // footer
  mpi::barrier();
  if(rank != world_size - 1) return;

  auto cmfi = cmf->open(true, false);
  auto footer_off = ctx_off.back();
  uint64_t footer_val = CCTDBftr;
  cmfi.writeat(footer_off, sizeof(footer_val), &footer_val);

}

//
// ctx offsets
//
void SparseDB::setCtxOffsets()
{
  std::vector<uint64_t> local_ctx_off (ctxcnt + 1, 0);

  //get local sizes
  for(uint i = 0; i < ctx_nzval_cnts1.size(); i++){
    local_ctx_off[i] = ctx_nzval_cnts1[i] * CMS_val_prof_idx_pair_SIZE;
    //empty context also has LastMidEnd in ctx_nzmids_cnts, so if the size is 1, offet should not count that pair for calculation
    if(mpi::World::rank() == 0 && ctx_nzmids_cnts[i] > 1) local_ctx_off[i] += ctx_nzmids_cnts[i] * CMS_m_pair_SIZE;
  }

  //get local offsets
  exscan<uint64_t>(local_ctx_off);

  //sum up local offsets to get global offsets
  ctx_off = mpi::allreduce(local_ctx_off, mpi::Op::sum());

}

void SparseDB::updateCtxOffsets()
{
  assert(ctx_off.size() == ctxcnt + 1);

  for(uint i = 0; i < ctxcnt + 1; i++)
    ctx_off[i] += (MULTIPLE_8(ctxcnt * CMS_ctx_info_SIZE)) + CMS_hdr_SIZE;

}



//
// hdr
//
void SparseDB::writeCMSHdr()
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

  auto cct_major_fi = cmf->open(true, true);
  cct_major_fi.writeat(0, CMS_real_hdr_SIZE, hdr.data());
}


//
// ctx info
//
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


void SparseDB::writeCtxInfoSec()
{
  assert(ctx_nzmids_cnts.size() == ctxcnt);
  std::vector<char> info_bytes;

  for(uint i = 0; i < ctxcnt; i++){
    uint16_t num_nzmids = (uint16_t)(ctx_nzmids_cnts[i] - 1);
    uint64_t num_vals = (num_nzmids == 0) ? 0 \
      : ((ctx_off[i+1] - ctx_off[i]) - (num_nzmids + 1) * CMS_m_pair_SIZE) / CMS_val_prof_idx_pair_SIZE;
    cms_ctx_info_t ci = {i, num_vals, num_nzmids, ctx_off[i]};

    auto bytes = std::move(ctxInfoBytes(ci));
    info_bytes.insert(info_bytes.end(), bytes.begin(), bytes.end());
  }

  assert(info_bytes.size() == CMS_ctx_info_SIZE * ctxcnt);
  auto cct_major_fi = cmf->open(true, true);
  cct_major_fi.writeat(CMS_hdr_SIZE, info_bytes.size(), info_bytes.data());
}


//
// helper - gather prof infos
//
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


std::vector<pms_profile_info_t> SparseDB::profInfoList()
{
  std::vector<pms_profile_info_t> prof_info;
  auto fhi = pmf->open(false, false);

  //read the number of profiles
  uint32_t num_prof = readAsByte4(fhi, (HPCPROFILESPARSE_FMT_MagicLen + HPCPROFILESPARSE_FMT_VersionLen));
  num_prof--; //minus the summary profile

  //read the whole Profile Information section
  int count = num_prof * PMS_prof_info_SIZE;
  char *input = new char[count];
  fhi.readat(PMS_hdr_SIZE + PMS_prof_info_SIZE, count, input); //skip one prof_info (summary)

  //interpret the section and store in a vector of pms_profile_info_t
  prof_info.resize(num_prof);
  for(int i = 0; i<count; i += PMS_prof_info_SIZE){
    auto pi = profInfo(input + i);
    pi.prof_info_idx = i/PMS_prof_info_SIZE + 1; // the first one is summary profile
    prof_info[i/PMS_prof_info_SIZE] = std::move(pi);
  }
  delete []input;

  return prof_info;
}

//
// helper - gather ctx id idx pairs
//
SparseDB::PMS_CtxIdIdxPair SparseDB::ctxIdIdxPair(const char *input)
{
  PMS_CtxIdIdxPair ctx_pair;
  ctx_pair.ctx_id  = interpretByte4(input);
  ctx_pair.ctx_idx = interpretByte8(input + PMS_ctx_id_SIZE);
  return ctx_pair;
}


void SparseDB::handleItemCiip(profCtxIdIdxPairs& ciip)
{
  auto pmfi = pmf->open(false, false);
  auto pi = *(ciip.pi);
  std::vector<PMS_CtxIdIdxPair> ctx_id_idx_pairs(pi.num_nzctxs + 1);
  if(pi.num_nzctxs == 0){
    *(ciip.prof_ctx_pairs) = std::move(ctx_id_idx_pairs);
    return;
  }

  //read the whole ctx_id_idx_pairs chunk
  int count = (pi.num_nzctxs + 1) * PMS_ctx_pair_SIZE;
  char *input = new char[count];
  uint64_t ctx_pairs_offset = pi.offset + pi.num_vals * (PMS_val_SIZE + PMS_mid_SIZE);
  pmfi.readat(ctx_pairs_offset, count, input);

  //interpret the chunk and store accordingly
  for(int i = 0; i<count; i += PMS_ctx_pair_SIZE)
    ctx_id_idx_pairs[i/PMS_ctx_pair_SIZE] = std::move(ctxIdIdxPair(input + i));

  delete []input;
  *(ciip.prof_ctx_pairs) = std::move(ctx_id_idx_pairs);
}

std::vector<std::vector<SparseDB::PMS_CtxIdIdxPair>>
SparseDB::allProfileCtxIdIdxPairs( std::vector<pms_profile_info_t>& prof_info)
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

//
// helper - extract one profile data
//
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
                                         const uint64_t& off)
{
  std::vector<char> bytes;
  if(my_ctx_pairs.size() <= 1) return bytes;

  uint64_t first_ctx_idx = my_ctx_pairs.front().second;
  uint64_t last_ctx_idx  = my_ctx_pairs.back().second;
  int val_mid_count = (last_ctx_idx - first_ctx_idx) * (PMS_val_SIZE + PMS_mid_SIZE);
  if(val_mid_count == 0) return bytes;

  bytes.resize(val_mid_count);
  uint64_t val_mid_start_pos = off + first_ctx_idx * (PMS_val_SIZE + PMS_mid_SIZE);

  auto pmfi = pmf->open(false, false);
  pmfi.readat(val_mid_start_pos, val_mid_count, bytes.data());
  return bytes;
}

void SparseDB::handleItemPd(profData& pd)
{
  uint i = pd.i;
  auto poff = pd.pi_list->at(i).offset;

  auto my_ctx_pairs = std::move(myCtxPairs(*(pd.ctx_ids), pd.all_prof_ctx_pairs->at(i)));
  pd.profiles_data->at(i) = {std::move(my_ctx_pairs), std::move(valMidsBytes(my_ctx_pairs, poff))};

}

std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>>
SparseDB::profilesData(std::vector<uint32_t>& ctx_ids, std::vector<pms_profile_info_t>& prof_info_list,
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


//
// helper - convert one profile data to a CtxMetricBlock
//
SparseDB::MetricValBlock SparseDB::metValBloc(const hpcrun_metricVal_t val,
                                              const uint16_t mid, const uint32_t prof_idx)
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


//
// helper - convert CtxMetricBlocks to correct bytes for writing
//
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

std::vector<char> SparseDB::cmbBytes(const CtxMetricBlock& cmb, const uint32_t& ctx_id)
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
  if(ctx_off[ctx_id] + bytes.size() !=  ctx_off[ctx_id+1]){
    util::log::fatal() << __FUNCTION__ << ": (ctx id: " << ctx_id
       << ") (num_vals: " << bytes.size()/CMS_val_prof_idx_pair_SIZE
       << ") (num_nzmids: " << metrics.size()
       << ") (ctx_off: " << ctx_off[ctx_id]
       << ") (next_ctx_off: " << ctx_off[ctx_id + 1] << ")";
  }

  return bytes;

}

//
// write contexts
//
void SparseDB::handleItemCtxs(ctxRange& cr)
{
  auto ofhi = cmf->open(true, true);
  std::vector<char> ctxRangeBytes;

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
    uint32_t first_ctx_id = heap.front().ctx_id;

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


      auto b = std::move(cmbBytes(cmb, ctx_id));
      ctxRangeBytes.insert(ctxRangeBytes.end(), b.begin(), b.end());

    }//END of while
    if((first_ctx_id != LastNodeEnd) && ctxRangeBytes.size() > 0){
      uint64_t ctxRangeBytes_off = ctx_off[first_ctx_id];
      ofhi.writeat(ctxRangeBytes_off, ctxRangeBytes.size(), ctxRangeBytes.data());
    }

  } //END of if my_start < my_end
}


void SparseDB::rwOneCtxGroup(std::vector<uint32_t>& ctx_ids, std::vector<pms_profile_info_t>& prof_info_list,
                             std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs)
{
  if(ctx_ids.size() == 0) return;

  //read corresponding ctx_id_idx pairs and relevant val&mids bytes
  auto profiles_data = std::move(profilesData(ctx_ids, prof_info_list, all_prof_ctx_pairs));

  //assign ctx_ids to diffrent threads based on size
  uint first_ctx_off = ctx_off[ctx_ids.front()];
  uint total_ctx_ids_size = ctx_off[ctx_ids.back() + 1] - first_ctx_off;
  uint thread_ctx_ids_size = round(total_ctx_ids_size/team_size);

  //record the start and end position of each thread's ctxs
  std::vector<uint64_t> t_starts (team_size, 0);
  std::vector<uint64_t> t_ends (team_size, 0);
  int cur_thread = 0;

  //make sure first thread at least gets one ctx
  size_t cur_size = ctx_off[ctx_ids.front() + 1] - first_ctx_off; //size of first ctx
  t_starts[cur_thread] = 0; //the first ctx goes to t_starts[0]
  if(team_size > 1){
    for(uint i = 2; i <= ctx_ids.size(); i++){
      auto cid = (i == ctx_ids.size()) ? ctx_ids[i-1] + 1 : ctx_ids[i];
      auto cid_size = ctx_off[cid] - ctx_off[ctx_ids[i-1]];

      if(cur_size > thread_ctx_ids_size){
        t_ends[cur_thread] = i-1;
        cur_thread++;
        t_starts[cur_thread] = i-1;
        cur_size = cid_size;
      }else{
        cur_size += cid_size;
      }
      if(cur_thread == team_size - 1) break;
    }
  }
  t_ends[cur_thread] = ctx_ids.size();


  // prepare for the real work
  std::vector<ctxRange> crs(team_size);
  auto pdptr = &profiles_data;
  auto cidsptr = &ctx_ids;
  auto pisptr = &prof_info_list;
  for(int i = 0; i < team_size; i++){
    ctxRange cr;
    cr.start = t_starts[i];
    cr.end = t_ends[i];
    cr.pd = pdptr;
    cr.ctx_ids = cidsptr;
    cr.pis = pisptr;
    crs[i] = std::move(cr);
  }
  parForCtxs.fill(std::move(crs));
  parForCtxs.reset();

}


void SparseDB::buildCtxGroupList()
{
  uint64_t cur_size = 0;
  uint64_t total_size = ctx_off.back();
  uint64_t size_limit = std::min<uint64_t>((uint64_t)1024*1024*1024*3,\
                        round(total_size/(3 * mpi::World::size())));

  ctx_group_list.emplace_back(0);
  for(uint i = 0; i < ctx_off.size() - 1; i++){
    uint64_t cur_ctx_size = ctx_off[i + 1] - ctx_off[i];

    if((cur_size + cur_ctx_size) > size_limit){
      ctx_group_list.emplace_back(i);
      cur_size = 0;
    }

    cur_size += cur_ctx_size;
  }
  ctx_group_list.emplace_back(ctx_off.size() - 1);

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


void SparseDB::rwAllCtxGroup(std::vector<pms_profile_info_t>& prof_info,
                              std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs)
{
  uint32_t idx = rank;
  uint32_t num_groups = ctx_group_list.size();
  std::vector<uint32_t> ctx_ids;

  while(idx < num_groups - 1){
    ctx_ids.clear();
    auto& start_id = ctx_group_list[idx];
    auto& end_id = ctx_group_list[idx + 1];
    for(uint i = start_id; i < end_id; i++) ctx_ids.emplace_back(i);
    rwOneCtxGroup(ctx_ids, prof_info, all_prof_ctx_pairs);
    idx = ctxGrpIdFetch(); //communicate between processes to get next group => "dynamic" load balance
  }

  parForPd.fill({});  // Make sure the workshare is non-empty
  parForPd.complete();

  parForCtxs.fill({});
  parForCtxs.complete();

}


//***************************************************************************
// others
//***************************************************************************

void SparseDB::merge(int threads, bool debug) {}



template <typename T>
void SparseDB::exscan(std::vector<T>& data) {
  int n = data.size();
  int rounds = ceil(std::log2(n));
  std::vector<T> tmp (n);
  int p;

  for(int i = 0; i<rounds; i++){
    p = (int)pow(2.0,i);
    //#pragma omp parallel for num_threads(threads)
    for(int j = 0; j < n; j++)
      tmp.at(j) = (j<p) ?  data.at(j) : data.at(j) + data.at(j-p);

    if(i<rounds-1) data = tmp;
  }

  if(n>0) data[0] = 0;
  //#pragma omp parallel for num_threads(threads)
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





