// -*-Mode, CUPTI_CB_DOMAIN_DRIVER_API)); C++;-*- // technically C99

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
// met, CUPTI_CB_DOMAIN_DRIVER_API));
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

//***************************************************************************
//
// File, CUPTI_CB_DOMAIN_DRIVER_API));
//   cupti-subscribers.c
//
// Purpose, CUPTI_CB_DOMAIN_DRIVER_API));
//   Register CUPTI callbacks for interesting CUDA APIs
//
//***************************************************************************


#include "cupti-subscribers.h"
#include "cupti-api.h"

#define DISPATCH_CALLBACK(enable, args) \
  if (enable == 0) { \
    cupti_callback_disable args; \
  } else { \
    cupti_callback_enable args; \
  }

#define REGISTER_DRIVER_CALLBACK(CB_ID) \
  DISPATCH_CALLBACK(enable, (subscriber, CB_ID, CUPTI_CB_DOMAIN_DRIVER_API))

#define REGISTER_RUNTIME_CALLBACK(CB_ID) \
  DISPATCH_CALLBACK(enable, (subscriber, CB_ID, CUPTI_CB_DOMAIN_RUNTIME_API))

#define REGISTER_RESOURCE_CALLBACK(CB_ID) \
  DISPATCH_CALLBACK(enable, (subscriber, CB_ID, CUPTI_CB_DOMAIN_RESOURCE))

#define CASE_SEQUENCE(CB_ID) \
  case CB_ID: 

#define FOREACH_DRIVER_MEMCPY_H2D(macro) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyHtoD) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyHtoDAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2_ptsz)

#define FOREACH_DRIVER_MEMCPY_D2H(macro) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoH) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoHAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2_ptsz)

#define FOREACH_DRIVER_MEMCPY(macro) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer_ptds) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoD) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoA) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyAtoD) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64Memcpy3D) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoDAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64Memcpy3DAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64Memcpy2D) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64Memcpy2DUnaligned) \
  macro(CUPTI_DRIVER_TRACE_CBID_cu64Memcpy2DAsync)

#define FOREACH_DRIVER_SYNC(macro) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuCtxSynchronize) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuEventSynchronize) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent_ptsz)

#define FOREACH_DRIVER_KERNEL(macro) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunch) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchGrid) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchGridAsync) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel_ptsz) \
  macro(CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice)

#define FOREACH_RUNTIME_MEMCPY(macro) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeer_v4000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeerAsync_v4000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_v4000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_v4000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_ptds_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_ptsz_v7000)

#define FOREACH_RUNTIME_SYNC(macro) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaEventSynchronize_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaStreamWaitEvent_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaDeviceSynchronize_v3020)

#define FOREACH_RUNTIME_KERNEL(macro) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_ptsz_v7000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_v9000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_ptsz_v9000) \
  macro(CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernelMultiDevice_v9000)

void
cupti_subscribers_driver_memcpy_htod_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_DRIVER_MEMCPY_H2D(REGISTER_DRIVER_CALLBACK)
}


void
cupti_subscribers_driver_memcpy_dtoh_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_DRIVER_MEMCPY_D2H(REGISTER_DRIVER_CALLBACK)
}


void
cupti_subscribers_driver_memcpy_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_DRIVER_MEMCPY(REGISTER_DRIVER_CALLBACK)
}


void
cupti_subscribers_driver_sync_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_DRIVER_SYNC(REGISTER_DRIVER_CALLBACK)
}


void
cupti_subscribers_driver_kernel_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_DRIVER_KERNEL(REGISTER_DRIVER_CALLBACK)
}


void
cupti_subscribers_runtime_memcpy_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_RUNTIME_MEMCPY(REGISTER_RUNTIME_CALLBACK)
}


void
cupti_subscribers_runtime_sync_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_RUNTIME_SYNC(REGISTER_RUNTIME_CALLBACK)
}


void
cupti_subscribers_runtime_kernel_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  FOREACH_RUNTIME_KERNEL(REGISTER_RUNTIME_CALLBACK)
}


void
cupti_subscribers_resource_module_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  REGISTER_RESOURCE_CALLBACK(CUPTI_CBID_RESOURCE_MODULE_LOADED)
  REGISTER_RESOURCE_CALLBACK(CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING)
}


void
cupti_subscribers_resource_context_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  REGISTER_RESOURCE_CALLBACK(CUPTI_CBID_RESOURCE_CONTEXT_CREATED)
  REGISTER_RESOURCE_CALLBACK(CUPTI_CBID_RESOURCE_CONTEXT_DESTROY_STARTING)
}

gpu_op_placeholder_flags_t 
cupti_runtime_flags_get
(
 CUpti_CallbackId cb_id
)
{
  gpu_op_placeholder_flags_t flags = 0;
  switch (cb_id) {
    FOREACH_RUNTIME_KERNEL(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_kernel);
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_trace);
      break;
    }
    FOREACH_RUNTIME_MEMCPY(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_copy);
      break;
    }
    FOREACH_DRIVER_SYNC(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_sync);
      break;
    }
    default:
    {
      break;
    }
  }

  return flags;
}


gpu_op_placeholder_flags_t 
cupti_driver_flags_get
(
 CUpti_CallbackId cb_id
)
{
  gpu_op_placeholder_flags_t flags = 0;
  switch (cb_id) {
    FOREACH_DRIVER_KERNEL(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_kernel);
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_trace);
      break;
    }
    FOREACH_DRIVER_MEMCPY_H2D(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_copyin);
      break;
    }
    FOREACH_DRIVER_MEMCPY_D2H(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_copyout);
      break;
    }
    FOREACH_DRIVER_MEMCPY(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_copy);
      break;
    }
    FOREACH_DRIVER_SYNC(CASE_SEQUENCE)
    {
      gpu_op_placeholder_flags_set(&flags, gpu_placeholder_type_sync);
      break;
    }
    default:
    {
      break;
    }
  }

  return flags;
}
