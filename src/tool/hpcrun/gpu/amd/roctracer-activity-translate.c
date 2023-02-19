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
// Copyright ((c)) 2002-2023, Rice University
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
// macros
//******************************************************************************

#define TRACK_SYNCHRONIZATION 0
#define DEBUG 0

// version number from ROCm 5.3.3 release using hipRuntimeGetVersion
#define ROCM_533_RELEASE_VERSION 50322062


//******************************************************************************
// ROCm includes
//******************************************************************************

#include <rocm_version.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-print.h>
#include <hpcrun/gpu/gpu-kernel-table.h>

#include "roctracer-activity-translate.h"
#include "hip/hip_runtime_api.h"


//******************************************************************************
// private operations
//******************************************************************************

static ip_normalized_t
lookup_kernel_pc
(
 roctracer_record_t *activity
)
{
  // if ROCm version < 5.3.3, roctracer_record_t doesn't have the
  // kernel_name field. return ip_normalized_NULL in such cases
#if ((ROCM_VERSION_MAJOR < 5) ||					\
     ((ROCM_VERSION_MAJOR == 5) &&					\
      ((ROCM_VERSION_MINOR < 3) ||					\
       ((ROCM_VERSION_MINOR == 3) && (ROCM_VERSION_PATCH < 3)))))
#warning "roctracer activity records in ROCm version < 5.3.3 don't support mapping costs to named kernels"
#else
  // If we are executing this code, hpctoolkit was compiled with ROCm
  // 5.3.3 or later.

  // For ROCm versions >= 5.3.3, AMD recommends obtaining the kernel
  // name from roctracer's activity API rather than the subscriber
  // callback.

  int version;
  if (hip_version(&version) == 0) {
    // A roctracer_record_t only has a kernel_name field for ROCm
    // version 5.3.3 or higher. However, we may be monitoring a code
    // compiled with an older version of ROCm. Only inspect the
    // kernel_name field if built with ROCm 5.3.3 or later.
    if (version >= ROCM_533_RELEASE_VERSION) {
      // Use the kernel name alone as the kernel PC.
      return gpu_kernel_table_get(activity->kernel_name, LOGICAL_MANGLING_CPP);
    }
  }
#endif
  // if the kernel name is not available to from the activity API,
  // there is no way to associate the kernel invocation with the
  // binary, so we don't try.
  return ip_normalized_NULL;
}


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
  ga->details.kernel.context_id = activity->device_id;
  ga->details.kernel.stream_id = activity->queue_id;
  ga->details.kernel.kernel_first_pc = lookup_kernel_pc(activity);
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
  ga->details.memcpy.context_id = activity->device_id;
  ga->details.memcpy.stream_id = activity->queue_id;
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
  ga->details.memset.context_id = activity->device_id;
  ga->details.memset.stream_id = activity->queue_id;
}


#if TRACK_SYNCHRONIZATION
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
  ga->details.synchronization.context_id = activity->device_id;
  ga->details.synchronization.stream_id = activity->queue_id;
}
#endif


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
  const char * name = roctracer_op_string(record->domain, record->op, record->kind);

  gpu_activity_init(ga);

  // HACK HACK HACK
  // decrease end timestamp to avoid abutting events
  // compensate for hpcviewer's arbitrary rendering of zero-length intervals
  // HACK HACK HACK
  if (record->end_ns - record->begin_ns > 1) {
    record->end_ns--;
  }

  if (record->domain == ACTIVITY_DOMAIN_HIP_OPS) {
    // TODO: I cannot find the corresponding enum in AMD toolchain
    // to identify different types of activities.
    // At this point, match the string name
    if (strcmp(name, "KernelExecution") == 0) {
      convert_kernel_launch(ga, record);
    } else if (strcmp(name, "CopyDeviceToDevice") == 0) {
      convert_memcpy(ga, record, GPU_MEMCPY_D2D);
    } else if (strcmp(name, "CopyDeviceToHost") == 0) {
      convert_memcpy(ga, record, GPU_MEMCPY_D2H);
    } else if (strcmp(name, "CopyHostToDevice") == 0) {
      convert_memcpy(ga, record, GPU_MEMCPY_H2D);
    } else if (strcmp(name, "FillBuffer") == 0) {
      convert_memset(ga, record);
    } else {
      convert_unknown(ga);
      PRINT("roctracer buffer event: Unhandled HIP OPS activity %s, duration %d\n", name, record->end_ns - record->begin_ns);
    }
  } else if (record->domain == ACTIVITY_DOMAIN_HIP_API) {
    // Ignore API tracing records
    convert_unknown(ga);
  } else {
    convert_unknown(ga);
    PRINT("roctracer buffer enent: Unhandled activity %s, domain %u, op %u, kind %u\n",
      name, record->domain, record->op, record->kind);
  }
  cstack_ptr_set(&(ga->next), 0);
}
