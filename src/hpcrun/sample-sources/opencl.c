// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "common.h"

#include "../libmonitor/monitor.h"
#include <pthread.h>

#include "../audit/audit-api.h"
#include "../device-finalizers.h"
#include "../gpu/trace/gpu-trace-api.h"
#include "../gpu/gpu-metrics.h"
#include "../gpu/trace/gpu-trace-api.h"
#include "../gpu/api/opencl/opencl-api.h"
#include "../gpu/blame-shifting/blame.h"
#include "../gpu/api/opencl/intel/papi/papi-metric-collector.h"
#include "../thread_data.h"
#include "../trace.h"

#include "../messages/messages.h"

#include "../utilities/tokenize.h"



//******************************************************************************
// macros
//******************************************************************************

#define ENABLE_OPENCL_MONITORING 0

#define OPENCL_OPTION "gpu=opencl"
#define INTEL_OPTIMIZATION_CHECK "intel_opt_check"
#define ENABLE_OPENCL_BLAME_SHIFTING "opencl-blame"
#define DEFAULT_INSTRUMENTATION "gpu=opencl,inst"
#define INSTRUMENTATION_PREFIX "gpu=opencl,inst="
#define EXECUTION_COUNT "count"
#define LATENCY "latency"
#define SIMD "simd"
#define INTEL_OPTIMIZATION_CHECK "intel_opt_check"
#define ENABLE_INTEL_GPU_UTILIZATION "intel_gpu_util"
#define NO_THRESHOLD  1L



//******************************************************************************
// local variables
//******************************************************************************

static char event_name[128];

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;

static gpu_instrumentation_t opencl_instrumentation_options;



//******************************************************************************
// interface operations
//******************************************************************************

static void
METHOD_FN(init)
{
  self->state = INIT;
}


static void
METHOD_FN(thread_init)
{
  TMSG(OPENCL, "thread_init");
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(OPENCL, "thread_init_action");
}


static void
METHOD_FN(start)
{
  TMSG(OPENCL, "start");
  TD_GET(ss_state)[self->sel_idx] = START;
}


static void
METHOD_FN(thread_fini_action)
{
  TMSG(OPENCL, "thread_fini_action");
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
#ifdef ENABLE_OPENCL
  return hpcrun_ev_is(ev_str, OPENCL_OPTION);
#else
  return false;
#endif
}


static void
METHOD_FN(process_event_list, int lush_metrics)
{
  hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);
  gpu_metrics_default_enable();
  gpu_metrics_KINFO_enable();

  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  long th;
  hpcrun_extract_ev_thresh(event, sizeof(event_name), event_name,
    &th, NO_THRESHOLD);

  gpu_instrumentation_options_set(event_name, OPENCL_OPTION, &opencl_instrumentation_options);
  if (gpu_instrumentation_enabled(&opencl_instrumentation_options)) {
     gpu_metrics_GPU_INST_enable();
  }
}


static void
METHOD_FN(finalize_event_list)
{
  opencl_api_initialize(&opencl_instrumentation_options);

  device_finalizer_flush.fn = opencl_api_thread_finalize;
  device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);

  device_finalizer_shutdown.fn = opencl_api_process_finalize;
  device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{

}


static void
METHOD_FN(display_events)
{
#ifdef ENABLE_OPENCL
  printf("===========================================================================\n");
  printf("Available events for monitoring GPU operations atop OpenCL\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("gpu=opencl\tOperation-level monitoring for OpenCL on a CPU or GPU.\n"
         "\t\tCollect timing information for GPU kernel invocations,\n"
         "\t\tmemory copies, etc.\n"
         "\n");
#else
  return;
#endif
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name opencl
#define ss_cls SS_HARDWARE
#define ss_sort_order  22

#include "ss_obj.h"
