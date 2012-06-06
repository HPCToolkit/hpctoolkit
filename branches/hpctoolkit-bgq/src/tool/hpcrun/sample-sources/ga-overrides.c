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
// Copyright ((c)) 2002-2012, Rice University
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


//***************************************************************************
// type definitions
//***************************************************************************

// FIXME: temporarily import GA declarations.  Unfortunately, some of
// these declarations are only in source-tree (as opposed to
// installed) header files, so currently there is no clean solution.

//   armci/gaf2c/typesf2c.h:F2C_INTEGER_C_TYPE
typedef long Integer; // word size
typedef Integer logical;

// global/src/globalp.h
#define GA_OFFSET   1000

// global/src/gacommon.h
#define GA_MAX_DIM 7

// global/src/gaconfig.h
#define MAXDIM  GA_MAX_DIM

// armci/src/include/armci.h
typedef long armci_size_t;

// global/src/base.h: global_array_t and GA
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

  // ENABLE_CHECKPOINT !!!
  //int record_id;               /* record id for writing ga to disk     */

} global_array_t;

extern global_array_t *GA;

#define gam_CountElems(ndim, lo, hi, pelems){                        \
  int _d;                                                            \
  for(_d=0,*pelems=1; _d< ndim;_d++)  *pelems *= hi[_d]-lo[_d]+1;    \
}


//***************************************************************************
// macros
//***************************************************************************

#define GA_SYNC_SMPL_PERIOD 293 /* if !0, sample synchronously */

// FIXME: hpcrun_sample_callpath() should also return a metric_set_t*!

#define ifSample(smplVal, metricVec, do1, do2)				\
{									\
  thread_data_t* td = hpcrun_get_thread_data();				\
  lushPthr_t* cnt = &td->pthr_metrics;					\
  cnt->doIdlenessCnt++;							\
  if (cnt->doIdlenessCnt == GA_SYNC_SMPL_PERIOD) {			\
    cnt->doIdlenessCnt = 0;						\
    if (hpcrun_safe_enter()) {						\
      ucontext_t uc;							\
      getcontext(&uc);							\
      /* N.B.: when tracing, this call generates a trace record */	\
      /*  traceMetricId, traceMetricIncr */				\
      sample_val_t smplVal =						\
	hpcrun_sample_callpath(&uc, 0/*metricId*/, 0/*mIncr*/,		\
			       0/*skipInner*/, 1/*isSync*/);		\
      metric_set_t* metricVec =						\
	metricVec = hpcrun_get_metric_set(smplVal.sample_node);		\
      do1; /* may use smplVal and metricVec */				\
      do2; /* may use smplVal and metricVec */				\
									\
      hpcrun_safe_exit();						\
    }									\
  }									\
}


#define collectMetric(metricVec, metricId, metricIncr) 			\
{									\
  if (metricId >= 0) {							\
    uint64_t mIncr = GA_SYNC_SMPL_PERIOD * metricIncr;			\
    hpcrun_metric_std_inc(metricId, metricVec,				\
			  (cct_metric_data_t){.i = mIncr});		\
  }									\
}


#define collectBytesXfr(metricVec, metricIdBytes, g_a, lo, hi)		\
{									\
  Integer ga_hndl = GA_OFFSET + g_a;					\
  int ga_ndim = GA[ga_hndl].ndim;					\
  /* char* ga_name = GA[ga_hndl].name; */				\
  int ga_elemsize = GA[ga_hndl].elemsize;				\
  Integer num_elems = 0;						\
  gam_CountElems(ga_ndim, lo, hi, &num_elems);				\
  int num_bytes = num_elems * ga_elemsize * GA_SYNC_SMPL_PERIOD;	\
  /*TMSG(GA, "ga_sampleAfter_bytes: %d", num_bytes); */			\
  if (metricIdBytes >= 0) {						\
    hpcrun_metric_std_inc(metricIdBytes, metricVec,			\
			  (cct_metric_data_t){.i = num_bytes});		\
  }									\
  /* **************************************** */			\
  int idx = ga_getDataIdx(ga_hndl);					\
  if (hpcrun_ga_dataIdx_isValid(idx)) {					\
    metricId_dataDesc_t* desc = hpcrun_ga_metricId_dataTbl_find(idx);	\
    hpcrun_metric_std_inc(desc->metricId, metricVec,			\
			  (cct_metric_data_t){.i = num_bytes});		\
  }									\
}


#define collect0() {}

// trace sample
//if (smpl->trace_node) {
//  hpcrun_trace_append(hpcrun_cct_persistent_id(smpl->trace_node));
//}

static inline int
ga_getDataIdx(Integer ga_hndl)
{
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

// ga_destroy

// ga_pgroup_create, ga_pgroup_destroy, ga_pgroup_get_default, ga_pgroup_set_default, ga_nodeid, ga_cluster_proc_nodeid
// ga_release / ga_release_update
// ga_error

// ga_read_inc

//***************************************************************************
// collectives: brdcst
//***************************************************************************

typedef void ga_brdcst_fn_t(Integer type, void *buf, Integer len, Integer originator);

MONITOR_EXT_DECLARE_REAL_FN(ga_brdcst_fn_t, real_pnga_brdcst);

void
MONITOR_EXT_WRAP_NAME(pnga_brdcst)(Integer type, void *buf, Integer len, Integer originator)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_brdcst, pnga_brdcst);

  int mId_collectiveOp = hpcrun_ga_metricId_collectiveOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_collectiveOp, 1/*metricIncr*/),
	   collectMetric(metricVec, mId_bytes, len/*metricIncr*/));

  real_pnga_brdcst(type, buf, len, originator);
}


typedef void ga_gop_fn_t(Integer type, void *x, Integer n, char *op);

MONITOR_EXT_DECLARE_REAL_FN(ga_gop_fn_t, real_pnga_gop);

void
MONITOR_EXT_WRAP_NAME(pnga_gop)(Integer type, void *x, Integer n, char *op)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_gop, pnga_gop);

  int mId_collectiveOp = hpcrun_ga_metricId_collectiveOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_collectiveOp, 1/*metricIncr*/),
	   collectMetric(metricVec, mId_bytes, 8/*metricIncr*/));
  // FIXME: # bytes depends on type

  real_pnga_gop(type, x, n, op);
}


typedef void ga_sync_fn_t();

MONITOR_EXT_DECLARE_REAL_FN(ga_sync_fn_t, real_pnga_sync);

void
MONITOR_EXT_WRAP_NAME(pnga_sync)()
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_sync, pnga_sync);

  int mId_collectiveOp = hpcrun_ga_metricId_collectiveOp();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_collectiveOp, 1/*metricIncr*/),
	   collect0());

  real_pnga_sync();
}


typedef void ga_zero_fn_t(Integer g_a);

MONITOR_EXT_DECLARE_REAL_FN(ga_zero_fn_t, real_pnga_zero);

void
MONITOR_EXT_WRAP_NAME(pnga_zero)(Integer g_a)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_zero, pnga_zero);

  int mId_collectiveOp = hpcrun_ga_metricId_collectiveOp();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_collectiveOp, 1/*metricIncr*/),
	   collect0());

  real_pnga_zero(g_a);
}


// TODO: ga_pgroup_dgop, ga_pgroup_sync

typedef void ga_pgroup_gop_fn_t(Integer p_grp, Integer type, void *x, Integer n, char *op);
typedef void ga_pgroup_sync_fn_t(Integer grp_id);


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

  int mId_onesided = hpcrun_ga_metricId_onesidedOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_onesided, 1/*metricIncr*/),
	   collectBytesXfr(metricVec, mId_bytes, g_a, lo, hi));

  real_pnga_get(g_a, lo, hi, buf, ld);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_getput_fn_t, real_pnga_put);

void
MONITOR_EXT_WRAP_NAME(pnga_put)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_put, pnga_put);

  int mId_onesided = hpcrun_ga_metricId_onesidedOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr(); 
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_onesided, 1/*metricIncr*/),
	   collectBytesXfr(metricVec, mId_bytes, g_a, lo, hi));

  real_pnga_put(g_a, lo, hi, buf, ld);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_acc_fn_t, real_pnga_acc);

void
MONITOR_EXT_WRAP_NAME(pnga_acc)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_acc, pnga_acc);

  int mId_onesided = hpcrun_ga_metricId_onesidedOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_onesided, 1/*metricIncr*/),
	   collectBytesXfr(metricVec, mId_bytes, g_a, lo, hi));

  real_pnga_acc(g_a, lo, hi, buf, ld, alpha);
}


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

  int mId_onesided = hpcrun_ga_metricId_onesidedOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_onesided, 1/*metricIncr*/),
	   collectBytesXfr(metricVec, mId_bytes, g_a, lo, hi));

  real_pnga_nbget(g_a, lo, hi, buf, ld, nbhandle);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_nbgetput_fn_t, real_pnga_nbput);

void
MONITOR_EXT_WRAP_NAME(pnga_nbput)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbput, pnga_nbput);

  int mId_onesided = hpcrun_ga_metricId_onesidedOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_onesided, 1/*metricIncr*/),
	   collectBytesXfr(metricVec, mId_bytes, g_a, lo, hi));

  real_pnga_nbput(g_a, lo, hi, buf, ld, nbhandle);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_nbacc_fn_t, real_pnga_nbacc);

void
MONITOR_EXT_WRAP_NAME(pnga_nbacc)(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbacc, pnga_nbacc);

  int mId_onesided = hpcrun_ga_metricId_onesidedOp();
  int mId_bytes = hpcrun_ga_metricId_bytesXfr();
  ifSample(smpl, metricVec,
	   collectMetric(metricVec, mId_onesided, 1/*metricIncr*/),
	   collectBytesXfr(metricVec, mId_bytes, g_a, lo, hi));

  real_pnga_nbacc(g_a, lo, hi, buf, ld, alpha, nbhandle);
}


#if 0
MONITOR_EXT_DECLARE_REAL_FN(ga_nbwait_fn_t, real_pnga_nbwait);

void
MONITOR_EXT_WRAP_NAME(pnga_nbwait)(Integer *nbhandle)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_nbwait, pnga_nbwait);

  //ifSample(smpl, metricVec, collect0(), collect0());

  real_pnga_nbwait(nbhandle);
}
#endif


//***************************************************************************
// 
//***************************************************************************

// ngai_get_common: (pnga_get, pnga_nbget, pnga_get_field, pnga_nbget_field)
