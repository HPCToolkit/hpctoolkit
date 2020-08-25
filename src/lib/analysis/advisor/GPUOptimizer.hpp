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
#include <tuple>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <lib/cuda/DotCFG.hpp>
#include <lib/cuda/AnalyzeInstruction.hpp>

#include "GPUArchitecture.hpp"
#include "GPUEstimator.hpp"
#include "GPUKernel.hpp"
#include "Inspection.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

struct InstructionBlame {
  CudaParse::InstructionStat *src_inst, *dst_inst;
  CudaParse::Block *src_block, *dst_block;
  // TODO(Keren): consider only intra procedural optimizations for now
  CudaParse::Function *src_function, *dst_function;
  Prof::Struct::ACodeNode *src_struct, *dst_struct;
  // src->dst instructions
  // 0: same instruction
  // -1: very long distance
  // >0: xact distance
  double distance;
  double stall_blame;
  double lat_blame;
  std::string blame_name;

  InstructionBlame(CudaParse::InstructionStat *src_inst, CudaParse::InstructionStat *dst_inst,
                   Prof::Struct::ACodeNode *src_struct, Prof::Struct::ACodeNode *dst_struct,
                   double distance, double stall_blame, double lat_blame,
                   const std::string &blame_name)
      : src_inst(src_inst),
        dst_inst(dst_inst),
        src_struct(src_struct),
        dst_struct(dst_struct),
        distance(distance),
        stall_blame(stall_blame),
        lat_blame(lat_blame),
        blame_name(blame_name) {}

  InstructionBlame(CudaParse::InstructionStat *src_inst, CudaParse::InstructionStat *dst_inst,
                   CudaParse::Block *src_block, CudaParse::Block *dst_block,
                   CudaParse::Function *src_function, CudaParse::Function *dst_function,
                   Prof::Struct::ACodeNode *src_struct, Prof::Struct::ACodeNode *dst_struct,
                   double distance, double stall_blame, double lat_blame,
                   const std::string &blame_name)
      : src_inst(src_inst),
        dst_inst(dst_inst),
        src_block(src_block),
        dst_block(dst_block),
        src_function(src_function),
        dst_function(dst_function),
        src_struct(src_struct),
        dst_struct(dst_struct),
        distance(distance),
        stall_blame(stall_blame),
        lat_blame(lat_blame),
        blame_name(blame_name) {}

  InstructionBlame() : src_inst(NULL), dst_inst(NULL), src_block(NULL), dst_block(NULL),
    src_function(NULL), dst_function(NULL), src_struct(NULL), dst_struct(NULL),
    distance(0), stall_blame(0), lat_blame(0) {}
};

struct InstructionBlameStallComparator {
  bool operator() (const InstructionBlame *l, const InstructionBlame *r) const {
    return l->stall_blame > r->stall_blame;
  }
};


struct InstructionBlameLatComparator {
  bool operator() (const InstructionBlame *l, const InstructionBlame *r) const {
    return l->lat_blame > r->lat_blame;
  }
};


struct InstructionBlameSrcComparator {
  bool operator() (const InstructionBlame *l, const InstructionBlame *r) const {
    return l->src_inst->pc < r->src_inst->pc;
  }
};


struct InstructionBlameDstComparator {
  bool operator() (const InstructionBlame *l, const InstructionBlame *r) const {
    return l->dst_inst->pc < r->dst_inst->pc;
  }
};


typedef std::vector<InstructionBlame> InstBlames;
typedef std::vector<InstructionBlame *> InstBlamePtrs;


struct KernelBlame {
  InstBlames inst_blames;
  InstBlamePtrs lat_inst_blame_ptrs;
  InstBlamePtrs stall_inst_blame_ptrs;
  std::map<std::string, double> stall_blames;
  std::map<std::string, double> lat_blames;
  double stall_blame;
  double lat_blame;

  KernelBlame() : stall_blame(0), lat_blame(0) {}
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
  macro(BLOCK_DECREASE, GPUBlockDecreaseOptimizer, 16) \
  macro(FAST_MATH, GPUFastMathOptimizer, 17)


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

  void set_estimator(GPUEstimator *estimator) {
    this->_estimator = estimator;
  }

  /**
   * @brief
   *
   * @param kernel_blame
   * @param kernel_stats
   * @return std::vector<BlameStats>
   */
  virtual std::vector<BlameStats> match_impl(const KernelBlame &kernel_blame,
                                             const KernelStats &kernel_stats) = 0;

  /**
   * @brief
   *
   * @param kernel_blame
   * @param kernel_stats
   * @return double
   */
  double match(const KernelBlame &kernel_blame, const KernelStats &kernel_stats);

  virtual ~GPUOptimizer() {}
 
 protected:
  std::string _name;

  const GPUArchitecture *_arch;

  GPUEstimator *_estimator;

  Inspection _inspection;

  const size_t _top_regions = 5;
  const size_t _top_hotspots = 5;
};

#define DECLARE_OPTIMIZER_CLASS(TYPE, CLASS, VALUE)                                           \
                                                                                              \
  class CLASS : public GPUOptimizer {                                                         \
   public:                                                                                    \
    CLASS(const std::string &name, const GPUArchitecture *arch) : GPUOptimizer(name, arch) {} \
    virtual std::vector<BlameStats> match_impl(const KernelBlame &kernel_blame,               \
                                               const KernelStats &kernel_stats);              \
    virtual ~CLASS() {}                                                                       \
  };

FORALL_OPTIMIZER_TYPES(DECLARE_OPTIMIZER_CLASS)

#undef DECLARE_OPTIMIZER_CLASS

// A factory method
GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type, GPUArchitecture *arch);

}  // namespace Analysis

#endif  // Analysis_Advisor_GPUOptimizer_hpp
