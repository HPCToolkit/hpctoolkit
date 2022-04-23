// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
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

#include "gpu_ctxt_actions.h"

#include <messages/messages.h>

// ******** CUDA API *************
#include <cuda.h>
#include <cuda_runtime.h>
// *******************************************************

// ****************** utility macros *********************
#define Cuda_RTcall(fn) cudaRuntimeFunctionPointer[fn##Enum].fn##Real
#define Cuda_Dcall(fn)  cuDriverFunctionPointer[fn##Enum].fn##Real
// *******************************************************

// ****************Atomic ops*****************************
#include "lib/prof-lean/stdatomic.h"
// *******************************************************

// ********** (table of) cuda API functions **************
#include "gpu_blame-cuda-driver-header.h"
#include "gpu_blame-cuda-runtime-header.h"
// *******************************************************

// keep count of # of contexts
// (If more than 1 simultaneous context is created, that is bad)
static uint64_t ncontexts = 0L;

// reset context counter to 0
void cuda_ncontexts_reset(void) {
  fetch_and_store(&ncontexts, 0);
}

uint64_t cuda_ncontexts_incr(void) {
  atomic_add_i64(&ncontexts, 1L);
  return ncontexts;
}

uint64_t cuda_ncontexts_decr(void) {
  atomic_add_i64(&ncontexts, -1L);
  return ncontexts;
}

struct cuda_ctxt_t {
  CUcontext ctxt;
} local_ctxt;

cuda_ctxt_t* cuda_capture_ctxt(void) {
  if (!Cuda_Dcall(cuCtxGetCurrent)) {
    EMSG("??? Called cuCtxGetCurrent, but fn ptr not set ???");
    return NULL;
  }
  Cuda_Dcall(cuCtxGetCurrent)(&local_ctxt.ctxt);
  return &local_ctxt;
}

void cuda_set_ctxt(cuda_ctxt_t* ctxt) {
  if (!Cuda_Dcall(cuCtxSetCurrent)) {
    EMSG("??? Called cuCtxSetCurrent, but fn ptr not set ???");
    return;
  }
  Cuda_Dcall(cuCtxSetCurrent)(ctxt->ctxt);
  local_ctxt = *ctxt;
}

void* cuda_get_handle(cuda_ctxt_t* ctxt) {
  return (void*)(ctxt->ctxt);
}
