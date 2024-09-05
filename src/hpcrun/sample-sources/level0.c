// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <alloca.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

#include <dlfcn.h>



//******************************************************************************
// libmonitor
//******************************************************************************

#include "../libmonitor/monitor.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "libdl.h"
#include "level0.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include "../audit/audit-api.h"
#include "../control-knob.h"
#include "../device-finalizers.h"
#include "../logical/common.h"
#include "../gpu/api/amd/roctracer-api.h"
#include "../gpu/activity/gpu-activity.h"
#include "../gpu/api/common/gpu-kernel-table.h"
#include "../gpu/gpu-metrics.h"
#include "../gpu/trace/gpu-trace-api.h"
#include "../gpu/api/common/gpu-instrumentation.h"
#include "../hpcrun_options.h"
#include "../hpcrun_stats.h"
#include "../metrics.h"
#include "../module-ignore-map.h"
#include "../ompt/ompt-interface.h"
#include "../safe-sampling.h"
#include "../sample_sources_registered.h"
#include "../sample_event.h"
#include "../thread_data.h"
#include "../trace.h"

#include "../utilities/tokenize.h"
#include "../messages/messages.h"
#include "../lush/lush-backtrace.h"
#include "../../common/lean/hpcrun-fmt.h"




//******************************************************************************
// macros
//******************************************************************************

#define LEVEL0 "gpu=level0"

#define NO_THRESHOLD  1L



//******************************************************************************
// local variables
//******************************************************************************

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;
//static device_finalizer_fn_entry_t device_finalizer_trace;

static char event_name[128];

static gpu_instrumentation_t level0_instrumentation_options;


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
  return strncmp(ev_str, LEVEL0, strlen(LEVEL0)) == 0;
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

  gpu_instrumentation_options_set(event_name, LEVEL0, &level0_instrumentation_options);
  if (gpu_instrumentation_enabled(&level0_instrumentation_options)) {
     gpu_metrics_GPU_INST_enable();
  }
}

static void
METHOD_FN(finalize_event_list)
{
  if (level0_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to Level0 library %s\n", dlerror());
    auditor_exports()->exit(-1);
  }

  level0_init(&level0_instrumentation_options);

  // Init records
  gpu_trace_init();

  device_finalizer_flush.fn = level0_flush;
  device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);

  device_finalizer_shutdown.fn = level0_fini;
  device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available events for monitoring GPU operations atop Intel's Level Zero \n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("gpu=level0\tOperation-level monitoring for GPU-accelerated applications\n"
         "\t\trunning atop Intel's Level Zero runtime. Collect timing \n"
         "\t\tinformation for GPU kernel invocations, memory copies, etc.\n"
         "\n");
#ifdef ENABLE_GTPIN
  printf("gpu=level0,inst=<comma-separated list of options>\n"
         "\t\tOperation-level monitoring for GPU-accelerated applications\n"
         "\t\trunning atop Intel's Level Zero runtime. Collect timing\n"
         "\t\tinformation for GPU kernel invocations, memory copies, etc.\n"
         "\t\tWhen running on Intel GPUs, use optional instrumentation\n"
         "\t\twithin GPU kernels to collect one or more of the following:\n"
         "\t\t  count:   count how many times each GPU instruction executes\n"
#if ENABLE_LATENCY_ANALYSIS
         "\t\t  latency: approximately attribute latency to GPU instructions\n"
#endif
#if ENABLE_SIMD_ANALYSIS
         "\t\t  simd:    analyze utilization of SIMD lanes\n"
#endif
         "\t\t  silent:  silence warnings from instrumentation\n"
         "\n");
#endif
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name level0
#define ss_cls SS_HARDWARE
#define ss_sort_order  21

#include "ss_obj.h"
