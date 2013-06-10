#include "gpu_ctxt_actions.h"

#include <cuda.h>
#include <cuda_runtime.h>

// ****************** utility macros *********************
#define Cuda_RTcall(fn) cudaRuntimeFunctionPointer[fn ## Enum].fn ## Real
#define Cuda_Dcall(fn)  cuDriverFunctionPointer[fn ## Enum].fn ## Real
// *******************************************************

// ********** (table of) cuda API functions **************
#include "gpu_blame-cuda-runtime-header.h"
#include "gpu_blame-cuda-driver-header.h"
// *******************************************************
static CUcontext the_ctxt = NULL;

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
