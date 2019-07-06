#include "cupti-analysis.h"

#include <stdint.h>

#include "cuda-device-map.h"
#include "cupti-correlation-id-map.h"

/******************************************************************************
 * macros
 *****************************************************************************/
#define CUPTI_ANALYSIS_DEBUG 0

#if CUPTI_ANALYSIS_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define MIN2(m1, m2) m1 > m2 ? m2 : m1
#define MIN3(m1, m2, m3) m1 > m2 ? (MIN2(m2, m3)) : (MIN2(m1, m3))
#define MIN4(m1, m2, m3, m4) m1 > m2 ? (MIN3(m2, m3, m4)) : (MIN3(m1, m3, m4))

#define UPPER_DIV(a, b) a == 0 ? 0 : (a - 1) / b + 1

/*
 * Theoretical occupancy = active_warps_per_sm / max_active_warps_per_sm
 * Ref: section 3.2.3 in
 * A Performance Analysis Framework for Exploiting GPU Microarchitectural Capability
 */
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
    uint32_t max_blocks_by_shared_memory = *block_shared_memory == 0 ? UINT32_MAX : sm_shared_memory / *block_shared_memory;
    *max_active_warps_per_sm = sm_threads / num_threads_per_warp;

    uint32_t active_blocks = MIN4(max_blocks_by_threads, max_blocks_by_registers,
      max_blocks_by_shared_memory, sm_blocks);
    *active_warps_per_sm = active_blocks * (UPPER_DIV(*block_threads, num_threads_per_warp));
    PRINT("sm_threads %u\n", sm_threads);
    PRINT("max_blocks_by_registers %u\n", max_blocks_by_registers);
    PRINT("max_blocks_by_threads %u\n", max_blocks_by_threads);
    PRINT("max_blocks_by_shared_memory %u\n", max_blocks_by_shared_memory);
    PRINT("max_blocks_per_multiprocessor %u\n", sm_blocks);
    PRINT("active_blocks %u\n", active_blocks);
    PRINT("block_threads %u\n", *block_threads);
    PRINT("num_threads_per_warp %u\n", num_threads_per_warp);
    PRINT("active_warps_per_sm %u\n", *active_warps_per_sm);
    PRINT("max_active_warps_per_sm %u\n", *max_active_warps_per_sm);
  }
}


/*
 * Two facts:
 * 1. SMs are profiled simultaneously, which means the total number
 * of samples equals to the sum of samples of each SM.
 * 2. On each SM, active warps are profiled concurrently in a round
 * robin manner, no matter a warp is issuing an instruction or not.
 * This fact can be confirmed in NVIDIA's Volta tuning guide. 
 *
 * sm_efficiency = active_cycles / elapsed_cycles
 * estimated sm_efficiency =
 * total_samples / (clock_rate * kernel_runtime * #SM / sample_rate).
 */
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
  cupti_correlation_id_map_entry_t *corr =
    cupti_correlation_id_map_lookup(pc_sampling_record_info->correlationId);
  if (corr != NULL) {
    uint32_t device_id = cupti_correlation_id_map_entry_device_id_get(corr);
    uint64_t start = cupti_correlation_id_map_entry_start_get(corr);
    uint64_t end = cupti_correlation_id_map_entry_end_get(corr);
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
      PRINT("sample_period_in_cycles %lu\n", sample_period_in_cycles);
      PRINT("core_clock_rate %lf\n", core_clock_rate);
      PRINT("num_multiprocessors %lu\n", num_multiprocessors);
      PRINT("kernel_time %lu\n", kernel_time);
      PRINT("total_samples %lu\n", *total_samples);
      PRINT("full_sm_samples %lu\n", *full_sm_samples);
      PRINT("dropped_samples %lu\n", pc_sampling_record_info->droppedSamples);
    }
  }
}
