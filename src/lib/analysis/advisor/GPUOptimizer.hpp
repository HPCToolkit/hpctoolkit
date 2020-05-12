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


#ifndef Analysis_Advisor_GPUOptimizer_hpp 
#define Analysis_Advisor_GPUOptimizer_hpp

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <lib/cuda/DotCFG.hpp>
#include <lib/cuda/AnalyzeInstruction.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {


struct KernelStats {
  int shared_memory;
  int registers;
  int blocks;
  double occupancy;
  double sm_efficiency;

  KernelStats() {}
};


struct InstructionBlame {
  // Use InstructionStat directly, so that in advisor::advise, we do not need a vma_inst_map again
  CudaParse::InstructionStat *src, *dst;
  Prof::CCT::ANode *src_node, *dst_node;
  int metric_id;
  double blame;

  InstructionBlame(
    CudaParse::InstructionStat *src, CudaParse::InstructionStat *dst,
    Prof::CCT::ANode *src_node, Prof::CCT::ANode *dst_node,
    int metric_id, double blame) : src(src), dst(dst),
    src_node(src_node), dst_node(dst_node),
    metric_id(metric_id), blame(blame) {}
  InstructionBlame() {}

  bool operator < (const InstructionBlame &other) const {
    return this->src->pc < other.src->pc;
  }
};


struct BlockBlame {
  int id;
  int function_index;
  CudaParse::Block *block;
  std::vector<InstructionBlame> inst_blames;
  std::map<int, double> blames;
  double blame;

  BlockBlame(int id, int function_index, CudaParse::Block *block) :
    id(id), function_index(function_index), block(block) {}
  BlockBlame() : BlockBlame(0, 0, NULL) {}

  bool operator < (const BlockBlame &other) const {
    return this->blame < other.blame;
  }
};


struct FunctionBlame {
  int index;
  CudaParse::Function *function;
  std::map<int, BlockBlame> block_blames;
  std::map<int, double> blames;
  double blame;

  FunctionBlame(int index, CudaParse::Function *function) :
    index(index), function(function) {}
  FunctionBlame() : FunctionBlame(0, NULL) {}
};


typedef std::vector<InstructionBlame> InstBlames;

typedef std::map<int, FunctionBlame> FunctionBlames;

typedef std::map<Prof::CCT::ANode *, FunctionBlames> CCTBlames;

typedef std::map<int, std::map<int, CCTBlames>> TotalBlames;


#define FORALL_OPTIMIZER_TYPES(macro) \
  macro(REGISTER_INCREASE, GPURegisterIncreaseOptimizer, 0) \
  macro(REGISTER_DECREASE, GPURegisterDecreaseOptimizer, 1) \
  macro(LOOP_UNROLL, GPULoopUnrollOptimizer, 2) \
  macro(LOOP_NOUNROLL, GPULoopNoUnrollOptimizer, 3) \
  macro(STRENGTH_REDUCTION, GPUStrengthReductionOptimizer, 4) \
  macro(WARP_BALANCE, GPUWarpBalanceOptimizer, 5) \
  macro(CODE_REORDER, GPUCodeReorderOptimizer, 6) \
  macro(KERNEL_MERGE, GPUKernelMergeOptimizer, 7) \
  macro(FUNCTION_INLINE, GPUFunctionInlineOptimizer, 8) \
  macro(SHARED_MEMORY_COALESCE, GPUSharedMemoryCoalesceOptimizer, 9) \
  macro(GLOBAL_MEMORY_COALESCE, GPUGlobalMemoryCoalesceOptimizer, 10) \
  macro(OCCUPANCY_INCREASE, GPUOccupancyIncreaseOptimizer, 11) \
  macro(OCCUPANCY_DECREASE, GPUOccupancyDecreaseOptimizer, 12) \
  macro(SM_BALANCE, GPUSMBalanceOptimizer, 13) \
  macro(BLOCK_INCREASE, GPUBlockIncreaseOptimizer, 14) \
  macro(BLOCK_DECREASE, GPUBlockDecreaseOptimizer, 15)


#define DECLARE_OPTIMIZER_TYPE(TYPE, CLASS, VALUE) \
  TYPE = VALUE,

enum GPUOptimizerType {
  FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_TYPE)
};

#undef DECLARE_OPTIMIZER_TYPE

class GPUOptimizer {
 public:
  GPUOptimizer() {}

  void clear() {
    _match.clear();
    _estimate.clear();
  }

  void assignKernel(const KernelStats &kernel_stats) {
    _kernel_stats = kernel_stats;
  }

  std::string advise() {
    return _match + _estimate;
  }

  // how many samples match the optimizer
  virtual double match(const BlockBlame &block_blame) = 0;

  // how many samples we can eliminate
  virtual double estimate(const BlockBlame &block_blame) = 0;

  virtual ~GPUOptimizer() {}
 
 protected:
  KernelStats _kernel_stats;
  std::string _match;
  std::string _estimate;
};


#define DECLARE_OPTIMIZER_CLASS(TYPE, CLASS, VALUE) \
\
class CLASS : public GPUOptimizer { \
 public: \
  CLASS() {} \
  virtual double match(const BlockBlame &block_blame); \
  virtual double estimate(const BlockBlame &block_blame); \
  virtual ~CLASS() {} \
};

FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CLASS)

#undef DECLARE_OPTIMIZER_CLASS

// A factory method
GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type);


}  // namespace Analysis

#endif  // Analysis_Advisor_GPUOptimizer_hpp
