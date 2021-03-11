// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
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

#ifndef gpu_instrumentation_gtpin_instrumentation_h
#define gpu_instrumentation_gtpin_instrumentation_h

//******************************************************************************
// local includes
//******************************************************************************

#include "simdgroup-map.h"



//******************************************************************************
// type definitions
//******************************************************************************

typedef struct gpu_op_ccts_t gpu_op_ccts_t;


typedef struct LatencyDataInternal
{
    uint32_t _freq;    ///< Kernel frequency
    uint32_t _cycles;  ///< Total number of cycles
    uint32_t _skipped; ///< Total number of skipped cycles
    uint32_t _pad;     ///< Padding
} LatencyDataInternal;


typedef struct SimdGroupNode {
  uint64_t key;
  simdgroup_map_entry_t *entry;
  GTPinMem mem_simd;
  struct SimdGroupNode *next;
} SimdGroupNode;


typedef struct SimdSectionNode {
  SimdGroupNode *groupHead;
  struct SimdSectionNode *next;
} SimdSectionNode;



//******************************************************************************
// interface operations
//******************************************************************************

void
gtpin_enable_profiling
(
 void
);


void
gtpin_produce_runtime_callstack
(
 gpu_op_ccts_t *
);


#endif
