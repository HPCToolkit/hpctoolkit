//
// Created by aleksa on 8/16/19.
//

#ifndef HPCTOOLKIT_GPU_RECORD_H
#define HPCTOOLKIT_GPU_RECORD_H

#include <lib/prof-lean/bi_unordered_channel.h>
#include <lib/prof-lean/stdatomic.h>
//#include <cupti_activity.h>
#include <hpcrun/cct/cct.h>

/*#include <roctracer_hip.h>
#include <roctracer_hcc.h>*/

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


typedef struct entry_activity {
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
} entry_activity_t;

// notification entry
typedef struct entry_correlation {
    uint64_t host_op_id;
    cct_node_t *cct_node;
    entry_data_t *data;
    void *record;
} entry_correlation_t;


typedef struct {
    s_element_ptr_t next;
    entry_correlation_t node;
} typed_stack_elem(entry_correlation_t);

typedef struct {
    s_element_ptr_t next;
    entry_activity_t node;
} typed_stack_elem(entry_activity_t);

typedef void (*gpu_fn_t)(entry_activity_t *node);

typedef bi_unordered_channel_t typed_bi_unordered_channel(entry_activity_t);
typedef bi_unordered_channel_t typed_bi_unordered_channel(entry_correlation_t);

typed_bi_unordered_channel_declare(entry_correlation_t)
typed_bi_unordered_channel_declare(entry_activity_t)

typedef typed_stack_elem(entry_correlation_t) correlation_elem;
typedef typed_stack_elem(entry_activity_t) activity_elem;

typedef typed_bi_unordered_channel(entry_activity_t) activity_channel_t;
typedef typed_bi_unordered_channel(entry_correlation_t) correlation_channel_t;

//typedef void (*cupti_stack_fn_t)(cupti_node_t *node);

#define correlation_bi_unordered_channel_pop typed_bi_unordered_channel_pop(entry_correlation_t)
#define correlation_bi_unordered_channel_push typed_bi_unordered_channel_push(entry_correlation_t)
#define correlation_bi_unordered_channel_steal typed_bi_unordered_channel_steal(entry_correlation_t)

#define activity_bi_unordered_channel_pop typed_bi_unordered_channel_pop(entry_activity_t)
#define activity_bi_unordered_channel_push typed_bi_unordered_channel_push(entry_activity_t)
#define activity_bi_unordered_channel_steal typed_bi_unordered_channel_steal(entry_activity_t)

// record type
typedef struct gpu_record {
    activity_channel_t activity_channel;
    correlation_channel_t correlation_channel;
} gpu_record_t;


// a list of thread records - cupti
typedef struct gpu_record_list {
    _Atomic(struct gpu_record_list *) next;
    gpu_record_t *record;
} gpu_record_list_t;


// apply functions
// cupti_worker_activity_apply
void
activities_consume
(
    gpu_fn_t fn
);

// worker_notification_apply
void
correlation_produce
(
     uint64_t host_op_id,
     cct_node_t *cct_node,
     entry_data_t *entry_data
);
//cupti-activ




// cupti-notif
void
correlations_consume
(

);

// getters
// called in target_callback before all events
void
gpu_worker_record_init
(
);

void
gpu_record_init
(
);


gpu_record_t *
gpu_record_get
(
);


#endif //HPCTOOLKIT_GPU_RECORD_H
