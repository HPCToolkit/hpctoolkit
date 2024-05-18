// SPDX-FileCopyrightText: 2019-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE
#define __HIP_PLATFORM_AMD__
#define __HIP_PLATFORM_HCC__

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

#include "amd.h"

#include "libdl.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "display.h"

#include "../control-knob.h"
#include "../device-finalizers.h"
#include "../gpu/api/amd/roctracer-api.h"
#include "../gpu/api/amd/rocprofiler-api.h"
#include "../gpu/activity/gpu-activity.h"
#include "../gpu/gpu-metrics.h"
#include "../gpu/trace/gpu-trace-api.h"
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

#include <roctracer/roctracer_hip.h>



//******************************************************************************
// macros
//******************************************************************************

#define AMD_ROCPROFILER_PREFIX "rocprof"

static device_finalizer_fn_entry_t device_finalizer_rocprofiler_shutdown;

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
  if (hpcrun_ev_is(ev_str, AMD_ROCPROFILER_PREFIX)) {
    rocprofiler_init();
    const char* roc_str = ev_str + sizeof(AMD_ROCPROFILER_PREFIX);
    while (*roc_str == ':') roc_str++;
    if (*roc_str == 0) return false;
    return rocprofiler_match_event(roc_str) != 0;
  }
  return false;
}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;
  TMSG(CUDA,"nevents = %d", nevents);
}

static void
METHOD_FN(finalize_event_list)
{
  // After going through all command line arguments,
  // we call this function to generate a list of counters
  // in rocprofiler's format and initialize corresponding
  // hpcrun metrics
  rocprofiler_finalize_event_list();
  // When the previous function finishes, we know the total_requested number
  // of counters that should be collected for each kernel.
  // Afterwards, we register callbacks for processing them.
  rocprofiler_register_counter_callbacks();

  device_finalizer_rocprofiler_shutdown.fn = rocprofiler_fini;
  device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_rocprofiler_shutdown);

  // Inform roctracer component that we will collect hardware counters,
  // which will serialize kernel launches
  roctracer_enable_counter_collection();
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{

}


static void
METHOD_FN(display_events)
{
  const char* rocp_metrics = getenv("ROCP_METRICS");
  const char* hsa_tools_lib = getenv("HSA_TOOLS_LIB");
  if (rocp_metrics == NULL || rocp_metrics[0] == '\0'
      || hsa_tools_lib == NULL || hsa_tools_lib[0] == '\0') {
    display_header(stdout, "AMD GPU hardware counter events");
    printf("ROCProfiler was not found, AMD hardware counters are not available.\n");
    printf("See hpcrun --help message for information on how to provide a ROCProfiler install.\n\n");
    return;
  }

  // initialize rocprofiler so that it assembles a list of supported hardware
  // counters
  rocprofiler_init();

  // extract and print information about available rocprofiler hardware counters
  int total_counters = rocprofiler_total_counters();
  if (total_counters > 0) {
    display_header(stdout, "AMD GPU hardware counter events");
    display_header_event(stdout);
    for (int i = 0; i < total_counters; ++i) {
      char event_name[1024];
      sprintf(event_name, "%s::%s", AMD_ROCPROFILER_PREFIX, rocprofiler_counter_name(i));
      display_event_info(stdout, event_name, rocprofiler_counter_description(i));
    }
  }
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name amd_rocprof
#define ss_cls SS_HARDWARE
#define ss_sort_order  24

#include "ss_obj.h"
