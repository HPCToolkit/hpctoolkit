// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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

//***************************************************************************
//
// File:
// $HeadURL$
//
// Purpose:
// GA sampling source.
//
// Note: Using the GA profiling interface would be nice.  However, it
// would require overriding weak symbols, which is not generally
// possible.
//
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************

#define _GNU_SOURCE

#include "ga-overrides.h"
#include <sys/types.h>
#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>
#include <limits.h>


//***************************************************************************
// local includes
//***************************************************************************

#include "../main.h"
#include "../metrics.h"
#include "../safe-sampling.h"
#include "../sample_event.h"
#include "../thread_data.h"
#include "../trace.h"

#include "../messages/messages.h"
#include "ga.h"
#include "ga-overrides.h"

#include "../../common/support-lean/timer.h"


//***************************************************************************
// type definitions
//***************************************************************************

#define gam_CountElems(ndim, lo, hi, pelems){                        \
  int _d;                                                            \
  for(_d=0,*pelems=1; _d< ndim;_d++)  *pelems *= hi[_d]-lo[_d]+1;    \
}

// tallent
#define G_A_NULL (INT_MAX - GA_OFFSET)


//***************************************************************************
// macros
//***************************************************************************

#define def_isSampled_blocking()                                        \
  def_isSampled();                                                      \
  def_timeBeg(isSampled);


#define def_isSampled_nonblocking()                                     \
  def_isSampled();                                                      \
  def_timeBeg(isSampled);


#define def_isSampled()                                                 \
  bool isSampled = false;                                               \
  thread_data_t* threadData = hpcrun_get_thread_data();                 \
  {                                                                     \
    lushPthr_t* xxx = &threadData->pthr_metrics;                        \
    xxx->doIdlenessCnt++;                                               \
    if (xxx->doIdlenessCnt == hpcrun_ga_period) {                       \
      xxx->doIdlenessCnt = 0;                                           \
      isSampled = true;                                                 \
    }                                                                   \
  }


#define def_timeBeg(isSampled)                                          \
  uint64_t timeBeg = 0;                                                 \
  if (isSampled) {                                                      \
    timeBeg = time_getTSC(); /* cycles */                               \
  }


//***************************************************************************

#define doSample_1sided_blocking(GA, g_a, lo, hi)                       \
  if (isSampled) {                                                      \
    double latency = timeElapsed(timeBeg);                              \
    uint64_t nbytes = bytesXfr(GA, g_a, lo, hi);                        \
                                                                        \
    doSample(GA, g_a,                                                   \
             doMetric(hpcrun_ga_metricId_onesidedOp, 1, i),             \
             doMetric(hpcrun_ga_metricId_latency, latency, r),          \
             doMetric(hpcrun_ga_metricId_bytesXfr, nbytes, i),          \
             doMetric(dataMetricId, nbytes, i));                        \
  }


#define doSample_1sided_nonblocking(GA, g_a, lo, hi)                    \
  if (isSampled) {                                                      \
    double latency = timeElapsed(timeBeg);                              \
    uint64_t nbytes = bytesXfr(GA, g_a, lo, hi);                        \
                                                                        \
    /* record time, cctNode, metricVec in nbhandle */                   \
    /* complete in: wait or sync */                                     \
                                                                        \
    doSample(GA, g_a,                                                   \
             doMetric(hpcrun_ga_metricId_onesidedOp, 1, i),             \
             doMetric(hpcrun_ga_metricId_latency, latency, r),          \
             doMetric(hpcrun_ga_metricId_bytesXfr, nbytes, i),          \
             doMetric(dataMetricId, nbytes, i));                        \
  }


#define doSample_collective_blocking(GA)                                \
  if (isSampled) {                                                      \
    double latency = timeElapsed(timeBeg);                              \
                                                                        \
    doSample(GA, G_A_NULL,                                              \
             doMetric(hpcrun_ga_metricId_collectiveOp, 1, i),           \
             doMetric(hpcrun_ga_metricId_latency, latency, r),          \
             do0(),                                                     \
             do0());                                                    \
  }


#define doSample(GA, g_a, do1, do2, do3, do4)                           \
{                                                                       \
  bool safe = false;                                                    \
  if ((safe = hpcrun_safe_enter())) {                                   \
    ucontext_t uc;                                                      \
    getcontext(&uc);                                                    \
                                                                        \
    hpcrun_ga_metricId_dataDesc_t* ga_desc = NULL;                      \
    unsigned int dataMetricId = HPCTRACE_FMT_MetricId_NULL;                     \
                                                                        \
    if (g_a != G_A_NULL) {                                              \
      int idx = ga_getDataIdx(GA, g_a);                                 \
      if (hpcrun_ga_dataIdx_isValid(idx)) {                             \
        ga_desc = hpcrun_ga_metricId_dataTbl_find(idx);                 \
        dataMetricId = ga_desc->metricId;                               \
      }                                                                 \
    }                                                                   \
                                                                        \
    /* N.B.: when tracing, this call generates a trace record */        \
    /* TODO: should return a 'metric_set_t*' */                         \
    sample_val_t smplVal =                                              \
      hpcrun_sample_callpath(&uc, dataMetricId,                         \
               (hpcrun_metricVal_t) {.i=0},     \
                             0/*skipInner*/, 1/*isSync*/, NULL);        \
                                                                        \
    /* namespace: g_a, ga_desc, dataMetricId, smplVal, metricVec */     \
    metric_data_list_t* metricVec =                                     \
      metricVec = hpcrun_get_metric_data_list(smplVal.sample_node);     \
    do1;                                                                \
    do2;                                                                \
    do3;                                                                \
    do4;                                                                \
                                                                        \
    if (safe) hpcrun_safe_exit();                                       \
  }                                                                     \
}


#define doMetric(metricIdExpr, metricIncr, type)                        \
{                                                                       \
  int mId = (metricIdExpr); /* eval only once */                        \
  if (mId >= 0 && mId != HPCTRACE_FMT_MetricId_NULL) {                  \
    /*TMSG(GA, "doMetric: %d", nbytes); */                              \
    hpcrun_metric_std_inc(mId, metricVec,                               \
           (cct_metric_data_t){.type = metricIncr * hpcrun_ga_period}); \
  }                                                                     \
}


#define do0() {}


//***************************************************************************

// timeElapsed: returns time in us but with ns resolution
static inline double
timeElapsed(uint64_t timeBeg)
{
  const double mhz = 2100; // FIXME;
  uint64_t timeEnd = time_getTSC();
  return ((double)(timeEnd - timeBeg) / mhz);
}


static inline unsigned int
bytesXfr(global_array_t* GA, Integer g_a, Integer* lo, Integer* hi)
{
  // TODO: can this information be communicated from the runtime
  // rather than being (re?)computed here?
  Integer ga_hndl = GA_OFFSET + g_a;
  unsigned int ga_ndim = GA[ga_hndl].ndim;
  unsigned int ga_elemsize = GA[ga_hndl].elemsize;
  Integer num_elems = 0;
  gam_CountElems(ga_ndim, lo, hi, &num_elems);
  unsigned int num_bytes = num_elems * ga_elemsize;
  return num_bytes;
}


static inline int
ga_getDataIdx(global_array_t* GA, Integer g_a)
{
  // FIXME: use a profiling slot rather than 'lock'
  Integer ga_hndl = GA_OFFSET + g_a;
  return (int) GA[ga_hndl].lock;
}


static inline void
ga_setDataIdx(global_array_t* GA, Integer g_a, int idx)
{
  Integer ga_hndl = GA_OFFSET + g_a;
  GA[ga_hndl].lock = idx;
}


//***************************************************************************
// bookkeeping
//***************************************************************************

// There are two basic interface:
//   1. pnga_create()
//   2. pnga_create_handle()
//      pnga_set_array_name() [optional]
//      pnga_set_*
//      pnga_allocate()

logical
foilbase_pnga_create(ga_create_fn_t* real_pnga_create, global_array_t* GA,
    Integer type, Integer ndim, Integer *dims, char* name,
    Integer *chunk, Integer *g_a)
{
  // collective
  logical ret = real_pnga_create(type, ndim, dims, name, chunk, g_a);

  int idx = -1;
#if (GA_DataCentric_Prototype)
  idx = hpcrun_ga_dataIdx_new(name);
#endif

  ga_setDataIdx(GA, *g_a, idx);

  return ret;
}

Integer
foilbase_pnga_create_handle(ga_create_handle_fn_t* real_pnga_create_handle, global_array_t* GA)
{
  // collective
  Integer g_a = real_pnga_create_handle();

  int idx = -1;
#if (GA_DataCentric_Prototype)
  char* name = "(unknown)";
  idx = hpcrun_ga_dataIdx_new(name);
#endif

  ga_setDataIdx(GA, g_a, idx);

  return g_a;
}

// TODO: void pnga_set_array_name(Integer g_a, char *array_name);


// ga_destroy

// ga_pgroup_create, ga_pgroup_destroy, ga_pgroup_get_default, ga_pgroup_set_default, ga_nodeid, ga_cluster_proc_nodeid
// ga_release / ga_release_update
// ga_error

// ga_read_inc


//***************************************************************************
// blocking get, put, acc, ...
//***************************************************************************

void
foilbase_pnga_get(ga_getput_fn_t* real_pnga_get, global_array_t* GA, Integer g_a, Integer* lo,
                  Integer* hi, void* buf, Integer* ld)
{
  def_isSampled_blocking();

  real_pnga_get(g_a, lo, hi, buf, ld);

  doSample_1sided_blocking(GA, g_a, lo, hi);
}


void
foilbase_pnga_put(ga_getput_fn_t* real_pnga_put, global_array_t* GA, Integer g_a, Integer* lo,
                  Integer* hi, void* buf, Integer* ld)
{
  def_isSampled_blocking();

  real_pnga_put(g_a, lo, hi, buf, ld);

  doSample_1sided_blocking(GA, g_a, lo, hi);
}


void
foilbase_pnga_acc(ga_acc_fn_t* real_pnga_acc, global_array_t* GA, Integer g_a, Integer *lo,
                  Integer *hi, void *buf, Integer *ld, void *alpha)
{
  def_isSampled_blocking();

  real_pnga_acc(g_a, lo, hi, buf, ld, alpha);

  doSample_1sided_blocking(GA, g_a, lo, hi);
}


// ngai_get_common: (pnga_get, pnga_nbget, pnga_get_field, pnga_nbget_field)


//***************************************************************************
// non-blocking get, put, acc, ...
//***************************************************************************

void
foilbase_pnga_nbget(ga_nbgetput_fn_t* real_pnga_nbget, global_array_t* GA, Integer g_a, Integer *lo,
                    Integer *hi, void *buf, Integer *ld, Integer *nbhandle)
{
  def_isSampled_nonblocking();

  real_pnga_nbget(g_a, lo, hi, buf, ld, nbhandle);

  doSample_1sided_nonblocking(GA, g_a, lo, hi);
}

void
foilbase_pnga_nbput(ga_nbgetput_fn_t* real_pnga_nbput, global_array_t* GA, Integer g_a, Integer *lo,
                    Integer *hi, void *buf, Integer *ld, Integer *nbhandle)
{
  def_isSampled_nonblocking();

  real_pnga_nbput(g_a, lo, hi, buf, ld, nbhandle);

  doSample_1sided_nonblocking(GA, g_a, lo, hi);
}


void
foilbase_pnga_nbacc(ga_nbacc_fn_t* real_pnga_nbacc, global_array_t* GA, Integer g_a, Integer *lo,
                    Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle)
{
  def_isSampled_nonblocking();

  real_pnga_nbacc(g_a, lo, hi, buf, ld, alpha, nbhandle);

  doSample_1sided_nonblocking(GA, g_a, lo, hi);
}


void
foilbase_pnga_nbwait(ga_nbwait_fn_t* real_pnga_nbwait, global_array_t* GA, Integer *nbhandle)
{
  // FIXME: measure only if tagged
  def_isSampled();
  def_timeBeg(isSampled);

  real_pnga_nbwait(nbhandle);

  if (isSampled) {
    double latency = timeElapsed(timeBeg);
    doSample(GA, G_A_NULL,
             doMetric(hpcrun_ga_metricId_latency, latency, r),
             do0(), do0(), do0());
  }
}


//***************************************************************************
// collectives: brdcst
//***************************************************************************

void
foilbase_pnga_brdcst(ga_brdcst_fn_t* real_pnga_brdcst, global_array_t* GA, Integer type, void *buf,
                     Integer len, Integer originator)
{
  def_isSampled_blocking();

  real_pnga_brdcst(type, buf, len, originator);

  doSample_collective_blocking(GA);
}


void
foilbase_pnga_gop(ga_gop_fn_t* real_pnga_gop, global_array_t* GA, Integer type, void *x, Integer n,
                  char *op)
{
  def_isSampled_blocking();

  real_pnga_gop(type, x, n, op);

  doSample_collective_blocking(GA);
}


void
foilbase_pnga_sync(ga_sync_fn_t* real_pnga_sync, global_array_t* GA)
{
  def_isSampled_blocking();

  real_pnga_sync();

  doSample_collective_blocking(GA);
}


// TODO: ga_pgroup_sync


// TODO: ga_pgroup_dgop

//***************************************************************************
