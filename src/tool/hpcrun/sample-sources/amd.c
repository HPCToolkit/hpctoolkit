//
// Created by user on 17.8.2019..
//

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

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>

#include "amd.h"
#include "gpu/amd/roctracer-api.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/control-knob.h>
#include <hpcrun/gpu/gpu-activity.h>

#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <ompt/ompt-interface.h>
#include <roctracer_hip.h>

#define AMD_ROCM "amd-rocm"
static char amd_name[128];

static const unsigned int MAX_CHAR_FORMULA = 32;

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
    return hpcrun_ev_is(ev_str, AMD_ROCM);
#else
    return false;
#endif


}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
    int nevents = (self->evl).nevents;

    TMSG(CUDA,"nevents = %d", nevents);


#ifndef HPCRUN_STATIC_LINK
    if (roctracer_bind()) {
        EEMSG("hpcrun: unable to bind to AMD roctracer library %s\n", dlerror());
        monitor_real_exit(-1);
    }
#endif

    // Fetch the event string for the sample source
    // only one event is allowed
    char* evlist = METHOD_CALL(self, get_event_str);
    char* event = start_tok(evlist);
    //if (hpcrun_ev_is(amd_name, AMD_ROCM)) {
        gpu_metrics_default_enable();
    //}    
    roctracer_init();
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}

static void
METHOD_FN(display_events)
{
    printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name amd_gpu
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations
 *****************************************************************************/
