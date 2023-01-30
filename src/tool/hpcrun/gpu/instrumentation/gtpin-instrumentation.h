
#ifdef __cplusplus
extern "C"
{
#endif

#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/cct/cct.h>
#include "external_functions.h"

    void gtpin_instrumentation_options(gpu_instrumentation_t *);
    void gtpin_produce_runtime_callstack(gpu_op_ccts_t *);
    void process_block_instructions(cct_node_t *);
    void init_external_functions(external_functions_t *);

#ifdef __cplusplus
};
#endif
