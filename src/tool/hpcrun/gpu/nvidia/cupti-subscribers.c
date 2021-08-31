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
//   cupti-api-subscribers.c
//
// Purpose, CUPTI_CB_DOMAIN_DRIVER_API));
//   Register CUPTI callbacks for interesting CUDA APIs
//
//***************************************************************************

#include "cupti-subscribers.h"
#include "cupti-api.h"


#define DISPATCH_CALLBACK(enable, args) \
  if (enable == 0) { \
    cupti_callback_enable args; \
  } else { \
    cupti_callback_disable args; \
  }

void
cupti_driver_memcpy_htod_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
}


void
cupti_driver_memcpy_dtoh_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
}


void
cupti_driver_memcpy_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer_ptds, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
}


void
cupti_driver_sync_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuCtxSynchronize, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuEventSynchronize, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
}


void
cupti_driver_kernel_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunch, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchGrid, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchGridAsync, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel_ptsz, CUPTI_CB_DOMAIN_DRIVER_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice, CUPTI_CB_DOMAIN_DRIVER_API));
}


void
cupti_runtime_memcpy_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeer_v4000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeerAsync_v4000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_v4000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_v4000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_ptds_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
}


void
cupti_runtime_sync_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaEventSynchronize_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaStreamWaitEvent_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaDeviceSynchronize_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
}


void
cupti_runtime_kernel_callbacks_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_ptsz_v7000, CUPTI_CB_DOMAIN_RUNTIME_API));

#if CUPTI_API_VERSION >= 10
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_v9000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_ptsz_v9000, CUPTI_CB_DOMAIN_RUNTIME_API));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernelMultiDevice_v9000, CUPTI_CB_DOMAIN_RUNTIME_API));
#endif
}


void
cupti_resource_module_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_CBID_RESOURCE_MODULE_LOADED, CUPTI_CB_DOMAIN_RESOURCE));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING, CUPTI_CB_DOMAIN_RESOURCE));
}


void
cupti_resource_context_subscribe
(
 uint32_t enable,
 CUpti_SubscriberHandle subscriber
)
{
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_CBID_RESOURCE_CONTEXT_CREATED, CUPTI_CB_DOMAIN_RESOURCE));
  DISPATCH_CALLBACK(enable, (subscriber, CUPTI_CBID_RESOURCE_CONTEXT_DESTROY_STARTING, CUPTI_CB_DOMAIN_RESOURCE));
}

