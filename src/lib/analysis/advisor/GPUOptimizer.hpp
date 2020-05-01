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

#include <lib/cuda/AnalyzeInstruction.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {


struct KernelStat {
  int shared_memory;
  int registers;
  int blocks;
  double occupancy;
  double sm_efficiency;

  KernelStat() {}
};


struct InstructionBlame {
  // Use InstructionStat directly, so that in advisor::advise, we do not need a vma_inst_map again
  CudaParse::InstructionStat *src, *dst;
  int metric_id;
  double value;

  InstructionBlame(CudaParse::InstructionStat *src, CudaParse::InstructionStat *dst,
    int metric_id, double value) : src(src), dst(dst), metric_id(metric_id), value(value) {}
  InstructionBlame() {}

  bool operator < (const InstructionBlame &other) const {
    return this->src->pc < other.src->pc;
  }
};


struct BlockBlame {
  int id;
  VMA start, end;
  std::vector<InstructionBlame> inst_blames;
  std::map<int, double> blames;
  double blame;

  BlockBlame(int id, VMA start, VMA end) : id(id), start(start), end(end), blame(0.0) {}
  BlockBlame(int id) : BlockBlame(id, 0, 0) {}
  BlockBlame() : BlockBlame(0) {}

  bool operator < (const BlockBlame &other) const {
    return this->blame < other.blame;
  }
};


struct FunctionBlame {
  int index;
  VMA start, end;
  std::map<int, BlockBlame> block_blames;
  std::map<int, double> blames;
  double blame;

  FunctionBlame(int index) : index(index), start(0), end(0), blame(0.0) {}
  FunctionBlame() : FunctionBlame(0) {}
};


typedef std::vector<InstructionBlame> InstBlames;

typedef std::map<int, FunctionBlame> FunctionBlames;

// This type is shared with GPUInstruction.hpp
typedef std::map<int, std::map<int, FunctionBlames>> FunctionBlamesMap;


enum GPUOptimizerType {
  LOOP_UNROLL,
  MEMORY_LAYOUT,
  STRENGTH_REDUCTION,
  ADJUST_THREADS,
  ADJUST_REGISTERS
};


class GPUOptimizer {
 public:
  GPUOptimizer() {}

  virtual double match(const BlockBlame &block_blame) = 0;

  virtual std::string advise() = 0;

  virtual ~GPUOptimizer() {}
};


class GPULoopUnrollOptimizer : public GPUOptimizer {
 public:
  GPULoopUnrollOptimizer() {}

  virtual double match(const BlockBlame &block_blame);

  virtual std::string advise();
};


class GPUMemoryLayoutOptimizer : public GPUOptimizer {
 public:
  GPUMemoryLayoutOptimizer() {}

  virtual double match(const BlockBlame &block_blame);

  virtual std::string advise();
};


class GPUStrengthReductionOptimizer : public GPUOptimizer {
 public:
  GPUStrengthReductionOptimizer() {}

  virtual double match(const BlockBlame &block_blame);

  virtual std::string advise();
};


class GPUAdjustRegistersOptimizer : public GPUOptimizer {
 public:
  GPUAdjustRegistersOptimizer() {}

  virtual double match(const BlockBlame &block_blame);

  virtual std::string advise();
};


class GPUAdjustThreadsOptimizer : public GPUOptimizer {
 public:
  GPUAdjustThreadsOptimizer() {}

  virtual double match(const BlockBlame &block_blame);

  virtual std::string advise();
};


// A factory method
GPUOptimizer *GPUOptimizerFactory(GPUOptimizerType type);


}  // namespace Analysis

#endif  // Analysis_Advisor_GPUOptimizer_hpp
