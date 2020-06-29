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

#include "lib/profile/sink.hpp"
#include "lib/profile/stdshim/filesystem.hpp"
#include "lib/profile/util/once.hpp"
#include "lib/profile/util/locked_unordered.hpp"

#include "lib/prof-lean/hpcrun-fmt.h"

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
  const int TMS_total_prof_SIZE   = 4;
  const int TMS_tid_SIZE          = 4;
  const int TMS_num_val_SIZE      = 8;
  const int TMS_num_nzcct_SIZE    = 4;
  const int TMS_prof_offset_SIZE  = 8;
  const int TMS_prof_skip_SIZE    = TMS_tid_SIZE + TMS_num_val_SIZE + TMS_num_nzcct_SIZE; 
  const int TMS_prof_info_SIZE    = TMS_tid_SIZE + TMS_num_val_SIZE + TMS_num_nzcct_SIZE + TMS_prof_offset_SIZE;

  const int TMS_cct_id_SIZE       = 4;
  const int TMS_cct_offset_SIZE   = 8;
  const int TMS_cct_pair_SIZE     = TMS_cct_id_SIZE + TMS_cct_offset_SIZE;
  const int TMS_val_SIZE          = 8;
  const int TMS_mid_SIZE          = 2;

  int writeThreadMajor(const int threads, const int world_rank, const int world_size, std::vector<uint64_t>& cct_local_sizes,std::vector<std::set<uint16_t>>& cct_nzmids);

  //***************************************************************************
  // cct_major_sparse.db  - YUMENG
  //***************************************************************************
  const int FIRST_CCT_ID          = 1;
  const uint NEED_NUM_VAL         = -1;

  const int CMS_num_cct_SIZE      = 4;
  const int CMS_cct_id_SIZE       = 4;
  const int CMS_num_val_SIZE      = 8;
  const int CMS_num_nzmid_SIZE    = 2;
  const int CMS_cct_offset_SIZE   = 8;
  const int CMS_cct_info_SIZE     = CMS_cct_id_SIZE + CMS_num_val_SIZE + CMS_num_nzmid_SIZE + CMS_cct_offset_SIZE;

  const int CMS_mid_SIZE          = 2;
  const int CMS_m_offset_SIZE     = 8;
  const int CMS_m_pair_SIZE       = CMS_mid_SIZE + CMS_m_offset_SIZE;
  const int CMS_val_SIZE          = 8;
  const int CMS_tid_SIZE          = 4;
  const int CMS_val_tid_pair_SIZE = CMS_val_SIZE + CMS_tid_SIZE;

  void writeCCTMajor(std::vector<uint64_t>& cct_local_sizes, std::vector<std::set<uint16_t>>& cct_nzmids,
    int ctxcnt, int world_rank, int world_size, int threads);

  //***************************************************************************
  // General  - YUMENG
  //***************************************************************************
  #define CCTIDX(c) ((c-1)/2)
  #define CCTID(c)  (c*2+1)
  
  void merge(int threads, std::size_t ctxcnt);
  template<typename T>
  void exscan(std::vector<T>& data,int threads); 


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
  void exitIfMPIError(int error_code, std::string info);
  int printIfMPIError(int error_code, std::string info);

  #define SPARSE_throwIfMPIError(r) if (r != MPI_SUCCESS) {return r; }

  //***************************************************************************
  // thread_major_sparse.db  - YUMENG
  //***************************************************************************
  void getProfileSizes(std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes, uint64_t& my_size);
  void getTotalNumProfiles(const uint32_t my_num_prof, uint32_t& total_prof);
  void getMyOffset(const uint64_t my_size, uint64_t& my_off, const int rank);
  void getMyProfOffset(std::vector<std::pair<uint32_t, uint64_t>>& prof_offsets,
      const std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes,
      const uint32_t total_prof, const uint64_t my_offset, const int threads);

  void collectCctMajorData(std::vector<uint64_t>& cct_local_sizes, std::vector<std::set<uint16_t>>& cct_nzmids, const std::vector<char>& bytes);
  int writeProfiles(const std::vector<std::pair<uint32_t, uint64_t>>& prof_offsets, 
    std::vector<uint64_t>& cct_local_sizes,std::vector<std::set<uint16_t>>& cct_nzmids,
    const std::vector<std::pair<const hpctoolkit::Thread*, uint64_t>>& profile_sizes,  
    const MPI_File fh, const int threads);


  //***************************************************************************
  // cct_major_sparse.db  - YUMENG
  //***************************************************************************
  struct ProfileInfo{
    uint32_t tid;
    uint64_t num_val;
    uint32_t num_nzcct;
    uint64_t offset;
  };

  //TODO: change names... these are bad...
  struct DataBlock{
    uint16_t mid;
    uint32_t num_values; // can be set at the end, used as offset for mid
    std::vector<std::pair<hpcrun_metricVal_t,uint32_t>> values_tids;
  };

  struct CCTDataPair{
    uint32_t cctid;
    std::vector<DataBlock> datas;
  };

  void unionMids(std::vector<std::set<uint16_t>>& cct_nzmids, int rank, int num_proc, int threads);
  void getCctOffset(std::vector<uint64_t>& cct_sizes, std::vector<std::set<uint16_t>>& cct_nzmids,
    std::vector<std::pair<uint32_t, uint64_t>>& cct_off,int threads, int rank);
  void getMyCCTs(std::vector<std::pair<uint32_t, uint64_t>>& cct_off,
    std::vector<uint32_t>& my_ccts,uint64_t& last_cct_size, uint64_t& total_size, 
    int num_ranks, int rank);
  void updateCctOffset(std::vector<std::pair<uint32_t, uint64_t>>& cct_off,uint64_t& total_size, size_t ctxcnt, int threads);
  
  void readProfileInfo(std::vector<ProfileInfo>& prof_info, int threads, MPI_File fh);
  int readCCToffsets(std::vector<std::pair<uint32_t,uint64_t>>& cct_offsets,
    MPI_File fh,MPI_Offset off);
  int binarySearchCCTid(std::vector<uint32_t>& cct_ids,
    std::vector<std::pair<uint32_t,uint64_t>>& profile_cct_offsets,
    std::vector<std::pair<uint32_t,uint64_t>>& my_cct_offsets);
  int readOneProfile(std::vector<uint32_t>& cct_ids, ProfileInfo& prof_info,
     std::vector<CCTDataPair>& cct_data_pairs,MPI_File fh);
  void dataPairs2Bytes(std::vector<CCTDataPair>& cct_data_pairs, 
    std::vector<std::pair<uint32_t, uint64_t>>& cct_off,std::vector<uint32_t>& cct_ids,
    std::vector<char>& info_bytes,std::vector<char>& metrics_bytes, int threads);
  void rwOneCCTgroup(std::vector<uint32_t>& cct_ids, std::vector<ProfileInfo>& prof_info, 
    std::vector<std::pair<uint32_t, uint64_t>>& cct_off, uint64_t total_size, int threads, MPI_File fh, MPI_File ofh);

};







#endif  // HPCTOOLKIT_PROF2MPI_SPARSE_H
