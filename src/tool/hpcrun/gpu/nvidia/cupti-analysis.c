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
// Copyright ((c)) 2002-2020, Rice University
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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/messages/messages.h>

#include "cupti-analysis.h"
#include "cuda-device-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define MIN2(m1, m2) m1 > m2 ? m2 : m1
#define MIN3(m1, m2, m3) m1 > m2 ? (MIN2(m2, m3)) : (MIN2(m1, m3))
#define MIN4(m1, m2, m3, m4) m1 > m2 ? (MIN3(m2, m3, m4)) : (MIN3(m1, m3, m4))

#define UPPER_DIV(a, b) a == 0 ? 0 : (a - 1) / b + 1


//*****************************************************************************
// interface operations
//*****************************************************************************


//------------------------------------------------------------------------------
// Theoretical occupancy = active_warps_per_sm / max_active_warps_per_sm
// Ref: section 3.2.3 in
// A Performance Analysis Framework for Exploiting GPU Microarchitectural Capability
//------------------------------------------------------------------------------

void
cupti_occupancy_analyze
(
 CUpti_ActivityKernel4 *kernel,
 uint32_t *active_warps_per_sm,
 uint32_t *max_active_warps_per_sm,
 uint32_t *thread_registers,
 uint32_t *block_threads,
 uint32_t *block_shared_memory
)
{
  *active_warps_per_sm = 0;
  *max_active_warps_per_sm = 0;
  cuda_device_map_entry_t *device = cuda_device_map_lookup(kernel->deviceId);
  if (device != NULL) {
    cuda_device_property_t *device_property =
      cuda_device_map_entry_device_property_get(device);

    uint32_t num_threads_per_warp = device_property->num_threads_per_warp;
    uint32_t sm_threads = device_property->sm_threads;
    uint32_t sm_registers = device_property->sm_registers;
    uint32_t sm_shared_memory = device_property->sm_shared_memory;
    uint32_t sm_blocks = device_property->sm_blocks;

    *thread_registers = kernel->registersPerThread;
    *block_threads = kernel->blockX * kernel->blockY * kernel->blockZ;
    *block_shared_memory = kernel->dynamicSharedMemory + kernel->staticSharedMemory;

    uint32_t block_registers = (kernel->registersPerThread) * (*block_threads);
    uint32_t max_blocks_by_threads = sm_threads / *block_threads;
    uint32_t max_blocks_by_registers = sm_registers / block_registers;

    uint32_t max_blocks_by_shared_memory =
      (*block_shared_memory == 0) ?  UINT32_MAX : sm_shared_memory / *block_shared_memory;

    *max_active_warps_per_sm = sm_threads / num_threads_per_warp;

    uint32_t active_blocks = MIN4(max_blocks_by_threads, max_blocks_by_registers,
      max_blocks_by_shared_memory, sm_blocks);

    *active_warps_per_sm = active_blocks * (UPPER_DIV(*block_threads, num_threads_per_warp));

    TMSG(CUDA_CUBIN, "sm_threads %u", sm_threads);
    TMSG(CUDA_CUBIN, "max_blocks_by_registers %u", max_blocks_by_registers);
    TMSG(CUDA_CUBIN, "max_blocks_by_threads %u", max_blocks_by_threads);
    TMSG(CUDA_CUBIN, "max_blocks_by_shared_memory %u", max_blocks_by_shared_memory);
    TMSG(CUDA_CUBIN, "max_blocks_per_multiprocessor %u", sm_blocks);
    TMSG(CUDA_CUBIN, "active_blocks %u", active_blocks);
    TMSG(CUDA_CUBIN, "block_threads %u", *block_threads);
    TMSG(CUDA_CUBIN, "num_threads_per_warp %u", num_threads_per_warp);
    TMSG(CUDA_CUBIN, "active_warps_per_sm %u", *active_warps_per_sm);
    TMSG(CUDA_CUBIN, "max_active_warps_per_sm %u", *max_active_warps_per_sm);
  }
}


//------------------------------------------------------------------------------
// Two facts:
// 1. SMs are profiled simultaneously, which means the total number
// of samples equals to the sum of samples of each SM.
// 2. On each SM, active warps are profiled concurrently in a round
// robin manner, no matter a warp is issuing an instruction or not.
// This fact can be confirmed in NVIDIA's Volta tuning guide.
//
// sm_efficiency = active_cycles / elapsed_cycles
// estimated sm_efficiency =
// total_samples / (clock_rate * kernel_runtime * #SM / sample_rate).
//------------------------------------------------------------------------------

void
cupti_sm_efficiency_analyze
(
 CUpti_ActivityPCSamplingRecordInfo *pc_sampling_record_info,
 uint64_t *total_samples,
 uint64_t *full_sm_samples
)
{
  *total_samples = 0;
  *full_sm_samples = 0;
  // correlation_id->device_id
  gpu_correlation_id_map_entry_t *corr =
    gpu_correlation_id_map_lookup(pc_sampling_record_info->correlationId);
  if (corr != NULL) {
    uint32_t device_id = gpu_correlation_id_map_entry_device_id_get(corr);
    uint64_t start = gpu_correlation_id_map_entry_start_get(corr);
    uint64_t end = gpu_correlation_id_map_entry_end_get(corr);
    cuda_device_map_entry_t *device =
      cuda_device_map_lookup(device_id);
    if (device != NULL) {
      cuda_device_property_t *device_property =
        cuda_device_map_entry_device_property_get(device);

      uint64_t sample_period_in_cycles = pc_sampling_record_info->samplingPeriodInCycles;
      // khz to hz to hz/ns
      double core_clock_rate = device_property->sm_clock_rate / 1000000.0;
      uint64_t num_multiprocessors = device_property->sm_count;
      // ns
      uint64_t kernel_time = end - start;
      *total_samples = pc_sampling_record_info->totalSamples;
      *full_sm_samples = ((uint64_t)(core_clock_rate * kernel_time) / sample_period_in_cycles) * num_multiprocessors;
      TMSG(CUDA_CUBIN, "sample_period_in_cycles %lu", sample_period_in_cycles);
      TMSG(CUDA_CUBIN, "core_clock_rate %lf", core_clock_rate);
      TMSG(CUDA_CUBIN, "num_multiprocessors %lu", num_multiprocessors);
      TMSG(CUDA_CUBIN, "kernel_time %lu", kernel_time);
      TMSG(CUDA_CUBIN, "total_samples %lu", *total_samples);
      TMSG(CUDA_CUBIN, "full_sm_samples %lu", *full_sm_samples);
      TMSG(CUDA_CUBIN, "dropped_samples %lu", pc_sampling_record_info->droppedSamples);
    }
  }
}
