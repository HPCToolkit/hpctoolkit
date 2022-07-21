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

#include "amd.h"

#include "libdl.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "display.h"

#include <hpcrun/control-knob.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/amd/roctracer-api.h>
#include <hpcrun/gpu/amd/rocprofiler-api.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-trace.h>
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

#include <roctracer_hip.h>



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
#ifndef HPCRUN_STATIC_LINK
  if (hpcrun_ev_is(ev_str, AMD_ROCPROFILER_PREFIX)) {
    rocprofiler_init();
    const char* roc_str = ev_str + sizeof(AMD_ROCPROFILER_PREFIX);
    while (*roc_str == ':') roc_str++;
    if (*roc_str == 0) return false;
    return rocprofiler_match_event(roc_str) != 0;
  }
  return false;
#else
    return false;
#endif


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

#include "ss_obj.h"
