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


#ifndef Analysis_Advisor_GPUKernel_hpp 
#define Analysis_Advisor_GPUKernel_hpp

//************************* System Include Files ****************************

#include <string>
#include <tuple>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

namespace Analysis {

struct KernelStats {
  uint32_t blocks;
  uint32_t threads;
  uint32_t shared_memory; // bytes per block
  uint32_t registers; // registers per thread
  uint32_t active_warps; // per sm
  double active_samples; // not stalled samples
  double total_samples; // total samples
  double expected_samples;
  double time;
  double sm_efficiency;
  uint64_t count; // kernel invocation times

  KernelStats(uint32_t blocks, uint32_t threads, uint32_t shared_memory, uint32_t registers,
    uint32_t active_warps, double active_samples, double total_samples, double expected_samples, 
    double time, double efficiency, uint64_t count) : blocks(blocks), threads(threads),
    shared_memory(shared_memory), registers(registers), active_warps(active_warps),
    active_samples(active_samples), total_samples(total_samples), expected_samples(expected_samples),
    time(time), sm_efficiency(efficiency), count(count) {}

  KernelStats() : blocks(0), threads(0), shared_memory(0),
    registers(0), active_warps(0), active_samples(0),
    total_samples(0), expected_samples(0),
    time(0), sm_efficiency(0), count(0) {}
};


struct BlameStats {
  double blame;
  double active_samples;
  double total_samples;

  BlameStats(double blame, double active_samples, double total_samples):
    blame(blame), active_samples(active_samples), total_samples(total_samples) {}

  BlameStats() : blame(0.0), active_samples(0.0), total_samples(0.0) {}

  bool operator < (const BlameStats &blame_stats) const {
    return this->blame > blame_stats.blame;
  }
};

}  // namespace Analysis

#endif
