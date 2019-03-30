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

/******************************************************************************
 * system includes
 *****************************************************************************/

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

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "nvidia.h"
#include "cupti-api.h"
#include "../simple_oo.h"
#include "../sample_source_obj.h"
#include "../common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/device-finalizers.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <ompt/ompt-interface.h>


/******************************************************************************
 * macros
 *****************************************************************************/
#define NVIDIA_DEBUG 0

#if NVIDIA_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define FORALL_ME(macro)	  \
  macro("ME_UNKNOWN_BYTES",       0) \
  macro("ME_PAGEABLE_BYTES",      1) \
  macro("ME_PINNED_BYTES",        2) \
  macro("ME_DEVICE_BYTES",        3) \
  macro("ME_ARRAY_BYTES",         4) \
  macro("ME_MANAGED_BYTES",       5) \
  macro("ME_DEVICE_STATIC_BYTES", 6)

#define FORALL_ME_TIME(macro) \
  macro("ME_TIME (us)",           7)

#define FORALL_KE(macro)	  \
  macro("KE_STATIC_MEM_BYTES",  0)       \
  macro("KE_DYNAMIC_MEM_BYTES", 1)       \
  macro("KE_LOCAL_MEM_BYTES",   2)       \
  macro("KE_ACTIVE_WARPS_PER_SM", 3)     \
  macro("KE_MAX_ACTIVE_WARPS_PER_SM", 4) \
  macro("KE_COUNT ", 5)                  \

#define FORALL_KE_TIME(macro) \
  macro("KE_TIME (us)",         6)

#define FORALL_EM(macro)	\
  macro("EM_INVALID",       0)	\
  macro("EM_HTOD_BYTES",    1)	\
  macro("EM_DTOH_BYTES",    2)	\
  macro("EM_HTOA_BYTES",    3)	\
  macro("EM_ATOH_BYTES",    4)	\
  macro("EM_ATOA_BYTES",    5)	\
  macro("EM_ATOD_BYTES",    6)	\
  macro("EM_DTOA_BYTES",    7)	\
  macro("EM_DTOD_BYTES",    8)	\
  macro("EM_HTOH_BYTES",    9)	\
  macro("EM_PTOP_BYTES",   10) 

#define FORALL_EM_TIME(macro)   \
  macro("EM_TIME (us)",    11)  

#define FORALL_SYNC(macro) \
  macro("SYNC_UNKNOWN (us)",     0) \
  macro("SYNC_EVENT (us)",       1) \
  macro("SYNC_STREAM (us)",      2) \
  macro("SYNC_STREAM_WAIT (us)", 3) \
  macro("SYNC_CONTEXT (us)",     4)

#define FORALL_GL(macro) \
  macro("GL_LOAD_CACHED_BYTES",             0) \
  macro("GL_LOAD_UNCACHED_BYTES",           1) \
  macro("GL_STORE_BYTES",                   2) \
  macro("GL_LOAD_CACHED_L2_TRANS",          3) \
  macro("GL_LOAD_UNCACHED_L2_TRANS",        4) \
  macro("GL_STORE_L2_TRANS",                5) \
  macro("GL_LOAD_CACHED_THEORETIC_TRANS",   6) \
  macro("GL_LOAD_UNCACHED_THEORETIC_TRANS", 7) \
  macro("GL_STORE_THEORETIC_TRANS",         8)

#define FORALL_SH(macro) \
  macro("SH_LOAD_BYTES",            0) \
  macro("SH_STORE_BYTES",           1) \
  macro("SH_LOAD_TRANS",            2) \
  macro("SH_STORE_TRANS",           3) \
  macro("SH_LOAD_THEORETIC_TRANS",  4) \
  macro("SH_STORE_THEORETIC_TRANS", 5)

#define FORALL_BH(macro) \
  macro("BH_WARP_DIVERGED", 0) \
  macro("BH_WARP_EXECUTED", 1)

#define FORALL_INFO(macro)	\
  macro("INFO_DROPPED_SAMPLES", 0)  \
  macro("INFO_PERIOD_IN_CYCLES", 1) \
  macro("INFO_TOTAL_SAMPLES", 2)    \
  macro("INFO_SM_FULL_SAMPLES", 3)

#if CUPTI_API_VERSION >= 10
#define FORALL_IM(macro)	\
  macro("IM_INVALID",       0)	\
  macro("IM_HTOD_BYTES",    1)	\
  macro("IM_DTOH_BYTES",    2)	\
  macro("IM_CPU_PF",        3)	\
  macro("IM_GPU_PF",        4)	\
  macro("IM_THRASH",        5)	\
  macro("IM_THROT",         6)	\
  macro("IM_RMAP",          7)	\
  macro("IM_DTOD_BYTES",    8)

#define FORALL_IM_TIME(macro)  \
  macro("IM_TIME (us)",     9)  
#else
#define FORALL_IM(macro)	\
  macro("IM_INVALID",       0)	\
  macro("IM_HTOD_BYTES",    1)	\
  macro("IM_DTOH_BYTES",    2)	\
  macro("IM_CPU_PF",        3)	\
  macro("IM_GPU_PF",        4)

#define FORALL_IM_TIME(macro)   \
  macro("IM_TIME (us)",     5)
#endif

#if CUPTI_API_VERSION >= 10
#define FORALL_STL(macro)	\
  macro("STL_INVALID",      0)	\
  macro("STL_NONE",         1)	\
  macro("STL_IFETCH",       2)	\
  macro("STL_EXC_DEP",      3)	\
  macro("STL_MEM_DEP",      4)	\
  macro("STL_TEX",          5)	\
  macro("STL_SYNC",         6)	\
  macro("STL_CMEM_DEP",     7)	\
  macro("STL_PIPE_BSY",     8)	\
  macro("STL_MEM_THR",      9)	\
  macro("STL_NOSEL",       10)	\
  macro("STL_OTHR",        11)	\
  macro("STL_SLEEP",       12)

#define FORALL_GPU_INST(macro)  \
  macro("GPU_ISAMP",        13)  

#define FORALL_GPU_INST_LAT(macro)  \
  macro("GPU_LAT_ISAMP",    14)  
#else
#define FORALL_STL(macro)	\
  macro("STL_INVALID",      0)	\
  macro("STL_NONE",         1)	\
  macro("STL_IFETCH",       2)	\
  macro("STL_EXC_DEP",      3)	\
  macro("STL_MEM_DEP",      4)	\
  macro("STL_TEX",          5)	\
  macro("STL_SYNC",         6)	\
  macro("STL_CMEM_DEP",     7)	\
  macro("STL_PIPE_BSY",     8)	\
  macro("STL_MEM_THR",      9)	\
  macro("STL_NOSEL",       10)	\
  macro("STL_OTHR",        11)

#define FORALL_GPU_INST(macro)  \
  macro("GPU_ISAMP",       12)  

#define FORALL_GPU_INST_LAT(macro)  \
  macro("GPU_LAT_ISAMP",   13)  
#endif


#define COUNT_FORALL_CLAUSE(a,b) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)

#if 0
#define OMPT_MEMORY_EXPLICIT  "dev_ex_memcpy"
#define OMPT_MEMORY_IMPLICIT  "dev_im_memcpy"
#define OMPT_KERNEL_EXECUTION "dev_kernel"
#endif

#define OMPT_NVIDIA "nvidia-ompt" 
#define OMPT_PC_SAMPLING "nvidia-ompt-pc-sampling"
#define CUDA_NVIDIA "nvidia-cuda" 
#define CUDA_PC_SAMPLING "nvidia-cuda-pc-sampling" 

/******************************************************************************
 * local variables 
 *****************************************************************************/

// finalizers
static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;

static kind_info_t* ke_kind; // kernel execution
static kind_info_t* em_kind; // explicit memory copies
static kind_info_t* im_kind; // implicit memory events
static kind_info_t* me_kind; // memory allocation and deletion
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

static int gl_metric_id[NUM_CLAUSES(FORALL_GL)];

static int sh_metric_id[NUM_CLAUSES(FORALL_SH)];

static int bh_metric_id[NUM_CLAUSES(FORALL_BH)];
static int bh_diverged_metric_id;
static int bh_executed_metric_id;

static int ke_metric_id[NUM_CLAUSES(FORALL_KE)+1];
static int ke_static_shared_metric_id;
static int ke_dynamic_shared_metric_id;
static int ke_local_metric_id;
static int ke_active_warps_per_sm_metric_id;
static int ke_max_active_warps_per_sm_metric_id;
static int ke_count_metric_id;
static int ke_time_metric_id;

static int info_metric_id[NUM_CLAUSES(FORALL_INFO)];
static int info_dropped_samples_id;
static int info_period_in_cycles_id;
static int info_total_samples_id;
static int info_sm_full_samples_id;

static int sync_metric_id[NUM_CLAUSES(FORALL_SYNC)];

static long pc_sampling_frequency = 1;

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
  CUPTI_ACTIVITY_KIND_MEMORY,
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
//  CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS,
//  CUPTI_ACTIVITY_KIND_SHARED_ACCESS,
  CUPTI_ACTIVITY_KIND_BRANCH,
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


void
cupti_activity_attribute(cupti_activity_t *activity, cct_node_t *cct_node)
{
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    {
      PRINT("CUPTI_ACTIVITY_KIND_PC_SAMPLING\n");
      if (activity->data.pc_sampling.stallReason != 0x7fffffff) {
        int index = stall_metric_id[activity->data.pc_sampling.stallReason];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = activity->data.pc_sampling.latencySamples});

        metrics = hpcrun_reify_metric_set(cct_node, gpu_inst_metric_id);
        hpcrun_metric_std_inc(gpu_inst_metric_id, metrics, (cct_metric_data_t){.i = activity->data.pc_sampling.samples});

        metrics = hpcrun_reify_metric_set(cct_node, gpu_inst_lat_metric_id);
        hpcrun_metric_std_inc(gpu_inst_lat_metric_id, metrics, (cct_metric_data_t){.i = activity->data.pc_sampling.latencySamples});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    {
      PRINT("CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO\n");
      metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, info_dropped_samples_id);
      hpcrun_metric_std_inc(info_dropped_samples_id, metrics,
        (cct_metric_data_t){.i = activity->data.pc_sampling_record_info.droppedSamples});

      metrics = hpcrun_reify_metric_set(cct_node, info_period_in_cycles_id);
      hpcrun_metric_std_set(info_period_in_cycles_id, metrics,
        (cct_metric_data_t){.i = activity->data.pc_sampling_record_info.samplingPeriodInCycles});

      metrics = hpcrun_reify_metric_set(cct_node, info_total_samples_id);
      hpcrun_metric_std_inc(info_total_samples_id, metrics,
        (cct_metric_data_t){.i = activity->data.pc_sampling_record_info.totalSamples});

      metrics = hpcrun_reify_metric_set(cct_node, info_sm_full_samples_id);
      hpcrun_metric_std_inc(info_sm_full_samples_id, metrics,
        (cct_metric_data_t){.i = activity->data.pc_sampling_record_info.fullSMSamples});
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMCPY:
    {
      PRINT("CUPTI_ACTIVITY_KIND_MEMCPY\n");
      if (activity->data.memcpy.copyKind != 0x7fffffff) {
        int index = em_metric_id[activity->data.memcpy.copyKind];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = activity->data.memcpy.bytes});

        metrics = hpcrun_reify_metric_set(cct_node, em_time_metric_id);
        hpcrun_metric_std_inc(em_time_metric_id, metrics, (cct_metric_data_t){.r =
          (activity->data.memcpy.end - activity->data.memcpy.start) / 1000.0});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_KERNEL:
    {
      PRINT("CUPTI_ACTIVITY_KIND_KERNEL\n");
      metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, ke_static_shared_metric_id);
      hpcrun_metric_std_inc(ke_static_shared_metric_id, metrics, (cct_metric_data_t){.i = activity->data.kernel.staticSharedMemory});

      metrics = hpcrun_reify_metric_set(cct_node, ke_dynamic_shared_metric_id);
      hpcrun_metric_std_inc(ke_dynamic_shared_metric_id, metrics, (cct_metric_data_t){.i = activity->data.kernel.dynamicSharedMemory});

      metrics = hpcrun_reify_metric_set(cct_node, ke_local_metric_id);
      hpcrun_metric_std_inc(ke_local_metric_id, metrics, (cct_metric_data_t){.i = activity->data.kernel.localMemoryTotal});

      metrics = hpcrun_reify_metric_set(cct_node, ke_active_warps_per_sm_metric_id);
      hpcrun_metric_std_inc(ke_active_warps_per_sm_metric_id, metrics,
        (cct_metric_data_t){.i = activity->data.kernel.activeWarpsPerSM});

      metrics = hpcrun_reify_metric_set(cct_node, ke_max_active_warps_per_sm_metric_id);
      hpcrun_metric_std_inc(ke_max_active_warps_per_sm_metric_id, metrics,
        (cct_metric_data_t){.i = activity->data.kernel.maxActiveWarpsPerSM});

      metrics = hpcrun_reify_metric_set(cct_node, ke_count_metric_id);
      hpcrun_metric_std_inc(ke_count_metric_id, metrics, (cct_metric_data_t){.i = 1});

      metrics = hpcrun_reify_metric_set(cct_node, ke_time_metric_id);
      hpcrun_metric_std_inc(ke_time_metric_id, metrics, (cct_metric_data_t){.r =
        (activity->data.kernel.end - activity->data.kernel.start) / 1000.0});
      break;
    }
    case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
    {
      PRINT("CUPTI_ACTIVITY_KIND_SYNCHRONIZATION\n");
      if (activity->data.synchronization.syncKind != 0x7fffffff) {
        int index = sync_metric_id[activity->data.synchronization.syncKind];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){ .r =
          (activity->data.synchronization.end - activity->data.synchronization.start) / 1000.0});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMORY:
    {
      PRINT("CUPTI_ACTIVITY_KIND_MEMORY\n");
      if (activity->data.memory.memKind != 0x7fffffff) {
        int index = me_metric_id[activity->data.memory.memKind];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = activity->data.memory.bytes});

        metrics = hpcrun_reify_metric_set(cct_node, me_time_metric_id);
        hpcrun_metric_std_inc(me_time_metric_id, metrics, (cct_metric_data_t){.r =
          (activity->data.memory.end - activity->data.memory.start) / 1000.0});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
    {
      PRINT("CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS\n");
      int type = activity->data.global_access.type;
      int l2_transactions_index = gl_metric_id[type];

      metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, l2_transactions_index);
      hpcrun_metric_std_inc(l2_transactions_index, metrics, (cct_metric_data_t){.i = activity->data.global_access.l2_transactions});

      int l2_theoretical_transactions_index = gl_metric_id[CUPTI_GLOBAL_ACCESS_COUNT + type];
      metrics = hpcrun_reify_metric_set(cct_node, l2_theoretical_transactions_index);
      hpcrun_metric_std_inc(l2_theoretical_transactions_index, metrics,
        (cct_metric_data_t){.i = activity->data.global_access.theoreticalL2Transactions});

      int bytes_index = gl_metric_id[CUPTI_GLOBAL_ACCESS_COUNT * 2 + type];
      metrics = hpcrun_reify_metric_set(cct_node, bytes_index);
      hpcrun_metric_std_inc(bytes_index, metrics, (cct_metric_data_t){.i = activity->data.global_access.bytes});
      break;
    }
    case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
    {
      PRINT("CUPTI_ACTIVITY_KIND_SHARED_ACCESS\n");
      int type = activity->data.shared_access.type;
      int shared_transactions_index = sh_metric_id[type];

      metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, shared_transactions_index);
      hpcrun_metric_std_inc(shared_transactions_index, metrics,
        (cct_metric_data_t){.i = activity->data.shared_access.sharedTransactions});

      int theoretical_shared_transactions_index = sh_metric_id[CUPTI_SHARED_ACCESS_COUNT + type];
      metrics = hpcrun_reify_metric_set(cct_node, theoretical_shared_transactions_index);
      hpcrun_metric_std_inc(theoretical_shared_transactions_index, metrics,
        (cct_metric_data_t){.i = activity->data.shared_access.theoreticalSharedTransactions});

      int bytes_index = sh_metric_id[CUPTI_SHARED_ACCESS_COUNT * 2 + type];
      metrics = hpcrun_reify_metric_set(cct_node, bytes_index);
      hpcrun_metric_std_inc(bytes_index, metrics, (cct_metric_data_t){.i = activity->data.shared_access.bytes});
      break;
    }
    case CUPTI_ACTIVITY_KIND_BRANCH:
    {
      PRINT("CUPTI_ACTIVITY_KIND_BRANCH\n");
      metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, bh_diverged_metric_id);
      hpcrun_metric_std_inc(bh_diverged_metric_id, metrics, (cct_metric_data_t){.i = activity->data.branch.diverged});

      metrics = hpcrun_reify_metric_set(cct_node, bh_executed_metric_id);
      hpcrun_metric_std_inc(bh_executed_metric_id, metrics, (cct_metric_data_t){.i = activity->data.branch.executed});
      break;
    }
    default:
      break;
  }
}


int
cupti_pc_sampling_frequency_get()
{
  return pc_sampling_frequency;
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
  return hpcrun_ev_is(ev_str, OMPT_NVIDIA) || hpcrun_ev_is(ev_str, CUDA_NVIDIA) ||
    hpcrun_ev_is(ev_str, OMPT_PC_SAMPLING) || hpcrun_ev_is(ev_str, CUDA_PC_SAMPLING);

#if 0
    hpcrun_ev_is(ev_str, OMPT_MEMORY_EXPLICIT) ||
    hpcrun_ev_is(ev_str, OMPT_MEMORY_IMPLICIT) ||
    hpcrun_ev_is(ev_str, OMPT_KERNEL_EXECUTION);
#endif
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;

  TMSG(CUDA,"nevents = %d", nevents);

#define getindex(name, index) index
#define declare_stall_metric(name, index) \
  stall_metric_id[index] = hpcrun_set_new_metric_info(ke_kind, name);

  ke_kind = hpcrun_metrics_new_kind();
  FORALL_STL(declare_stall_metric);	
  FORALL_GPU_INST(declare_stall_metric);
  FORALL_GPU_INST_LAT(declare_stall_metric);
  gpu_inst_metric_id = stall_metric_id[FORALL_GPU_INST(getindex)];
  gpu_inst_lat_metric_id = stall_metric_id[FORALL_GPU_INST_LAT(getindex)];
  hpcrun_close_kind(ke_kind);

#define declare_im_metric(name, index) \
  im_metric_id[index] = hpcrun_set_new_metric_info(im_kind, name);

#define declare_im_metric_time(name, index) \
  im_metric_id[index] = hpcrun_set_new_metric_info_and_period(im_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

  im_kind = hpcrun_metrics_new_kind();
  FORALL_IM(declare_im_metric);	
  FORALL_IM_TIME(declare_im_metric_time);
  im_time_metric_id = im_metric_id[FORALL_IM_TIME(getindex)];
  hpcrun_close_kind(im_kind);

#define declare_em_metric(name, index) \
  em_metric_id[index] = hpcrun_set_new_metric_info(em_kind, name);

#define declare_em_metric_time(name, index) \
  em_metric_id[index] = hpcrun_set_new_metric_info_and_period(em_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

  em_kind = hpcrun_metrics_new_kind();
  FORALL_EM(declare_em_metric);	
  FORALL_EM_TIME(declare_em_metric_time);
  em_time_metric_id = em_metric_id[FORALL_EM_TIME(getindex)];
  hpcrun_close_kind(em_kind);

#define declare_me_metric(name, index) \
  me_metric_id[index] = hpcrun_set_new_metric_info(me_kind, name);

#define declare_me_metric_time(name, index) \
  me_metric_id[index] = hpcrun_set_new_metric_info_and_period(me_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

  me_kind = hpcrun_metrics_new_kind();
  FORALL_ME(declare_me_metric);	
  FORALL_ME_TIME(declare_me_metric_time);
  me_time_metric_id = me_metric_id[FORALL_ME_TIME(getindex)];
  hpcrun_close_kind(me_kind);

#define declare_ke_metric(name, index) \
  ke_metric_id[index] = hpcrun_set_new_metric_info(ke_kind, name);

#define declare_ke_metric_time(name, index) \
  ke_metric_id[index] = hpcrun_set_new_metric_info_and_period(ke_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

  ke_kind = hpcrun_metrics_new_kind();
  FORALL_KE(declare_ke_metric);	
  FORALL_KE_TIME(declare_ke_metric_time);	
  ke_static_shared_metric_id = ke_metric_id[0];
  ke_dynamic_shared_metric_id = ke_metric_id[1];
  ke_local_metric_id = ke_metric_id[2];
  ke_active_warps_per_sm_metric_id = ke_metric_id[3];
  ke_max_active_warps_per_sm_metric_id = ke_metric_id[4];
  ke_count_metric_id = ke_metric_id[5];
  ke_time_metric_id = ke_metric_id[6];
  hpcrun_close_kind(ke_kind);

#define declare_sync_metric(name, index) \
  sync_metric_id[index] = hpcrun_set_new_metric_info_and_period(sync_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

  sync_kind = hpcrun_metrics_new_kind();
  FORALL_SYNC(declare_sync_metric);	
  hpcrun_close_kind(sync_kind);

#define declare_gl_metric(name, index) \
  gl_metric_id[index] = hpcrun_set_new_metric_info(gl_kind, name);

  gl_kind = hpcrun_metrics_new_kind();
  FORALL_GL(declare_gl_metric);	
  hpcrun_close_kind(gl_kind);

#define declare_sh_metric(name, index) \
  sh_metric_id[index] = hpcrun_set_new_metric_info(sh_kind, name);

  sh_kind = hpcrun_metrics_new_kind();
  FORALL_SH(declare_sh_metric);	
  hpcrun_close_kind(sh_kind);

#define declare_bh_metric(name, index) \
  bh_metric_id[index] = hpcrun_set_new_metric_info(bh_kind, name);

  bh_kind = hpcrun_metrics_new_kind();
  FORALL_BH(declare_bh_metric);	
  bh_diverged_metric_id = bh_metric_id[0];
  bh_executed_metric_id = bh_metric_id[1];
  hpcrun_close_kind(bh_kind);

#define declare_info_metrics(name, index) \
  info_metric_id[index] = hpcrun_set_new_metric_info(info_kind, name);

  info_kind = hpcrun_metrics_new_kind();
  FORALL_INFO(declare_info_metrics);	
  info_dropped_samples_id = info_metric_id[0];
  info_period_in_cycles_id = info_metric_id[1];
  info_total_samples_id = info_metric_id[2];
  info_sm_full_samples_id = info_metric_id[3];
  hpcrun_close_kind(info_kind);

  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  char name[128];
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &pc_sampling_frequency, 1);

  if (hpcrun_ev_is(name, CUDA_NVIDIA) || hpcrun_ev_is(name, CUDA_PC_SAMPLING)) {
    // Register device finailzers
    device_finalizer_flush.fn = cupti_device_flush;
    device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);
    device_finalizer_shutdown.fn = cupti_device_shutdown;
    device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);

    cupti_monitoring_set(driver_activities, true);

    cupti_monitoring_set(runtime_activities, true);

    //// TODO(keren) ppecify desired monitoring
    //} else {
    //}

    if (hpcrun_ev_is(name, CUDA_PC_SAMPLING)) {
      cupti_pc_sampling_enable();
    }

    cupti_monitoring_set(kernel_invocation_activities, true);

    cupti_monitoring_set(kernel_execution_activities, true);

    cupti_monitoring_set(data_motion_explicit_activities, true);

    cupti_metrics_init();

    cupti_trace_init();

    // Cannot set pc sampling frequency without knowing context,
    // ompt_set_pc_sampling_frequency(device, cupti_get_pc_sampling_frequency());
    cupti_callbacks_subscribe();

    cupti_correlation_enable();

    cupti_trace_start();
  }

  if (hpcrun_ev_is(name, OMPT_PC_SAMPLING)) {
    ompt_pc_sampling_enable();
  }
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

#include "../ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/
