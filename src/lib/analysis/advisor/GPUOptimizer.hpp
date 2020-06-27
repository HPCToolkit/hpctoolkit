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

#include "GPUArchitecture.hpp"
#include "Inspection.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {


struct KernelStats {
  uint32_t blocks;
  uint32_t threads;
  uint32_t shared_memory; // bytes per block
  uint32_t registers; // registers per thread
  uint32_t active_warps; // per sm
  uint64_t active_samples; // not stalled samples
  uint64_t total_samples; // total samples
  double time;
  double sm_efficiency;

  KernelStats(uint32_t blocks, uint32_t threads, uint32_t shared_memory, uint32_t registers,
    uint32_t active_warps, uint64_t active_samples, uint64_t total_samples, double time,
    double sm_efficiency) : blocks(blocks), threads(threads), shared_memory(shared_memory),
    registers(registers), active_warps(active_warps), active_samples(active_samples),
    total_samples(total_samples), time(time), sm_efficiency(sm_efficiency) {}

  KernelStats() {}
};


struct InstructionBlame {
  // TODO(Keren):
  // Use InstructionBlameDetail pointer to save space and more crystal 
  // Use InstructionStat directly, so that in advisor::advise, we do not need a vma_inst_map again
  CudaParse::InstructionStat *src, *dst;
  Prof::Struct::ACodeNode *src_struct, *dst_struct;
  std::string blame_name;
  CudaParse::Function *function;
  CudaParse::Block *block;
  double stall_blame;
  double lat_blame;

  InstructionBlame(
    CudaParse::InstructionStat *src, CudaParse::InstructionStat *dst,
    Prof::Struct::ACodeNode *src_struct, Prof::Struct::ACodeNode *dst_struct,
    std::string &blame_name, double stall_blame, double lat_blame) :
    src(src), dst(dst), src_struct(src_struct), dst_struct(dst_struct),
    blame_name(blame_name), stall_blame(stall_blame), lat_blame(lat_blame) {}

  InstructionBlame(
    CudaParse::InstructionStat *src, CudaParse::InstructionStat *dst,
    Prof::Struct::ACodeNode *src_struct, Prof::Struct::ACodeNode *dst_struct,
    std::string &blame_name, CudaParse::Function *function, CudaParse::Block *block,
    double stall_blame, double lat_blame) : src(src), dst(dst),
    src_struct(src_struct), dst_struct(dst_struct),
    blame_name(blame_name), function(function), block(block),
    stall_blame(stall_blame), lat_blame(lat_blame) {}

  InstructionBlame() {}
};


struct InstructionBlameStallComparator {
  bool operator() (const InstructionBlame &l, const InstructionBlame &r) const {
    return l.stall_blame > r.stall_blame;
  }
};


struct InstructionBlameLatComparator {
  bool operator() (const InstructionBlame &l, const InstructionBlame &r) const {
    return l.lat_blame > r.lat_blame;
  }
};


typedef std::vector<InstructionBlame> InstBlames;


struct KernelBlame {
  InstBlames stall_inst_blames;
  InstBlames lat_inst_blames;
  std::map<std::string, double> stall_blames;
  std::map<std::string, double> lat_blames;
  double stall_blame;
  double lat_blame;

  KernelBlame() {}
};

typedef std::map<int, std::map<int, KernelBlame>> CCTBlames;


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
  macro(FUNCTION_SPLIT, GPUFunctionSplitOptimizer, 9) \
  macro(SHARED_MEMORY_COALESCE, GPUSharedMemoryCoalesceOptimizer, 10) \
  macro(GLOBAL_MEMORY_COALESCE, GPUGlobalMemoryCoalesceOptimizer, 11) \
  macro(OCCUPANCY_INCREASE, GPUOccupancyIncreaseOptimizer, 12) \
  macro(OCCUPANCY_DECREASE, GPUOccupancyDecreaseOptimizer, 13) \
  macro(SM_BALANCE, GPUSMBalanceOptimizer, 14) \
  macro(BLOCK_INCREASE, GPUBlockIncreaseOptimizer, 15) \
  macro(BLOCK_DECREASE, GPUBlockDecreaseOptimizer, 16)


#define DECLARE_OPTIMIZER_TYPE(TYPE, CLASS, VALUE) \
  TYPE = VALUE,

enum GPUOptimizerType {
  FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_TYPE)
};

#undef DECLARE_OPTIMIZER_TYPE


class GPUOptimizer {
 public:
  GPUOptimizer(const std::string &name, const GPUArchitecture *arch) : 
    _name(name), _arch(arch) {}

  void clear() {
    _inspection.clear();
  }

  std::string name() {
    return _name;
  }

  std::string advise(InspectionFormatter *formatter) {
    return formatter->format(_inspection);
  }

  // Code optimizer:
  // Estimate speedup by supposing all latency samples can be eliminated
  //
  // Parallel optimizer:
  // Estimate speedup by raising parallelism
  //
  // @Return speedup
  virtual double match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats) = 0;

  virtual ~GPUOptimizer() {}
 
 protected:
  std::string _name;

  const GPUArchitecture *_arch;

  Inspection _inspection;

  const int _top_regions = 3;
};


#define DECLARE_OPTIMIZER_CLASS(TYPE, CLASS, VALUE) \
\
class CLASS : public GPUOptimizer { \
 public: \
  CLASS(const std::string &name, const GPUArchitecture *arch) : GPUOptimizer(name, arch) {} \
  virtual double match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats); \
  virtual ~CLASS() {} \
};

FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CLASS)

#undef DECLARE_OPTIMIZER_CLASS

// A factory method
GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type, GPUArchitecture *arch);


}  // namespace Analysis

#endif  // Analysis_Advisor_GPUOptimizer_hpp
