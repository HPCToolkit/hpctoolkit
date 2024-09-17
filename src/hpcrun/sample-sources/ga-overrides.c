// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
#include "../foil/ga.h"

#include "../../common/lean/timer.h"


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
hpcrun_pnga_create(
    Integer type, Integer ndim, Integer *dims, char* name,
    Integer *chunk, Integer *g_a, const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  // collective
  logical ret = f_pnga_create(type, ndim, dims, name, chunk, g_a, dispatch);

  int idx = -1;
#if (GA_DataCentric_Prototype)
  idx = hpcrun_ga_dataIdx_new(name);
#endif

  ga_setDataIdx(f_GA(dispatch), *g_a, idx);

  return ret;
}

Integer
hpcrun_pnga_create_handle(const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  // collective
  Integer g_a = f_pnga_create_handle(dispatch);

  int idx = -1;
#if (GA_DataCentric_Prototype)
  char* name = "(unknown)";
  idx = hpcrun_ga_dataIdx_new(name);
#endif

  ga_setDataIdx(f_GA(dispatch), g_a, idx);

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
hpcrun_pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_blocking();

  f_pnga_get(g_a, lo, hi, buf, ld, dispatch);

  doSample_1sided_blocking(f_GA(dispatch), g_a, lo, hi);
}


void
hpcrun_pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_blocking();

  f_pnga_put(g_a, lo, hi, buf, ld, dispatch);

  doSample_1sided_blocking(f_GA(dispatch), g_a, lo, hi);
}


void
hpcrun_pnga_acc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                void *alpha, const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_blocking();

  f_pnga_acc(g_a, lo, hi, buf, ld, alpha, dispatch);

  doSample_1sided_blocking(f_GA(dispatch), g_a, lo, hi);
}


// ngai_get_common: (pnga_get, pnga_nbget, pnga_get_field, pnga_nbget_field)


//***************************************************************************
// non-blocking get, put, acc, ...
//***************************************************************************

void
hpcrun_pnga_nbget(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                  Integer *nbhandle, const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_nonblocking();

  f_pnga_nbget(g_a, lo, hi, buf, ld, nbhandle, dispatch);

  doSample_1sided_nonblocking(f_GA(dispatch), g_a, lo, hi);
}

void
hpcrun_pnga_nbput(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                  Integer *nbhandle, const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_nonblocking();

  f_pnga_nbput(g_a, lo, hi, buf, ld, nbhandle, dispatch);

  doSample_1sided_nonblocking(f_GA(dispatch), g_a, lo, hi);
}


void
hpcrun_pnga_nbacc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                  void *alpha, Integer *nbhandle,
                  const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_nonblocking();

  f_pnga_nbacc(g_a, lo, hi, buf, ld, alpha, nbhandle, dispatch);

  doSample_1sided_nonblocking(f_GA(dispatch), g_a, lo, hi);
}


void
hpcrun_pnga_nbwait(Integer *nbhandle, const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  // FIXME: measure only if tagged
  def_isSampled();
  def_timeBeg(isSampled);

  f_pnga_nbwait(nbhandle, dispatch);

  if (isSampled) {
    double latency = timeElapsed(timeBeg);
    doSample(f_GA(dispatch), G_A_NULL,
             doMetric(hpcrun_ga_metricId_latency, latency, r),
             do0(), do0(), do0());
  }
}


//***************************************************************************
// collectives: brdcst
//***************************************************************************

void
hpcrun_pnga_brdcst(Integer type, void *buf, Integer len, Integer originator,
                   const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_blocking();

  f_pnga_brdcst(type, buf, len, originator, dispatch);

  doSample_collective_blocking(f_GA(dispatch));
}


void
hpcrun_pnga_gop(Integer type, void *x, Integer n, char *op,
                const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_blocking();

  f_pnga_gop(type, x, n, op, dispatch);

  doSample_collective_blocking(f_GA(dispatch));
}


void
hpcrun_pnga_sync(const struct hpcrun_foil_appdispatch_ga* dispatch)
{
  def_isSampled_blocking();

  f_pnga_sync(dispatch);

  doSample_collective_blocking(f_GA(dispatch));
}


// TODO: ga_pgroup_sync


// TODO: ga_pgroup_dgop

//***************************************************************************
