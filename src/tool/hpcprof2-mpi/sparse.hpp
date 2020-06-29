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

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof/tms-format.h>
#include <lib/prof/cms-format.h>

#include <vector>
#include <mpi.h>

class SparseDB : public hpctoolkit::ProfileSink {
public:
  SparseDB(const hpctoolkit::stdshim::filesystem::path&);
  SparseDB(hpctoolkit::stdshim::filesystem::path&&);
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
    return ExtensionClass::identifier;
  }

  void notifyWavefront(hpctoolkit::DataClass::singleton_t) noexcept override;
  void notifyThreadFinal(const hpctoolkit::Thread::Temporary&) override;

  //***************************************************************************
  // thread_major_sparse.db  - YUMENG
  //***************************************************************************
  void writeThreadMajor(const int threads, 
                        const int world_rank, 
                        const int world_size, 
                        std::vector<uint64_t>& ctx_nzval_cnts,
                        std::vector<std::set<uint16_t>>& ctx_nzmids);

  //***************************************************************************
  // cct_major_sparse.db  - YUMENG
  //***************************************************************************
  void writeCCTMajor(const std::vector<uint64_t>& cct_local_sizes, 
                     std::vector<std::set<uint16_t>>& cct_nzmids,
                     const int ctxcnt, 
                     const int world_rank, 
                     const int world_size, 
                     const int threads);

  //***************************************************************************
  // General  - YUMENG
  //***************************************************************************
  #define CTX_VEC_IDX(c) ((c-1)/2)
  #define CTXID(c)       (c*2+1)
  
  void merge(int threads, std::size_t ctxcnt);
  template<typename T>
  void exscan(std::vector<T>& data,int threads); 
  template <typename T, typename MemberT>
  int struct_member_binary_search(const std::vector<T>& datas, const T target, const MemberT target_type, const int length); 


  //***************************************************************************
  // Work with bytes  - YUMENG
  //***************************************************************************
  int writeAsByte4(uint32_t val, MPI_File fh, MPI_Offset off);
  int writeAsByte8(uint64_t val, MPI_File fh, MPI_Offset off);
  int writeAsByteX(std::vector<char>& val, size_t size, MPI_File fh, MPI_Offset off);
  int readAsByte4(uint32_t& val, MPI_File fh, MPI_Offset off);
  int readAsByte8(uint64_t& val, MPI_File fh, MPI_Offset off);
  void interpretByte2(uint16_t& val, const char *input);
  void interpretByte4(uint32_t& val, const char *input);
  void interpretByte8(uint64_t& val, const char *input);
  void convertToByte2(uint16_t val, char* bytes);
  void convertToByte4(uint32_t val, char* bytes);
  void convertToByte8(uint64_t val, char* bytes);




private:
  hpctoolkit::stdshim::filesystem::path dir;
  void merge0(int, MPI_File&, const std::vector<std::pair<hpctoolkit::ThreadAttributes,
    hpctoolkit::stdshim::filesystem::path>>&);
  void mergeN(int, MPI_File&);

  std::vector<std::reference_wrapper<const hpctoolkit::Context>> contexts;
  unsigned int ctxMaxId;
  hpctoolkit::util::OnceFlag contextPrep;
  void prepContexts() noexcept;

  hpctoolkit::util::locked_unordered_map<const hpctoolkit::Thread*,
    hpctoolkit::stdshim::filesystem::path> outputs;
  std::atomic<std::size_t> outputCnt;

  //***************************************************************************
  // general - YUMENG
  //***************************************************************************
  const int SPARSE_ERR = -1;
  const int SPARSE_OK  =  0;

  void exitMPIError(int error_code, std::string info);
  void exitError(std::string info);

  #define SPARSE_throwIfMPIError(r) if (r != MPI_SUCCESS) {return r; }
  #define SPARSE_exitIfMPIError(r,info) if(r != MPI_SUCCESS) {exitMPIError(r, info);}

  //***************************************************************************
  // thread_major_sparse.db  - YUMENG
  //***************************************************************************

  //---------------------------------------------------------------------------
  // get profile's size and corresponding offsets
  //---------------------------------------------------------------------------
  void getProfileSizes(std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes, 
                       uint64_t& my_size); 

  void getTotalNumProfiles(const uint32_t my_num_prof, 
                           uint32_t& total_num_prof);

  void getMyOffset(const uint64_t my_size, 
                   const int rank, 
                   uint64_t& my_offset);
                   
  void getMyProfOffset(const std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes,
                       const uint32_t total_prof, 
                       const uint64_t my_offset, 
                       const int threads, 
                       std::vector<uint64_t>& prof_offsets);

  //---------------------------------------------------------------------------
  // get profile's real data (bytes)
  //---------------------------------------------------------------------------
  void interpretOneCtxMids(const char* input,
                           const uint64_t ctx_nzval_cnt,
                           std::set<uint16_t>& ctx_nzmids);

  void interpretOneCtxNzvalCnt(const char* input,
                               uint64_t& ctx_nzval_cnt,
                               uint64_t& ctx_idx);

  void interpretOneCtxValCntMids(const char* val_cnt_input, 
                                 const char* mids_input,
                                 std::vector<std::set<uint16_t>>& ctx_nzmids,
                                 std::vector<uint64_t>& ctx_nzval_cnts);                                                 

  void collectCctMajorData(std::vector<char>& bytes,
                           std::vector<uint64_t>& ctx_nzval_cnts, 
                           std::vector<std::set<uint16_t>>& ctx_nzmids);

  //---------------------------------------------------------------------------
  // write profiles 
  //---------------------------------------------------------------------------

  void writeOneProfInfo(const std::vector<char>& info_bytes, 
                        const uint32_t tid,
                        const MPI_File fh);

  void writeOneProfileData(const std::vector<char>& bytes, 
                           const uint64_t offset, 
                           const uint32_t tid,
                           const MPI_File fh);

  void writeOneProfile(const hpctoolkit::Thread* threadp,
                       const MPI_Offset my_prof_offset, 
                       std::vector<uint64_t>& ctx_nzval_cnts,
                       std::vector<std::set<uint16_t>>& ctx_nzmids,
                       MPI_File fh);

  void writeProfiles(const std::vector<uint64_t>& prof_offsets, 
                     const std::vector<std::pair<const hpctoolkit::Thread*,uint64_t>>& profile_sizes,
                     const MPI_File fh, 
                     const int threads,
                     std::vector<uint64_t>& cct_local_sizes,
                     std::vector<std::set<uint16_t>>& ctx_nzmids);


  //***************************************************************************
  // cct_major_sparse.db  - YUMENG
  //***************************************************************************
  //---------------------------------------------------------------------------
  // calculate the offset for each cct node's section in cct_major_sparse.db
  // assign cct nodes to different ranks
  //---------------------------------------------------------------------------
  void unionMids(std::vector<std::set<uint16_t>>& ctx_nzmids, 
                 const int rank, 
                 const int num_proc, 
                 const int threads);

  void getLocalCtxOffset(const std::vector<uint64_t>& ctx_val_cnts, 
                         const std::vector<std::set<uint16_t>>& ctx_nzmids,
                         const int threads, 
                         const int rank,
                         std::vector<uint64_t>& ctx_off);
  
  void getGlobalCtxOffset(const std::vector<uint64_t>& local_ctx_off,
                          std::vector<uint64_t>& global_ctx_off);
  
  void getCtxOffset(const std::vector<uint64_t>& ctx_val_cnts, 
                    const std::vector<std::set<uint16_t>>& ctx_nzmids,
                    const int threads, 
                    const int rank,
                    std::vector<uint64_t>& ctx_off);
                    
  void getMyCtxs(const std::vector<uint64_t>& ctx_off,
                 const int num_ranks, 
                 const int rank,
                 std::vector<uint32_t>& my_ctxs);
                 
  void updateCtxOffset(const size_t ctxcnt, 
                       const int threads, 
                       std::vector<uint64_t>& ctx_off);
  

  //---------------------------------------------------------------------------
  // get a list of profile info
  //---------------------------------------------------------------------------
  void interpretOneProfInfo(const char *input,
                            tms_profile_info_t& pi);

  void readProfileInfo(const int threads, 
                       const MPI_File fh,
                       std::vector<tms_profile_info_t>& prof_info);


  //---------------------------------------------------------------------------
  // read and interpret one profie - CtxIdIdxPairs
  //---------------------------------------------------------------------------
  const int SPARSE_NOT_FOUND = -1;
  const int SPARSE_END       = -2;

  struct TMS_CtxIdIdxPair{
    uint32_t ctx_id;  // = cct node id
    uint64_t ctx_idx; //starting location of the context's values in value array
  };

  void interpretOneCtxIdIdxPair(const char *input,
                                TMS_CtxIdIdxPair& ctx_pair);
                        
  int readCtxIdIdxPairs(const MPI_File fh, 
                        const MPI_Offset off, 
                        std::vector<TMS_CtxIdIdxPair>& ctx_id_idx_pairs);

  int findOneCtxIdIdxPair(const uint32_t target_ctx_id,
                          const std::vector<TMS_CtxIdIdxPair>& profile_ctx_pairs,
                          const uint length, 
                          const bool found,
                          const int found_ctx_idx, 
                          std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs);

  void findCtxIdIdxPairs(const std::vector<uint32_t>& ctx_ids,
                         const std::vector<TMS_CtxIdIdxPair>& profile_ctx_pairs,
                         std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs);


  int getMyCtxIdIdxPairs(const tms_profile_info_t& prof_info,
                         const std::vector<uint32_t>& ctx_ids,
                         const MPI_File fh,
                         std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs);


  //---------------------------------------------------------------------------
  // read and interpret one profie - ValMid
  //---------------------------------------------------------------------------
  struct MetricValBlock{
    uint16_t mid;
    uint32_t num_values; // can be set at the end, used as idx for mid
    std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_tids;
  };

  struct CtxMetricBlock{
    uint32_t ctx_id;
    std::vector<MetricValBlock> metrics;
  };

  void readValMidsBytes(const std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs,
                        const tms_profile_info_t& prof_info,
                        const MPI_File fh,
                        std::vector<char>& bytes);

  MetricValBlock createNewMetricValBlock(const hpcrun_metricVal_t val, 
                                         const uint16_t mid,
                                         const uint32_t tid);


  CtxMetricBlock createNewCtxMetricBlock(const hpcrun_metricVal_t val, 
                                         const uint16_t mid,
                                         const uint32_t tid,
                                         const uint32_t ctx_id);


  void insertValMidPair2OneCtxMetBlock(const hpcrun_metricVal_t val, 
                                       const uint16_t mid,
                                       const uint32_t tid,
                                       CtxMetricBlock& cmb);

  void insertValMidCtxId2CtxMetBlocks(const hpcrun_metricVal_t val, 
                                      const uint16_t mid,
                                      const uint32_t tid,
                                      const uint32_t ctx_id,
                                      std::vector<CtxMetricBlock>& ctx_met_blocks);

  void interpretOneValMidPair(const char *input,
                              hpcrun_metricVal_t& val,
                              uint16_t& mid);

  void interpretValMidsBytes(char *vminput,
                             const uint32_t tid,
                             const std::vector<TMS_CtxIdIdxPair>& my_ctx_pairs,
                             std::vector<CtxMetricBlock>& ctx_met_blocks);

  
  void readOneProfile(const std::vector<uint32_t>& ctx_ids, 
                      const tms_profile_info_t& prof_info,
                      const MPI_File fh,
                      std::vector<CtxMetricBlock>& ctx_met_blocks);


  //---------------------------------------------------------------------------
  // read and interpret ALL profies 
  //---------------------------------------------------------------------------

  void mergeCtxMetBlocks(const CtxMetricBlock& source,
                         CtxMetricBlock& dest);

  void mergeOneCtxAllThreadBlocks(const std::vector<std::vector<CtxMetricBlock> *>& threads_ctx_met_blocks,
                                  CtxMetricBlock& dest);

  void sortCtxMetBlocks(std::vector<CtxMetricBlock>& ctx_met_blocks);

  void readProfiles(const std::vector<uint32_t>& ctx_ids, 
                    const std::vector<tms_profile_info_t>& prof_info,
                    int threads,
                    MPI_File fh,
                    std::vector<CtxMetricBlock>& ctx_met_blocks);                    


  //---------------------------------------------------------------------------
  // convert ctx_met_blocks to correct bytes for writing
  //---------------------------------------------------------------------------
  int convertOneMetricValBlock(const MetricValBlock& mvb,                                        
                               char *bytes);
                                
  int convertCtxMetrics(const std::vector<MetricValBlock>& metrics,                                        
                        char *bytes);

  int buildCtxMetIdIdxPairsBytes(const std::vector<MetricValBlock>& metrics,                                        
                                 char *bytes);

  int convertOneCtxMetricBlock(const CtxMetricBlock& cmb,                                        
                               char *bytes,
                               uint16_t& num_nzmids,
                               uint64_t& num_vals);

  int convertOneCtxInfo(const cms_ctx_info_t& ctx_info,                                        
                        char *bytes);

  void convertOneCtx(const uint32_t ctx_id, 
                     const std::vector<uint64_t>& ctx_off,    
                     const std::vector<CtxMetricBlock>& ctx_met_blocks, 
                     const uint64_t first_ctx_off,
                     uint& met_byte_cnt,
                     uint& info_byte_cnt, 
                     char* met_bytes,                                 
                     char* info_bytes);

  void ctxBlocks2Bytes(const std::vector<CtxMetricBlock>& ctx_met_blocks, 
                       const std::vector<uint64_t>& ctx_off, 
                       const std::vector<uint32_t>& ctx_ids,
                       int threads,
                       std::vector<char>& info_bytes,
                       std::vector<char>& metrics_bytes);

  void writeCtxGroup(const std::vector<uint32_t>& ctx_ids,
                             const std::vector<uint64_t>& ctx_off,
                             const std::vector<CtxMetricBlock>& ctx_met_blocks,
                             const int threads,
                             MPI_File ofh);

  //---------------------------------------------------------------------------
  // read and write for all contexts in this rank's list
  //---------------------------------------------------------------------------

  void rwOneCtxGroup(const std::vector<uint32_t>& ctx_ids, 
                     const std::vector<tms_profile_info_t>& prof_info, 
                     const std::vector<uint64_t>& ctx_off, 
                     const int threads, 
                     const MPI_File fh, 
                     MPI_File ofh);


  void rwAllCtxGroup(const std::vector<uint32_t>& my_ctxs, 
                     const std::vector<tms_profile_info_t>& prof_info, 
                     const std::vector<uint64_t>& ctx_off, 
                     const int threads, 
                     const MPI_File fh, 
                     MPI_File ofh);

};







#endif  // HPCTOOLKIT_PROF2MPI_SPARSE_H
