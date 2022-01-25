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
// Copyright ((c)) 2002-2022, Rice University
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
#include <hpcrun/gpu/opencl/intel/papi/papi_metric_collector.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>

#include <messages/messages.h>

#include <utilities/tokenize.h>



//******************************************************************************
// type declarations
//******************************************************************************

#define GPU_STRING "gpu=opencl"
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

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;


//******************************************************************************
// local variables
//******************************************************************************

static char opencl_name[128];
static pthread_t gpu_utilization_tid;
static bool gpu_utilization_enabled = false;



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
  if (gpu_utilization_enabled) {
    notify_gpu_util_thr_hpcrun_completion();
  }
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
  if (gpu_utilization_enabled) {
    notify_gpu_util_thr_hpcrun_completion();
  }
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
  #ifndef HPCRUN_STATIC_LINK
  return (hpcrun_ev_is(ev_str, GPU_STRING) || hpcrun_ev_is(ev_str, DEFAULT_INSTRUMENTATION)
                                           || strstr(ev_str, INSTRUMENTATION_PREFIX)
                                           || hpcrun_ev_is(ev_str, INTEL_OPTIMIZATION_CHECK)
                                           || hpcrun_ev_is(ev_str, ENABLE_OPENCL_BLAME_SHIFTING)
                                           || hpcrun_ev_is(ev_str, ENABLE_INTEL_GPU_UTILIZATION)
         );
  #else
  return false;
  #endif
}


static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;
  TMSG(OPENCL,"nevents = %d", nevents);
  gpu_metrics_default_enable();
  gpu_metrics_KINFO_enable();
  hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);

  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    long th;
    hpcrun_extract_ev_thresh(event, sizeof(opencl_name), opencl_name,
      &th, NO_THRESHOLD);

    if (hpcrun_ev_is(opencl_name, GPU_STRING)) {
    } else if (hpcrun_ev_is(opencl_name, DEFAULT_INSTRUMENTATION)) {
      opencl_instrumentation_latency_enable();
      opencl_instrumentation_count_enable();
      gpu_metrics_GPU_INST_enable();
      opencl_instrumentation_enable();
    } else if (strstr(opencl_name, INSTRUMENTATION_PREFIX)) {

      int suffix_length = strlen(opencl_name) - strlen(INSTRUMENTATION_PREFIX);
      char instrumentation_suffix[suffix_length + 1];
      strncpy(instrumentation_suffix, opencl_name + strlen(INSTRUMENTATION_PREFIX), suffix_length);
      instrumentation_suffix[suffix_length] = 0;

      bool validInst = false;
      char *inst = strtok(instrumentation_suffix, ",");
      while(inst) {
          if (strstr(inst, SIMD)) {
            validInst = true;
            opencl_instrumentation_simd_enable();
            // we need to enable insertion of count probes for calculating total available SIMD lanes 
            opencl_instrumentation_count_enable();
          } else if (strstr(inst, LATENCY)) {
            validInst = true;
            opencl_instrumentation_latency_enable();
          } else if (strstr(inst, EXECUTION_COUNT)) {
            validInst = true;
            opencl_instrumentation_count_enable();
          } else {
            EEMSG("hpcrun: Unrecognized Intel GPU instrumentation knob\n");
          }
          inst = strtok(NULL, ",");
      }

			if (validInst) {
        gpu_metrics_GPU_INST_enable();
        opencl_instrumentation_enable();
      }
		} else if (hpcrun_ev_is(opencl_name, INTEL_OPTIMIZATION_CHECK)) {
      opencl_optimization_check_enable();
      gpu_metrics_INTEL_OPTIMIZATION_enable();
    } else if (hpcrun_ev_is(opencl_name, ENABLE_OPENCL_BLAME_SHIFTING)) {
			opencl_blame_shifting_enable();
		} else if (hpcrun_ev_is(opencl_name, ENABLE_INTEL_GPU_UTILIZATION)) {
      // papi metric collection for OpenCL
      int err = pthread_create(&gpu_utilization_tid, NULL, &papi_metric_callback, NULL);
      gpu_utilization_enabled = true;
      gpu_metrics_gpu_utilization_enable();
      set_gpu_utilization_tid(gpu_utilization_tid);
		}
	}
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
  opencl_api_initialize();

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
  printf("===========================================================================\n");
  printf("Available OPENCL GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tOperation-level monitoring for opencl on a GPU.\n"
    "\t\tCollect timing information on GPU kernel invocations,\n"
    "\t\tmemory copies, etc.\n",
    GPU_STRING);
  printf("\n");

  printf("%1$s%2$s\n\t\tIntel GPU instrumentation(for opencl, dpcpp).\n"
    "\t\tCollect instrumentation results on GPU kernel.\n"
    "\t\tAvailable instrumentation support (tokens in brackets are to be passed as options):\n"
    "\t\t1. Execution Count(count): Counts the no. of GPU instructions executed\n"
    "\t\t2. Latency(latency): Execution cycles taken up by GPU instructions\n"
    "\t\t3. SIMD Lanes(simd): SIMD lanes used by GPU instructions\n"
    "\t\te.g. %1$s%3$s,%4$s enables %3$s and %4$s instrumentation\n"
    "\t\te.g. %1$s%3$s,%4$s,%5$s enables %3$s, %4$s and %5$s instrumentation\n"
    "\t\tIf %6$s is passed(default mode), %3$s and %4$s instrumentation is turned on\n",
    INSTRUMENTATION_PREFIX, "<comma-separated instrumentation options>",
    EXECUTION_COUNT, LATENCY, SIMD, DEFAULT_INSTRUMENTATION);
  printf("\n");

  printf("%s\tIntel Optimization suggestions.\n"
    "\t\tprovides oneapi optimization suggestions from the optimization guide.\n"
    "\t\tTo use it, pass '-e %s -e %s' to your hpcrun command\n",
    INTEL_OPTIMIZATION_CHECK, GPU_STRING, INTEL_OPTIMIZATION_CHECK);
  printf("\n");

  printf("%s\tIntel GPU utilization metrics.\n"
    "\t\tprovides GPU active, stalled and idle times for kernel executions.\n"
    "\t\tTo use it, pass '-e %s' to your hpcrun command\n",
    ENABLE_INTEL_GPU_UTILIZATION, GPU_STRING, ENABLE_INTEL_GPU_UTILIZATION);
  printf("\n");
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name opencl
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
