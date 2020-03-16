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
#include <lib/cuda/DotCFG.hpp>

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

  void configInst(const std::vector<CudaParse::Function *> &functions);

  void configGPURoot(Prof::CCT::ADynNode *gpu_root);

  void blame(FunctionBlamesMap &function_blames);

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
  typedef std::vector<InstructionBlame> InstBlames;

  typedef std::vector<FunctionBlame> FunctionBlames;

  typedef std::priority_queue<BlockBlame> BlockBlameQueue;

  typedef std::map<CudaOptimizer *, double> OptimizerScoreMap;

  typedef std::map<int, std::map<int, std::vector<std::vector<CudaParse::Block *> > > > CCTEdgePathMap;

  struct VMAProperty {
    VMA vma;
    Prof::CCT::ADynNode *prof_node;
    Prof::Struct::Stmt *struct_node;
    CudaParse::InstructionStat *inst;
    CudaParse::Function *function;
    CudaParse::Block *block;
    int latency_lower;
    int latency_upper;
    int latency_issue;

    VMAProperty() : vma(0), prof_node(NULL), struct_node(NULL),
      inst(NULL), function(NULL), block(NULL), latency_lower(0),
      latency_upper(0), latency_issue(0) {}
  };

  typedef std::map<VMA, VMAProperty> VMAPropertyMap;

 private:
  void initCCTDepGraph(int mpi_rank, int thread_id,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void pruneCCTDepGraphOpcode(int mpi_rank, int thread_id,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void pruneCCTDepGraphBarrier(int mpi_rank, int thread_id,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void pruneCCTDepGraphLatency(int mpi_rank, int thread_id,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
    CCTEdgePathMap &cct_edge_path_map);

  void trackReg(int to_vma, int from_vma, int reg,
    CudaParse::Block *to_block, CudaParse::Block *from_block,
    int latency_issue, int latency, std::set<CudaParse::Block *> &visited_blocks,
    std::vector<CudaParse::Block *> &path,
    std::vector<std::vector<CudaParse::Block *>> &paths);

  double computePathNoStall(int mpi_rank, int thread_id, int from_vma, int to_vma,
    std::vector<CudaParse::Block *> &path);
    
  void blameCCTDepGraph(int mpi_rank, int thread_id,
    CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph,
    CCTEdgePathMap &cct_edge_path_map,
    InstBlames &inst_blames);

  void overlayInstBlames(const InstBlames &inst_blames, FunctionBlames &function_blames);

  void selectTopBlockBlames(const FunctionBlames &function_blames, BlockBlameQueue &top_block_blames);

  void rankOptimizers(BlockBlameQueue &top_block_blames, OptimizerScoreMap &optimizer_scores);

  void concatAdvise(const OptimizerScoreMap &optimizer_scores);
  
  // Helper functions
  int demandNodeMetric(int mpi_rank, int thread_id, Prof::CCT::ADynNode *node);

  std::string debugInstOffset(int vma);

  void debugInstDepGraph();

  void debugCCTDepPaths(CCTEdgePathMap &cct_edge_path_map);

  void debugCCTDepGraph(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void debugCCTDepGraphNoPath(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void debugCCTDepGraphStallExec(int mpi_rank, int thread_id, CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);
  
  void debugCCTDepGraphSinglePath(CCTGraph<Prof::CCT::ADynNode *> &cct_dep_graph);

  void debugInstBlames(InstBlames &inst_blames);

 private:
  std::string _inst_metric;
  std::string _stall_metric;
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
  std::map<int, std::string> _function_offset;

  CCTGraph<CudaParse::InstructionStat *> _inst_dep_graph;
  VMAPropertyMap _vma_prop_map;

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
