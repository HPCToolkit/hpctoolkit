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

#ifndef Analysis_Advisor_GPUEstimator_hpp
#define Analysis_Advisor_GPUEstimator_hpp

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <tuple>
#include <vector>

#include "GPUKernel.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

class GPUArchitecture;

enum GPUEstimatorType {
  SEQ = 0,
  SEQ_LAT = 1,
  PARALLEL = 2,
  PARALLEL_LAT = 3
};


class GPUEstimator {
 public:
  GPUEstimator(GPUArchitecture *arch, GPUEstimatorType type) : _arch(arch), _type(type) {}

  // <ratio, speedup>
  virtual std::pair<std::vector<double>, std::vector<double>>
    estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats) = 0;

  virtual ~GPUEstimator() {}

 protected:
  GPUArchitecture *_arch;
  GPUEstimatorType _type;
};


class SequentialLatencyGPUEstimator : public GPUEstimator {
 public:
   SequentialLatencyGPUEstimator(GPUArchitecture *arch) : GPUEstimator(arch, SEQ_LAT) {}
 
   // <ratio, speedup>
  virtual std::pair<std::vector<double>, std::vector<double>>
    estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats);
 
   virtual ~SequentialLatencyGPUEstimator() {}
};


class SequentialGPUEstimator : public GPUEstimator {
 public:
  SequentialGPUEstimator(GPUArchitecture *arch) : GPUEstimator(arch, SEQ) {}
 
  // <ratio, speedup>
  virtual std::pair<std::vector<double>, std::vector<double>>
    estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats);
 
  virtual ~SequentialGPUEstimator() {}
};


class ParallelLatencyGPUEstimator : public GPUEstimator {
 public:
  ParallelLatencyGPUEstimator(GPUArchitecture *arch) : GPUEstimator(arch, PARALLEL_LAT) {}
 
  // <ratio, speedup>
  virtual std::pair<std::vector<double>, std::vector<double>>
    estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats);
 
  virtual ~ParallelLatencyGPUEstimator() {}
};


class ParallelGPUEstimator : public GPUEstimator {
 public:
  ParallelGPUEstimator(GPUArchitecture *arch) : GPUEstimator(arch, PARALLEL) {}
 
  // <ratio, speedup>
  virtual std::pair<std::vector<double>, std::vector<double>>
    estimate(const std::vector<BlameStats> &blame_stats, const KernelStats &kernel_stats);
 
  virtual ~ParallelGPUEstimator() {}
};


// A factory method
GPUEstimator *GPUEstimatorFactory(GPUArchitecture *arch, GPUEstimatorType type);

} // namespace Analysis

#endif // Analysis_Advisor_Inspection_hpp
