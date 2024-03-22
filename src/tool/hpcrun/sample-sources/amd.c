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

#include "../audit/audit-api.h"
#include "../control-knob.h"
#include "../device-finalizers.h"
#include "../gpu/amd/roctracer-api.h"
#include "../gpu/amd/rocprofiler-api.h"
#include "../gpu/amd/hip-api.h"
#include "../gpu/gpu-activity.h"
#include "../gpu/gpu-metrics.h"
#include "../gpu/gpu-trace.h"
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
#include "../../../lib/prof-lean/hpcrun-fmt.h"

#include <roctracer/roctracer_hip.h>



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
    return hpcrun_ev_is(ev_str, AMD_ROCM);
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
  if (roctracer_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to AMD roctracer library %s\n", dlerror());
    auditor_exports->exit(-1);
  }

  if (hip_bind()) {
    EEMSG("hpcrun: unable to bind to HIP AMD library %s\n", dlerror());
    auditor_exports->exit(-1);
  }

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
  printf("Available events for monitoring HIP on AMD GPUs\n");
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
#define ss_sort_order  23

#include "ss_obj.h"
