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
#include <hpcrun/gpu/opencl/opencl-api.h>
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

#ifdef ENABLE_CUDA
#include "gpu_blame.h"
#endif // ENABLE_CUDA

// ****************** utility macros *********************
#define Cuda_RTcall(fn) cudaRuntimeFunctionPointer[fn ## Enum].fn ## Real
#define Cuda_Dcall(fn)  cuDriverFunctionPointer[fn ## Enum].fn ## Real
// ************************************************

// ******* Global Variables ***********
int g_cpu_gpu_proxy_count = 0; 
bool g_cpu_gpu_enabled = false;


// Various CPU-GPU metrics
int cpu_idle_metric_id;
int gpu_time_metric_id;
int cpu_idle_cause_metric_id;
int gpu_idle_metric_id;
int gpu_overload_potential_metric_id;
int stream_special_metric_id;
int h_to_d_data_xfer_metric_id;
int d_to_h_data_xfer_metric_id;
int h_to_h_data_xfer_metric_id;
int d_to_d_data_xfer_metric_id;
int uva_data_xfer_metric_id;


uint64_t g_active_threads;

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
    g_active_threads = 1;
    self->state = INIT;                                    
}

static void METHOD_FN(thread_init)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread init");
    atomic_add_i64(&g_active_threads, 1L);
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
    atomic_add_i64(&g_active_threads, -1L);
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
    
    kind_info_t *blame_kind = hpcrun_metrics_new_kind();
    // Create metrics for CPU/GPU blame shifting
    // cpu_idle_metric_id a.k.a CPU_IDLE measures the time when CPU is idle waiting for GPU to finish 
    cpu_idle_metric_id = hpcrun_set_new_metric_info(blame_kind, "CPU_IDLE");
    // gpu_idle_metric_id a.k.a GPU_IDLE_CAUSE measures the time when GPU is idle and blames CPU CCT node
    // for not creating work
    gpu_idle_metric_id = hpcrun_set_new_metric_info(blame_kind, "GPU_IDLE_CAUSE");
    // cpu_idle_cause_metric_id a.k.a CPU_IDLE_CAUSE blames GPU kernels (CCT nodes which launched them) 
    // that are keeping the CPU  idle 
    cpu_idle_cause_metric_id = hpcrun_set_new_metric_info_and_period(blame_kind, "CPU_IDLE_CAUSE",
								     MetricFlags_ValFmt_Real, 1, metric_property_none);
    // gpu_time_metric_id a.k.a. GPU_ACTIVITY_TIME accounts the absolute running time of a kernel (CCT node which launched it)
    gpu_time_metric_id = hpcrun_set_new_metric_info(blame_kind, "GPU_TIME");
    // h_to_d_data_xfer_metric_id is the number of bytes xfered from CPU to GPU
    h_to_d_data_xfer_metric_id = hpcrun_set_new__metric_info(blame_kind, "H_TO_D_BYTES");
    // d_to_h_data_xfer_metric_id is the number of bytes xfered from GPU to CPU
    d_to_h_data_xfer_metric_id = hpcrun_set_new_metric_info(blame_kind, "D_TO_H_BYTES");
    // h_to_h_data_xfer_metric_id is the number of bytes xfered from CPU to CPU
    h_to_h_data_xfer_metric_id = hpcrun_set_new_metric_info(blame_kind, "H_TO_H_BYTES");
    // d_to_d_data_xfer_metric_id is the number of bytes xfered from GPU to GPU
    d_to_d_data_xfer_metric_id = hpcrun_set_new_metric_info(blame_kind, "D_TO_D_BYTES");
    // uva_data_xfer_metric_id is the number of bytes xfered over CUDA unified virtual address
    uva_data_xfer_metric_id = hpcrun_set_new_metric_info(blame_kind, "UVA_BYTES");
    // Accumulates the time between last kernel end to current Sync point as a potential GPU overload factor
    gpu_overload_potential_metric_id = hpcrun_set_new_metric_info(blame_kind, "GPU_OVERLOAD_POTENTIAL");

    hpcrun_close_kind(blame_kind);
   
	 	if (ENABLE_CUDA) {	// is this check sufficient
			bs_entry.fn = dlsym(RTLD_DEFAULT, "gpu_blame_shifter");
		} else if (ENABLE_OPENCL && is_opencl_blame_shifting_enabled()) {
			printf("test: gpu_blame.c--->OPENCL\n");
    	bs_entry.fn = dlsym(RTLD_DEFAULT, "opencl_gpu_blame_shifter");
		}
    bs_entry.next = 0;
    blame_shift_register(&bs_entry);
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

