// SPDX-FileCopyrightText: 2019-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

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

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include "../control-knob.h"
#include "../device-finalizers.h"
#include "../gpu/api/amd/roctracer-api.h"
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
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name openmp_gpu
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
