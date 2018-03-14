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

#if 0
#include <cuda.h>
#include <cupti.h>
#endif

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "nvidia.h"
#include "cupti-activity-api.h"
#include "../simple_oo.h"
#include "../sample_source_obj.h"
#include "../common.h"
#include "../../device-finalizers.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>


/******************************************************************************
 * macros
 *****************************************************************************/
#define PRINT(...) fprintf(stderr, __VA_ARGS__)

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
  macro("KE_STATIC_MEM_BYTES",  0) \
  macro("KE_DYNAMIC_MEM_BYTES", 1) \
  macro("KE_LOCAL_MEM_BYTES",   2) \
  macro("KE_TIME (us)",         3) \

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

#define FORALL_EM_TIME(macro)  \
  macro("EM_TIME (us)",    11)  

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

#define FORALL_IM_TIME(macro)  \
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

#define FORALL_STL_LAT(macro)	\
  macro("STL_LAT_INVALID",  13)	\
  macro("STL_LAT_NONE",     14)	\
  macro("STL_LAT_IFETCH",   15)	\
  macro("STL_LAT_EXC_DEP",  16)	\
  macro("STL_LAT_MEM_DEP",  17)	\
  macro("STL_LAT_TEX",      18)	\
  macro("STL_LAT_SYNC",     19)	\
  macro("STL_LAT_CMEM_DEP", 20)	\
  macro("STL_LAT_PIPE_BSY", 21)	\
  macro("STL_LAT_MEM_THR",  22)	\
  macro("STL_LAT_NOSEL",    23)	\
  macro("STL_LAT_OTHR",     24)	\
  macro("STL_LAT_SLEEP",    25)

#define FORALL_GPU_INST(macro)  \
  macro("GPU_ISAMP",        26)  

#define FORALL_GPU_INST_LAT(macro)  \
  macro("GPU_LAT_ISAMP",    27)  
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

#define FORALL_STL_LAT(macro)	  \
  macro("STL_LAT_INVALID", 12)	\
  macro("STL_LAT_NONE",    13)	\
  macro("STL_LAT_IFETCH",  14)	\
  macro("STL_LAT_EXC_DEP", 15)	\
  macro("STL_LAT_MEM_DEP", 16)	\
  macro("STL_LAT_TEX",     17)	\
  macro("STL_LAT_SYNC",    18)	\
  macro("STL_LAT_CMEM_DEP",19)	\
  macro("STL_LAT_PIPE_BSY",20)	\
  macro("STL_LAT_MEM_THR", 21)	\
  macro("STL_LAT_NOSEL",   22)	\
  macro("STL_LAT_OTHR",    23)

#define FORALL_GPU_INST(macro)  \
  macro("GPU_ISAMP",       24)  

#define FORALL_GPU_INST_LAT(macro)  \
  macro("GPU_LAT_ISAMP",   25)  
#endif


#define COUNT_FORALL_CLAUSE(a,b) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)

#if 0
#define OMPT_MEMORY_EXPLICIT  "dev_ex_memcpy"
#define OMPT_MEMORY_IMPLICIT  "dev_im_memcpy"
#define OMPT_KERNEL_EXECUTION "dev_kernel"
#endif

#define OMPT_NVIDIA "nvidia-ompt" 
#define CUDA_NVIDIA "nvidia-cuda" 

/******************************************************************************
 * local variables 
 *****************************************************************************/

// finalizers
static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;


static kind_info_t* me_kind; // memory allocation
static kind_info_t* ke_kind; // kernel execution
static kind_info_t* em_kind; // explicit memory copies
static kind_info_t* im_kind; // implicit memory events


static int stall_metric_id[NUM_CLAUSES(FORALL_STL)+NUM_CLAUSES(FORALL_STL_LAT)+2];
static int gpu_inst_metric_id;
static int gpu_inst_lat_metric_id;

static int em_metric_id[NUM_CLAUSES(FORALL_EM)+1];
static int em_time_metric_id;

static int im_metric_id[NUM_CLAUSES(FORALL_IM)+1];
static int im_time_metric_id;

static int ke_metric_id[NUM_CLAUSES(FORALL_KE)];
static int ke_static_shared_metric_id;
static int ke_dynamic_shared_metric_id;
static int ke_local_metric_id;
static int ke_time_metric_id;

static int me_metric_id[NUM_CLAUSES(FORALL_ME)+1];
static int me_time_metric_id;

static int pc_sampling_frequency = 1;

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
  CUPTI_ACTIVITY_KIND_INVALID
};


CUpti_ActivityKind
kernel_execution_activities[] = {
  CUPTI_ACTIVITY_KIND_CONTEXT,
  CUPTI_ACTIVITY_KIND_FUNCTION,
  CUPTI_ACTIVITY_KIND_PC_SAMPLING,
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
cupti_activity_attribute(CUpti_Activity *record, cct_node_t *node)
{
  switch (record->kind) {
    case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    {
#if CUPTI_API_VERSION >= 10
      CUpti_ActivityPCSampling3 *activity_sample = (CUpti_ActivityPCSampling3 *)record;
#else
      CUpti_ActivityPCSampling2 *activity_sample = (CUpti_ActivityPCSampling2 *)record;
#endif
      if (activity_sample->stallReason != 0x7fffffff) {
        int index = stall_metric_id[activity_sample->stallReason];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = activity_sample->samples});

        index = stall_metric_id[activity_sample->stallReason+NUM_CLAUSES(FORALL_STL)];
        metrics = hpcrun_reify_metric_set(node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = activity_sample->latencySamples});

        metrics = hpcrun_reify_metric_set(node, gpu_inst_metric_id);
        hpcrun_metric_std_inc(gpu_inst_metric_id, metrics, (cct_metric_data_t){.i = activity_sample->samples});

        metrics = hpcrun_reify_metric_set(node, gpu_inst_lat_metric_id);
        hpcrun_metric_std_inc(gpu_inst_lat_metric_id, metrics, (cct_metric_data_t){.i = activity_sample->latencySamples});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_MEMCPY:
    {
      CUpti_ActivityMemcpy *activity_memcpy = (CUpti_ActivityMemcpy *)record;
      if (activity_memcpy->copyKind != 0x7fffffff) {
        int index = em_metric_id[activity_memcpy->copyKind];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = activity_memcpy->bytes});

        metrics = hpcrun_reify_metric_set(node, em_time_metric_id);
        hpcrun_metric_std_inc(em_time_metric_id, metrics, (cct_metric_data_t){.i = activity_memcpy->end - activity_memcpy->start});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
    {
      CUpti_ActivityUnifiedMemoryCounter *activity_unified = (CUpti_ActivityUnifiedMemoryCounter *)record;
      if (activity_unified->counterKind != 0x7fffffff) {
        int index = im_metric_id[activity_unified->counterKind];
        metric_data_list_t *metrics = hpcrun_reify_metric_set(node, index);
        hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t){.i = 1});

        metrics = hpcrun_reify_metric_set(node, im_time_metric_id);
        hpcrun_metric_std_inc(im_time_metric_id, metrics, (cct_metric_data_t){.i = activity_unified->timestamp});
      }
      break;
    }
    case CUPTI_ACTIVITY_KIND_KERNEL:
    {
      CUpti_ActivityKernel4 *activity_kernel = (CUpti_ActivityKernel4 *)record;
      metric_data_list_t *metrics = hpcrun_reify_metric_set(node, ke_static_shared_metric_id);
      hpcrun_metric_std_inc(ke_static_shared_metric_id, metrics, (cct_metric_data_t){.i = activity_kernel->staticSharedMemory});

      metrics = hpcrun_reify_metric_set(node, ke_dynamic_shared_metric_id);
      hpcrun_metric_std_inc(ke_dynamic_shared_metric_id, metrics, (cct_metric_data_t){.i = activity_kernel->dynamicSharedMemory});

      metrics = hpcrun_reify_metric_set(node, ke_local_metric_id);
      hpcrun_metric_std_inc(ke_local_metric_id, metrics, (cct_metric_data_t){.i = activity_kernel->localMemoryTotal});

      metrics = hpcrun_reify_metric_set(node, ke_time_metric_id);
      hpcrun_metric_std_inc(ke_time_metric_id, metrics, (cct_metric_data_t){.i = activity_kernel->end - activity_kernel->start});
      break;
    }
    default:
      break;
  }
}


extern int cupti_pc_sampling_frequency_get()
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
  thread_data_t *td = hpcrun_get_thread_data();

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
  return hpcrun_ev_is(ev_str, OMPT_NVIDIA) || hpcrun_ev_is(ev_str, CUDA_NVIDIA);

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
  FORALL_STL_LAT(declare_stall_metric);	
  FORALL_GPU_INST(declare_stall_metric);
  FORALL_GPU_INST_LAT(declare_stall_metric);
  gpu_inst_metric_id = stall_metric_id[FORALL_GPU_INST(getindex)];
  gpu_inst_lat_metric_id = stall_metric_id[FORALL_GPU_INST_LAT(getindex)];
  hpcrun_close_kind(ke_kind);

#define declare_im_metric(name, index) \
  im_metric_id[index] = hpcrun_set_new_metric_info(im_kind, name);

  im_kind = hpcrun_metrics_new_kind();
  FORALL_IM(declare_im_metric);	
  FORALL_IM_TIME(declare_im_metric);
  im_time_metric_id = im_metric_id[FORALL_IM_TIME(getindex)];
  hpcrun_close_kind(im_kind);

#define declare_em_metric(name, index) \
  em_metric_id[index] = hpcrun_set_new_metric_info(em_kind, name);

  em_kind = hpcrun_metrics_new_kind();
  FORALL_EM(declare_em_metric);	
  FORALL_EM_TIME(declare_em_metric);
  em_time_metric_id = em_metric_id[FORALL_EM_TIME(getindex)];
  hpcrun_close_kind(em_kind);

#define declare_ke_metric(name, index) \
  ke_metric_id[index] = hpcrun_set_new_metric_info(ke_kind, name);

  ke_kind = hpcrun_metrics_new_kind();
  FORALL_KE(declare_ke_metric);	
  ke_static_shared_metric_id = ke_metric_id[0];
  ke_dynamic_shared_metric_id = ke_metric_id[1];
  ke_local_metric_id = ke_metric_id[2];
  ke_time_metric_id = ke_metric_id[3];
  hpcrun_close_kind(ke_kind);

  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  char name[10];
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &pc_sampling_frequency, 1);

  if (hpcrun_ev_is(event, CUDA_NVIDIA)) {
    // Register device finailzers
    device_finalizer_flush.fn = cupti_device_flush;
    device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);
    device_finalizer_shutdown.fn = cupti_device_shutdown;
    device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);

    // Specify desired monitoring
    cupti_monitoring_set(kernel_invocation_activities, true);
                        
    cupti_monitoring_set(kernel_execution_activities, true);
                        
    cupti_monitoring_set(driver_activities, true);
                        
    cupti_monitoring_set(data_motion_explicit_activities, true);
                        
    cupti_monitoring_set(runtime_activities, true);

    cupti_metrics_init();

    cupti_trace_init();

    // Cannot set pc sampling frequency without knowing context
    // ompt_set_pc_sampling_frequency(device, cupti_get_pc_sampling_frequency());
    cupti_callbacks_subscribe();

    cupti_correlation_enable();

    cupti_trace_start();
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
