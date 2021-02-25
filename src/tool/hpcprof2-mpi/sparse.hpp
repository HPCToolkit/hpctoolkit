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

#define HPCTOOLKIT_PROF2MPI_SPARSE_H
#ifdef HPCTOOLKIT_PROF2MPI_SPARSE_H

#include <lib/profile/sink.hpp>
#include <lib/profile/stdshim/filesystem.hpp>
#include <lib/profile/util/once.hpp>
#include <lib/profile/util/locked_unordered.hpp>
#include <lib/profile/util/file.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/id-tuple.h>
#include <lib/prof/pms-format.h>
#include <lib/prof/cms-format.h>

#include <vector>
#include <mpi.h>

class SparseDB : public hpctoolkit::ProfileSink {
public:
  SparseDB(const hpctoolkit::stdshim::filesystem::path&, int threads);
  SparseDB(hpctoolkit::stdshim::filesystem::path&&, int threads);
  ~SparseDB() = default;

  void write() override;

  hpctoolkit::DataClass accepts() const noexcept override {
    using namespace hpctoolkit;
    return DataClass::threads | DataClass::contexts | DataClass::metrics;
  }

  hpctoolkit::DataClass wavefronts() const noexcept override {
    using namespace hpctoolkit;
    return DataClass::contexts;
  }

  hpctoolkit::ExtensionClass requires() const noexcept override {
    using namespace hpctoolkit;
    return ExtensionClass::identifier + ExtensionClass::mscopeIdentifiers;
  }

  void notifyWavefront(hpctoolkit::DataClass) noexcept override;
  void notifyThreadFinal(const hpctoolkit::Thread::Temporary&) override;

  void writeProfileMajor(const int threads, const int world_rank, const int world_size, 
                         std::vector<uint64_t>& ctx_nzval_cnts,
                         std::vector<std::set<uint16_t>>& ctx_nzmids);

  void writeCCTMajor(const std::vector<uint64_t>& cct_local_sizes, 
                     std::vector<std::set<uint16_t>>& cct_nzmids,
                     const int world_rank, const int world_size,const int threads);
  
  void merge(int threads, bool debug);

  //local exscan over a vector of T, value after exscan will be stored in the original vector
  template<typename T>
  void exscan(std::vector<T>& data,int threads); 

  //binary search over a vector of T, unlike std::binary_search, which only returns true/false, 
  //this returns the idx of found one, SPARSE_ERR as NOT FOUND
  template <typename T, typename MemberT>
  int struct_member_binary_search(const std::vector<T>& datas, const T target, const MemberT target_type, const int length); 
  
  //create a MPI pair type 
  template<class A, class B>
  MPI_Datatype createPairType(MPI_Datatype aty, MPI_Datatype bty);

  //***************************************************************************
  // Work with bytes  
  //***************************************************************************
  void writeAsByte4(uint32_t val, hpctoolkit::util::File::Instance& fh, MPI_Offset off);
  void writeAsByte8(uint64_t val, hpctoolkit::util::File::Instance& fh, MPI_Offset off);
  uint32_t readAsByte4(hpctoolkit::util::File::Instance& fh, MPI_Offset off);
  uint64_t readAsByte8(hpctoolkit::util::File::Instance& fh, MPI_Offset off);
  uint16_t interpretByte2(const char *input);
  uint32_t interpretByte4(const char *input);
  uint64_t interpretByte8(const char *input);
  std::vector<char> convertToByte2(uint16_t val);
  std::vector<char> convertToByte4(uint32_t val);
  std::vector<char> convertToByte8(uint64_t val);


private:
  hpctoolkit::stdshim::filesystem::path dir;
  std::size_t ctxcnt;

  std::vector<std::reference_wrapper<const hpctoolkit::Context>> contexts;
  unsigned int ctxMaxId;
  hpctoolkit::util::Once contextWavefront;

  hpctoolkit::util::locked_unordered_map<const hpctoolkit::Thread*,
    hpctoolkit::stdshim::filesystem::path> outputs;

  std::mutex outputs_l;
  std::vector<pms_profile_info_t> prof_infos;
  size_t cur_position; //next available starting position for a profile in buffer
  std::vector<std::vector<char>> obuffers; //profiles in binary form waiting to be written
  int cur_obuf_idx;
  std::optional<hpctoolkit::util::File> pmf;
  uint64_t fpos;
  MPI_Win win;

  std::atomic<std::size_t> outputCnt;
  int team_size;
  hpctoolkit::stdshim::filesystem::path summaryOut;
  std::vector<std::pair<const uint32_t,
    std::string>> sparseInputs;

  bool keepTemps;

  //***************************************************************************
  // general 
  //***************************************************************************
  #define CTX_VEC_IDX(c) c
  #define CTXID(c)       c
  #define MULTIPLE_8(v) ((v + 7) & ~7)


  //***************************************************************************
  // profile.db
  //***************************************************************************
  #define not_assigned (uint)-1
  #define RANK_SPOT    (uint16_t)65535 //indicate the IntPair records rank number not tuples
  #define IDTUPLE_SUMMARY_LENGTH        1
  #define IDTUPLE_SUMMARY_PROF_INFO_IDX 0
  #define IDTUPLE_SUMMARY_IDX           0 //kind 0 idx 0

  uint64_t id_tuples_sec_size;
  uint64_t prof_info_sec_size;
  uint64_t id_tuples_sec_ptr;
  uint64_t prof_info_sec_ptr;

  std::vector<uint64_t> profile_sizes;
  std::vector<uint64_t> prof_offsets;

  std::vector<std::pair<uint32_t, uint64_t>> rank_idx_ptr_pairs;
  std::vector<uint64_t> id_tuple_ptrs;
  uint32_t min_prof_info_idx;
  std::vector<std::vector<uint32_t>> buffered_prof_idxs;


  void assignSparseInputs(int world_rank);

  uint32_t getTotalNumProfiles(const uint32_t my_num_prof);

  //---------------------------------------------------------------------------
  // header
  //---------------------------------------------------------------------------
  void writePMSHdr(const uint32_t total_num_prof, const hpctoolkit::util::File& fh);

  //---------------------------------------------------------------------------
  // profile offsets
  //---------------------------------------------------------------------------
  // iterate through rank's profile list, assign profile_sizes
  // return total size of rank's profiles 
  uint64_t getProfileSizes();

  // get the offset for this rank's start in profile.db
  uint64_t getMyOffset(const uint64_t my_size, const int rank);
                   
  // get the final global offsets for each profile in this rank
  void getMyProfOffset(const uint32_t total_prof, const uint64_t my_offset, 
                       const int threads);

  //work on profile_sizes and prof_offsets (two private variables) used later for writing profiles
  void workProfSizesOffsets(const int world_rank,  const int total_prof, const int threads);

  //---------------------------------------------------------------------------
  // profile id tuples 
  //---------------------------------------------------------------------------
  std::vector<std::pair<uint16_t, uint64_t>> getMyIdTuplesPairs();

  std::vector<pms_id_tuple_t> intPairs2Tuples(const std::vector<std::pair<uint16_t, uint64_t>>& all_pairs);

  //rank 0 gather all the tuples as pairs format from other ranks
  std::vector<std::pair<uint16_t, uint64_t>> gatherIdTuplesPairs(const int world_rank, const int world_size,
                                                                 const int threads, MPI_Datatype IntPairType,
                                                                 const std::vector<std::pair<uint16_t, uint64_t>>& rank_pairs);
  
  //rank 0 scatter the calculated id_tuple ptrs and prof_info_idx of each profile back to their coming ranks
  void scatterIdxPtrs(const std::vector<std::pair<uint32_t, uint64_t>>& idx_ptr_buffer,
                      const size_t num_prof, const int world_size, const int world_rank,
                      const int threads);

  void sortIdTuples(std::vector<pms_id_tuple_t>& all_tuples);

  //sort a vector of id_tuples based on prof_info_idx
  void sortIdTuplesOnProfInfoIdx(std::vector<pms_id_tuple_t>& all_tuples);


  void fillIdxPtrBuffer(std::vector<pms_id_tuple_t>& all_tuples,
                        std::vector<std::pair<uint32_t, uint64_t>>& buffer,
                        const int threads);

  void freeIdTuples(std::vector<pms_id_tuple_t>& all_tuples, const int threads);

  std::vector<char> convertTuple2Bytes(const pms_id_tuple_t& tuple);

  void writeAllIdTuples(const std::vector<pms_id_tuple_t>& all_tuples,
                        const hpctoolkit::util::File& fh);

  //all work related to IdTuples Section, 
  void workIdTuplesSection1(const int total_num_prof, const hpctoolkit::util::File& pmf);

  //other sections only need the vector of prof_info_idx and id_tuple_ptr pairs
  void workIdTuplesSection(const int world_rank, const int world_size,
                           const int threads, const int num_prof,
                           const hpctoolkit::util::File& fh);


  //---------------------------------------------------------------------------
  // get profile's real data (bytes)
  //---------------------------------------------------------------------------
  #define mode_reg_thr 0
  #define mode_wrt_nroot 1
  #define mode_wrt_root 2

  std::vector<char> profBytes(hpcrun_fmt_sparse_metrics_t* sm);

  uint64_t writeProf(const std::vector<char>& prof_bytes, uint32_t prof_info_idx, int mode); // return relative offset

  uint64_t filePosFetchOp(uint64_t val);

  void updateCtxMids(const char* input, const uint64_t ctx_nzval_cnt, std::set<uint16_t>& ctx_nzmids);

  void interpretOneCtxValCntMids(const char* val_cnt_input, 
                                 const char* mids_input,
                                 std::vector<std::set<uint16_t>>& ctx_nzmids,
                                 std::vector<uint64_t>& ctx_nzval_cnts);                                                 

  void collectCctMajorData(const uint32_t prof_info_idx,
                           std::vector<char>& bytes,
                           std::vector<uint64_t>& ctx_nzval_cnts, 
                           std::vector<std::set<uint16_t>>& ctx_nzmids);

  //---------------------------------------------------------------------------
  // write profiles 
  //---------------------------------------------------------------------------
  void writeProfInfos();

  std::vector<char> profInfoBytes(const std::vector<char>& partial_info_bytes, 
                                  const uint64_t id_tuple_ptr, const uint64_t metadata_ptr,
                                  const uint64_t spare_one_ptr, const uint64_t spare_two_ptr,
                                  const uint64_t prof_offset);
                                      
  void writeOneProfile(const std::pair<uint32_t, std::string>& tupleFn,
                       const MPI_Offset my_prof_offset, 
                       const std::pair<uint32_t,uint64_t>& prof_idx_tuple_ptr_pair,
                       std::vector<uint64_t>& ctx_nzval_cnts,
                       std::vector<std::set<uint16_t>>& ctx_nzmids,
                       hpctoolkit::util::File::Instance& fh);

  void writeProfiles(const hpctoolkit::util::File& fh, const int threads,
                     std::vector<uint64_t>& cct_local_sizes,
                     std::vector<std::set<uint16_t>>& ctx_nzmids);


  //***************************************************************************
  // cct.db 
  //***************************************************************************
  #define SPARSE_NOT_FOUND -1
  #define SPARSE_END       -2


  struct PMS_CtxIdIdxPair{
    uint32_t ctx_id;  // = cct node id
    uint64_t ctx_idx; //starting location of the context's values in value array
  };

  struct MetricValBlock{
    uint16_t mid;
    uint32_t num_values; // can be set at the end, used as idx for mid
    std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_prof_idxs;
  };

  struct CtxMetricBlock{
    uint32_t ctx_id;
    std::map<uint16_t, MetricValBlock> metrics;
  };

  //---------------------------------------------------------------------------
  // header
  //---------------------------------------------------------------------------
  void writeCMSHdr(hpctoolkit::util::File::Instance& cct_major_f);

  //---------------------------------------------------------------------------
  //  ctx info
  //---------------------------------------------------------------------------
  std::vector<char> ctxInfoBytes(const cms_ctx_info_t& ctx_info);

  void writeCtxInfoSec(const std::vector<std::set<uint16_t>>& ctx_nzmids,
                       const std::vector<uint64_t>& ctx_off,
                       hpctoolkit::util::File::Instance& ofh);

  //---------------------------------------------------------------------------
  // ctx offsets
  //---------------------------------------------------------------------------
  // union ctx_nzmids from all ranks to root, in order to avoid double counting for offset calculation
  void unionMids(std::vector<std::set<uint16_t>>& ctx_nzmids, 
                 const int rank, const int num_proc, const int threads);

  std::vector<uint64_t> ctxOffsets(const std::vector<uint64_t>& ctx_val_cnts, 
                                   const std::vector<std::set<uint16_t>>& ctx_nzmids,
                                   const int threads, const int rank);
                    
  std::vector<uint32_t> myCtxs(const std::vector<uint64_t>& ctx_off,
                               const int num_ranks,const int rank);
                 
  void updateCtxOffsets(const int threads,std::vector<uint64_t>& ctx_off);

  //---------------------------------------------------------------------------
  // get a list of profile info
  //---------------------------------------------------------------------------
  pms_profile_info_t profInfo(const char *input);

  std::vector<pms_profile_info_t> profInfoList(const int threads,const hpctoolkit::util::File& fh);


  //---------------------------------------------------------------------------
  // get all profiles' CtxIdIdxPairs
  //---------------------------------------------------------------------------
  //get one ctx_id & ctx_idx pair based on input
  PMS_CtxIdIdxPair ctxIdIdxPair(const char *input);

  //get thd ctx_id & ctx_idx pairs for a profile represented by pi                      
  std::vector<PMS_CtxIdIdxPair> ctxIdIdxPairs(hpctoolkit::util::File::Instance& fh, 
                                              const pms_profile_info_t pi);

  std::vector<std::vector<PMS_CtxIdIdxPair>> 
  allProfileCtxIdIdxPairs(const hpctoolkit::util::File& fh, const int threads,
                          const std::vector<pms_profile_info_t>& prof_info);      


  //---------------------------------------------------------------------------
  // read/extract profiles data - my_ctx_id_idx_pairs and my val&mid bytes
  //---------------------------------------------------------------------------
  //in a vector of PMS_CtxIdIdxPair, find one with target context id
  //output: found idx / SPARSE_END(already end of the vector, not found) / SPARSE_NOT_FOUND
  int findOneCtxIdIdxPair(const uint32_t target_ctx_id,
                          const std::vector<PMS_CtxIdIdxPair>& profile_ctx_pairs,
                          const uint length, const int round, const int found_ctx_idx,
                          std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs);

  std::vector<std::pair<uint32_t, uint64_t>> 
  myCtxPairs(const std::vector<uint32_t>& ctx_ids, const std::vector<PMS_CtxIdIdxPair>& profile_ctx_pairs);

  std::vector<char> valMidsBytes(std::vector<std::pair<uint32_t, uint64_t>>& my_ctx_pairs,
                                 const uint64_t& off, hpctoolkit::util::File::Instance& fh);

  //read all the profiles and convert data to appropriate bytes for a group of contexts
  std::vector<std::pair<std::vector<std::pair<uint32_t,uint64_t>>, std::vector<char>>>
  profilesData(const std::vector<uint32_t>& ctx_ids,const std::vector<pms_profile_info_t>& prof_info_list,
               int threads,const std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
               const hpctoolkit::util::File& fh);                               


  //---------------------------------------------------------------------------
  // interpret the data bytes and convert to CtxMetricBlock
  //---------------------------------------------------------------------------
  //create and return a new MetricValBlock
  MetricValBlock metValBloc(const hpcrun_metricVal_t val,const uint16_t mid, const uint32_t prof_idx);


  void updateCtxMetBloc(const hpcrun_metricVal_t val, const uint16_t mid,
                        const uint32_t prof_idx, CtxMetricBlock& cmb);


  //interpret val_mids bytes for one ctx
  void interpretValMidsBytes(char *vminput,const uint32_t prof_idx,
                             const std::pair<uint32_t,uint64_t>& ctx_pair,
                             const uint64_t next_ctx_idx,const uint64_t first_ctx_idx,
                             CtxMetricBlock& cmb);                  


  //---------------------------------------------------------------------------
  // convert ctx_met_blocks to correct bytes for writing
  //---------------------------------------------------------------------------
  //return bytes of one MetricValBlock converted
  std::vector<char> mvbBytes(const MetricValBlock& mvb);

  //return bytes of ALL MetricValBlock of one CtxMetricBlock converted                              
  std::vector<char> mvbsBytes(std::map<uint16_t, MetricValBlock>& metrics);

  //build metric id and idx pairs for one context as bytes 
  std::vector<char> metIdIdxPairsBytes(const std::map<uint16_t, MetricValBlock>& metrics);

  //return bytes of one CtxMetricBlock
  std::vector<char> cmbBytes(const CtxMetricBlock& cmb, const std::vector<uint64_t>& ctx_off, 
                             const uint32_t& ctx_id);

  //---------------------------------------------------------------------------
  // read and write for all contexts in this rank's list
  //---------------------------------------------------------------------------

  void writeOneCtx(const uint32_t& ctx_id, const std::vector<uint64_t>& ctx_off,
                   const CtxMetricBlock& cmb,hpctoolkit::util::File::Instance& ofh);

  //read a context group's data and write them out
  void rwOneCtxGroup(const std::vector<uint32_t>& ctx_ids, 
                     const std::vector<pms_profile_info_t>& prof_info, 
                     const std::vector<uint64_t>& ctx_off, 
                     const int threads, 
                     const std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                     const hpctoolkit::util::File& fh,
                     const hpctoolkit::util::File& ofh);

  //read ALL context groups' data and write them out
  void rwAllCtxGroup(const std::vector<uint32_t>& my_ctxs, 
                     const std::vector<pms_profile_info_t>& prof_info, 
                     const std::vector<uint64_t>& ctx_off, 
                     const int threads,
                     const std::vector<std::vector<PMS_CtxIdIdxPair>>& all_prof_ctx_pairs,
                     const hpctoolkit::util::File& fh,
                     const hpctoolkit::util::File& ofh);

};


#endif  // HPCTOOLKIT_PROF2MPI_SPARSE_H
