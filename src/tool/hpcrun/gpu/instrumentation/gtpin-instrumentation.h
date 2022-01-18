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
// Copyright ((c)) 2002-2022, Rice University
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
  uint32_t instCount;
  struct SimdGroupNode *next;
} SimdGroupNode;


typedef struct SimdSectionNode {
  SimdGroupNode *groupHead;
  struct SimdSectionNode *next;
} SimdSectionNode;


typedef enum {
  GEN_UNKNOWN = 0,
  GEN8        = 8,
  GEN9        = 9,
  GEN10       = 10,
  GEN11       = 11,
  GEN12          = 12
} PlatformGen;


// Message latencies
static const uint16_t LegacyFFLatency[] = {
  2,   // 0: SFID_NULL
  300, // 1: SFID_SAMPLER
  200, // 2: SFID_GATEWAY
  400, // 3: SFID_DP_READ, SFID_DP_DC2
  200, // 4: SFID_DP_WRITE i.e DATAPORT WRITE
  50,  // 5: SFID_URB
  50,  // 6: SFID_SPAWNER  i.e THREAD SPAWNER
  50,  // 7: SFID_VME      i.e VIDEO MOTION ESTIMATION 
  60,  // 8: SFID_DP_CC    i.e CONSTANT CACHE DATAPORT
  400, // 9: SFID_DP_DC    i.e DATA CACHE DATAPORT 
  50,  //10: SFID_DP_PI    i.e PIXEL INTERPOLATOR
  400, //11: SFID_DP_DC1   i.e DATA CACHE DATAPORT1
  200, //12: SFID_CRE      i.e CHECK & REFINEMENT ENGINE
  200  //13: unknown, SFID_NUM
};


typedef enum {
  //  General instruction latencies
  // To be comptabile with send cycles, don't normalized them to 1
  LegacyLatencies_IVB_PIPELINE_LENGTH     = 14,
  LegacyLatencies_EDGE_LATENCY_MATH       = 22,
  LegacyLatencies_EDGE_LATENCY_MATH_TYPE2 = 30,
  LegacyLatencies_EDGE_LATENCY_SEND_WAR   = 36
} LegacyLatencies;


typedef enum {
  // General instruction latencies
  FPU_ACC                 = 6,    // SIMD8 latency if dst is acc.
  FPU                     = 10,   // SIMD8 latency for general FPU ops.
  MATH                    = 17,   // Math latency.
  BRANCH                  = 23,   // Latency for SIMD16 branch.
  BARRIER                 = 30,   // Latency for barrier.
  DELTA                   = 1,    // Extra cycles for wider SIMD sizes, compute only.
  DELTA_MATH              = 4,
  ARF                     = 16,   // latency for ARF dependencies (flag, address, etc.)
  // Latency for dpas 8x1
  // Latency for dpas 8x8 is 21 + 7 = 28
  DPAS = 21,

  // Message latencies
  // Latency for SIMD16 SLM messages. If accessing
  // the same location, it takes 28 cycles. For the
  // sequential access pattern, it takes 26 cycles.
  SLM                     = 28,
  SEND_OTHERS             = 50,   // Latency for other messages.
  DP_L3                   = 146,  // Dataport L3 hit
  SAMPLER_L3              = 214,  // Sampler L3 hit
  SLM_FENCE               = 23,   // Fence SLM
  LSC_UNTYPED_L1          = 45,   // LSC untyped L1 cache hit
  LSC_UNTYPED_L3          = 200,  // LSC untyped L3 cache hit
  LSC_UNTYPED_FENCE       = 35,   // LSC untyped fence (best case)
  LSC_TYPED_L1            = 75,   // LSC typed L1 cache hit
  LSC_TYPED_L3            = 200,  // LSC typed L3 cache hit
  LSC_TYPED_FENCE         = 60,   // LSC typed fence
} LatenciesXe;



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


void
gtpin_enable_instrumentation
(
 void
);


void
gtpin_simd_enable
(
 void
);


void
gtpin_latency_enable
(
 void
);


void
gtpin_count_enable
(
 void
);

#endif
