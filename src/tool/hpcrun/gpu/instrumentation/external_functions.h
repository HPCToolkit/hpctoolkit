#include <hpcrun/gpu/gpu-correlation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/cct/cct.h>

#include <hpcrun/safe-sampling.h>
#include <include/gpu-binary.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-operation-multiplexer.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>
#include <hpcrun/gpu/gpu-metrics.h>

#include <gtpin.h>

typedef struct external_functions_t
{
    void (*gpu_binary_path_generate)(
        const char *,
        char *);
    void (*gpu_operation_multiplexer_push)(
        gpu_activity_channel_t *,
        atomic_int *,
        gpu_activity_t *);
    block_metrics_t (*fetch_block_metrics)(cct_node_t *);
    void (*get_cct_node_id)(cct_node_t *, uint16_t *, uintptr_t *);
    void (*cstack_ptr_set)(
        s_element_ptr_t *,
        s_element_t *);
    uint64_t (*hpcrun_nanotime)();
    void (*hpcrun_thread_init_mem_pool_once)(
        int,
        cct_ctxt_t *,
        hpcrun_trace_type_t,
        bool);
    void (*attribute_instruction_metrics)(cct_node_t *, instruction_metrics_t);
    gpu_activity_channel_t *(*gpu_activity_channel_get)(
        void);
    int (*crypto_compute_hash_string)(
        const void *,
        size_t,
        char *,
        unsigned int);
    cct_node_t *(*hpcrun_cct_insert_instruction_child)(cct_node_t *, int32_t);
    cct_node_t *(*gpu_op_ccts_get)(
        gpu_op_ccts_t *,
        gpu_placeholder_type_t);
    uint32_t (*gpu_binary_loadmap_insert)(
        const char *,
        bool);
    bool (*gpu_binary_store)(
        const char *,
        const void *,
        size_t);
    cct_node_t *(*hpcrun_cct_insert_ip_norm)(cct_node_t *, ip_normalized_t, bool);
    void (*hpcrun_cct_walk_node_1st)(cct_node_t *,
                                     cct_op_t, cct_op_arg_t);
} external_functions_t;
