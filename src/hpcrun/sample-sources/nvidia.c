// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// CUPTI synchronous sampling via PAPI sample source simple oo interface
//

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <dlfcn.h>



//******************************************************************************
// libmonitor includes
//******************************************************************************

#include "../libmonitor/monitor.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "libdl.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "nvidia.h"

#include "../control-knob.h"

#include "../gpu/gpu-metrics.h"
#include "../gpu/common/gpu-monitoring.h"

#include "../gpu/trace/gpu-trace-api.h"
#include "../gpu/api/nvidia/cuda-api.h"
#include "../gpu/api/nvidia/cupti-api.h"

#include "../audit/audit-api.h"
#include "../messages/messages.h"
#include "../device-finalizers.h"
#include "../sample_sources_registered.h"
#include "../utilities/tokenize.h"
#include "../thread_data.h"
#include "../trace.h"



/******************************************************************************
 * macros
 *****************************************************************************/

#define NVIDIA_CUDA "gpu=nvidia"
#define NVIDIA_CUDA_PC_SAMPLING "gpu=nvidia,pc"
#define NVIDIA_CUDA_NV_LINK "nvlink"


/******************************************************************************
 * local variables
 *****************************************************************************/

// finalizers
static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;
static device_finalizer_fn_entry_t device_trace_finalizer_shutdown;


// default trace all the activities
// -1: disabled, >0: x ms per activity
// static long trace_frequency = -1;
static long trace_frequency = -1;
static long trace_frequency_default = -1;


// -1: disabled, 5-31: 2^frequency
static long pc_sampling_frequency = -1;
static long pc_sampling_frequency_default = 12;


static int cupti_enabled_activities = 0;

// event name, which is nvidia-cuda
static char nvidia_name[128];

//******************************************************************************
// constants
//******************************************************************************

CUpti_ActivityKind
external_correlation_activities[] = {
  CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
data_motion_explicit_activities[] = {
  CUPTI_ACTIVITY_KIND_MEMCPY2,
  CUPTI_ACTIVITY_KIND_MEMCPY,
  CUPTI_ACTIVITY_KIND_MEMSET,
// FIXME(keren): memory activity does not have a correlation id
// CUPTI_ACTIVITY_KIND_MEMORY,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
data_motion_implicit_activities[] = {
  CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
kernel_invocation_activities[] = {
  CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL,
  // CUPTI currently provides host side time stamps for synchronization.
  //CUPTI_ACTIVITY_KIND_SYNCHRONIZATION,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
kernel_execution_activities[] = {
  CUPTI_ACTIVITY_KIND_CONTEXT,
  CUPTI_ACTIVITY_KIND_FUNCTION,
// FIXME(keren): cupti does not allow the following activities to be profiled with pc sampling
// CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS,
// CUPTI_ACTIVITY_KIND_SHARED_ACCESS,
// CUPTI_ACTIVITY_KIND_BRANCH,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
overhead_activities[] = {
  CUPTI_ACTIVITY_KIND_OVERHEAD,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
driver_activities[] = {
  CUPTI_ACTIVITY_KIND_DEVICE,
  CUPTI_ACTIVITY_KIND_DRIVER,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
runtime_activities[] = {
  CUPTI_ACTIVITY_KIND_DEVICE,
  CUPTI_ACTIVITY_KIND_RUNTIME,
  CUPTI_ACTIVITY_KIND_INVALID
};


typedef enum cupti_activities_flags {
  CUPTI_DATA_MOTION_EXPLICIT = 1,
  CUPTI_DATA_MOTION_IMPLICIT = 2,
  CUPTI_KERNEL_INVOCATION    = 4,
  CUPTI_KERNEL_EXECUTION     = 8,
  CUPTI_DRIVER               = 16,
  CUPTI_RUNTIME              = 32,
  CUPTI_OVERHEAD             = 64
} cupti_activities_flags_t;


int
cupti_pc_sampling_frequency_get()
{
  return pc_sampling_frequency;
}


int
cupti_trace_frequency_get()
{
  return trace_frequency;
}


void
cupti_enable_activities
(
)
{
  TMSG(CUPTI, "Enter cupti_enable_activities");

  cupti_correlation_enable();

  #define FORALL_ACTIVITIES(macro)                                      \
    macro(CUPTI_DATA_MOTION_EXPLICIT, data_motion_explicit_activities)  \
    macro(CUPTI_KERNEL_INVOCATION, kernel_invocation_activities)        \
    macro(CUPTI_KERNEL_EXECUTION, kernel_execution_activities)          \
    macro(CUPTI_DRIVER, driver_activities)                              \
    macro(CUPTI_RUNTIME, runtime_activities)                            \
    macro(CUPTI_OVERHEAD, overhead_activities)

  #define CUPTI_SET_ACTIVITIES(activity_kind, activity)  \
    if (cupti_enabled_activities & activity_kind) {      \
      cupti_monitoring_set(activity, true);     \
    }

  FORALL_ACTIVITIES(CUPTI_SET_ACTIVITIES)

  TMSG(CUPTI, "Exit cupti_enable_activities");
}


//******************************************************************************
// interface operations
//******************************************************************************

static void
METHOD_FN(init)
{
  self->state = INIT;

  control_knob_register("HPCRUN_CUDA_DEVICE_BUFFER_SIZE", "8388608", ck_int);

  // Note: no longer used since CUDA 12.3
  control_knob_register("HPCRUN_CUDA_DEVICE_SEMAPHORE_SIZE", "65536", ck_int);

  control_knob_register("HPCRUN_CUDA_KERNEL_SERIALIZATION", "FALSE", ck_string);

  // Reset cupti flags
  cupti_device_init();

  // Init records
  gpu_trace_init();
}

static void
METHOD_FN(thread_init)
{
  TMSG(CUDA, "thread_init");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(CUDA, "thread_init_action");
}
static void
METHOD_FN(start)
{
  TMSG(CUDA, "start");
  TD_GET(ss_state)[self->sel_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(CUDA, "thread_fini_action");
}

static void
METHOD_FN(stop)
{
  hpcrun_get_thread_data();

  TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return hpcrun_ev_is(ev_str, NVIDIA_CUDA) || hpcrun_ev_is(ev_str, NVIDIA_CUDA_PC_SAMPLING)
                                                                                                                                                                                        || hpcrun_ev_is(ev_str, NVIDIA_CUDA_NV_LINK);
}

// FIXME: The contents of this function (and potentially the entire sample source
//        callback set) should be rearranged to better handle fork().
// As part of fork() handling, the CUPTI sample source is shut down nearly
// completely, and doesn't get started again upon a call to gen_event_set below.
// As such, after a fork() no CUPTI data is processed even though CUPTI activities
// are delivered, resulting in (at best) mysteriously blank trace lines.
//
// The current hotfix is to not zero out as much in cupti_device_init (called
// by init above, called by the post-fork() hook in main.c). For PyTorch it
// functions sufficiently for testing purposes.
//
// A proper fix will require investigation and reevaluation of the design for
// sample sources' lifetimes.

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;

  TMSG(CUDA,"nevents = %d", nevents);
  hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);

  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  long int frequency = 0;
  int frequency_default = -1;

  hpcrun_extract_ev_thresh(event, sizeof(nvidia_name), nvidia_name,
    &frequency, frequency_default);

        for (; event != NULL; event = next_tok()) {
                if (hpcrun_ev_is(event, NVIDIA_CUDA)) {
                        trace_frequency =
                        (frequency == frequency_default) ? trace_frequency_default : frequency;
                        gpu_monitoring_trace_sample_frequency_set(trace_frequency);
                } else if (hpcrun_ev_is(event, NVIDIA_CUDA_PC_SAMPLING)) {
                        pc_sampling_frequency = (frequency == frequency_default) ?
                                                                                                                        pc_sampling_frequency_default : frequency;

                        gpu_monitoring_instruction_sample_frequency_set(pc_sampling_frequency);

                        gpu_metrics_GPU_INST_enable(); // instruction counts

                        gpu_metrics_GPU_INST_STALL_enable(); // stall metrics

    gpu_metrics_GSAMP_enable(); // GPU utilization from sampling

    // pc sampling cannot be on with concurrent kernels
    kernel_invocation_activities[0] = CUPTI_ACTIVITY_KIND_KERNEL;
    }   else if (hpcrun_ev_is(event, NVIDIA_CUDA_NV_LINK)) {
      gpu_metrics_GXFER_enable();
    }
  }

  gpu_metrics_default_enable();
  gpu_metrics_KINFO_enable();

  if (cuda_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to NVIDIA CUDA library %s\n", dlerror());
    auditor_exports()->exit(-1);
  }

  if (cupti_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to NVIDIA CUPTI library %s\n", dlerror());
    auditor_exports()->exit(-1);
  }

  // Register hpcrun callbacks
  device_finalizer_flush.fn = cupti_device_flush;
  device_finalizer_register(device_finalizer_type_flush,
                            &device_finalizer_flush);

  device_finalizer_shutdown.fn = cupti_device_shutdown;
  device_finalizer_register(device_finalizer_type_shutdown,
                            &device_finalizer_shutdown);

  // Get control knobs
  int device_buffer_size;
  if (control_knob_value_get_int("HPCRUN_CUDA_DEVICE_BUFFER_SIZE", &device_buffer_size) != 0)
    auditor_exports()->exit(-1);

  // Note: no longer used since CUDA 12.3
  int device_semaphore_size;
  if (control_knob_value_get_int("HPCRUN_CUDA_DEVICE_SEMAPHORE_SIZE", &device_semaphore_size) != 0)
    auditor_exports()->exit(-1);

  char *kernel_serialization = NULL;
  if (control_knob_value_get_string("HPCRUN_CUDA_KERNEL_SERIALIZATION", &kernel_serialization) != 0)
    auditor_exports()->exit(-1);

  TMSG(CUDA, "Device buffer size %d", device_buffer_size);
  TMSG(CUDA, "Device semaphore size %d", device_semaphore_size);
  TMSG(CUDA, "Kernel serialization %s", kernel_serialization);

  // By default we enable concurrent kernel monitoring,
  // which instruments the begin and exit of a block to measure running time.
  // It introduces not neglibile overhead if a kernel has many short-lived blocks.
  // Force kernels to serialize will reduce the measurement overhead and improve
  // accuracy for applications that use a single stream.
  if (strcmp(kernel_serialization, "TRUE") == 0) {
    kernel_invocation_activities[0] = CUPTI_ACTIVITY_KIND_KERNEL;
  }

  monitor_disable_new_threads();

  cupti_device_buffer_config(device_buffer_size, device_semaphore_size);

  // Register cupti callbacks
  cupti_init();
  cupti_callbacks_subscribe();
  cupti_start();

  // Set enabling activities
  cupti_enabled_activities |= CUPTI_DRIVER;
  cupti_enabled_activities |= CUPTI_RUNTIME;
  cupti_enabled_activities |= CUPTI_KERNEL_EXECUTION;
  cupti_enabled_activities |= CUPTI_KERNEL_INVOCATION;
  cupti_enabled_activities |= CUPTI_DATA_MOTION_EXPLICIT;
  cupti_enabled_activities |= CUPTI_OVERHEAD;

  // Register shutdown functions to write trace files
  device_trace_finalizer_shutdown.fn = gpu_trace_fini;
  device_finalizer_register(device_finalizer_type_shutdown,
                            &device_trace_finalizer_shutdown);

  monitor_enable_new_threads();
}

static void
METHOD_FN(finalize_event_list)
{
  cupti_enable_activities();
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available events for monitoring CUDA on NVIDIA GPUs\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tComprehensive operation-level monitoring of CUDA on NVIDIA GPUs.\n"
         "\t\tCollect timing information on GPU kernel invocations,\n"
         "\t\tmemory copies (implicit and explicit), driver and runtime\n"
         "\t\tactivity, and overhead.\n",
         NVIDIA_CUDA);
  printf("\n");
  printf("%s\tComprehensive monitoring on an NVIDIA GPU as described above\n"
         "\t\t" "with the addition of PC sampling. PC sampling attributes\n"
         "\t\t" "STALL reasons to individual GPU instructions. PC sampling also\n"
         "\t\t" "records aggregate statistics about the TOTAL number of samples measured,\n"
         "\t\t" "the number of samples EXPECTED, and the number of samples DROPPED.\n"
         "\t\t" "GPU utilization for a kernel may be computed as (TOTAL+DROPPED)/EXPECTED.\n",
         NVIDIA_CUDA_PC_SAMPLING);
  printf("\n");
}


//******************************************************************************
// object
//******************************************************************************

#define ss_name nvidia_gpu
#define ss_cls SS_HARDWARE
#define ss_sort_order  20

#include "ss_obj.h"
