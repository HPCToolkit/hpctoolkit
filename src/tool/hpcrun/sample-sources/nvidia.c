// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/papi.c $
// $Id: papi.c 3328 2010-1/2-23 23:39:09Z tallent $
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

//
// CUPTI synchronous sampling via PAPI sample source simple oo interface
//

//******************************************************************************
// system includes
//******************************************************************************

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif



//******************************************************************************
// libmonitor includes
//******************************************************************************

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "libdl.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "nvidia.h"

#include <hpcrun/control-knob.h>

#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring.h>

#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/gpu/nvidia/cuda-api.h>
#include <hpcrun/gpu/nvidia/cupti-api.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/utilities/tokenize.h>
#include <hpcrun/thread_data.h>



/******************************************************************************
 * macros
 *****************************************************************************/

#define NVIDIA_CUDA "gpu=nvidia"
#define NVIDIA_CUDA_PC_SAMPLING "gpu=nvidia,pc"



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
  CUPTI_ACTIVITY_KIND_SYNCHRONIZATION,
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
  CUPTI_RUNTIME	             = 32,
  CUPTI_OVERHEAD	     = 64
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
#ifndef HPCRUN_STATIC_LINK
  return hpcrun_ev_is(ev_str, NVIDIA_CUDA) || hpcrun_ev_is(ev_str, NVIDIA_CUDA_PC_SAMPLING);
#else
  return false;
#endif
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

  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  long int frequency = 0;
  int frequency_default = -1;
  hpcrun_extract_ev_thresh(event, sizeof(nvidia_name), nvidia_name,
    &frequency, frequency_default);

  if (hpcrun_ev_is(nvidia_name, NVIDIA_CUDA)) {
    trace_frequency =
      (frequency == frequency_default) ? trace_frequency_default : frequency;
    gpu_monitoring_trace_sample_frequency_set(trace_frequency);
  } else if (hpcrun_ev_is(nvidia_name, NVIDIA_CUDA_PC_SAMPLING)) {
    pc_sampling_frequency = (frequency == frequency_default) ?
      pc_sampling_frequency_default : frequency;

    gpu_monitoring_instruction_sample_frequency_set(pc_sampling_frequency);

    gpu_metrics_GPU_INST_enable(); // instruction counts

    gpu_metrics_GPU_INST_STALL_enable(); // stall metrics

    gpu_metrics_GSAMP_enable(); // GPU utilization from sampling
    
    // pc sampling cannot be on with concurrent kernels
    kernel_invocation_activities[0] = CUPTI_ACTIVITY_KIND_KERNEL;
  }

  gpu_metrics_default_enable();
  gpu_metrics_KINFO_enable();

#ifndef HPCRUN_STATIC_LINK
  if (cuda_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to NVIDIA CUDA library %s\n", dlerror());
    monitor_real_exit(-1);
  }

  if (cupti_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to NVIDIA CUPTI library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif

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
    monitor_real_exit(-1);

  int device_semaphore_size;
  if (control_knob_value_get_int("HPCRUN_CUDA_DEVICE_SEMAPHORE_SIZE", &device_semaphore_size) != 0)
    monitor_real_exit(-1);

  char *kernel_serialization = NULL;
  if (control_knob_value_get_string("HPCRUN_CUDA_KERNEL_SERIALIZATION", &kernel_serialization) != 0)
    monitor_real_exit(-1);

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
  printf("Available NVIDIA GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tComprehensive operation-level monitoring on an NVIDIA GPU.\n"
	 "\t\tCollect timing information on GPU kernel invocations,\n"
	 "\t\tmemory copies (implicit and explicit), driver and runtime\n"
	 "\t\tactivity, and overhead.\n",
	 NVIDIA_CUDA);
  printf("\n");
  printf("%s\tComprehensive monitoring on an NVIDIA GPU as described above\n"
	 "\t\twith the addition of PC sampling. PC sampling attributes\n"
	 "\t\tSTALL reasons to individual GPU instructions. PC sampling also\n"
	 "\t\trecords aggregate statistics about the TOTAL number of samples measured,\n"
	 "\t\tthe number of samples EXPECTED, and the number of samples DROPPED.\n"
	 "\t\tGPU utilization for a kernel may be computed as (TOTAL+DROPPED)/EXPECTED.\n",
	 NVIDIA_CUDA_PC_SAMPLING);
  printf("\n");
}



//******************************************************************************
// object
//******************************************************************************

#define ss_name nvidia_gpu
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
