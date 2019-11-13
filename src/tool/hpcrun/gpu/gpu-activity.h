#ifndef gpu_activity_h
#define gpu_activity_h



//******************************************************************************
// global includes
//******************************************************************************

#include <inttypes.h>



//******************************************************************************
// local includes
//******************************************************************************

// #include <lib/prof-lean/stdatomic.h>



//******************************************************************************
// forward declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;

typedef struct gpu_activity_channel_t gpu_activity_channel_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum {
    GPU_ENTRY_TYPE_ACTIVITY = 1,
    GPU_ENTRY_TYPE_NOTIFICATION = 2,
    GPU_ENTRY_TYPE_BUFFER = 3,
    GPU_ENTRY_TYPE_COUNT = 4
} gpu_entry_type_t;


typedef enum {
  GPU_GLOBAL_ACCESS_LOAD_CACHED,
  GPU_GLOBAL_ACCESS_LOAD_UNCACHED,
  GPU_GLOBAL_ACCESS_STORE,
  GPU_GLOBAL_ACCESS_COUNT,
} gpu_global_access_type_t;


typedef enum {
  GPU_SHARED_ACCESS_LOAD,
  GPU_SHARED_ACCESS_STORE,
  GPU_SHARED_ACCESS_COUNT,
} gpu_shared_access_type_t;


// pc sampling
typedef struct gpu_pc_sampling {
  uint32_t pcOffset;
  uint32_t samples;
  uint32_t latencySamples;
  uint32_t stallReason;    
} gpu_pc_sampling_t;


typedef struct gpu_pc_sampling_record_info {
  uint64_t droppedSamples;
  uint64_t samplingPeriodInCycles;
  uint64_t totalSamples;
  uint64_t fullSMSamples;
} gpu_pc_sampling_record_info_t;


// memory
typedef struct gpu_memcpy {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t copyKind;
} gpu_memcpy_t;


typedef struct gpu_memory {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t memKind;
} gpu_memory_t;


typedef struct gpu_memset {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t memKind;
} gpu_memset_t;


// kernel
typedef struct gpu_kernel {
  uint64_t start;
  uint64_t end;
  uint32_t context_id,
  uint32_t stream_id,
  int32_t dynamicSharedMemory;
  int32_t staticSharedMemory;
  int32_t localMemoryTotal;
  uint32_t activeWarpsPerSM;
  uint32_t maxActiveWarpsPerSM;
  uint32_t threadRegisters;
  uint32_t blockThreads;
  uint32_t blockSharedMemory;
} gpu_kernel_t;

typedef struct gpu_cdpkernel {
  uint64_t start;
  uint64_t end;
  uint32_t context_id,
  uint32_t stream_id
} gpu_cdpkernel_t;

typedef struct gpu_global_access {
  uint64_t pcOffset;
  uint64_t l2_transactions;
  uint64_t theoreticalL2Transactions;
  uint64_t bytes;
  gpu_global_access_type_t type;  
} gpu_global_access_t;


typedef struct gpu_shared_access {
  uint64_t pcOffset;
  uint64_t sharedTransactions;
  uint64_t theoreticalSharedTransactions;
  uint64_t bytes;
  gpu_shared_access_type_t type;
} gpu_shared_access_t;


typedef struct gpu_branch {
  uint64_t pcOffset;
  uint32_t diverged;
  uint32_t executed;
} gpu_branch_t;


// synchronization
typedef struct gpu_synchronization {
  uint64_t start;
  uint64_t end;
  uint32_t syncKind;
} gpu_synchronization_t;

typedef struct gpu_correlation {
  uint64_t externalId;
} gpu_correlation_t;


// Accessors for a subset of activity kinds

typedef struct gpu_activity_interval {
  uint64_t start;
  uint64_t end;
} gpu_activity_interval_t;

typedef struct gpu_instruction {
  uint64_t pcOffset;
} gpu_instruction_t;

typedef struct entry_data {
  union {
    /* Each field stores the complete information needed
       for processing each activity kind.
     */
    gpu_pc_sampling_t pc_sampling;
    gpu_pc_sampling_record_info_t pc_sampling_record_info;
    gpu_memcpy_t memcpy;
    gpu_memory_t memory;
    gpu_memset_t memset;
    gpu_kernel_t kernel;
    gpu_cdpkernel_t cdpkernel;
    gpu_global_access_t global_access;
    gpu_shared_access_t shared_access;
    gpu_branch_t branch;
    gpu_synchronization_t synchronization;
    gpu_correlation_t correlation;

    /* Access short cut for activitiy fields shared by multiple kinds */

    /* Activity interval is used to access start time and end time of 
       an coarse grained activity.
     */
    gpu_activity_interval_t interval;

    /* Fine grained measurement contains information about specific instructions */
    gpu_instruction_t instruction;
  };
} gpu_activity_details_t;

typedef enum gpu_activity_kind {    
    GPU_ACTIVITY_KIND_PC_SAMPLING,
    GPU_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO,
    GPU_ACTIVITY_KIND_MEMCPY,
    GPU_ACTIVITY_KIND_MEMCPY2,
    GPU_ACTIVITY_KIND_MEMSET,
    GPU_ACTIVITY_KIND_KERNEL,
    GPU_ACTIVITY_KIND_SYNCHRONIZATION,
    GPU_ACTIVITY_KIND_MEMORY,    
    GPU_ACTIVITY_KIND_GLOBAL_ACCESS,
    GPU_ACTIVITY_KIND_SHARED_ACCESS,
    GPU_ACTIVITY_KIND_BRANCH,
    GPU_ACTIVITY_KIND_EXTERNAL_CORRELATION,
    GPU_ACTIVITY_KIND_CDP_KERNEL
} gpu_activity_kind_t;

typedef struct gpu_activity_t {
  gpu_activity_kind_t kind;
  uint32_t correlationId;
  gpu_activity_details_t details;
  cct_node_t *cct_node;
} gpu_activity_t;



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_activity_consume
(
 gpu_activity_t *activity
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
set_gpu_instruction
(
  gpu_instruction_t* , 
  uint64_t
);

void
set_gpu_activity_interval
(
  gpu_activity_interval_t* interval,
  uint64_t start,
  uint64_t end
);

#endif
