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


// pc sampling
typedef struct gpu_pc_sampling {
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
    int32_t dynamicSharedMemory;
    int32_t staticSharedMemory;
    int32_t localMemoryTotal;
    uint32_t activeWarpsPerSM;
    uint32_t maxActiveWarpsPerSM;
    uint32_t threadRegisters;
    uint32_t blockThreads;
    uint32_t blockSharedMemory;
} gpu_kernel_t;


typedef enum {
    GPU_GLOBAL_ACCESS_LOAD_CACHED,
    GPU_GLOBAL_ACCESS_LOAD_UNCACHED,
    GPU_GLOBAL_ACCESS_STORE,
    GPU_GLOBAL_ACCESS_COUNT,
} gpu_global_access_type_t;


typedef struct gpu_global_access {
    uint64_t l2_transactions;
    uint64_t theoreticalL2Transactions;
    uint64_t bytes;
    gpu_global_access_type_t type;
} gpu_global_access_t;


typedef enum {
    GPU_SHARED_ACCESS_LOAD,
    GPU_SHARED_ACCESS_STORE,
    GPU_SHARED_ACCESS_COUNT,
} gpu_shared_access_type_t;


typedef struct gpu_shared_access {
    uint64_t sharedTransactions;
    uint64_t theoreticalSharedTransactions;
    uint64_t bytes;
    gpu_shared_access_type_t type;
} gpu_shared_access_t;


typedef struct gpu_branch {
    uint32_t diverged;
    uint32_t executed;
} gpu_branch_t;


// synchronization
typedef struct gpu_synchronization {
    uint64_t start;
    uint64_t end;
    uint32_t syncKind;
} gpu_synchronization_t;


typedef struct entry_data {
    union {
        gpu_pc_sampling_t pc_sampling;
        gpu_pc_sampling_record_info_t pc_sampling_record_info;
        gpu_memcpy_t memcpy;
        gpu_memory_t memory;
        gpu_memset_t memset;
        gpu_kernel_t kernel;
        gpu_global_access_t global_access;
        gpu_shared_access_t shared_access;
        gpu_branch_t branch;
        gpu_synchronization_t synchronization;
    };
} entry_data_t;


typedef struct gpu_activity_t {
    union {
        struct {
            uint32_t op;
            uint32_t domain;
        } roctracer_kind;
        struct {
            uint32_t kind;
        } cupti_kind;
    };
    entry_data_t* data;
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



#endif
