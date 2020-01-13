// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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
// attribute GPU performance metrics
//

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct2metrics.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/thread_data.h>

#include "gpu-activity.h"
#include "gpu-metrics.h"
#include "gpu-monitoring.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define FORALL_INDEXED_METRIC_KINDS(macro)	\
  macro(GMEM, 0)				\
  macro(GMSET, 1)				\
  macro(GPU_INST_STALL, 2)			\
  macro(GXCOPY, 3)				\
  macro(GSYNC, 4)				\
  macro(GGMEM, 5)				\
  macro(GLMEM, 6)


#define FORALL_SCALAR_METRIC_KINDS(macro)	\
  macro(GBR, 7)					\
  macro(GICOPY, 8)				\
  macro(GPU_INST, 9)				\
  macro(GTIMES, 10)				\
  macro(KINFO, 11)				\
  macro(GSAMP, 12)			


#define FORALL_METRIC_KINDS(macro)	\
  FORALL_INDEXED_METRIC_KINDS(macro)	\
  FORALL_SCALAR_METRIC_KINDS(macro)


//------------------------------------------------------------------------------
// macros for declaring metrics and kinds
//------------------------------------------------------------------------------

// macros for counting entries in a FORALL macro
#define COUNT_FORALL_CLAUSE(a,b,c) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)

#define METRIC_KIND(name)			\
name ## _metric_kind

#define INITIALIZE_METRIC_KINDS(name, value)	\
  static kind_info_t * METRIC_KIND(name) = NULL;

#define METRIC_ID(name) \
  name ## _metric_id

#define INITIALIZE_INDEXED_METRIC(name, value)		\
  static int METRIC_ID(name)[NUM_CLAUSES( FORALL_ ## name)];

#define INITIALIZE_SCALAR_METRIC(string, name, desc)	\
  static int METRIC_ID(name);

#define INITIALIZE_SCALAR_METRIC_KIND(kind, value)	\
  FORALL_ ## kind (INITIALIZE_SCALAR_METRIC)


//------------------------------------------------------------------------------
// metric initialization
//------------------------------------------------------------------------------

#define APPLY(f,n) f(n)

#define INITIALIZE_METRIC_KIND()					\
  APPLY(METRIC_KIND,CURRENT_METRIC) = hpcrun_metrics_new_kind()


#define FINALIZE_METRIC_KIND()				\
  hpcrun_close_kind(APPLY(METRIC_KIND,CURRENT_METRIC))


#define INITIALIZE_INDEXED_METRIC_INT(metric_name, index, metric_desc)	\
   APPLY(METRIC_ID,CURRENT_METRIC)[index] =				\
     hpcrun_set_new_metric_desc_and_period				\
     (APPLY(METRIC_KIND,CURRENT_METRIC), metric_name, metric_desc,	\
     MetricFlags_ValFmt_Int, 1, metric_property_none);


#define INITIALIZE_INDEXED_METRIC_REAL(metric_name, index, metric_desc)	\
  APPLY(METRIC_ID,CURRENT_METRIC)[index] =				\
    hpcrun_set_new_metric_desc_and_period				\
    (APPLY(METRIC_KIND,CURRENT_METRIC), metric_name, metric_desc,	\
     MetricFlags_ValFmt_Real, 1, metric_property_none);


#define INITIALIZE_SCALAR_METRIC_INT(metric_name, metric_var, metric_desc) \
  METRIC_ID(metric_var) =						\
    hpcrun_set_new_metric_desc_and_period				\
    (APPLY(METRIC_KIND,CURRENT_METRIC), metric_name, metric_desc,	\
     MetricFlags_ValFmt_Int, 1, metric_property_none);


#define INITIALIZE_SCALAR_METRIC_REAL(metric_name, metric_var, metric_desc) \
  METRIC_ID(metric_var) =						\
    hpcrun_set_new_metric_desc_and_period				\
    (APPLY(METRIC_KIND,CURRENT_METRIC), metric_name, metric_desc,	\
     MetricFlags_ValFmt_Real, 1, metric_property_none);


#define HIDE_INDEXED_METRIC(name,  index) \
   hpcrun_set_display(APPLY(METRIC_ID,CURRENT_METRIC)[index], 0);



//*****************************************************************************
// local variables 
//*****************************************************************************

FORALL_METRIC_KINDS(INITIALIZE_METRIC_KINDS)

FORALL_INDEXED_METRIC_KINDS(INITIALIZE_INDEXED_METRIC)

FORALL_SCALAR_METRIC_KINDS(INITIALIZE_SCALAR_METRIC_KIND)

static const unsigned int MAX_CHAR_FORMULA = 32;



//*****************************************************************************
// private operations
//*****************************************************************************

static void
gpu_metrics_attribute_metric_int
(
 metric_data_list_t *metrics, 
 int metric_index,
 int value
)
{
  hpcrun_metric_std_inc(metric_index, metrics, (cct_metric_data_t){.i = value});
}


static void
gpu_metrics_attribute_metric_real
(
 metric_data_list_t *metrics, 
 int metric_index,
 float value
)
{
  hpcrun_metric_std_inc(metric_index, metrics, (cct_metric_data_t){.r = value});
}


static void
gpu_metrics_attribute_metric_time_interval
(
 cct_node_t *cct_node,
 int time_index,
 gpu_interval_t *i
)
{
  metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, time_index);

  // convert from ns to s
  gpu_metrics_attribute_metric_real(metrics, time_index, (i->end - i->start) / 1.0e9);
}


static void
gpu_metrics_attribute_pc_sampling
(
 gpu_activity_t *activity
)
{
  uint32_t sample_period = 
    1 << gpu_monitoring_instruction_sample_frequency_get();

  gpu_pc_sampling_t *sinfo = &(activity->details.pc_sampling);
  cct_node_t *cct_node = activity->cct_node;

  int inst_count = sinfo->samples * sample_period;

  metric_data_list_t *inst_metric = 
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_INST_ALL));

  // instruction execution metric
  gpu_metrics_attribute_metric_int(inst_metric, METRIC_ID(GPU_INST_ALL), 
				   inst_count);

  if (sinfo->stallReason != GPU_INST_STALL_INVALID) {
    int stall_summary_metric_index = 
      METRIC_ID(GPU_INST_STALL)[GPU_INST_STALL_ANY];

    int stall_kind_metric_index = METRIC_ID(GPU_INST_STALL)[sinfo->stallReason];

    metric_data_list_t *stall_metrics = 
      hpcrun_reify_metric_set(cct_node, stall_kind_metric_index);

    int stall_count = sinfo->latencySamples * sample_period;

    // stall summary metric
    gpu_metrics_attribute_metric_int(stall_metrics, 
				     stall_summary_metric_index, stall_count);

    // stall reason specific metric
    gpu_metrics_attribute_metric_int(stall_metrics, 
				     stall_kind_metric_index, stall_count);
  }
}


static void
gpu_metrics_attribute_pc_sampling_info
(
 gpu_activity_t *activity
)
{
  gpu_pc_sampling_info_t *s = &(activity->details.pc_sampling_info);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics = 
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_SAMPLE_DROPPED));

  
  // OK to use set here because sampling cycle is changed during execution
  hpcrun_metric_std_set(METRIC_ID(GPU_SAMPLE_PERIOD), metrics,
			(cct_metric_data_t){.i = s->samplingPeriodInCycles});
  
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_SAMPLE_TOTAL), 
				   s->totalSamples);
  
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_SAMPLE_EXPECTED), 
				   s->fullSMSamples);

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_SAMPLE_DROPPED), 
				   s->droppedSamples);
}


static void
gpu_metrics_attribute_mem_op
(
 cct_node_t *cct_node,
 int bytes_metric_index, 
 int time_metric_index, 
 gpu_mem_t *m
)
{
  metric_data_list_t *bytes_metrics = 
    hpcrun_reify_metric_set(cct_node, bytes_metric_index);

  gpu_metrics_attribute_metric_int(bytes_metrics, bytes_metric_index, m->bytes);

  gpu_metrics_attribute_metric_time_interval(cct_node, time_metric_index, 
					     (gpu_interval_t *) m);
}


static void
gpu_metrics_attribute_memory
(
 gpu_activity_t *activity
)
{
  gpu_memory_t *m = &(activity->details.memory);
  cct_node_t *cct_node = activity->cct_node;

  int bytes_metric_index = METRIC_ID(GMEM)[m->memKind];

  gpu_metrics_attribute_mem_op(cct_node, bytes_metric_index, 
			       METRIC_ID(GPU_TIME_MEM), (gpu_mem_t *) m);
}


static void
gpu_metrics_attribute_memcpy
(
 gpu_activity_t *activity
)
{
  gpu_memcpy_t *m = &(activity->details.memcpy);
  cct_node_t *cct_node = activity->cct_node;

  int bytes_metric_index = METRIC_ID(GXCOPY)[m->copyKind];

  gpu_metrics_attribute_mem_op(cct_node, bytes_metric_index, 
			       METRIC_ID(GPU_TIME_XCOPY), (gpu_mem_t *) m);
}


static void
gpu_metrics_attribute_memset
(
 gpu_activity_t *activity
)
{
  gpu_memset_t *m = &(activity->details.memset);
  cct_node_t *cct_node = activity->cct_node;

  int bytes_metric_index = METRIC_ID(GMSET)[m->memKind];

  gpu_metrics_attribute_mem_op(cct_node, bytes_metric_index, 
			       METRIC_ID(GPU_TIME_MSET), (gpu_mem_t *) m);
}


static void
gpu_metrics_attribute_kernel
(
 gpu_activity_t *activity
)
{
  gpu_kernel_t *k = &(activity->details.kernel);
  cct_node_t *cct_node = activity->cct_node;

  if (METRIC_KIND(KINFO)) {
    metric_data_list_t *metrics = 
      hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_KINFO_STMEM));

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_STMEM), 
				     k->staticSharedMemory);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_DYMEM), 
				     k->dynamicSharedMemory);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_LMEM), 
				     k->localMemoryTotal);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_FGP_ACT), 
				     k->activeWarpsPerSM);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_FGP_MAX), 
				     k->maxActiveWarpsPerSM);
  
    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_REGISTERS), 
				     k->threadRegisters);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_BLK_THREADS), 
				     k->blockThreads);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_BLK_SMEM), 
				     k->blockSharedMemory);
  
    // number of kernel launches
    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_COUNT), 1);
  }
  
  // kernel execution time
  gpu_metrics_attribute_metric_time_interval(cct_node, METRIC_ID(GPU_TIME_KER), 
				     (gpu_interval_t *) k);
}


static void
gpu_metrics_attribute_synchronization
(
 gpu_activity_t *activity
)
{
  gpu_synchronization_t *s = &(activity->details.synchronization);
  cct_node_t *cct_node = activity->cct_node;

  int sync_kind_metric_id = METRIC_ID(GSYNC)[s->syncKind];

  gpu_metrics_attribute_metric_time_interval(cct_node, sync_kind_metric_id, 
				     (gpu_interval_t *) s);

  gpu_metrics_attribute_metric_time_interval(cct_node, 
					     METRIC_ID(GPU_TIME_SYNC), 
					     (gpu_interval_t *) s);
}


static void
gpu_metrics_attribute_global_access
(
 gpu_activity_t *activity
)
{
  gpu_global_access_t *g = &(activity->details.global_access);
  cct_node_t *cct_node = activity->cct_node;

  int type = g->type;

  int l2t_index = METRIC_ID(GGMEM)[GPU_GMEM_LD_CACHED_L2TRANS + type];

  metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, l2t_index);

  gpu_metrics_attribute_metric_int(metrics, l2t_index, g->l2_transactions);
  
  int l2t_theoretical_index = 
    METRIC_ID(GMEM)[GPU_GMEM_LD_CACHED_L2TRANS_THEOR + type];

  gpu_metrics_attribute_metric_int(metrics, l2t_theoretical_index, 
				   g->theoreticalL2Transactions);

  int bytes_index = METRIC_ID(GMEM)[GPU_GMEM_LD_CACHED_BYTES + type];
  gpu_metrics_attribute_metric_int(metrics, bytes_index, g->bytes);
}


static void
gpu_metrics_attribute_local_access
(
 gpu_activity_t *activity
)
{
  gpu_local_access_t *l = &(activity->details.local_access);
  cct_node_t *cct_node = activity->cct_node;

  int type = l->type;

  int lmem_trans_index = METRIC_ID(GLMEM)[GPU_LMEM_LD_TRANS + type];
  
  metric_data_list_t *metrics = 
    hpcrun_reify_metric_set(cct_node, lmem_trans_index);

  gpu_metrics_attribute_metric_int(metrics, lmem_trans_index, 
				   l->sharedTransactions);
  
  int lmem_trans_theor_index = METRIC_ID(GLMEM)[GPU_LMEM_LD_TRANS_THEOR + type];
  gpu_metrics_attribute_metric_int(metrics, lmem_trans_theor_index, 
				   l->theoreticalSharedTransactions);
  
  int bytes_index = METRIC_ID(GLMEM)[GPU_LMEM_LD_BYTES + type];
  gpu_metrics_attribute_metric_int(metrics, bytes_index, l->bytes);
}


static void
gpu_metrics_attribute_branch
(
 gpu_activity_t *activity
)
{
  gpu_branch_t *b = &(activity->details.branch);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics = 
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_BR_DIVERGED));

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BR_DIVERGED), 
				   b->diverged);

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BR_EXECUTED), 
				   b->executed);
}


//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_metrics_attribute
(
 gpu_activity_t *activity
)
{
  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead++;
  hpcrun_safe_enter();

  switch (activity->kind) {
  case GPU_ACTIVITY_PC_SAMPLING: 
    gpu_metrics_attribute_pc_sampling(activity);
    break;

  case GPU_ACTIVITY_PC_SAMPLING_INFO:
    gpu_metrics_attribute_pc_sampling_info(activity);
    break;

  case GPU_ACTIVITY_MEMCPY:
    gpu_metrics_attribute_memcpy(activity);
    break;

  case GPU_ACTIVITY_MEMSET:
    gpu_metrics_attribute_memset(activity);
    break;
    
  case GPU_ACTIVITY_KERNEL:
    gpu_metrics_attribute_kernel(activity);
    break;
    
  case GPU_ACTIVITY_SYNCHRONIZATION:
    gpu_metrics_attribute_synchronization(activity);
    break;

  case GPU_ACTIVITY_MEMORY:
    gpu_metrics_attribute_memory(activity);
    break;

  case GPU_ACTIVITY_GLOBAL_ACCESS:
    gpu_metrics_attribute_global_access(activity);
    break;

  case GPU_ACTIVITY_LOCAL_ACCESS:
    gpu_metrics_attribute_local_access(activity);
    break;

  case GPU_ACTIVITY_BRANCH:
    gpu_metrics_attribute_branch(activity);
    break;

  default:
    break;
  }

  hpcrun_safe_exit();
  td->overhead--;
}



void
gpu_metrics_default_enable
(
 void
)
{

// Execution time metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GTIMES

  INITIALIZE_METRIC_KIND();

  FORALL_GTIMES(INITIALIZE_SCALAR_METRIC_REAL)

  FINALIZE_METRIC_KIND();

// Memcpy metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GMEM

  INITIALIZE_METRIC_KIND();

  FORALL_GMEM(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();

// Memset metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GMSET

  INITIALIZE_METRIC_KIND();

  FORALL_GMSET(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();

// GPU explicit copy merics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GXCOPY

  INITIALIZE_METRIC_KIND();

  FORALL_GXCOPY(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();

// GPU synchonrization merics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GSYNC

  INITIALIZE_METRIC_KIND();

  FORALL_GSYNC(INITIALIZE_INDEXED_METRIC_REAL)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_KINFO_enable
(
 void
)
{
// GPU kernel characteristics metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC KINFO

  INITIALIZE_METRIC_KIND();

  FORALL_KINFO(INITIALIZE_SCALAR_METRIC_INT)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_GICOPY_enable
(
 void
)
{
// GPU implicit copy metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GICOPY

  INITIALIZE_METRIC_KIND();

  FORALL_GICOPY(INITIALIZE_SCALAR_METRIC_REAL)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_GLMEM_enable
(
 void
)
{
// GPU local memory access metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GLMEM

  INITIALIZE_METRIC_KIND();

  FORALL_GLMEM(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_GGMEM_enable
(
 void
)
{
// GPU global memory access metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GGMEM

  INITIALIZE_METRIC_KIND();

  FORALL_GGMEM(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_GPU_INST_enable
(
 void
)
{
#undef CURRENT_METRIC 
#define CURRENT_METRIC GPU_INST

  INITIALIZE_METRIC_KIND();

  FORALL_GPU_INST(INITIALIZE_SCALAR_METRIC_INT)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_GSAMP_enable
(
 void
)
{
#undef CURRENT_METRIC 
#define CURRENT_METRIC GSAMP

  INITIALIZE_METRIC_KIND();

  FORALL_GSAMP_INT(INITIALIZE_SCALAR_METRIC_INT);

  FORALL_GSAMP_REAL(INITIALIZE_SCALAR_METRIC_REAL);

  FINALIZE_METRIC_KIND();

  hpcrun_set_percent(METRIC_ID(GPU_SAMPLE_UTILIZATION), 1);

  metric_desc_t* util_metric = 
    hpcrun_id2metric_linked(METRIC_ID(GPU_SAMPLE_UTILIZATION));

  char *util_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);

  sprintf(util_formula, "$%d/$%d", METRIC_ID(GPU_SAMPLE_TOTAL), 
	  METRIC_ID(GPU_SAMPLE_EXPECTED));

  util_metric->formula = util_formula;
}


void
gpu_metrics_GBR_enable
(
 void
)
{
// GPU branch instruction metrics
#undef CURRENT_METRIC 
#define CURRENT_METRIC GBR

  INITIALIZE_METRIC_KIND();

  FORALL_GBR(INITIALIZE_SCALAR_METRIC_INT)

  FINALIZE_METRIC_KIND();
}


void
gpu_metrics_GPU_INST_STALL_enable
(
 void
)
{
#undef CURRENT_METRIC 
#define CURRENT_METRIC GPU_INST_STALL

  INITIALIZE_METRIC_KIND();

  FORALL_GPU_INST_STALL(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();
}
