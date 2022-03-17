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

#include <hpcrun/control-knob.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/amd/roctracer-api.h>
#include <hpcrun/gpu/amd/rocprofiler-api.h>
#include <hpcrun/gpu/amd/hip-api.h>
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

#define AMD_ROCM "gpu=amd"

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;
static device_finalizer_fn_entry_t device_trace_finalizer_shutdown;


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
    return hpcrun_ev_is(ev_str, AMD_ROCM);
#else
    return false;
#endif


}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
    int nevents = (self->evl).nevents;
    gpu_metrics_default_enable();
    hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);
    TMSG(CUDA,"nevents = %d", nevents);


#ifndef HPCRUN_STATIC_LINK
  if (hip_bind()) {
    EEMSG("hpcrun: unable to bind to HIP AMD library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif
}

static void
METHOD_FN(finalize_event_list)
{
#ifndef HPCRUN_STATIC_LINK
  if (roctracer_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to AMD roctracer library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif

#if 0
  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
#endif
  roctracer_init();

  device_finalizer_flush.fn = roctracer_flush;
  device_finalizer_register(device_finalizer_type_flush, 
                            &device_finalizer_flush);

  device_finalizer_shutdown.fn = roctracer_fini;
  device_finalizer_register(device_finalizer_type_shutdown, 
                            &device_finalizer_shutdown);

  // initialize gpu tracing 
  gpu_trace_init();

  // Register shutdown function to finalize gpu tracing and write trace files
  device_trace_finalizer_shutdown.fn = gpu_trace_fini;
  device_finalizer_register(device_finalizer_type_shutdown, 
                            &device_trace_finalizer_shutdown);
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{

}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available AMD GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\t\tComprehensive operation-level monitoring on an AMD GPU.\n"
	 "\t\tCollect timing information on GPU kernel invocations,\n"
	 "\t\tmemory copies, etc.\n",
	 AMD_ROCM);
  printf("\n");
}


//**************************************************************************
// object
//**************************************************************************

#define ss_name amd_gpu
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
