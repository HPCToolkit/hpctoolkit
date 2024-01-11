//******************************************************************************
// system includes
//******************************************************************************

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

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "amd.h"

#include "libdl.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/control-knob.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/amd/roctracer-api.h>
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




//******************************************************************************
// macros
//******************************************************************************

#define OPENMP_TARGET "gpu=openmp"



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
    return hpcrun_ev_is(ev_str, OPENMP_TARGET);
}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
    int nevents = (self->evl).nevents;
    gpu_metrics_default_enable();
    hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);
    TMSG(CUDA,"nevents = %d", nevents);
}

static void
METHOD_FN(finalize_event_list)
{
    gpu_metrics_default_enable();
    gpu_trace_init();
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{

}


static void
METHOD_FN(display_events)
{
#if 0
  // this gets turned on automatically if there is target OMPT support
  printf("===========================================================================\n");
  printf("Available events for monitoring OpenMP offloading on AMD GPUs \n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tOperation-level monitoring of OpenMP offloading.\n"
         "\t\tCollect timing information on GPU kernel invocations,\n"
         "\t\tmemory copies, etc.\n",
         OPENMP_TARGET);
  printf("\n");
#endif
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name openmp_gpu
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
