// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

//******************************************************************************
// macros
//******************************************************************************

#define TRACK_SYNCHRONIZATION 0
#define DEBUG 0



//******************************************************************************
// local includes
//******************************************************************************

#include "../../common/gpu-print.h"
#include "../common/gpu-kernel-table.h"
#include "../../../foil/roctracer.h"

#include "roctracer-activity-translate.h"
#include <hip/hip_runtime_api.h>


//******************************************************************************
// private operations
//******************************************************************************

static ip_normalized_t
lookup_kernel_pc
(
 roctracer_record_t *activity
)
{
  // For modern ROCm versions, AMD recommends obtaining the kernel
  // name from roctracer's activity API rather than the subscriber
  // callback.

  // The 'kernel_name' field is only available in ROCm 5.3.0+.
  // Unfortunately, AMD hasn't seen fit to change the roctracer
  // version number to account for this change. The roctracer version
  // number has been 4.1 since ROCm 5.1.0.

  // The 'bytes' field in a roctracer_record_t is at the same offset
  // in the structure as 'kernel_name' (i.e. in a union) in ROCm
  // 5.3.0+. In older ROCm versions, 'kernel_name' isn't a field.
  // However, bytes "should probably" be 0 (say some AMD folks) for
  // kernel launch records in older ROCm versions. Counting on this is
  // a terrible way to achieve cross version compatibility! However,
  // checking the HIP runtime version is out of the question because
  // under some circumstances calling hipRuntimeGetVersion acquires locks
  // and creates threads.

  const char *kernel_name = 0;

  // We use a configure test here to check for the availability of the
  // kernel_name field in roctracer_record_t because AMD didn't tick
  // the roctracer version when it was added and we can't check
  // include the rocm_version.h file to check the version of ROCm
  // because someone at AMD neglected to create a spack package for
  // rocm-core, which provides rocm_version.h
#ifdef HAVE_ROCM_ACTIVITY_KERNEL_NAME
  kernel_name = activity->kernel_name;
#endif

  if (kernel_name) {
    return gpu_kernel_table_get(kernel_name, LOGICAL_MANGLING_CPP);
  }

  // If the kernel name is not available from the activity API,
  // there is no way to associate the kernel invocation with the
  // binary, so we don't try.
  return ip_normalized_NULL;
}


static void
convert_kernel_launch
(
 gpu_activity_t *ga,
 roctracer_record_t *activity,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_KERNEL;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.kernel.correlation_id = activity->correlation_id;
  ga->details.kernel.context_id = activity->device_id;
  ga->details.kernel.stream_id = activity->queue_id;
  ga->details.kernel.kernel_first_pc = lookup_kernel_pc(activity);

  *correlation_id = activity->correlation_id;
}


static void
convert_memcpy
(
 gpu_activity_t *ga,
 roctracer_record_t *activity,
 gpu_memcpy_type_t kind,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_MEMCPY;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.memcpy.correlation_id = activity->correlation_id;
  ga->details.memcpy.copyKind = kind;
  ga->details.memcpy.context_id = activity->device_id;
  ga->details.memcpy.stream_id = activity->queue_id;

  *correlation_id = activity->correlation_id;
}


static void
convert_memset
(
 gpu_activity_t *ga,
 roctracer_record_t *activity,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_MEMSET;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.memset.correlation_id = activity->correlation_id;
  ga->details.memset.context_id = activity->device_id;
  ga->details.memset.stream_id = activity->queue_id;

  *correlation_id = activity->correlation_id;
}


#if TRACK_SYNCHRONIZATION
static void
convert_sync
(
 gpu_activity_t *ga,
 roctracer_record_t *activity,
 gpu_sync_type_t syncKind,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_SYNCHRONIZATION;
  gpu_interval_set(&ga->details.interval, activity->begin_ns, activity->end_ns);
  ga->details.synchronization.syncKind = syncKind;
  ga->details.synchronization.correlation_id = activity->correlation_id;
  ga->details.synchronization.context_id = activity->device_id;
  ga->details.synchronization.stream_id = activity->queue_id;

  *correlation_id = activity->correlation_id;
}
#endif


static void
convert_unknown
(
 gpu_activity_t *ga,
 roctracer_record_t *record,
 uint64_t *correlation_id
)
{
  ga->kind = GPU_ACTIVITY_UNKNOWN;

  *correlation_id = 0;
}



//******************************************************************************
// interface operations
//******************************************************************************

void
roctracer_activity_translate
(
 gpu_activity_t *ga,
 roctracer_record_t *record,
 uint64_t *correlation_id
)
{
  const char * name = f_roctracer_op_string(record->domain, record->op, record->kind);

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
      convert_kernel_launch(ga, record, correlation_id);
    } else if (strcmp(name, "CopyDeviceToDevice") == 0) {
      convert_memcpy(ga, record, GPU_MEMCPY_D2D, correlation_id);
    } else if (strcmp(name, "CopyDeviceToHost") == 0) {
      convert_memcpy(ga, record, GPU_MEMCPY_D2H, correlation_id);
    } else if (strcmp(name, "CopyHostToDevice") == 0) {
      convert_memcpy(ga, record, GPU_MEMCPY_H2D, correlation_id);
    } else if (strcmp(name, "FillBuffer") == 0) {
      convert_memset(ga, record, correlation_id);
    } else {
      convert_unknown(ga, record, correlation_id);
      PRINT("roctracer buffer event: Unhandled HIP OPS activity %s, duration %d\n", name, record->end_ns - record->begin_ns);
    }
  } else if (record->domain == ACTIVITY_DOMAIN_HIP_API) {
    // Ignore API tracing records
    convert_unknown(ga, record, correlation_id);
  } else {
    convert_unknown(ga, record, correlation_id);
    PRINT("roctracer buffer enent: Unhandled activity %s, domain %u, op %u, kind %u\n",
      name, record->domain, record->op, record->kind);
  }
  cstack_ptr_set(&(ga->next), 0);
}
