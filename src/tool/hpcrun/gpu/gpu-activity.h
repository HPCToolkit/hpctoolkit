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

#ifndef gpu_activity_h
#define gpu_activity_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stacks.h>
#include <hpcrun/utilities/ip-normalized.h>



//******************************************************************************
// forward declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;

typedef struct gpu_activity_channel_t gpu_activity_channel_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum {    
  GPU_ACTIVITY_UNKNOWN                 = 0,
  GPU_ACTIVITY_KERNEL                  = 1,
  GPU_ACTIVITY_KERNEL_BLOCK             = 2,  
  GPU_ACTIVITY_MEMCPY                  = 3,
  GPU_ACTIVITY_MEMCPY2                 = 4,
  GPU_ACTIVITY_MEMSET                  = 5,
  GPU_ACTIVITY_MEMORY                  = 6,    
  GPU_ACTIVITY_SYNCHRONIZATION         = 7,
  GPU_ACTIVITY_GLOBAL_ACCESS           = 8,
  GPU_ACTIVITY_LOCAL_ACCESS            = 9,
  GPU_ACTIVITY_BRANCH                  = 10,
  GPU_ACTIVITY_CDP_KERNEL              = 11,
  GPU_ACTIVITY_PC_SAMPLING             = 12,
  GPU_ACTIVITY_PC_SAMPLING_INFO        = 13, 
  GPU_ACTIVITY_EXTERNAL_CORRELATION    = 14,
  GPU_ACTIVITY_EVENT                   = 15,
  GPU_ACTIVITY_FUNCTION                = 16,
  GPU_ACTIVITY_FLUSH                   = 17
} gpu_activity_kind_t;


typedef enum {
  GPU_ENTRY_TYPE_ACTIVITY     = 1,
  GPU_ENTRY_TYPE_NOTIFICATION = 2,
  GPU_ENTRY_TYPE_BUFFER       = 3,
  GPU_ENTRY_TYPE_COUNT        = 4
} gpu_entry_type_t;


// values are used to index within GMEM metric kinds
typedef enum {
  GPU_GLOBAL_ACCESS_LOAD_CACHED = 0,
  GPU_GLOBAL_ACCESS_LOAD_UNCACHED = 1,
  GPU_GLOBAL_ACCESS_STORE = 2,
  GPU_GLOBAL_ACCESS_COUNT = 3
} gpu_global_access_type_t;


typedef enum {
  GPU_LOCAL_ACCESS_LOAD,
  GPU_LOCAL_ACCESS_STORE,
  GPU_LOCAL_ACCESS_COUNT
} gpu_local_access_type_t;


typedef enum {    
  GPU_SYNC_UNKNOWN               = 0,
  GPU_SYNC_EVENT                 = 1,
  GPU_SYNC_STREAM_EVENT_WAIT     = 2,
  GPU_SYNC_STREAM                = 3,
  GPU_SYNC_CONTEXT               = 4,
  GPU_SYNC_COUNT                 = 5
} gpu_sync_type_t;


typedef enum {
  GPU_MEMCPY_UNK = 0,  // unknown
  GPU_MEMCPY_H2D = 1,  // host to device 
  GPU_MEMCPY_D2H = 2,  // device to host 
  GPU_MEMCPY_H2A = 3,  // host to device array 
  GPU_MEMCPY_A2H = 4,  // device array to host
  GPU_MEMCPY_A2A = 5,  // device array to device array
  GPU_MEMCPY_A2D = 6,  // device array to device 
  GPU_MEMCPY_D2A = 7,  // device to device array
  GPU_MEMCPY_D2D = 8,  // device to device
  GPU_MEMCPY_H2H = 9,  // host to host
  GPU_MEMCPY_P2P = 10, // device peer to device peer
  GPU_MEMCPY_COUNT = 11
} gpu_memcpy_type_t;


typedef enum {    
  GPU_INST_STALL_NONE         = 1,
  GPU_INST_STALL_IFETCH       = 2,
  GPU_INST_STALL_IDEPEND      = 3,
  GPU_INST_STALL_GMEM         = 4,
  GPU_INST_STALL_TMEM         = 5,
  GPU_INST_STALL_SYNC         = 6,
  GPU_INST_STALL_CMEM         = 7,
  GPU_INST_STALL_PIPE_BUSY    = 8,
  GPU_INST_STALL_MEM_THROTTLE = 9,
  GPU_INST_STALL_NOT_SELECTED = 10,
  GPU_INST_STALL_OTHER        = 11,
  GPU_INST_STALL_SLEEP        = 12,
  GPU_INST_STALL_INVALID      = 13
} gpu_inst_stall_t;


typedef enum {
  GPU_MEM_ARRAY           = 0,
  GPU_MEM_DEVICE          = 1,
  GPU_MEM_MANAGED         = 2,
  GPU_MEM_PAGEABLE        = 3,
  GPU_MEM_PINNED          = 4,
  GPU_MEM_DEVICE_STATIC   = 5,
  GPU_MEM_MANAGED_STATIC  = 6,
  GPU_MEM_UNKNOWN         = 7,
  GPU_MEM_COUNT           = 8
} gpu_mem_type_t;


// pc sampling
typedef struct gpu_pc_sampling_t {
  uint32_t correlation_id;
  ip_normalized_t pc;
  uint32_t samples;
  uint32_t latencySamples;
  gpu_inst_stall_t stallReason;    
} gpu_pc_sampling_t;


typedef struct gpu_pc_sampling_info_t {
  uint32_t correlation_id;
  uint64_t droppedSamples;
  uint64_t samplingPeriodInCycles;
  uint64_t totalSamples;
  uint64_t fullSMSamples;
} gpu_pc_sampling_info_t;

// a special flush record to notify all operations have been consumed
typedef struct gpu_flush_t {
  atomic_bool *wait;
} gpu_flush_t;

// this type is prefix of all memory structures
// gpu_interval_t is a prefix 
typedef struct gpu_mem_t {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
} gpu_mem_t;


// gpu_mem_t is a prefix 
typedef struct gpu_memcpy_t {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint64_t submit_time;
  uint32_t correlation_id;
  uint32_t device_id;
  uint32_t context_id;
  uint32_t stream_id;
  gpu_memcpy_type_t copyKind;
} gpu_memcpy_t;


// memory allocation or free operation
// gpu_mem_t is a prefix 
typedef struct gpu_memory_t {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t correlation_id;
  uint32_t device_id;
  uint32_t context_id;
  gpu_mem_type_t memKind;
} gpu_memory_t;


// gpu_mem_t is a prefix 
typedef struct gpu_memset_t {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t correlation_id;
  uint32_t device_id;
  uint32_t context_id;
  uint32_t stream_id;
  gpu_mem_type_t memKind;
} gpu_memset_t;


// gpu kernel execution
typedef struct gpu_kernel_t {
  uint64_t start;
  uint64_t end;
  uint64_t submit_time;
  uint32_t correlation_id;
  uint32_t device_id;
  uint32_t context_id;
  uint32_t stream_id;
  int32_t dynamicSharedMemory;
  int32_t staticSharedMemory;
  int32_t localMemoryTotal;
  uint32_t activeWarpsPerSM;
  uint32_t maxActiveWarpsPerSM;
  uint32_t threadRegisters;
  uint32_t blocks;
  uint32_t blockThreads;
  uint32_t blockSharedMemory;
} gpu_kernel_t;


typedef struct gpu_kernel_block_t {
  uint64_t external_id;
  uint32_t bb_instruction_count;
  uint64_t execution_count;
  uint64_t latency;
  uint64_t active_simd_lanes;
  uint64_t total_simd_lanes;
  uint64_t scalar_simd_loss;
  ip_normalized_t pc;
} gpu_kernel_block_t;


typedef struct gpu_cdpkernel_t {
  uint64_t start;
  uint64_t end;
  uint32_t correlation_id;
  uint32_t device_id;
  uint32_t context_id;
  uint32_t stream_id;
} gpu_cdpkernel_t;


typedef struct gpu_function_t {
  uint32_t function_id;
  ip_normalized_t pc;
} gpu_function_t;


typedef struct gpu_event_t {
  uint32_t event_id;
  uint32_t context_id;
  uint32_t stream_id;
} gpu_event_t;


typedef struct gpu_global_access_t {
  uint32_t correlation_id;
  ip_normalized_t pc;
  uint64_t l2_transactions;
  uint64_t theoreticalL2Transactions;
  uint64_t bytes;
  gpu_global_access_type_t type;  
} gpu_global_access_t;


typedef struct gpu_local_access_t {
  uint32_t correlation_id;
  ip_normalized_t pc;
  uint64_t sharedTransactions;
  uint64_t theoreticalSharedTransactions;
  uint64_t bytes;
  gpu_local_access_type_t type;
} gpu_local_access_t;


typedef struct gpu_branch_t {
  uint32_t correlation_id;
  ip_normalized_t pc;
  uint32_t diverged;
  uint32_t executed;
} gpu_branch_t;


typedef struct gpu_synchronization_t {
  uint64_t start;
  uint64_t end;
  uint32_t correlation_id;
  uint32_t context_id;
  uint32_t stream_id;
  uint32_t event_id;
  gpu_sync_type_t syncKind;
} gpu_synchronization_t;


typedef struct gpu_host_correlation_t {
  uint32_t correlation_id;
  uint64_t host_correlation_id;
} gpu_host_correlation_t;


// a type that can be used to access start and end times
// for a subset of activity kinds including kernel execution,
// memcpy, memset, 
typedef struct gpu_interval_t {
  uint64_t start;
  uint64_t end;
} gpu_interval_t;


typedef struct gpu_instruction_t {
  uint32_t correlation_id;
  ip_normalized_t pc;
} gpu_instruction_t;


typedef struct gpu_activity_details_t { 
  union {
    /* Each field stores the complete information needed
       for processing each activity kind.
     */
    gpu_pc_sampling_t pc_sampling;
    gpu_pc_sampling_info_t pc_sampling_info;
    gpu_memcpy_t memcpy;
    gpu_memory_t memory;
    gpu_memset_t memset;
    gpu_kernel_t kernel;
    gpu_kernel_block_t kernel_block;
    gpu_function_t function;
    gpu_cdpkernel_t cdpkernel;
    gpu_event_t event;
    gpu_global_access_t global_access;
    gpu_local_access_t local_access;
    gpu_branch_t branch;
    gpu_synchronization_t synchronization;
    gpu_host_correlation_t correlation;
    gpu_flush_t flush;

    /* Access short cut for activitiy fields shared by multiple kinds */

    /* Activity interval is used to access start time and end time of 
       an coarse grained activity.
     */
    gpu_interval_t interval;

    /* Fine grained measurement contains information about specific instructions */
    gpu_instruction_t instruction;
  };
} gpu_activity_details_t;


typedef struct gpu_activity_t {
  s_element_ptr_t next;
  gpu_activity_kind_t kind;
  gpu_activity_details_t details;
  cct_node_t *cct_node;
} gpu_activity_t;


typedef void (*gpu_activity_attribute_fn_t)
(
 gpu_activity_t *a
);



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_activity_consume
(
 gpu_activity_t *activity,
 gpu_activity_attribute_fn_t aa_fn
);


gpu_activity_t *
gpu_activity_alloc
(
 gpu_activity_channel_t *channel
);


void
gpu_activity_free
(
 gpu_activity_channel_t *channel, 
 gpu_activity_t *a
);


void
gpu_interval_set
(
 gpu_interval_t* interval,
 uint64_t start,
 uint64_t end
);


void
gpu_context_activity_dump
(
 gpu_activity_t *activity,
 const char *context
);


const char*
gpu_kind_to_string
(
gpu_activity_kind_t kind
);


const char*
gpu_type_to_string
(
gpu_memcpy_type_t type
);

#endif
