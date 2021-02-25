/// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/sample-sources/gpu_blame.c $
// $Id: itimer.c 3784 2012-05-10 22:35:51Z mc29 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
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
// **

//
// Blame shiting interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */
#include <dlfcn.h>


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/
#include "sample_source_obj.h"
#include "common.h"
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>

#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/splay-macros.h>
#include "blame-shift/blame-shift.h"

#include <hpcrun/gpu/blame-shifting/blame.h>
#if defined(ENABLE_CUDA) || defined(ENABLE_OPENCL)
#include "cpu_gpu_idle.h"
#endif // ENABLE_CUDA || ENABLE_OPENCL

#ifdef ENABLE_OPENCL
#include <hpcrun/gpu/opencl/opencl-api.h>
#endif // ENABLE_OPENCL

// ****************** utility macros *********************
#define Cuda_RTcall(fn) cudaRuntimeFunctionPointer[fn ## Enum].fn ## Real
#define Cuda_Dcall(fn)  cuDriverFunctionPointer[fn ## Enum].fn ## Real
// ************************************************

// ******* Global Variables ***********
int g_cpu_gpu_proxy_count = 0; 
bool g_cpu_gpu_enabled = false;


atomic_uint_fast64_t g_active_threads = { 1 };



// blame shift registration info
static bs_fn_entry_t bs_entry = {};

// ****** UTILITY Functions (public)

void hpcrun_set_gpu_proxy_present() {
    g_cpu_gpu_proxy_count ++;
}

// ******* METHOD DEFINITIONS ***********

static void METHOD_FN(init)
{
    TMSG(CPU_GPU_BLAME_CTL, "setting up CPU_GPU_BLAME");
    //active threads represents the total number of threads in the system
    //including the main thread 
    self->state = INIT;                                    
}

static void METHOD_FN(thread_init)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread init");
    atomic_fetch_add(&g_active_threads, 1L);
}

static void METHOD_FN(thread_init_action)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread action (noop)");
}

static void
METHOD_FN(start)
{
    TMSG(CPU_GPU_BLAME_CTL,"starting CPU_GPU_BLAME");
    if (! blame_shift_source_available(bs_type_timer)) {
        EMSG("Either pass -e WALLCLOCK or -e REALTIME to enable CPU_GPU_BLAME");
        monitor_real_abort();
    }
    g_cpu_gpu_enabled = true;
    TD_GET(ss_state)[self->sel_idx] = START;
}

static void METHOD_FN(thread_fini_action)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread action ");
    atomic_fetch_add(&g_active_threads, -1L);
}

static void METHOD_FN(stop)
{
    TMSG(CPU_GPU_BLAME_CTL, "stopping CPU_GPU_BLAME");
    TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void METHOD_FN(shutdown)
{
    TMSG(CPU_GPU_BLAME_CTL, "shutodown CPU_GPU_BLAME_CTL");
    METHOD_CALL(self, stop);    // make sure stop has been called
    self->state = UNINIT;
}

static bool METHOD_FN(supports_event, const char *ev_str)
{    
  return hpcrun_ev_is(ev_str, "CPU_GPU_IDLE");
}



static void METHOD_FN(process_event_list, int lush_metrics)
{
    
    TMSG(CPU_GPU_BLAME_CTL, "process event list, lush_metrics = %d", lush_metrics);

    gpu_metrics_BLAME_SHIFT_enable();
    TMSG(OPENCL, "registering gpu_blame_shift callback with itimer");
    bs_entry.fn = &gpu_idle_blame;
    // bs_entry.fn = dlsym(RTLD_DEFAULT, "gpu_idle_blame");
    bs_entry.next = 0;
    blame_shift_register(&bs_entry);

    // The blame-shift registration for OPENCL and CUDA are kept in separate if-blocks and not if-else'd because someone may want to enable both 
		#ifdef ENABLE_OPENCL
			opencl_blame_shifting_enable();
		#endif
		
		#ifdef ENABLE_CUDA
			// CUDA blame-shifting: (disabled because support is out-of-date)
			// bs_entry.fn = dlsym(RTLD_DEFAULT, "gpu_blame_shifter");
			// bs_entry.next = 0;
			// blame_shift_register(&bs_entry);
		#endif
}


static void
METHOD_FN(finalize_event_list)
{
}
static void METHOD_FN(gen_event_set, int lush_metrics)
{
    // There is NO signal hander for us, we proxy with itimer or PAPI_TOT_CYC
}

static void METHOD_FN(display_events)
{
    printf("===========================================================================\n");
    printf("Available CPU_GPU_IDLE events\n");
    printf("===========================================================================\n");
    printf("Name\t\tDescription\n");
    printf("---------------------------------------------------------------------------\n");
    printf("CPU_GPU_IDLE\tCPU GPU idleness\n");
    printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name cpu_gpu_idle
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

