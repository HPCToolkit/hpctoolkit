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

  //Add the extra cct node id and offset pair, to mark the end of cct node - YUMENG
  cids.push_back(LastNodeEnd);
  coffsets.push_back(values.size());

  // Put together the sparse_metrics structure
  hpcrun_fmt_sparse_metrics_t sm;
  sm.tid = t.attributes.has_threadid() ? t.attributes.threadid() : 0;
  sm.num_vals = values.size();
  sm.num_cct = contexts.size();
  sm.num_nz_cct = coffsets.size() - 1;
  sm.values = values.data();
  sm.mids = mids.data();
  sm.cct_id = cids.data();
  sm.cct_off = coffsets.data();

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
[Profile informations for 72 profiles (thread id : number of nonzero values : number of nonzero CCTs : offset)
  (0:186:112:65258   1:136:74:98930   2:138:75:107934   3:136:74:6224   4:131:71:70016   5:148:85:91202   ...)
]
[
  (value:metric id): 2.48075:1  2.48075:1  2.48075:1  0.000113:1  0.000113:0  0.000113:1  ...
  (cct id : offset): 1:0 7:1 9:2 11:3 15:4 ...
]
...same [sparse metrics] for all rest profiles 
*/
//
// SIZE CHART: data(size in bytes)
//    Number of profiles        (4)
// [Profile informations] 
//    Thread ID                 (4) 
//    Number of non-zero values (8) 
//    Number of non-zero CCTs   (4) 
//    Profile offset            (8)
// [sparse metrics] 
//    Non-zero values           (8)
//    Metric IDs of values      (2)
//    CCT ID                    (4)
//    CCT offset                (8)

//***************************************************************************
//iterate through rank's profile list, assign profile_sizes (profile Thread object : size)
void SparseDB::getProfileSizes(std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes, uint64_t& my_size){
  for(const auto& tp: outputs.citerate()) {
    struct stat buf;
    stat(tp.second.string().c_str(),&buf);
    my_size += (buf.st_size - TMS_prof_skip_SIZE);
    profile_sizes.emplace_back(tp.first,(buf.st_size - TMS_prof_skip_SIZE));    
  }
}

//assign total number of profiles across all ranks
void SparseDB::getTotalNumProfiles(const uint32_t my_num_prof, uint32_t& total_num_prof){
  MPI_Allreduce(&my_num_prof, &total_num_prof, 1, mpi_data<uint32_t>::type, MPI_SUM, MPI_COMM_WORLD);
}

//assign the offset for this rank's profile chunk in thread_major_sparse.db
void SparseDB::getMyOffset(const uint64_t my_size, uint64_t& my_offset, const int rank){
  MPI_Exscan(&my_size, &my_offset, 1, mpi_data<uint64_t>::type, MPI_SUM, MPI_COMM_WORLD);
  if(rank == 0) my_offset = 0;
}

//assign the final global offsets for each profile
void SparseDB::getMyProfOffset(std::vector<std::pair<uint32_t, uint64_t>>& prof_offsets,
    const std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes,
    const uint32_t total_prof, const uint64_t my_offset, const int threads)
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
    if(i < tmp.size() - 1) assert(tmp[i]+profile_sizes[i].second == tmp[i+1]);

    prof_offsets[i].first  = profile_sizes[i].first->attributes.threadid();
    prof_offsets[i].second = tmp[i] + my_offset + (total_prof * TMS_prof_info_SIZE) + TMS_total_prof_SIZE; 
  }

}


//collect data for cct_major_sparse.db while processing all the profiles the first time
void SparseDB::collectCctMajorData(std::vector<uint64_t>& cct_local_sizes, std::vector<std::set<uint16_t>>& cct_nzmids, const std::vector<char>& bytes)
{ 
  assert(cct_local_sizes.size() == cct_nzmids.size());
  assert(cct_local_sizes.size() > 0);

  uint64_t num_val;
  uint32_t num_nzcct;
  interpretByte8(num_val,   bytes.data() + TMS_tid_SIZE);
  interpretByte4(num_nzcct, bytes.data() + TMS_tid_SIZE + TMS_num_val_SIZE);
  int before_cct_SIZE     = TMS_prof_skip_SIZE + num_val * (TMS_val_SIZE + TMS_mid_SIZE);

  uint64_t check_num_val = 0;
  for(uint i = 0; i < num_nzcct; i++){
    uint32_t cct_id;

    //local sizes
    uint64_t cct_offset;
    uint64_t next_cct_offset;
    interpretByte4(cct_id,          bytes.data() + before_cct_SIZE + TMS_cct_pair_SIZE * i);
    interpretByte8(cct_offset,      bytes.data() + before_cct_SIZE + TMS_cct_pair_SIZE * i + TMS_cct_id_SIZE);
    interpretByte8(next_cct_offset, bytes.data() + before_cct_SIZE + TMS_cct_pair_SIZE * (i+1) + TMS_cct_id_SIZE);
    uint64_t num_val_this_cct = next_cct_offset - cct_offset;
    cct_local_sizes[CCTIDX(cct_id)] += num_val_this_cct;
    check_num_val += num_val_this_cct;    

    
    //nz_mids (number of non-zero values = number of non-zero metric ids for one profile one cct)
    for(uint m = 0; m < num_val_this_cct; m++){
      uint16_t mid;
      interpretByte2(mid, bytes.data() + TMS_prof_skip_SIZE + (cct_offset + m) * (TMS_mid_SIZE + TMS_val_SIZE) + TMS_val_SIZE);
      //if(cct_nzmids[CCTIDX(cct_id)].size() == 0){
      //  std::set<uint16_t> mids;
      //  cct_nzmids[CCTIDX(cct_id)] = mids;
      //}
      cct_nzmids[CCTIDX(cct_id)].insert(mid); 
    }
    
  } //END of cct loop

  if(num_val != check_num_val) 
    exitError("CCT data collected while writing thread_major_sparse.db is not correct! (won't influence thread_major_sparse.db)");
    //util::log::fatal() << "CCT data collected while writing thread_major_sparse.db is not correct! (won't influence thread_major_sparse.db)";
}

//write all the profiles at the correct place, and collect data needed for cct_major_sparse.db 
int SparseDB::writeProfiles(const std::vector<std::pair<uint32_t, uint64_t>>& prof_offsets, std::vector<uint64_t>& cct_local_sizes,
    std::vector<std::set<uint16_t>>& cct_nzmids, const std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes, 
    const MPI_File fh, const int threads)
{

  assert(cct_local_sizes.size() == cct_nzmids.size());
  assert(cct_local_sizes.size() > 0);


  #pragma omp declare reduction (vectorSum : std::vector<uint64_t> : \
      std::transform(omp_out.begin(), omp_out.end(), omp_in.begin(), omp_out.begin(), std::plus<uint64_t>()))\
      initializer(omp_priv = decltype(omp_orig)(omp_orig.size()))

  #pragma omp parallel num_threads(threads)
  {
    std::set<uint16_t> empty;
    std::vector<std::set<uint16_t>> thread_cct_nzmids (cct_nzmids.size(),empty);

    #pragma omp for reduction(vectorSum : cct_local_sizes)
    for(uint i = 0; i<profile_sizes.size();i++){

      //to read and write: get file name, size, offset
      const hpctoolkit::Thread* threadp = profile_sizes.at(i).first;
      uint32_t tid = (uint32_t)threadp->attributes.threadid();

      std::string fn = outputs.at(threadp).string();
      MPI_Offset my_prof_offset = prof_offsets.at(i).second;
      if(tid != prof_offsets.at(i).first){
        exitError("For profile " + std::to_string(tid) + ", prof_offsets and profile_sizes don't match!");
        //util::log::fatal() << "For profile " << tid << ", prof_offsets and profile_sizes don't match!";
      }

      //get all bytes from a profile
      std::ifstream input(fn.c_str(), std::ios::binary);
      std::vector<char> bytes(
          (std::istreambuf_iterator<char>(input)),
          (std::istreambuf_iterator<char>()));
      input.close();

      //collect cct local sizes and nz_mids
      collectCctMajorData(cct_local_sizes,thread_cct_nzmids,bytes);

      //write profile information at top
      std::vector<char> info (TMS_prof_info_SIZE);
      std::copy(bytes.begin(), bytes.begin()+TMS_prof_skip_SIZE, info.begin());
      convertToByte8(prof_offsets[i].second, info.data() + TMS_prof_skip_SIZE);
      MPI_Offset info_off = TMS_total_prof_SIZE + tid * TMS_prof_info_SIZE;
      MPI_Status stat;
      SPARSE_exitIfMPIError(MPI_File_write_at(fh, info_off, info.data(), TMS_prof_info_SIZE, MPI_BYTE, &stat), 
        __FUNCTION__ +  std::string(": profile ") + std::to_string(tid) + ": write prof info");

      //write the profile data at its offset
      SPARSE_exitIfMPIError(MPI_File_write_at(fh, my_prof_offset, bytes.data()+TMS_prof_skip_SIZE, bytes.size()-TMS_prof_skip_SIZE, MPI_BYTE, &stat),
        __FUNCTION__ +  std::string(": profile ") + std::to_string(tid) + ": write prof bytes");
    
    }//END of for loop to write profiles

    #pragma omp critical
    {
      for(uint j = 0; j<cct_nzmids.size(); j++){
        std::set_union(cct_nzmids[j].begin(), cct_nzmids[j].end(),
              thread_cct_nzmids[j].begin(), thread_cct_nzmids[j].end(),
              std::inserter(cct_nzmids[j], cct_nzmids[j].begin()));
      }
    }

  }//END of parallel region

  return SPARSE_OK;
}

void SparseDB::writeThreadMajor(const int threads, const int world_rank, const int world_size, std::vector<uint64_t>& cct_local_sizes,std::vector<std::set<uint16_t>>& cct_nzmids){
  //
  // profile_sizes: vector of (thread attributes: its own size)
  // prof_offsets:  vector of (thread id: final global offset)
  // my_size:       the size of this rank's all profiles  
  // total_prof:    total number of profiles across ranks
  // prof_infos:    vector of (thread id: bytes representing <threadID, num_val, num_nzcct, offset>)
  //

  std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>> profile_sizes;
  uint64_t my_size = 0;
  getProfileSizes(profile_sizes, my_size);
  std::vector<std::pair<uint32_t, uint64_t>> prof_offsets (profile_sizes.size());
  uint32_t total_prof;
  getTotalNumProfiles(profile_sizes.size(), total_prof);
  uint64_t my_off;
  getMyOffset(my_size,my_off,world_rank);
  getMyProfOffset(prof_offsets,profile_sizes,total_prof, my_off, threads/world_size);


  MPI_File thread_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "thread_major_sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &thread_major_f);


  if(world_rank == 0) SPARSE_exitIfMPIError(writeAsByte4(total_prof, thread_major_f, 0), __FUNCTION__ + std::string(": write the number of profiles wrong"));
  writeProfiles(prof_offsets, cct_local_sizes, cct_nzmids, profile_sizes, thread_major_f, threads/world_size);

  MPI_File_close(&thread_major_f);
}

//***************************************************************************
// cct_major_sparse.db  - YUMENG
//
/*EXAMPLE
[CCT informations for 232 CCTs (CCT id : number of nonzero values : number of nonzero metric ids : offset)
  (1:72:1:5108   3:71:1:5982   5:71:1:6844   7:70:1:7706   ...)
[
  (value:threadID)  : 3.01672:0 3.03196:1 3.02261:2 3.00047:3 ...
  (metricID:offset) : 1:0
]
...same [sparse metrics] for all rest ccts 
*/
//
// SIZE CHART: data(size in bytes)
//    Number of ccts                (4)
// [CCT informations] 
//    CCT ID                        (4)  
//    Number of non-zero values     (8)  
//    Number of non-zero metric IDs (2) 
//    Offset                        (8)
// [sparse metrics] 
//    Non-zero values               (8)
//    Thread IDs of non-zero values (4)
//    Metric ID                     (2) 
//    Metric offset                 (8)

//***************************************************************************

// union cct_nzmids from all ranks to root, in order to avoid double counting for offset calculation
void SparseDB::unionMids(std::vector<std::set<uint16_t>>& cct_nzmids, int rank, int num_proc, int threads)
{
  assert(cct_nzmids.size() > 0);

  //convert to a long vector with stopper between mids for different ccts
  uint16_t stopper = -1;
  std::vector<uint16_t> rank_all_mids;
  for(auto cct : cct_nzmids){
    for(auto mid: cct){
      rank_all_mids.emplace_back(mid);
    }
    rank_all_mids.emplace_back(stopper);
  }

  //prepare for later gatherv
  int rank_mids_size = rank_all_mids.size();
  std::vector<int> mids_sizes;
  if(rank == 0) mids_sizes.resize(num_proc);
  MPI_Gather(&rank_mids_size, 1, MPI_INT, mids_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  int total_size = 0;
  std::vector<int> mids_disps; 
  std::vector<uint16_t> global_all_mids;
  if(rank == 0){
    mids_disps.resize(num_proc);

    #pragma omp parallel for num_threads(threads)
    for(int i = 0; i<num_proc; i++) mids_disps[i] = mids_sizes[i];
    exscan<int>(mids_disps,threads); 

    total_size = mids_disps.back() + mids_sizes.back();
    global_all_mids.resize(total_size);
  }

  //gather all the rank_all_mids (i.e. cct_nzmids) to root
  MPI_Gatherv(rank_all_mids.data(),rank_mids_size, mpi_data<uint16_t>::type, \
    global_all_mids.data(), mids_sizes.data(), mids_disps.data(), mpi_data<uint16_t>::type, 0, MPI_COMM_WORLD);

  if(rank == 0){
    int num_stopper = 0;
    int num_cct     = cct_nzmids.size();
    for(int i = 0; i< total_size; i++) {
      uint16_t mid = global_all_mids[i];
      if(mid == stopper){
        num_stopper++;
      }else{
        cct_nzmids[num_stopper % num_cct].insert(mid); 
      }
    }

    //Add extra space for marking end location for the last mid
    #pragma omp parallel for num_threads(threads)
    for(uint i = 0; i<cct_nzmids.size(); i++){
      cct_nzmids[i].insert(LastMidEnd);
    }
  }


}

void vSum ( uint64_t *, uint64_t *, int *, MPI_Datatype * );
void vSum(uint64_t *invec, uint64_t *inoutvec, int *len, MPI_Datatype *dtype)
{
    int i;
    for ( i=0; i<*len; i++ )
        inoutvec[i] += invec[i];
}

//input: local sizes for all cct, output: global offsets for all cct
void SparseDB::getCctOffset(std::vector<uint64_t>& cct_sizes, std::vector<std::set<uint16_t>>& cct_nzmids,
    std::vector<std::pair<uint32_t, uint64_t>>& cct_off,int threads, int rank)
{
  assert(cct_sizes.size() == cct_nzmids.size());
  assert(cct_sizes.size() == cct_off.size() - 1);

  std::vector<uint64_t> tmp (cct_sizes.size() + 1, 0);
  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < tmp.size() - 1; i++){
    tmp[i] = cct_sizes[i] * CMS_val_tid_pair_SIZE;
    if(rank == 0 && cct_nzmids[i].size() > 1) tmp[i] += cct_nzmids[i].size() * CMS_m_pair_SIZE; 
  }

  exscan<uint64_t>(tmp,threads); //get local offsets

  //sum up local offsets to get global offsets
  MPI_Op vectorSum;
  MPI_Op_create((MPI_User_function *)vSum, 1, &vectorSum);
  MPI_Allreduce(MPI_IN_PLACE, tmp.data(), tmp.size() ,mpi_data<uint64_t>::type, vectorSum, MPI_COMM_WORLD);

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i<tmp.size();i++){
    cct_off[i].first = (i < tmp.size() - 1) ? CCTID(i) : LastNodeEnd;
    cct_off[i].second = tmp[i];
  }

  MPI_Op_free(&vectorSum);

}

//input: final offsets for all cct, output: my(rank) own responsible cct list
void SparseDB::getMyCCTs(std::vector<std::pair<uint32_t, uint64_t>>& cct_off,
    std::vector<uint32_t>& my_ccts, int num_ranks, int rank)
{
  assert(cct_off.size() > 0);

 // MPI_Allreduce(MPI_IN_PLACE, &last_cct_size, 1 ,mpi_data<uint64_t>::type, MPI_SUM, MPI_COMM_WORLD);

  //total_size = cct_off.back().second + last_cct_size;
  uint64_t total_size = cct_off.back().second;
  uint64_t max_size_per_rank = round(total_size/num_ranks);
  uint64_t my_start = rank * max_size_per_rank;
  uint64_t my_end = (rank == num_ranks - 1) ? total_size : (rank + 1) * max_size_per_rank;

  //if(rank == 0) my_ccts.emplace_back(FIRST_CCT_ID); 
  for(uint i = 1; i<cct_off.size(); i++){
    if(cct_off[i].second > my_start && cct_off[i].second <= my_end) my_ccts.emplace_back(cct_off[i-1].first);
  }
  //if(rank == num_ranks - 1) my_ccts.emplace_back(cct_off.back().first);

}

//update the global offsets with calculated CCT info section size
void SparseDB::updateCctOffset(std::vector<std::pair<uint32_t, uint64_t>>& cct_off, size_t ctxcnt, int threads)
{
  assert(cct_off.size() == ctxcnt + 1);

  #pragma omp parallel for num_threads(threads)
  for(uint i = 0; i < ctxcnt + 1; i++){
    cct_off[i].second += ctxcnt * CMS_cct_info_SIZE + CMS_num_cct_SIZE;
  }
}


//read the Profile Information section of thread_major_sparse.db to get the list of profiles 
void SparseDB::readProfileInfo(std::vector<ProfileInfo>& prof_info, int threads, MPI_File fh)
{
  //read the number of profiles
  uint32_t num_prof;
  SPARSE_exitIfMPIError(readAsByte4(num_prof,fh,0), __FUNCTION__ + std::string(": cannot read the number of profiles"));
  int count = num_prof * TMS_prof_info_SIZE; 
  char input[count];

  //read the whole Profile Information section
  MPI_Status stat;
  SPARSE_exitIfMPIError(MPI_File_read_at(fh, TMS_total_prof_SIZE, &input, count, MPI_BYTE, &stat), 
    __FUNCTION__ + std::string(": cannot read the whole Profile Information section"));

  //interpret the section and store in a vector of ProfileInfo
  prof_info.resize(num_prof);
  #pragma omp parallel for num_threads(threads)
  for(int i = 0; i<count; i += TMS_prof_info_SIZE){
    uint32_t tid;
    uint64_t num_val;
    uint32_t num_nzcct; 
    uint64_t offset;
    interpretByte4(tid,       input + i);
    interpretByte8(num_val,   input + i + TMS_tid_SIZE);
    interpretByte4(num_nzcct, input + i + TMS_tid_SIZE + TMS_num_val_SIZE);
    interpretByte8(offset,    input + i + TMS_tid_SIZE + TMS_num_val_SIZE + TMS_num_nzcct_SIZE);
    ProfileInfo pi = {tid, num_val, num_nzcct, offset};
    prof_info[i/TMS_prof_info_SIZE] = pi;
  }

}


//read cct_offsets from profile (at off of fh), with known cct_offsets size
int SparseDB::readCCToffsets(std::vector<std::pair<uint32_t,uint64_t>>& cct_offsets,
    MPI_File fh, MPI_Offset off)
{
  assert(cct_offsets.size() > 0);

  //read the whole cct_offsets chunk
  int count = cct_offsets.size() * TMS_cct_pair_SIZE; 
  char input[count];

  MPI_Status stat;
  SPARSE_throwIfMPIError(MPI_File_read_at(fh,off,&input,count,MPI_BYTE,&stat));

  //interpret the chunk and store accordingly
  for(int i = 0; i<count; i += TMS_cct_pair_SIZE){
    uint32_t cct_id;
    uint64_t cct_off;
    interpretByte4(cct_id,  input + i);
    interpretByte8(cct_off, input + i + TMS_cct_id_SIZE);
    auto p = std::make_pair(cct_id, cct_off);
    cct_offsets[i/TMS_cct_pair_SIZE] = p;
  }

  return SPARSE_OK;
    
}

//given full cct offsets from the profile and a group of cct ids, return a vector of <cct id, offset> with number of nzcct in this group + 1
//the last pair (extra one) can be used to calculate the size of the last cct id
void SparseDB::binarySearchCCTid(std::vector<uint32_t>& cct_ids,
    std::vector<std::pair<uint32_t,uint64_t>>& profile_cct_offsets,
    std::vector<std::pair<uint32_t,uint64_t>>& my_cct_offsets)
{
  assert(profile_cct_offsets.size() > 0);

  uint n = profile_cct_offsets.size() - 1; //since the last is LastNodeEnd
  uint m; //index of current pair in profile_cct_offsets
  int found = 0;
  uint32_t target;

  for(uint i = 0; i<cct_ids.size(); i++){
    target = cct_ids[i];
    
    if(found){ // if already found one cct id, no need to binary search again, since it's sorted
      m += 1;

      //when the cct_ids list is not finished, but the profile_cct_offsets has been searched through
      if(m == n) { 
        m -= 1;
        break;
      }

      //next cct id in profile_cct_offsets 
      uint32_t prof_cct_id = profile_cct_offsets[m].first;
      if(prof_cct_id == target){
        my_cct_offsets.emplace_back(target,profile_cct_offsets[m].second);
      }else if(prof_cct_id > target){
        m -= 1; //back to original since this might be next target
      }else{ //profile_cct_offsets[m].first < target, should not happen
        std::ostringstream ss;
        ss << __FUNCTION__ << "cct id " << prof_cct_id << " in a profile does not exist in the full cct list while searching for " 
          << target << " with m " << m;
        exitError(ss.str());
        //util::log::fatal() << __FUNCTION__ << "cct id " << prof_cct_id << " in a profile does not exist in the full cct list while searching for " 
        //  << target << " with m " << m;
      }

    }else{
      //simple binary search 
      int L = 0;
      int R = n - 1;
      while(L <= R){
        m = (L + R) / 2;
        uint32_t prof_cct_id = profile_cct_offsets[m].first;
        if(prof_cct_id < target){
          L = m + 1;
        }else if(prof_cct_id > target){
          R = m - 1;
        }else{ //find match
          my_cct_offsets.emplace_back(target,profile_cct_offsets[m].second);
          found = 1;
          break;
        }
      }
    } //END of searching for current cct_id
       
  } //END of cct_ids loop

  //add one extra offset pair for later use
  my_cct_offsets.emplace_back(LastNodeEnd, profile_cct_offsets[m+1].second);

  assert(my_cct_offsets.size() <= cct_ids.size() + 1);
  
}

//read all the data for a group of ccts in cct_ids, from one profile with its prof_info, store data to cct_data_pairs 
void SparseDB::readOneProfile(std::vector<uint32_t>& cct_ids, ProfileInfo& prof_info,
    std::vector<CCTDataPair>& cct_data_pairs, MPI_File fh)
{
  //find the corresponding cct and its offset in values and mids (this offset is not the offset of cct_major_sparse.db)
  MPI_Offset offset = prof_info.offset;
  MPI_Offset cct_offsets_offset = offset + prof_info.num_val * (TMS_val_SIZE + TMS_mid_SIZE);
  std::vector<std::pair<uint32_t,uint64_t>> full_cct_offsets (prof_info.num_nzcct + 1);
  if(full_cct_offsets.size() == 1) return;

  SPARSE_exitIfMPIError(readCCToffsets(full_cct_offsets,fh,cct_offsets_offset), 
                          __FUNCTION__ + std::string(": cannot read CCT offsets for profile ") + std::to_string(prof_info.tid));

  std::vector<std::pair<uint32_t,uint64_t>> my_cct_offsets;
  binarySearchCCTid(cct_ids,full_cct_offsets,my_cct_offsets);

  //read all values and metric ids for this group of cct at once
  //if(my_cct_offsets.back().second == NEED_NUM_VAL) my_cct_offsets.back().second = prof_info.num_val;
  uint64_t first_offset = my_cct_offsets.front().second;
  uint64_t last_offset  = my_cct_offsets.back().second;
  MPI_Offset val_mid_start_pos = offset + first_offset * (TMS_val_SIZE + TMS_mid_SIZE);
  int val_mid_count = (last_offset - first_offset) * (TMS_val_SIZE + TMS_mid_SIZE);
  char vminput[val_mid_count];
  MPI_Status stat;
  if(val_mid_count != 0) {
    SPARSE_exitIfMPIError(MPI_File_read_at(fh, val_mid_start_pos, &vminput, val_mid_count, MPI_BYTE, &stat),
                        __FUNCTION__ + std::string(": cannot read real data for profile ") + std::to_string(prof_info.tid));
  }

  //for each cct, keep track of the values,metric ids, and thread ids
  for(uint c = 0; c < my_cct_offsets.size() - 1; c++) 
  {
    uint32_t cct_id  = my_cct_offsets[c].first;
    uint64_t cct_off = my_cct_offsets[c].second;
    uint64_t num_val_this_cct = my_cct_offsets[c + 1].second - cct_off;
    char* val_mid_start_this_cct = vminput + (cct_off - first_offset) * (TMS_val_SIZE + TMS_mid_SIZE);


    for(uint i = 0; i < num_val_this_cct; i++){
      //get a pair of val and mid
      hpcrun_metricVal_t val;
      uint16_t mid;
      interpretByte8(val.bits,val_mid_start_this_cct + i * (TMS_val_SIZE + TMS_mid_SIZE));
      interpretByte2(mid,     val_mid_start_this_cct + i * (TMS_val_SIZE + TMS_mid_SIZE) + TMS_val_SIZE);

      
      //store them - if cct_id already exists in cct_data_pairs
      //std::unordered_map<uint32_t,std::vector<DataBlock>>::iterator got = cct_data_pairs.find (cct_id);
      std::vector<CCTDataPair>::iterator got = std::find_if(cct_data_pairs.begin(), cct_data_pairs.end(), 
                       [&cct_id] (const CCTDataPair& cdp) { 
                          return cdp.cctid == cct_id; 
                       });

      if ( got == cct_data_pairs.end() ){
        //this cct doesn't exist in cct_data_paris yet   
        //create a DataBlock for this mid and this cct    
        DataBlock data;
        data.mid = mid;
        std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_tids;
        values_tids.emplace_back(val, prof_info.tid);
        data.values_tids = values_tids;

        //create a vector of DataBlock for this cct
        std::vector<DataBlock> datas;
        datas.emplace_back(data);

        //store it
        CCTDataPair cdp = {cct_id,datas};
        cct_data_pairs.emplace_back(cdp);
      }else{
        //found the cct, try to find if this mid exists
        std::vector<DataBlock>& datas = got->datas; 
        std::vector<DataBlock>::iterator it = std::find_if(datas.begin(), datas.end(), 
                       [&mid] (const DataBlock& d) { 
                          return d.mid == mid; 
                       });

        if(it != datas.end()){ //found mid
          it->values_tids.emplace_back(val, prof_info.tid);
        }else{ 
          //create new DataBlock for this mid
          DataBlock data;
          data.mid = mid;
          std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_tids;
          values_tids.emplace_back(val, prof_info.tid);
          data.values_tids = values_tids;

          //store it to this cct datas
          datas.emplace_back(data);
        }

      }//END of cct_id found in cct_data_pair 
      
    }//END of storing values and thread ids for this cct
  }//END of storing values and thread ids for ALL cct

  assert(my_cct_offsets.size() - 1 <= cct_data_pairs.size());

}

//convert collected data for a group of ccts to appropriate bytes 
void SparseDB::dataPairs2Bytes(std::vector<CCTDataPair>& cct_data_pairs, 
    std::vector<std::pair<uint32_t, uint64_t>>& cct_off, std::vector<uint32_t>& cct_ids,
    std::vector<char>& info_bytes,std::vector<char>& metrics_bytes, int threads)
{
  assert(cct_data_pairs.size() > 0);
  assert(cct_data_pairs.size() <= cct_ids.size());
  assert(cct_off.size() > 0);

  uint64_t first_cct_off =  cct_off[CCTIDX(cct_ids[0])].second; 
  uint info_byte_cnt = 0;
  uint met_byte_cnt  = 0;

  #pragma omp parallel for num_threads(threads) reduction(+:info_byte_cnt, met_byte_cnt)
  for(uint i = 0; i<cct_ids.size(); i++ ){
    //INFO_BYTES
    uint32_t cct_id = cct_ids[i];
    uint64_t num_val = 0;
    uint16_t num_nzmid = 0;
    uint64_t offset = cct_off[CCTIDX(cct_id)].second; 
    //std::unordered_map<uint32_t,std::vector<DataBlock>>::iterator got = cct_data_pairs.find (cct_id);
    std::vector<CCTDataPair>::iterator cdp = std::find_if(cct_data_pairs.begin(), cct_data_pairs.end(), 
                       [&cct_id] (const CCTDataPair& cdp) { 
                          return cdp.cctid == cct_id; 
                       });

    if(cdp != cct_data_pairs.end()){ //cct_data_pairs has the cct_id, only update metric_bytes if it has the cct_id
      std::vector<DataBlock>& datas = cdp->datas;
      //INFO_BYTES
      num_nzmid = datas.size();
      //METRIC_BYTES
      uint64_t this_cct_start_pos = offset - first_cct_off; //position relative to the beginning of metric_bytes

      uint64_t pre_val_tid_pair_size = 0; //keep track of how many val_tid pairs (sum over previous DataBlock for different mids) have been converted
      for(int j = 0; j < num_nzmid; j++){
        DataBlock& d = datas[j];
        //INFO_BYTES
        num_val += d.num_values ;

        //METRIC_BYTES - val_tid_pair
        for(uint k = 0; k < d.values_tids.size(); k++){
          auto& pair = d.values_tids[k];
          convertToByte8(pair.first.bits, metrics_bytes.data() + this_cct_start_pos + pre_val_tid_pair_size + k * CMS_val_tid_pair_SIZE);
          convertToByte4(pair.second,     metrics_bytes.data() + this_cct_start_pos + pre_val_tid_pair_size + k * CMS_val_tid_pair_SIZE + CMS_val_SIZE);
          met_byte_cnt += CMS_val_tid_pair_SIZE;
        }
        pre_val_tid_pair_size = num_val * CMS_val_tid_pair_SIZE;

      }//END OF DataBlock loop

      //METRIC_BYTES - metricID_offset_pair
      uint64_t m_off = 0;
      for(int j =0; j < num_nzmid; j++){   
        convertToByte2(datas[j].mid, metrics_bytes.data() + this_cct_start_pos + num_val * CMS_val_tid_pair_SIZE + j * CMS_m_pair_SIZE);
        convertToByte8(m_off,        metrics_bytes.data() + this_cct_start_pos + num_val * CMS_val_tid_pair_SIZE + j * CMS_m_pair_SIZE + CMS_mid_SIZE);
        m_off += datas[j].num_values;
        met_byte_cnt += CMS_m_pair_SIZE;
      }
      //Add the extra end mark for last metric id
      convertToByte2(LastMidEnd, metrics_bytes.data() + this_cct_start_pos + num_val * CMS_val_tid_pair_SIZE + num_nzmid * CMS_m_pair_SIZE);
      convertToByte8(m_off,      metrics_bytes.data() + this_cct_start_pos + num_val * CMS_val_tid_pair_SIZE + num_nzmid * CMS_m_pair_SIZE + CMS_mid_SIZE);
      met_byte_cnt += CMS_m_pair_SIZE;

    }//END OF if cct_data_pairs has the cct_id

    if(num_nzmid != 0)
      if(offset + num_val * CMS_val_tid_pair_SIZE + (num_nzmid+1) * CMS_m_pair_SIZE !=  cct_off[CCTIDX(cct_id+2)].second )
        exitError("collected cct data (num_val/num_nzmid) were wrong !");
        //util::log::fatal() << "collected cct data (num_val/num_nzmid) were wrong !";


    //ALL cct has a spot in CCT Information section
    uint64_t pre = i * CMS_cct_info_SIZE;
    convertToByte4(cct_id,   info_bytes.data() + pre);
    convertToByte8(num_val,  info_bytes.data() + pre + CMS_cct_id_SIZE);
    convertToByte2(num_nzmid,info_bytes.data() + pre + CMS_cct_id_SIZE + CMS_num_val_SIZE);
    convertToByte8(offset,   info_bytes.data() + pre + CMS_cct_id_SIZE + CMS_num_val_SIZE + CMS_num_nzmid_SIZE);
    info_byte_cnt += CMS_cct_info_SIZE;

  } //END OF CCT LOOP

  if(info_byte_cnt != info_bytes.size())    exitError("the count of info_bytes converted is not as expected");
  //util::log::fatal() << "the count of info_bytes converted is not as expected";
  if(met_byte_cnt  != metrics_bytes.size()) exitError("the count of metrics_bytes converted is not as expected" 
    + std::to_string(met_byte_cnt) + " != " + std::to_string(metrics_bytes.size()));
  //util::log::fatal() << "the count of metrics_bytes converted is not as expected";
}



//read a CCT group's data and write them out
void SparseDB::rwOneCCTgroup(std::vector<uint32_t>& cct_ids, std::vector<ProfileInfo>& prof_info, 
    std::vector<std::pair<uint32_t, uint64_t>>& cct_off, int threads, MPI_File fh, MPI_File ofh)
{
  std::vector<CCTDataPair> cct_data_pairs;

  #pragma omp parallel num_threads(threads) 
  {
    //std::unordered_map<uint32_t,std::vector<DataBlock>> thread_cct_data_pairs;
    std::vector<CCTDataPair> thread_cct_data_pairs;
    //read all profiles for this cct_ids group
    #pragma omp for 
    for(uint i = 0; i < prof_info.size(); i++){
      ProfileInfo pi = prof_info[i];
      readOneProfile(cct_ids, pi, thread_cct_data_pairs, fh);
    }

    #pragma omp critical
    {
      for(CCTDataPair tcdp : thread_cct_data_pairs){
        uint32_t cctid = tcdp.cctid;
        std::vector<CCTDataPair>::iterator gcdp = std::find_if(cct_data_pairs.begin(), cct_data_pairs.end(), 
                       [&cctid] (const CCTDataPair& cdp) { 
                          return cdp.cctid == cctid; 
                       });

        if(gcdp == cct_data_pairs.end()){
          cct_data_pairs.emplace_back(tcdp);
        }else{
          std::vector<DataBlock>& gdatas = gcdp->datas;
          for(DataBlock tdata : tcdp.datas){
            const uint16_t& mid = tdata.mid;
            std::vector<DataBlock>::iterator gdata = std::find_if(gdatas.begin(), gdatas.end(), 
                       [&mid] (const DataBlock& d) { 
                          return d.mid == mid; 
                       });

            if(gdata == gdatas.end()){
              gdatas.emplace_back(tdata);
            }else{
              gdata->values_tids.reserve(gdata->values_tids.size() + tdata.values_tids.size());
              gdata->values_tids.insert(gdata->values_tids.end(), tdata.values_tids.begin(), tdata.values_tids.end());
            }

          }//END of iterating over exiting cct's mid-datas
        }//END of if/else this cct exists 
      } //END of iterating over all cct data pairs this thread has
    } //END of critical region to synchronize cct_data_pairs
    #pragma omp barrier

    //for each cct id sort based on metric id, within each metric id data block, sort based on thread id
    #pragma omp for
    for(uint i = 0; i < cct_data_pairs.size(); i++){
      CCTDataPair& cdp = cct_data_pairs[i];
      for(uint j = 0; j < cdp.datas.size(); j++){
        DataBlock& data = cdp.datas[j];
        std::sort(data.values_tids.begin(), data.values_tids.end(), 
          [](const std::pair<hpcrun_metricVal_t,uint32_t>& lhs, const std::pair<hpcrun_metricVal_t,uint32_t>& rhs) {
            return lhs.second < rhs.second;
          });  
        data.num_values = data.values_tids.size(); //update each mid's num_values
      }
      std::sort(cdp.datas.begin(), cdp.datas.end(), [](const DataBlock& lhs, const DataBlock& rhs) {
          return lhs.mid < rhs.mid;
      });    
    }//END of sorting
    
  } //END of parallel region for reading


  //write for this cct_ids group
  std::vector<char> info_bytes (CMS_cct_info_SIZE * cct_ids.size());
  uint32_t first_cct_id = cct_ids.front();
  uint32_t last_cct_id  = cct_ids.back();
  int metric_bytes_size = cct_off[CCTIDX(last_cct_id) + 1].second - cct_off[CCTIDX(first_cct_id)].second;
  std::vector<char> metrics_bytes (metric_bytes_size);
  dataPairs2Bytes(cct_data_pairs, cct_off, cct_ids, info_bytes, metrics_bytes, threads);

  MPI_Status stat;
  MPI_Offset info_off = CMS_num_cct_SIZE + CCTIDX(first_cct_id) * CMS_cct_info_SIZE;
  std::ostringstream s;
  for(auto cid : cct_ids) s << cid << " ";
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, info_off, info_bytes.data(), info_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Info Bytes for (") + s.str() + ")");

  MPI_Offset metrics_off = cct_off[CCTIDX(first_cct_id)].second;
  SPARSE_exitIfMPIError(MPI_File_write_at(ofh, metrics_off, metrics_bytes.data(), metrics_bytes.size(), MPI_BYTE, &stat),
                  __FUNCTION__ + std::string(": write Metrics Bytes for (") + s.str() + ")");
}

void SparseDB::writeCCTMajor(std::vector<uint64_t>& cct_local_sizes, std::vector<std::set<uint16_t>>& cct_nzmids,
    int ctxcnt, int world_rank, int world_size, int threads)
{
  //Prepare a union cct_nzmids
  unionMids(cct_nzmids,world_rank,world_size, threads/world_size);

  //Get CCT global final offsets for cct_major_sparse.db
  std::vector<std::pair<uint32_t, uint64_t>> cct_off (ctxcnt + 1);
  getCctOffset(cct_local_sizes, cct_nzmids, cct_off, threads/world_size, world_rank);
  //uint64_t last_cct_size = cct_local_sizes.back() * CMS_val_tid_pair_SIZE;
  //if(world_rank == 0) last_cct_size += cct_nzmids.back().size() * CMS_m_pair_SIZE;
  std::vector<uint32_t> my_cct;
  getMyCCTs(cct_off,my_cct, world_size, world_rank);
  updateCctOffset(cct_off, ctxcnt, threads/world_size);


  //Prepare files to read and write, get the list of profiles
  MPI_File thread_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "thread_major_sparse.db").c_str(),
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &thread_major_f);
  MPI_File cct_major_f;
  MPI_File_open(MPI_COMM_WORLD, (dir / "cct_major_sparse.db").c_str(),
                  MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &cct_major_f);
  SPARSE_exitIfMPIError(writeAsByte4(ctxcnt,cct_major_f,0), __FUNCTION__ + std::string(": write the number of ccts"));

  std::vector<ProfileInfo> prof_info;
  readProfileInfo(prof_info,threads/world_size, thread_major_f);

  //For each cct group (< memory limit) this rank is in charge of, read and write
  std::vector<uint32_t> cct_ids;
  size_t cur_size = 0;
  for(uint i =0; i<my_cct.size(); i++){
    size_t cur_cct_size = cct_off[CCTIDX(my_cct[i]) + 1].second - cct_off[CCTIDX(my_cct[i])].second;

    if((cur_size + cur_cct_size) <= pow(10,4)) { //temp 10^4 TODO: user-defined memory limit
      cct_ids.emplace_back(my_cct[i]);
      cur_size += cur_cct_size;
    }else{
      rwOneCCTgroup(cct_ids, prof_info, cct_off, threads/world_size, thread_major_f, cct_major_f);
      cct_ids.clear();
      cct_ids.emplace_back(my_cct[i]);
      cur_size = cur_cct_size;
    }   

    // final cct group
    if((i == my_cct.size() - 1) && (cct_ids.size() != 0)) rwOneCCTgroup(cct_ids, prof_info, cct_off, threads/world_size, thread_major_f, cct_major_f);

  }

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

  std::vector<uint64_t> cct_local_sizes (ctxcnt,0);
  std::set<uint16_t> empty;
  std::vector<std::set<uint16_t>> cct_nzmids(ctxcnt,empty);
  writeThreadMajor(threads,world_rank,world_size, cct_local_sizes,cct_nzmids);
  writeCCTMajor(cct_local_sizes,cct_nzmids, ctxcnt, world_rank, world_size, threads);
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


