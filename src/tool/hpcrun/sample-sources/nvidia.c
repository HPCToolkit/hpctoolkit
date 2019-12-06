// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/papi.c $
// $Id: papi.c 3328 2010-12-23 23:39:09Z tallent $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2017, Rice University
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
// ******************************************************* EndRiceCopyright *

//
// CUPTI synchronous sampling via PAPI sample source simple oo interface
//

//******************************************************************************
// system includes
//******************************************************************************

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>


#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif



//******************************************************************************
// nvidia includes
//******************************************************************************

#include <cupti_activity.h>



//******************************************************************************
// libmonitor includes
//******************************************************************************

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "nvidia.h"
// #include "cubin-id-map.h"
// #include "cuda-state-placeholders.h"
// #include "gpu-driver-state-placeholders.h"

#include "gpu/gpu-activity.h"
#include "gpu/nvidia/cuda-api.h"
#include "gpu/nvidia/cupti-api.h"
#include "gpu/gpu-trace.h"
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
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>


/******************************************************************************
 * macros
 *****************************************************************************/
#define NVIDIA_DEBUG 0

#if NVIDIA_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define FORALL_ME(macro) \
  macro("GMAL:UNK (b)",       0) \
  macro("GMAL:PAG (b)",      1) \
  macro("GMAL:PIN (b)",       2) \
  macro("GMAL:DEV (b)",       3) \
  macro("GMAL:ARY (b)",       4) \
  macro("GMAL:MAN (b)",       5) \
  macro("GMAL:DST (b)",     6)

#define FORALL_ME_TIME(macro) \
  macro("MEM_ALLOC:TIME (us)",           7)

#define FORALL_ME_SET(macro) \
  macro("GMSET:UNK (b)",       0) \
  macro("GMSET:PAG (b)",       1) \
  macro("GMSET:PIN (b)",       2) \
  macro("GMSET:DEV (b)",       3) \
  macro("GMSET:ARY (b)",       4) \
  macro("GMSET:MAN (b)",       5) \
  macro("GMSET:DST (b)",       6)

#define FORALL_ME_SET_TIME(macro) \
  macro("GMSET:TIME (us)",           7)

#define FORALL_KE(macro) \
  macro("GKER:STMEM (b)",  0)       \
  macro("GKER:DYMEM (b)", 1)       \
  macro("GKER:LMEM (b)",   2)       \
  macro("GKER:ACT_FGP", 3)     \
  macro("GKER:MAX_FGP", 4) \
  macro("GKER:THR_REG", 5)       \
  macro("GKER:BLK_THR", 6)           \
  macro("GKER:BLK_SM", 7)     \
  macro("GKER:COUNT ", 8)                  \

#define FORALL_KE_TIME(macro) \
  macro("KERNEL:TIME (us)", 9)

#define FORALL_KE_OCCUPANCY(macro) \
  macro("GKER:OCC", 10)

#define FORALL_EM(macro) \
  macro("GXMV:INVL",       0)	\
  macro("GXMV:H2DB",    1)	\
  macro("GXMV:D2HB",    2)	\
  macro("GXMV:H2AB",    3)	\
  macro("GXMV:A2HB",    4)	\
  macro("GXMV:A2AB",    5)	\
  macro("GXMV:A2DB",    6)	\
  macro("GXMV:D2AB",    7)	\
  macro("GXMV:D2DB",    8)	\
  macro("GXMV:H2HB",    9)	\
  macro("GXMV:P2PB",   10) 

#define FORALL_EM_TIME(macro) \
  macro("GXMV:TIME (us)",    11)  

#define FORALL_SYNC(macro) \
  macro("GSYNC:UNK (us)",       0) \
  macro("GSYNC:EVT (us)",       1) \
  macro("GSYNC:STRW (us)",      2) \
  macro("GSYNC:STR (us)",       3) \
  macro("GSYNC:CTX (us)",       4)

#define FORALL_SYNC_TIME(macro) \
  macro("GSYNC:TIME (us)",     5)

#define FORALL_GL(macro)	    \
  macro("GGM:CLD (B)",          0) \
  macro("GGM:ULD (B)",          1) \
  macro("GGM:ST (B)",           2) \
  macro("GGM:CLD (L2T)",          3)		\
  macro("GGM:ULD (L2T)",          4)		\
  macro("GGM:ST (L2T)",           5)		\
  macro("GGM:TCLD (L2T)",          6)		\
  macro("GGM:TULD (L2T)",          7)		\
  macro("GGM:TST (L2T)",           8)

#define FORALL_SH(macro) \
  macro("GLM:LD (B)",            0)	\
  macro("GLM:ST (B)",           1)	 \
  macro("GLM:LD (T)",            2)	 \
  macro("GLM:ST (T)",           3)	 \
  macro("GLM:TLD (T)",  4)	\
  macro("GLM:TST (T)", 5)

#define FORALL_BH(macro) \
  macro("BRANCH:WARP_DIVERGED", 0) \
  macro("BRANCH:WARP_EXECUTED", 1)

#define FORALL_INFO(macro) \
  macro("GKER:SAMPLES_DROPPED",   0) \
  macro("GKER:PERIOD_CYCLES",     1) \
  macro("GKER:SAMPLES_TOTAL",     2) \
  macro("GKER:SAMPLES_EXPECTED",  3)

#define FORALL_INFO_EFFICIENCY(macro) \
  macro("GKER:SM_EFFICIENCY", 4)

#if CUPTI_API_VERSION >= 10
#define FORALL_IM(macro) \
  macro("GIMV:INVALID",       0)	\
  macro("GIMV:H2D (B)",    1)		\
  macro("GIMV:D2H (B)",    2)		\
  macro("GIMV:CPU_PF",        3)	\
  macro("GIMV:GPU_PF",        4)	\
  macro("GIMV:THRASH",        5)	\
  macro("GIMV:THROT",         6)	\
  macro("GIMV:RMAP",          7)	\
  macro("GIMV:D2D (B)",    8)

#define FORALL_IM_TIME(macro) \
  macro("GIMV:TIME (us)",     9)  
#else
#define FORALL_IM(macro) \
  macro("GIMV:INVL",       0)	\
  macro("GIMV:H2D (B)",    1)	\
  macro("GIMV:D2H (B)",    2)			\
  macro("GIMV:CPU (PF)",        3)		\
  macro("GIMV:GPU (PF)",        4)

#define F
  macro("GIMV:TIME (us)",     5)
#endif

// note: the code in this file related to stall reasons should be
// improved. it uses stall reason codes as indices. it expects stall
// reason codes to be dense. there should be a level of indirection
// between stall reason codes and metric indices.

#if CUPTI_API_VERSION >= 10
#define FORALL_STL(macro)	\
  macro("GSTL:INVL",         GPU_INST_STALL_INVALID)	   \
  macro("GSTL:NONE",         GPU_INST_STALL_NONE)		   \
  macro("GSTL:IFET",         GPU_INST_STALL_IFETCH)		   \
  macro("GSTL:IDEP",         GPU_INST_STALL_IDEPEND)	   \
  macro("GSTL:GMEM",         GPU_INST_STALL_GMEM)		   \
  macro("GSTL:TMEM",         GPU_INST_STALL_TMEM)		   \
  macro("GSTL:SYNC",         GPU_INST_STALL_SYNC)		   \
  macro("GSTL:CMEM",         GPU_INST_STALL_CMEM)		   \
  macro("GSTL:PIPE",         GPU_INST_STALL_PIPE_BUSY)	   \
  macro("GSTL:MTHR",         GPU_INST_STALL_MEM_THROTTLE) \
  macro("GSTL:NSEL",         GPU_INST_STALL_NOT_SELECTED) \
  macro("GSTL:OTHR",         GPU_INST_STALL_OTHER)	   \
  macro("GSTL:SLP",          GPU_INST_STALL_SLEEP)

#define FORALL_GPU_INST(macro) \
  macro("GINS",           13)  

#define FORALL_GPU_INST_LAT(macro) \
  macro("GSTL",          14)  
#else
#define FORALL_STL(macro)	\
  macro("GSTL:INVL",         GPU_GSTL_INVALID)		   \
  macro("GSTL:NONE",         GPU_GSTL_NONE)		   \
  macro("GSTL:IFET",         GPU_GSTL_IFETCH)		   \
  macro("GSTL:IDEP",         GPU_GSTL_IDEPEND)		   \
  macro("GSTL:GMEM",         GPU_GSTL_GMEM)		   \
  macro("GSTL:TMEM",         GPU_GSTL_TMEM)		   \
  macro("GSTL:SYNC",         GPU_GSTL_SYNC)		   \
  macro("GSTL:CMEM",         GPU_GSTL_CMEM)		   \
  macro("GSTL:PIPE",         GPU_GSTL_PIPE_BUSY)	   \
  macro("GSTL:MTHR",         GPU_GSTL_MEM_THROTTLE)   \
  macro("GSTL:NSEL",         GPU_GSTL_NOT_SELECTED)   \
  macro("GSTL:OTHR",         GPU_GSTL_OTHER)	   

#define FORALL_GPU_INST(macro) \
  macro("GINS",           12)  

#define FORALL_GPU_INST_LAT(macro) \
  macro("GSTL",          13)  
#endif


#define COUNT_FORALL_CLAUSE(a,b) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)


#define NVIDIA_CUDA "nvidia-cuda" 
#define NVIDIA_CUDA_PC_SAMPLING "nvidia-cuda-pc-sampling" 

/******************************************************************************
 * local variables 
 *****************************************************************************/

// finalizers
static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;
static device_finalizer_fn_entry_t device_trace_finalizer_shutdown;

static kind_info_t* stall_kind; // gpu insts
static kind_info_t* ke_kind; // kernel execution
static kind_info_t* em_kind; // explicit memory copies
static kind_info_t* im_kind; // implicit memory events
static kind_info_t* me_kind; // memory allocation and deletion
static kind_info_t* me_set_kind; // memory set
static kind_info_t* gl_kind; // global memory accesses
static kind_info_t* sh_kind; // shared memory accesses
static kind_info_t* bh_kind; // branches
static kind_info_t* sync_kind; // stream, context, or event sync
static kind_info_t* info_kind; // pc sampling info

static int stall_metric_id[NUM_CLAUSES(FORALL_STL)+2];
static int gpu_inst_metric_id;
static int gpu_inst_lat_metric_id;

static int em_metric_id[NUM_CLAUSES(FORALL_EM)+1];
static int em_time_metric_id;

static int im_metric_id[NUM_CLAUSES(FORALL_IM)+1];
static int im_time_metric_id;

static int me_metric_id[NUM_CLAUSES(FORALL_ME)+1];
static int me_time_metric_id;

static int me_set_metric_id[NUM_CLAUSES(FORALL_ME_SET)+1];
static int me_set_time_metric_id;

static int gl_metric_id[NUM_CLAUSES(FORALL_GL)];

static int sh_metric_id[NUM_CLAUSES(FORALL_SH)];

static int bh_metric_id[NUM_CLAUSES(FORALL_BH)];
static int bh_diverged_metric_id;
static int bh_executed_metric_id;

static int ke_metric_id[NUM_CLAUSES(FORALL_KE)+2];
static int ke_static_shared_metric_id;
static int ke_dynamic_shared_metric_id;
static int ke_local_metric_id;
static int ke_active_warps_per_sm_metric_id;
static int ke_max_active_warps_per_sm_metric_id;
static int ke_thread_registers_id;
static int ke_block_threads_id;
static int ke_block_shared_memory_id;
static int ke_count_metric_id;
static int ke_time_metric_id;
static int ke_occupancy_metric_id;

static int info_metric_id[NUM_CLAUSES(FORALL_INFO)+1];
static int info_dropped_samples_id;
static int info_period_in_cycles_id;
static int info_total_samples_id;
static int info_sm_full_samples_id;
static int info_sm_efficiency_id;

static int sync_metric_id[NUM_CLAUSES(FORALL_SYNC)+1];
static int sync_time_metric_id;

// default trace all the activities
// -1: disabled, >0: x ms per activity
static long trace_frequency = -1;
static long trace_frequency_default = -1;
// -1: disabled, 5-31: 2^frequency
static long pc_sampling_frequency = -1;
static long pc_sampling_frequency_default = 12;
static int cupti_enabled_activities = 0;
// event name, which is nvidia-cuda
static char nvidia_name[128];

static const unsigned int MAX_CHAR_FORMULA = 32;
static const size_t DEFAULT_DEVICE_BUFFER_SIZE = 1024 * 1024 * 8;
static const size_t DEFAULT_DEVICE_SEMAPHORE_SIZE = 65536;

//******************************************************************************
// constants
//******************************************************************************

CUpti_ActivityKind
external_correlation_activities[] = { 
  CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION, 
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
data_motion_explicit_activities[] = { 
  CUPTI_ACTIVITY_KIND_MEMCPY2,
  CUPTI_ACTIVITY_KIND_MEMCPY, 
  CUPTI_ACTIVITY_KIND_MEMSET, 
// FIXME(keren): memory activity does not have a correlation id
// CUPTI_ACTIVITY_KIND_MEMORY,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
data_motion_implicit_activities[] = { 
  CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
kernel_invocation_activities[] = { 
  CUPTI_ACTIVITY_KIND_KERNEL,
  CUPTI_ACTIVITY_KIND_SYNCHRONIZATION,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
kernel_execution_activities[] = {
  CUPTI_ACTIVITY_KIND_CONTEXT,
  CUPTI_ACTIVITY_KIND_FUNCTION,
// FIXME(keren): cupti does not allow the following activities to be profiled with pc sampling
// CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS,
// CUPTI_ACTIVITY_KIND_SHARED_ACCESS,
// CUPTI_ACTIVITY_KIND_BRANCH,
  CUPTI_ACTIVITY_KIND_INVALID
};                                   


CUpti_ActivityKind
overhead_activities[] = {
  CUPTI_ACTIVITY_KIND_OVERHEAD,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
driver_activities[] = {
  CUPTI_ACTIVITY_KIND_DEVICE,
  CUPTI_ACTIVITY_KIND_DRIVER,
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
runtime_activities[] = {
  CUPTI_ACTIVITY_KIND_DEVICE,
  CUPTI_ACTIVITY_KIND_RUNTIME,
  CUPTI_ACTIVITY_KIND_INVALID
};


typedef enum cupti_activities_flags {
  CUPTI_DATA_MOTION_EXPLICIT = 1,
  CUPTI_DATA_MOTION_IMPLICIT = 2,
  CUPTI_KERNEL_INVOCATION    = 4,
  CUPTI_KERNEL_EXECUTION     = 8,
  CUPTI_DRIVER               = 16,
  CUPTI_RUNTIME	             = 32,
  CUPTI_OVERHEAD	     = 64
} cupti_activities_flags_t;

int
cupti_pc_sampling_frequency_get()
{
  return pc_sampling_frequency;
}


int
cupti_trace_frequency_get()
{
  return trace_frequency;
}


void
cupti_enable_activities
(
 CUcontext context
)
{
  PRINT("Enter cupti_enable_activities\n");

  #define FORALL_ACTIVITIES(macro, context)                                      \
    macro(context, CUPTI_DATA_MOTION_EXPLICIT, data_motion_explicit_activities)  \
    macro(context, CUPTI_KERNEL_INVOCATION, kernel_invocation_activities)        \
    macro(context, CUPTI_KERNEL_EXECUTION, kernel_execution_activities)          \
    macro(context, CUPTI_DRIVER, driver_activities)                              \
    macro(context, CUPTI_RUNTIME, runtime_activities)                            \
    macro(context, CUPTI_OVERHEAD, overhead_activities)

  #define CUPTI_SET_ACTIVITIES(context, activity_kind, activity)  \
    if (cupti_enabled_activities & activity_kind) {               \
      cupti_monitoring_set(context, activity, true);              \
    }

  FORALL_ACTIVITIES(CUPTI_SET_ACTIVITIES, context);

  if (pc_sampling_frequency != -1) {
    PRINT("pc sampling enabled\n");
    cupti_pc_sampling_enable(context, pc_sampling_frequency);
  }

  cupti_correlation_enable();

  PRINT("Exit cupti_enable_activities\n");
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

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
  return hpcrun_ev_is(ev_str, NVIDIA_CUDA) || hpcrun_ev_is(ev_str, NVIDIA_CUDA_PC_SAMPLING);
#else
  return false;
#endif
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;

  TMSG(CUDA,"nevents = %d", nevents);

  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  long int frequency = 0;
  int frequency_default = -1;
  hpcrun_extract_ev_thresh(event, sizeof(nvidia_name), nvidia_name,
    &frequency, frequency_default);

  if (hpcrun_ev_is(nvidia_name, NVIDIA_CUDA)) {
    trace_frequency = frequency == frequency_default ?
      trace_frequency_default : frequency;
    pc_sampling_frequency = frequency_default;
  } else if (hpcrun_ev_is(nvidia_name, NVIDIA_CUDA_PC_SAMPLING)) {
    pc_sampling_frequency = frequency == frequency_default ?
      pc_sampling_frequency_default : frequency;
    trace_frequency = frequency_default;
  }

#define getindex(name, index) index

#define declare_cur_metrics(name, index) \
  cur_metrics[index] = hpcrun_set_new_metric_info(cur_kind, name);

#define declare_cur_metrics_real(name, index) \
  cur_metrics[index] = hpcrun_set_new_metric_info_and_period(cur_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

#define hide_cur_metrics(name, index) \
  hpcrun_set_display(cur_metrics[index], 0);

#define create_cur_kind cur_kind = hpcrun_metrics_new_kind()
#define close_cur_kind hpcrun_close_kind(cur_kind)

  if (hpcrun_ev_is(nvidia_name, NVIDIA_CUDA_PC_SAMPLING)) {
#define cur_kind stall_kind
#define cur_metrics stall_metric_id

    create_cur_kind;
    // GPU INST must be the first kind for sample apportion
    FORALL_GPU_INST(declare_cur_metrics);
    FORALL_GPU_INST_LAT(declare_cur_metrics);
    FORALL_STL(declare_cur_metrics);	
    FORALL_STL(hide_cur_metrics);
    gpu_inst_metric_id = stall_metric_id[FORALL_GPU_INST(getindex)];
    gpu_inst_lat_metric_id = stall_metric_id[FORALL_GPU_INST_LAT(getindex)];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics
  }

#if 0 // Do not process im metrics for now
#define cur_kind im_kind
#define cur_metrics im_metric_id

  create_cur_kind;
  FORALL_IM(declare_cur_metrics);	
  FORALL_IM(hide_cur_metrics);	
  FORALL_IM_TIME(declare_cur_metrics_real);
  im_time_metric_id = im_metric_id[FORALL_IM_TIME(getindex)];
  close_cur_kind;

#undef cur_kind
#undef cur_metrics
#endif

#define cur_kind em_kind
#define cur_metrics em_metric_id

  create_cur_kind;
  FORALL_EM(declare_cur_metrics);	
  FORALL_EM(hide_cur_metrics);
  FORALL_EM_TIME(declare_cur_metrics_real);
  em_time_metric_id = em_metric_id[FORALL_EM_TIME(getindex)];
  close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind me_kind
#define cur_metrics me_metric_id

  create_cur_kind;
  FORALL_ME(declare_cur_metrics);	
  FORALL_ME(hide_cur_metrics);
  FORALL_ME_TIME(declare_cur_metrics_real);
  me_time_metric_id = me_metric_id[FORALL_ME_TIME(getindex)];
  close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind me_set_kind
#define cur_metrics me_set_metric_id

  create_cur_kind;
  FORALL_ME_SET(declare_cur_metrics);	
  FORALL_ME_SET(hide_cur_metrics);
  FORALL_ME_SET_TIME(declare_cur_metrics_real);
  me_set_time_metric_id = me_set_metric_id[FORALL_ME_SET_TIME(getindex)];
  close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind ke_kind
#define cur_metrics ke_metric_id

  create_cur_kind;
  FORALL_KE(declare_cur_metrics);	
  FORALL_KE(hide_cur_metrics);	
  FORALL_KE_TIME(declare_cur_metrics_real);	
  FORALL_KE_OCCUPANCY(declare_cur_metrics_real);
  ke_static_shared_metric_id = ke_metric_id[0];
  ke_dynamic_shared_metric_id = ke_metric_id[1];
  ke_local_metric_id = ke_metric_id[2];
  ke_active_warps_per_sm_metric_id = ke_metric_id[3];
  ke_max_active_warps_per_sm_metric_id = ke_metric_id[4];
  ke_thread_registers_id = ke_metric_id[5];
  ke_block_threads_id = ke_metric_id[6];
  ke_block_shared_memory_id = ke_metric_id[7];
  ke_count_metric_id = ke_metric_id[8];
  ke_time_metric_id = ke_metric_id[9];
  ke_occupancy_metric_id = ke_metric_id[10];

  hpcrun_set_percent(ke_occupancy_metric_id, 1);
  metric_desc_t* ke_occupancy_metric = hpcrun_id2metric_linked(ke_occupancy_metric_id);
  char *ke_occupancy_buffer = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);
  sprintf(ke_occupancy_buffer, "$%d/$%d", ke_active_warps_per_sm_metric_id, ke_max_active_warps_per_sm_metric_id);
  ke_occupancy_metric->formula = ke_occupancy_buffer;

  close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind sync_kind
#define cur_metrics sync_metric_id

  create_cur_kind;
  FORALL_SYNC(declare_cur_metrics_real);	
  FORALL_SYNC(hide_cur_metrics);	
  FORALL_SYNC_TIME(declare_cur_metrics_real);
  sync_time_metric_id = sync_metric_id[5];
  close_cur_kind;

#undef cur_kind
#undef cur_metrics

#if 0 // do not process global memory metrics for now
#define cur_kind gl_kind
#define cur_metrics gl_metric_id

  create_cur_kind;
  FORALL_GL(declare_cur_metrics);	
  FORALL_GL(hide_cur_metrics);	
  close_cur_kind;

#undef cur_kind
#undef cur_metrics
#endif

#if 0 // do not process shared memory metrics for now
#define cur_kind sh_kind
#define cur_metrics sh_metric_id

  create_cur_kind;
  FORALL_SH(declare_cur_metrics);	
  FORALL_SH(hide_cur_metrics);	
  close_cur_kind;

#undef cur_kind
#undef cur_metrics
#endif

#if 0 // do not process branch metrics for now
#define cur_kind bh_kind
#define cur_metrics bh_metric_id

  create_cur_kind;
  FORALL_BH(declare_cur_metrics);	
  FORALL_BH(hide_cur_metrics);	
  bh_diverged_metric_id = bh_metric_id[0];
  bh_executed_metric_id = bh_metric_id[1];
  close_cur_kind;

#undef cur_kind
#undef cur_metrics
#endif

  if (hpcrun_ev_is(nvidia_name, NVIDIA_CUDA_PC_SAMPLING)) {
#define cur_kind info_kind
#define cur_metrics info_metric_id

    create_cur_kind;
    FORALL_INFO(declare_cur_metrics);	
    FORALL_INFO(hide_cur_metrics);
    FORALL_INFO_EFFICIENCY(declare_cur_metrics_real);
    info_dropped_samples_id = info_metric_id[0];
    info_period_in_cycles_id = info_metric_id[1];
    info_total_samples_id = info_metric_id[2];
    info_sm_full_samples_id = info_metric_id[3];
    info_sm_efficiency_id = info_metric_id[4];

    hpcrun_set_percent(info_sm_efficiency_id, 1);
    metric_desc_t* sm_efficiency_metric = hpcrun_id2metric_linked(info_sm_efficiency_id);
    char *sm_efficiency_buffer = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);
    sprintf(sm_efficiency_buffer, "$%d/$%d", info_total_samples_id, info_sm_full_samples_id);
    sm_efficiency_metric->formula = sm_efficiency_buffer;
    close_cur_kind;

#undef cur_kind
#undef cur_metrics
  }


#ifndef HPCRUN_STATIC_LINK
  if (cuda_bind()) {
    EEMSG("hpcrun: unable to bind to NVIDIA CUDA library %s\n", dlerror());
    monitor_real_exit(-1);
  }

  if (cupti_bind()) {
    EEMSG("hpcrun: unable to bind to NVIDIA CUPTI library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif

#if 0
  cuda_init_placeholders();
  gpu_driver_init_placeholders();
#endif

  // Register hpcrun callbacks
  device_finalizer_flush.fn = cupti_device_flush;
  device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);
  device_finalizer_shutdown.fn = cupti_device_shutdown;
  device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);

  // Get control knobs
  int device_buffer_size = control_knob_value_get_int(HPCRUN_CUDA_DEVICE_BUFFER_SIZE);
  int device_semaphore_size = control_knob_value_get_int(HPCRUN_CUDA_DEVICE_SEMAPHORE_SIZE);
  if (device_buffer_size == 0) {
    device_buffer_size = DEFAULT_DEVICE_BUFFER_SIZE;
  }
  if (device_semaphore_size == 0) {
    device_semaphore_size = DEFAULT_DEVICE_SEMAPHORE_SIZE;
  }

  PRINT("Device buffer size %d\n", device_buffer_size);
  PRINT("Device semaphore size %d\n", device_semaphore_size);
  cupti_device_buffer_config(device_buffer_size, device_semaphore_size);

  // Register cupti callbacks
  cupti_init();
  cupti_callbacks_subscribe();
  cupti_start();

  // Set enabling activities
  cupti_enabled_activities |= CUPTI_DRIVER;
  cupti_enabled_activities |= CUPTI_RUNTIME;
  cupti_enabled_activities |= CUPTI_KERNEL_EXECUTION;
  cupti_enabled_activities |= CUPTI_KERNEL_INVOCATION;
  cupti_enabled_activities |= CUPTI_DATA_MOTION_EXPLICIT;
  cupti_enabled_activities |= CUPTI_OVERHEAD;

  // Init records
  gpu_trace_init();

  // Register shutdown functions to write trace files
  device_trace_finalizer_shutdown.fn = gpu_trace_fini;
  device_finalizer_register(device_finalizer_type_shutdown, &device_trace_finalizer_shutdown);
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

#define ss_name nvidia_gpu
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/
