// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// local includes
//******************************************************************************

#include "roctracer-activity-translate.h"

#define DEBUG 0

#include <hpcrun/gpu/gpu-print.h>

//******************************************************************************
// private operations
//******************************************************************************

static void
convert_kernel_launch
(
 gpu_activity_t *ga,
 roctracer_record_t *activity
)
{
  ga->kind = GPU_ACTIVITY_KERNEL;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.kernel.correlation_id = activity->correlation_id;
}


static void
convert_memcpy
(
 gpu_activity_t *ga,
 roctracer_record_t *activity,
 gpu_memcpy_type_t kind
)
{
  ga->kind = GPU_ACTIVITY_MEMCPY;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.memcpy.correlation_id = activity->correlation_id;
  ga->details.memcpy.copyKind = kind;
}


static void
convert_memset
(
 gpu_activity_t *ga,
 roctracer_record_t *activity
)
{
  ga->kind = GPU_ACTIVITY_MEMSET;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.memset.correlation_id = activity->correlation_id;
}


static void
convert_sync
(
 gpu_activity_t *ga,
 roctracer_record_t *activity,
 gpu_sync_type_t syncKind
)
{
  ga->kind = GPU_ACTIVITY_SYNCHRONIZATION;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.synchronization.syncKind = syncKind;
  ga->details.synchronization.correlation_id = activity->correlation_id;
}


static void
convert_unknown
(
 gpu_activity_t *ga
)
{
  ga->kind = GPU_ACTIVITY_UNKNOWN;
}



//******************************************************************************
// interface operations
//******************************************************************************

void
roctracer_activity_translate
(
 gpu_activity_t *ga,
 roctracer_record_t *record
)
{
#if DEBUG
  const char * name = roctracer_op_string(record->domain, record->op, record->kind);
#endif
  memset(ga, 0, sizeof(gpu_activity_t));

  if (record->domain == ACTIVITY_DOMAIN_HIP_API) {
    switch(record->op){
    case HIP_API_ID_hipMemcpyDtoD:
    case HIP_API_ID_hipMemcpyDtoDAsync:
      convert_memcpy(ga, record, GPU_MEMCPY_D2D);
      break;
    case HIP_API_ID_hipMemcpy2DToArray:
      convert_memcpy(ga, record, GPU_MEMCPY_D2D);
      break;
    case HIP_API_ID_hipMemcpyAtoH:
      convert_memcpy(ga, record, GPU_MEMCPY_A2H);
      break;
    case HIP_API_ID_hipMemcpyHtoD:
    case HIP_API_ID_hipMemcpyHtoDAsync:
      convert_memcpy(ga, record, GPU_MEMCPY_H2D);
      break;
    case HIP_API_ID_hipMemcpyHtoA:
      convert_memcpy(ga, record, GPU_MEMCPY_H2A);
      break;
    case HIP_API_ID_hipMemcpyDtoH:
    case HIP_API_ID_hipMemcpyDtoHAsync:
      convert_memcpy(ga, record, GPU_MEMCPY_D2H);
      break;
    case HIP_API_ID_hipMemcpyAsync:
    case HIP_API_ID_hipMemcpyFromSymbol:
    case HIP_API_ID_hipMemcpy3D:
    case HIP_API_ID_hipMemcpy2D:
    case HIP_API_ID_hipMemcpyPeerAsync:
    case HIP_API_ID_hipMemcpyFromArray:
    case HIP_API_ID_hipMemcpy2DAsync:
    case HIP_API_ID_hipMemcpyToArray:
    case HIP_API_ID_hipMemcpyToSymbol:
    case HIP_API_ID_hipMemcpyPeer:
    case HIP_API_ID_hipMemcpyParam2D:
    case HIP_API_ID_hipMemcpy:
    case HIP_API_ID_hipMemcpyToSymbolAsync:
    case HIP_API_ID_hipMemcpyFromSymbolAsync:
      convert_memcpy(ga, record, GPU_MEMCPY_UNK);
      break;
    case HIP_API_ID_hipModuleLaunchKernel:
    case HIP_API_ID_hipLaunchCooperativeKernel:
    case HIP_API_ID_hipHccModuleLaunchKernel:
    case HIP_API_ID_hipExtModuleLaunchKernel:
    case HIP_API_ID_hipLaunchKernel:
      convert_kernel_launch(ga, record);
      break;
    case HIP_API_ID_hipCtxSynchronize:
      convert_sync(ga, record, GPU_SYNC_UNKNOWN);
      break;
    case HIP_API_ID_hipStreamSynchronize:
      convert_sync(ga, record, GPU_SYNC_STREAM);
      break;
    case HIP_API_ID_hipStreamWaitEvent:
      convert_sync(ga, record, GPU_SYNC_STREAM_EVENT_WAIT);
      break;
    case HIP_API_ID_hipDeviceSynchronize:
      convert_sync(ga, record, GPU_SYNC_UNKNOWN);
      break;
    case HIP_API_ID_hipEventSynchronize:
      convert_sync(ga, record, GPU_SYNC_EVENT);
      break;
    case HIP_API_ID_hipMemset3DAsync:
    case HIP_API_ID_hipMemset2D:
    case HIP_API_ID_hipMemset2DAsync:
    case HIP_API_ID_hipMemset:
    case HIP_API_ID_hipMemsetD8:
    case HIP_API_ID_hipMemset3D:
    case HIP_API_ID_hipMemsetAsync:
    case HIP_API_ID_hipMemsetD32Async:
      convert_memset(ga, record);
      break;
    default:
      convert_unknown(ga);
      PRINT("roctracer buffer event: Unhandled HIP API activity %s\n", name);
    }
  } else if (record->domain == ACTIVITY_DOMAIN_HIP_OPS) {
    // Async HIP events
    switch (record->op) {

    }
  } else {
    convert_unknown(ga);
    PRINT("roctracer buffer enent: Unhandled activity %s, domain %u, op %u, kind %u\n", 
      name, record->domain, record->op, record->kind);
  }
  cstack_ptr_set(&(ga->next), 0);
}
