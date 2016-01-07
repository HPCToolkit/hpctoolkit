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
// Copyright ((c)) 2002-2016, Rice University
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

#include <sys/types.h>
#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>
#include <limits.h>


//***************************************************************************
// local includes
//***************************************************************************

#include <main.h>
#include <metrics.h>
#include <safe-sampling.h>
#include <sample_event.h>
#include <thread_data.h>
#include <trace.h>

#include <messages/messages.h>
#include <monitor-exts/monitor_ext.h>
#include <sample-sources/ga.h>

#include <lib/support-lean/timer.h>


//***************************************************************************
// type definitions
//***************************************************************************

// FIXME: temporarily import GA 5.2 declarations.  Unfortunately, some
// of these declarations are only in the source tree (as opposed to
// the installation), so currently there is no clean solution.

// Need: field for a profiler

// ${GA-install}/include/typesf2c.h
// ${GA-build}/gaf2c/typesf2c.h
// ${GA-src}/gaf2c/typesf2c.h.in [F2C_INTEGER_C_TYPE]
typedef int Integer; // integer size
typedef Integer logical;

// ${GA-install}/include/armci.h
// ${GA-src}/armci/src/include/armci.h
typedef long armci_size_t;

// ${GA-install}/include/gacommon.h
// ${GA-src}/global/src/gacommon.h
#define GA_MAX_DIM 7

// ${GA-src}/global/src/gaconfig.h
#define MAXDIM  GA_MAX_DIM

// ${GA-src}/global/src/globalp.h
#define GA_OFFSET   1000

// ${GA-src}/global/src/base.h
#define FNAM        31              /* length of array names   */

typedef Integer C_Integer;
typedef armci_size_t C_Long;

typedef struct {
       short int  ndim;             /* number of dimensions                 */
       short int  irreg;            /* 0-regular; 1-irregular distribution  */
       int  type;                   /* data type in array                   */
       int  actv;                   /* activity status, GA is allocated     */
       int  actv_handle;            /* handle is created                    */
       C_Long   size;               /* size of local data in bytes          */
       int  elemsize;               /* sizeof(datatype)                     */
       int  ghosts;                 /* flag indicating presence of ghosts   */
       long lock;                   /* lock                                 */
       long id;                     /* ID of shmem region / MA handle       */
       C_Integer  dims[MAXDIM];     /* global array dimensions              */
       C_Integer  chunk[MAXDIM];    /* chunking                             */
       int  nblock[MAXDIM];         /* number of blocks per dimension       */
       C_Integer  width[MAXDIM];    /* boundary cells per dimension         */
       C_Integer  first[MAXDIM];    /* (Mirrored only) first local element  */
       C_Integer  last[MAXDIM];     /* (Mirrored only) last local element   */
       C_Long  shm_length;          /* (Mirrored only) local shmem length   */
       C_Integer  lo[MAXDIM];       /* top/left corner in local patch       */
       double scale[MAXDIM];        /* nblock/dim (precomputed)             */
       char **ptr;                  /* arrays of pointers to remote data    */
       C_Integer  *mapc;            /* block distribution map               */
       char name[FNAM+1];           /* array name                           */
       int p_handle;                /* pointer to processor list for array  */
       double *cache;               /* store for frequently accessed ptrs   */
       int corner_flag;             /* flag for updating corner ghost cells */
       int block_flag;              /* flag to indicate block-cyclic data   */
       int block_sl_flag;           /* flag to indicate block-cyclic data   */
                                    /* using ScaLAPACK format               */
       C_Integer block_dims[MAXDIM];/* array of block dimensions            */
       C_Integer num_blocks[MAXDIM];/* number of blocks in each dimension   */
       C_Integer block_total;       /* total number of blocks in array      */
                                    /* using restricted arrays              */
       C_Integer *rstrctd_list;     /* list of processors with data         */
       C_Integer num_rstrctd;       /* number of processors with data       */
       C_Integer has_data;          /* flag that processor has data         */
       C_Integer rstrctd_id;        /* rank of processor in restricted list */
       C_Integer *rank_rstrctd;     /* ranks of processors with data        */

#ifdef ENABLE_CHECKPOINT
       int record_id;               /* record id for writing ga to disk     */
#endif
} global_array_t;

extern global_array_t *GA;

#define gam_CountElems(ndim, lo, hi, pelems){                        \
  int _d;                                                            \
  for(_d=0,*pelems=1; _d< ndim;_d++)  *pelems *= hi[_d]-lo[_d]+1;    \
}

// tallent
#define G_A_NULL (INT_MAX - GA_OFFSET)


//***************************************************************************
// macros
//***************************************************************************

#define def_isSampled_blocking()					\
  def_isSampled();							\
  def_timeBeg(isSampled);


#define def_isSampled_nonblocking()					\
  def_isSampled();							\
  def_timeBeg(isSampled);


#define def_isSampled()							\
  bool isSampled = false;						\
  thread_data_t* threadData = hpcrun_get_thread_data();			\
  {									\
    lushPthr_t* xxx = &threadData->pthr_metrics;			\
    xxx->doIdlenessCnt++;						\
    if (xxx->doIdlenessCnt == hpcrun_ga_period) {			\
      xxx->doIdlenessCnt = 0;						\
      isSampled = true;							\
    }									\
  }


#define def_timeBeg(isSampled)						\
  uint64_t timeBeg = 0;      						\
  if (isSampled) {							\
    timeBeg = time_getTSC(); /* cycles */				\
  }


//***************************************************************************

#define doSample_1sided_blocking(g_a, lo, hi)				\
  if (isSampled) {							\
    double latency = timeElapsed(timeBeg);				\
    uint64_t nbytes = bytesXfr(g_a, lo, hi);				\
									\
    doSample(g_a,							\
	     doMetric(hpcrun_ga_metricId_onesidedOp, 1, i),		\
	     doMetric(hpcrun_ga_metricId_latency, latency, r),		\
	     doMetric(hpcrun_ga_metricId_bytesXfr, nbytes, i),		\
	     doMetric(dataMetricId, nbytes, i));			\
  }


#define doSample_1sided_nonblocking(g_a, lo, hi)			\
  if (isSampled) {							\
    double latency = timeElapsed(timeBeg);				\
    uint64_t nbytes = bytesXfr(g_a, lo, hi);				\
									\
    /* record time, cctNode, metricVec in nbhandle */			\
    /* complete in: wait or sync */					\
									\
    doSample(g_a,							\
	     doMetric(hpcrun_ga_metricId_onesidedOp, 1, i),		\
	     doMetric(hpcrun_ga_metricId_latency, latency, r),		\
	     doMetric(hpcrun_ga_metricId_bytesXfr, nbytes, i),		\
	     doMetric(dataMetricId, nbytes, i));			\
  }


#define doSample_collective_blocking()					\
  if (isSampled) {							\
    double latency = timeElapsed(timeBeg);				\
									\
    doSample(G_A_NULL,							\
	     doMetric(hpcrun_ga_metricId_collectiveOp, 1, i),		\
	     doMetric(hpcrun_ga_metricId_latency, latency, r),		\
	     do0(),							\
	     do0());							\
  }


#define doSample(g_a, do1, do2, do3, do4)				\
{									\
  bool safe = false;                                                    \
  if ((safe = hpcrun_safe_enter())) {					\
    ucontext_t uc;							\
    getcontext(&uc);							\
									\
    hpcrun_ga_metricId_dataDesc_t* ga_desc = NULL;			\
    uint dataMetricId = HPCRUN_FMT_MetricId_NULL;			\
									\
    if (g_a != G_A_NULL) {						\
      int idx = ga_getDataIdx(g_a);					\
      if (hpcrun_ga_dataIdx_isValid(idx)) {				\
	ga_desc = hpcrun_ga_metricId_dataTbl_find(idx);			\
	dataMetricId = ga_desc->metricId;				\
      }									\
    }									\
      									\
    /* N.B.: when tracing, this call generates a trace record */	\
    /* TODO: should return a 'metric_set_t*' */				\
    sample_val_t smplVal =						\
      hpcrun_sample_callpath(&uc, dataMetricId, 0/*metricIncr*/,	\
			     0/*skipInner*/, 1/*isSync*/);		\
    									\
    /* namespace: g_a, ga_desc, dataMetricId, smplVal, metricVec */	\
    metric_set_t* metricVec =						\
      metricVec = hpcrun_get_metric_set(smplVal.sample_node);		\
    do1;								\
    do2;								\
    do3;								\
    do4;								\
    									\
    if (safe) hpcrun_safe_exit();					\
  }									\
}


#define doMetric(metricIdExpr, metricIncr, type)			\
{									\
  int mId = (metricIdExpr); /* eval only once */			\
  if (mId >= 0 && mId != HPCRUN_FMT_MetricId_NULL) {			\
    /*TMSG(GA, "doMetric: %d", nbytes); */				\
    hpcrun_metric_std_inc(mId, metricVec,				\
	   (cct_metric_data_t){.type = metricIncr * hpcrun_ga_period}); \
  }									\
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


static inline uint
bytesXfr(Integer g_a, Integer* lo, Integer* hi)
{
  // TODO: can this information be communicated from the runtime
  // rather than being (re?)computed here?
  Integer ga_hndl = GA_OFFSET + g_a;
  uint ga_ndim = GA[ga_hndl].ndim;
  uint ga_elemsize = GA[ga_hndl].elemsize;
  Integer num_elems = 0;
  gam_CountElems(ga_ndim, lo, hi, &num_elems);
  uint num_bytes = num_elems * ga_elemsize;
  return num_bytes;
}


static inline int
ga_getDataIdx(Integer g_a)
{
  // FIXME: use a profiling slot rather than 'lock'
  Integer ga_hndl = GA_OFFSET + g_a;
  return (int) GA[ga_hndl].lock;
}


static inline void
ga_setDataIdx(Integer g_a, int idx)
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

typedef logical ga_create_fn_t(Integer type, Integer ndim,
			       Integer *dims, char* name,
			       Integer *chunk, Integer *g_a);

MONITOR_EXT_DECLARE_REAL_FN(ga_create_fn_t, real_pnga_create);

logical
MONITOR_EXT_WRAP_NAME(pnga_create)(Integer type, Integer ndim,
				   Integer *dims, char* name,
				   Integer *chunk, Integer *g_a)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_create, pnga_create);

  // collective
  logical ret = real_pnga_create(type, ndim, dims, name, chunk, g_a);

  int idx = -1;
#if (GA_DataCentric_Prototype)
  idx = hpcrun_ga_dataIdx_new(name);
#endif

  ga_setDataIdx(*g_a, idx);
  
  return ret;
}


typedef Integer ga_create_handle_fn_t();

MONITOR_EXT_DECLARE_REAL_FN(ga_create_handle_fn_t, real_pnga_create_handle);

Integer
MONITOR_EXT_WRAP_NAME(pnga_create_handle)()
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_create_handle, pnga_create_handle);

  // collective
  Integer g_a = real_pnga_create_handle();

  int idx = -1;
#if (GA_DataCentric_Prototype)
  char* name = "(unknown)";
  idx = hpcrun_ga_dataIdx_new(name);
#endif

  ga_setDataIdx(g_a, idx);
  
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

typedef void ga_getput_fn_t(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld);

typedef void ga_acc_fn_t(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha);


MONITOR_EXT_DECLARE_REAL_FN(ga_getput_fn_t, real_pnga_get);

void
MONITOR_EXT_WRAP_NAME(pnga_get)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_get, pnga_get);
  
  def_isSampled_blocking();

  real_pnga_get(g_a, lo, hi, buf, ld);

  doSample_1sided_blocking(g_a, lo, hi);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_getput_fn_t, real_pnga_put);

void
MONITOR_EXT_WRAP_NAME(pnga_put)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_put, pnga_put);

  def_isSampled_blocking();

  real_pnga_put(g_a, lo, hi, buf, ld);

  doSample_1sided_blocking(g_a, lo, hi);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_acc_fn_t, real_pnga_acc);

void
MONITOR_EXT_WRAP_NAME(pnga_acc)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_acc, pnga_acc);

  def_isSampled_blocking();

  real_pnga_acc(g_a, lo, hi, buf, ld, alpha);

  doSample_1sided_blocking(g_a, lo, hi);
}


// ngai_get_common: (pnga_get, pnga_nbget, pnga_get_field, pnga_nbget_field)


//***************************************************************************
// non-blocking get, put, acc, ...
//***************************************************************************

typedef void ga_nbgetput_fn_t(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle);

typedef void ga_nbacc_fn_t(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle);

typedef void ga_nbwait_fn_t(Integer *nbhandle);


MONITOR_EXT_DECLARE_REAL_FN(ga_nbgetput_fn_t, real_pnga_nbget);

void
MONITOR_EXT_WRAP_NAME(pnga_nbget)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbget, pnga_nbget);

  def_isSampled_nonblocking();

  real_pnga_nbget(g_a, lo, hi, buf, ld, nbhandle);

  doSample_1sided_nonblocking(g_a, lo, hi);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_nbgetput_fn_t, real_pnga_nbput);

void
MONITOR_EXT_WRAP_NAME(pnga_nbput)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbput, pnga_nbput);

  def_isSampled_nonblocking();

  real_pnga_nbput(g_a, lo, hi, buf, ld, nbhandle);

  doSample_1sided_nonblocking(g_a, lo, hi);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_nbacc_fn_t, real_pnga_nbacc);

void
MONITOR_EXT_WRAP_NAME(pnga_nbacc)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbacc, pnga_nbacc);

  def_isSampled_nonblocking();

  real_pnga_nbacc(g_a, lo, hi, buf, ld, alpha, nbhandle);

  doSample_1sided_nonblocking(g_a, lo, hi);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_nbwait_fn_t, real_pnga_nbwait);

void
MONITOR_EXT_WRAP_NAME(pnga_nbwait)(Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbwait, pnga_nbwait);

  // FIXME: measure only if tagged
  def_isSampled();
  def_timeBeg(isSampled);

  real_pnga_nbwait(nbhandle);

  if (isSampled) {
    double latency = timeElapsed(timeBeg);
    doSample(G_A_NULL, 
	     doMetric(hpcrun_ga_metricId_latency, latency, r),
	     do0(), do0(), do0());
  }
}


//***************************************************************************
// collectives: brdcst
//***************************************************************************

typedef void ga_brdcst_fn_t(Integer type, void *buf, Integer len, Integer originator);

MONITOR_EXT_DECLARE_REAL_FN(ga_brdcst_fn_t, real_pnga_brdcst);

void
MONITOR_EXT_WRAP_NAME(pnga_brdcst)(Integer type, void *buf, Integer len, Integer originator)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_brdcst, pnga_brdcst);

  def_isSampled_blocking();
  
  real_pnga_brdcst(type, buf, len, originator);

  doSample_collective_blocking();
}


typedef void ga_gop_fn_t(Integer type, void *x, Integer n, char *op);

MONITOR_EXT_DECLARE_REAL_FN(ga_gop_fn_t, real_pnga_gop);

void
MONITOR_EXT_WRAP_NAME(pnga_gop)(Integer type, void *x, Integer n, char *op)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_gop, pnga_gop);

  def_isSampled_blocking();

  real_pnga_gop(type, x, n, op);

  doSample_collective_blocking();
}


typedef void ga_sync_fn_t();

MONITOR_EXT_DECLARE_REAL_FN(ga_sync_fn_t, real_pnga_sync);

void
MONITOR_EXT_WRAP_NAME(pnga_sync)()
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_sync, pnga_sync);

  def_isSampled_blocking();

  real_pnga_sync();

  doSample_collective_blocking();
}


// TODO: ga_pgroup_sync
typedef void ga_pgroup_sync_fn_t(Integer grp_id);


// TODO: ga_pgroup_dgop
typedef void ga_pgroup_gop_fn_t(Integer p_grp, Integer type, void *x, Integer n, char *op);


//***************************************************************************
