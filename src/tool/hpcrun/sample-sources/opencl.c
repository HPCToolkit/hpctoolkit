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

//#include "amd.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/control-knob.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/intel/opencl-setup.h> // opencl_initialize, opencl_bind
#include <hpcrun/gpu/intel/opencl-api.h> //opencl_finalize
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/ompt/ompt-interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>

#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>


//******************************************************************************
// macros
//******************************************************************************

#define INTEL_OPENCL "gpu=opencl"

static device_finalizer_fn_entry_t device_finalizer_shutdown;

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
  fprintf(stderr, "===============================================\n");
  TMSG(OPENCL, "start");
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
fprintf(stderr, "===============================================\n");
#ifndef HPCRUN_STATIC_LINK
  return hpcrun_ev_is(ev_str, INTEL_OPENCL);
#else
  return false;
#endif


}

  static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;
  gpu_metrics_default_enable();
  TMSG(OPENCL,"nevents = %d", nevents);
}

  static void
METHOD_FN(finalize_event_list)
{
#ifndef HPCRUN_STATIC_LINK
  if (opencl_bind()) {
	EEMSG("hpcrun: unable to bind to intel opencl library %s\n", dlerror());
	monitor_real_exit(-1);
  }
#endif

#if 0
  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
#endif
  opencl_initialize();

  device_finalizer_shutdown.fn = opencl_finalize;
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
  printf("Available AMD GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\t\tComprehensive operation-level monitoring on an AMD GPU.\n"
	  "\t\tCollect timing information on GPU kernel invocations,\n"
	  "\t\tmemory copies, etc.\n",
	  INTEL_OPENCL);
  printf("\n");
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name intel_opencl
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
