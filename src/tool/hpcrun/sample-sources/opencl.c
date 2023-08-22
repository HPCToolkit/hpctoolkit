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
// local includes
//******************************************************************************

#include "common.h"

#include <monitor.h>
#include <pthread.h>

#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/gpu/opencl/opencl-api.h>
#include <hpcrun/gpu/blame-shifting/blame.h>
#include <hpcrun/gpu/opencl/intel/papi/papi-metric-collector.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>

#include <messages/messages.h>

#include <utilities/tokenize.h>



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
#if ENABLE_OPENCL
#ifndef HPCRUN_STATIC_LINK
  return hpcrun_ev_is(ev_str, OPENCL_OPTION);
#if 0
  return strncmp(ev_str, OPENCL_OPTION, strlen(OPENCL_OPTION)) == 0;
  return (hpcrun_ev_is(ev_str, OPENCL) || hpcrun_ev_is(ev_str, DEFAULT_INSTRUMENTATION)
                                           || strstr(ev_str, INSTRUMENTATION_PREFIX)
                                           || hpcrun_ev_is(ev_str, INTEL_OPTIMIZATION_CHECK)
                                           || hpcrun_ev_is(ev_str, ENABLE_OPENCL_BLAME_SHIFTING)
                                           || hpcrun_ev_is(ev_str, ENABLE_INTEL_GPU_UTILIZATION)
         );
#endif
#else
  return false;
#endif
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

#if 0
  int nevents = (self->evl).nevents;
  TMSG(OPENCL,"nevents = %d", nevents);

  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    long th;
    hpcrun_extract_ev_thresh(event, sizeof(event_name), event_name,
      &th, NO_THRESHOLD);

    if (hpcrun_ev_is(opencl_name, INTEL_OPTIMIZATION_CHECK)) {
      opencl_optimization_check_enable();
      gpu_metrics_INTEL_OPTIMIZATION_enable();
    } else if (hpcrun_ev_is(event_name, ENABLE_OPENCL_BLAME_SHIFTING)) {
                        opencl_blame_shifting_enable();
                } else if (hpcrun_ev_is(event_name, ENABLE_INTEL_GPU_UTILIZATION)) {
      // papi metric collection for OpenCL
      intel_papi_setup();
      gpu_metrics_gpu_utilization_enable();
      set_gpu_utilization_flag();
    }
  }
#endif
}


static void
METHOD_FN(finalize_event_list)
{
  #ifndef HPCRUN_STATIC_LINK
  if (opencl_bind()) {
    EEMSG("hpcrun: unable to bind to opencl library %s\n", dlerror());
    monitor_real_exit(-1);
  }
  #endif
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
#if ENABLE_OPENCL
  printf("===========================================================================\n");
  printf("Available events for monitoring GPU operations atop OpenCL\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("gpu=opencl\tOperation-level monitoring for OpenCL on a CPU or GPU.\n"
         "\t\tCollect timing information for GPU kernel invocations,\n"
         "\t\tmemory copies, etc.\n"
         "\n");

#if 0
// these options can't be enabled without further work.
// GTPin_getElf is currently returns something much different for
// Level 0 vs. OpenCL. Fixing it isn't worth the effort until
// zeBinary becomes the standard across both.
#ifdef ENABLE_GTPIN
  printf("gpu=opencl,inst=<comma-separated list of options>\n"
         "\t\tOperation-level monitoring for GPU-accelerated applications\n"
         "\t\trunning on an Intel GPU atop Intel's OpenCL runtime. Collect\n"
         "\t\ttiming information for GPU kernel invocations, memory copies, etc.\n"
         "\t\tUse optional instrumentation within GPU kernels to collect\n"
         "\t\tone or more of the following:\n"
         "\t\t  count:   count how many times each GPU instruction executes\n"
         "\t\t  latency: approximately attribute latency to GPU instructions\n"
         "\t\t  simd:    analyze utilization of SIMD lanes\n"
         "\t\t  silent:  silence warnings from instrumentation\n"
         "\n");
#endif

  printf("%s\tIntel Optimization suggestions.\n"
    "\t\tprovides oneapi optimization suggestions from the optimization guide.\n"
    "\t\tTo use it, pass '-e %s -e %s' to your hpcrun command\n",
    INTEL_OPTIMIZATION_CHECK, OPENCL_OPTION, INTEL_OPTIMIZATION_CHECK);
  printf("\n");

  printf("%s\tIntel GPU utilization metrics.\n"
    "\t\tprovides GPU active, stalled and idle times for kernel executions.\n"
    "\t\tTo use it, pass '-e %s' to your hpcrun command\n",
    ENABLE_INTEL_GPU_UTILIZATION, OPENCL_OPTION, ENABLE_INTEL_GPU_UTILIZATION);
  printf("\n");
#endif
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
