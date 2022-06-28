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
// system includes
//******************************************************************************

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif



//******************************************************************************
// libmonitor
//******************************************************************************

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "libdl.h"
#include "level0.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/control-knob.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/amd/roctracer-api.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/ompt/ompt-interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>

#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>




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
#ifndef HPCRUN_STATIC_LINK
  return strncmp(ev_str, LEVEL0, strlen(LEVEL0)) == 0; 
#else
  return false;
#endif


}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
#if 0
  int nevents = (self->evl).nevents;
#endif

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
#ifndef HPCRUN_STATIC_LINK
  if (level0_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to Level0 library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif

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
	 "\t\t  latency: approximately attribute latency to GPU instructions\n"
	 "\t\t  simd:    analyze utilization of SIMD lanes\n"
	 "\n");
#endif
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name level0
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
