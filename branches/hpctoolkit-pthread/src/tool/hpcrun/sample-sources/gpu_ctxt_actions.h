#ifndef GPU_CTXT_ACTIONS
#define GPU_CTXT_ACTIONS

#include <stdint.h>

// opaque data type -- so that clients do not have to include entire gpu
// API
typedef struct cuda_ctxt_t cuda_ctxt_t;

// Opaque, atomic context counter ops
// reset context counter to 0
extern void cuda_ncontexts_reset(void);
extern uint64_t cuda_ncontexts_incr(void);
extern uint64_t cuda_ncontexts_decr(void);

// NOTE: These functions have hidden side effect !!!
extern cuda_ctxt_t* cuda_capture_ctxt(void);
extern void cuda_set_ctxt(cuda_ctxt_t* ctxt);
extern void* cuda_get_handle(cuda_ctxt_t* ctxt);
extern void cuda_ncontexts_reset(void);
#endif // GPU_CTXT_ACTIONS
