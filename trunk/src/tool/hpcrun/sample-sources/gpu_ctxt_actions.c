#include <messages/messages.h>
#include "gpu_ctxt_actions.h"

// ******** CUDA API *************
#include <cuda.h>
#include <cuda_runtime.h>
// *******************************************************

// ****************** utility macros *********************
#define Cuda_RTcall(fn) cudaRuntimeFunctionPointer[fn ## Enum].fn ## Real
#define Cuda_Dcall(fn)  cuDriverFunctionPointer[fn ## Enum].fn ## Real
// *******************************************************

// ****************Atomic ops*****************************
#include <lib/prof-lean/atomic.h>
// *******************************************************


// ********** (table of) cuda API functions **************
#include "gpu_blame-cuda-runtime-header.h"
#include "gpu_blame-cuda-driver-header.h"
// *******************************************************

// keep count of # of contexts
// (If more than 1 simultaneous context is created, that is bad)
static uint64_t ncontexts = 0L;

// reset context counter to 0
void
cuda_ncontexts_reset(void)
{
  fetch_and_store(&ncontexts, 0);
}

uint64_t 
cuda_ncontexts_incr(void)
{
  atomic_add_i64(&ncontexts, 1L);
  return ncontexts;
}

uint64_t
cuda_ncontexts_decr(void)
{
  atomic_add_i64(&ncontexts, -1L);
  return ncontexts;
}


struct cuda_ctxt_t {
  CUcontext ctxt;
} local_ctxt;

cuda_ctxt_t*
cuda_capture_ctxt(void)
{
  if (! Cuda_Dcall(cuCtxGetCurrent)) {
    EMSG("??? Called cuCtxGetCurrent, but fn ptr not set ???");
    return NULL;
  }
  Cuda_Dcall(cuCtxGetCurrent)(&local_ctxt.ctxt);
  return &local_ctxt;
}

void
cuda_set_ctxt(cuda_ctxt_t* ctxt)
{
  if (! Cuda_Dcall(cuCtxSetCurrent)) {
    EMSG("??? Called cuCtxSetCurrent, but fn ptr not set ???");
    return;
  }
  Cuda_Dcall(cuCtxSetCurrent)(ctxt->ctxt);
  local_ctxt = *ctxt;
}

void*
cuda_get_handle(cuda_ctxt_t* ctxt)
{
  return (void*) (ctxt->ctxt);
}
