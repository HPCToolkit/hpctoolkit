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
// Copyright ((c)) 2002-2018, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#ifndef Analysis_CallPath_CallPath_CudaAdvisor_hpp 
#define Analysis_CallPath_CallPath_CudaAdvisor_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>
#include <map>
#include <queue>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <lib/cuda/AnalyzeInstruction.hpp>

#include "CallPath-CudaOptimizer.hpp"
#include "CCTGraph.hpp"
#include "MetricNameProfMap.hpp"
#include "GPUArchitecture.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {


class CudaAdvisor {
 public:
  explicit CudaAdvisor(Prof::CallPath::Profile *prof, MetricNameProfMap *metric_name_prof_map) :
    _prof(prof), _metric_name_prof_map(metric_name_prof_map) {}

  void init();

  void config(Prof::CCT::ADynNode *gpu_root);

  void blame(const std::vector<CudaParse::Function *> &functions, FunctionBlamesMap &function_blames);

  void advise(const FunctionBlamesMap &function_blames);

  void save(const std::string &file_name);
 
  ~CudaAdvisor() {
    for (auto *optimizer : _intra_warp_optimizers) {
      delete optimizer;
    }
    for (auto *optimizer : _inter_warp_optimizers) {
      delete optimizer;
    }
    delete _arch;
  }

 private:
  typedef std::map<VMA, Prof::CCT::ADynNode *> VMAProfMap;

  typedef std::map<VMA, CudaParse::InstructionStat *> VMAInstMap;

  typedef std::vector<InstructionBlame> InstBlames;

  typedef std::vector<FunctionBlame> FunctionBlames;

  typedef std::priority_queue<BlockBlame> BlockBlameQueue;

  typedef std::map<CudaOptimizer *, double> OptimizerScoreMap;

  typedef std::map<CudaParse::InstructionStat *, CudaParse::InstructionStat *> InstPairs;

 private:
  void constructVMAProfMap(VMAProfMap &vma_prof_map);

  void constructVMAInstMap(const std::vector<CudaParse::Function *> &functions,
    VMAInstMap &vma_inst_map);

  void constructVMAStructMap(VMAStructMap &vma_struct_map);

  void initInstDepGraph(const std::vector<CudaParse::Function *> &functions,
    const VMAInstMap &vma_inst_map, CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph,
    std::map<VMA, int> &vma_latency_lower, std::map<VMA, int> &vma_latency_upper,
    std::map<VMA, int> &vma_latency_throughput);

  void distInstDepGraph(
    const std::vector<CudaParse::Function *> &functions,
    const std::map<VMA, int> &vma_latency_lower,
    const std::map<VMA, int> &vma_latency_upper,
    const std::map<VMA, int> &vma_latency_throughput,
    CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph,
    std::map<VMA, int> &vma_min_dist, std::map<VMA, int> &vma_max_dist);

  void propagateCCTDepGraph(int mpi_rank, int thread_id,
    const VMAInstMap &vma_inst_map, VMAProfMap &vma_prof_map, 
    CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void pruneCCTDepGraph(int mpi_rank, int thread_id,
    const VMAInstMap &vma_inst_map, const VMAProfMap &vma_prof_map,
    const std::map<VMA, int> &vma_min_dist, const std::map<VMA, int> &vma_max_dist,
    const std::map<VMA, int> &vma_latency_lower, const std::map<VMA, int> &vma_latency_upper, 
    const std::map<VMA, int> &vma_latency_throughput, 
    CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);
    
  void blameCCTDepGraph(int mpi_rank, int thread_id,
    const VMAInstMap &vma_inst_map,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
    InstBlames &inst_blames);

  void overlayInstBlames(const std::vector<CudaParse::Function *> &functions, const InstBlames &inst_blames,
    FunctionBlames &function_blames);

  void selectTopBlockBlames(const FunctionBlames &function_blames, BlockBlameQueue &top_block_blames);

  void rankOptimizers(const VMAStructMap &vma_struct_map,
    BlockBlameQueue &top_block_blames, OptimizerScoreMap &optimizer_scores);

  void concatAdvise(const OptimizerScoreMap &optimizer_scores);
  
  // Helper functions
  int demandNodeMetric(int mpi_rank, int thread_id, Prof::CCT::ADynNode *node);

  void debugInstDepGraph(CCTGraph<CudaParse::InstructionStat *> &inst_dep_graph);

  void debugCCTDepGraph(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void debugCCTDepGraphSinglePath(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void debugInstBlames(InstBlames &inst_blames);

 private:
  std::string _inst_metric;
  std::string _issue_metric;

  std::string _invalid_stall_metric;
  std::string _tex_stall_metric;
  std::string _ifetch_stall_metric;
  std::string _pipe_bsy_stall_metric;
  std::string _mem_thr_stall_metric;
  std::string _nosel_stall_metric;
  std::string _other_stall_metric;
  std::string _sleep_stall_metric;
  std::string _cmem_stall_metric;

  std::string _exec_dep_stall_metric;
  std::string _mem_dep_stall_metric;
  std::string _sync_stall_metric;

  std::set<std::string> _inst_stall_metrics;
  std::set<std::string> _dep_stall_metrics;

  Prof::CallPath::Profile *_prof;
  MetricNameProfMap *_metric_name_prof_map;

  Prof::CCT::ADynNode *_gpu_root;

  std::string _cache;

  std::vector<CudaOptimizer *> _intra_warp_optimizers;
  std::vector<CudaOptimizer *> _inter_warp_optimizers;

  CudaOptimizer *_loop_unroll_optimizer;
  CudaOptimizer *_memory_layout_optimizer;
  CudaOptimizer *_strength_reduction_optimizer;
  CudaOptimizer *_adjust_threads_optimizer;
  CudaOptimizer *_adjust_registers_optimizer;

  GPUArchitecture *_arch;
 
 private:
  const int _top_block_blames = 5;
  const int _top_optimizers = 5;
};


}  // CallPath

}  // Analysis

#endif  // Analysis_CallPath_CallPath_CudaAdvisor_hpp
