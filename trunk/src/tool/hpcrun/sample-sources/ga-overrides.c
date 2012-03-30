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

// FIXME: temporarily import one line from
//   armci/gaf2c/typesf2c.h:F2C_INTEGER_C_TYPE
typedef long Integer; // word size


//***************************************************************************
// macros
//***************************************************************************

typedef void ga_getput_fn_t(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld);

typedef void ga_brdcst_fn_t(Integer type, void *buf, Integer len, Integer originator);

#define SAMPLE_CALLPATH_BEFORE(smpl, metricId, metricIncr)	\
  sample_val_t smpl;						\
  hpcrun_sample_val_init(&smpl);				\
  if (metricId >= 0 && hpcrun_safe_enter()) {			\
    ucontext_t uc;						\
    getcontext(&uc);						\
    smpl = hpcrun_sample_callpath(&uc, metricId, metricIncr,	\
				    0, 1/*isSync*/);		\
    hpcrun_safe_exit();						\
  }

#define SAMPLE_CALLPATH_AFTER(smpl)					   \
  if (hpcrun_safe_enter()) {					   	   \
    /* metric_set_t* metrics = hpcrun_get_metric_set(smpl.sample_node); */ \
    /*metrics[after_metric] += 1; */					   \
    if (/*trace_isactive() &&*/ smpl.trace_node) {			   \
      hpcrun_trace_append(hpcrun_cct_persistent_id(smpl.trace_node));	   \
    }									   \
    hpcrun_safe_exit();							   \
  }

// ga_create
// ga_brdcst
// ga_put
// ga_get
// ga_brdcst
// ga_acc
// ga_nbacc
// ga_nbput

// ngai_get_common: (pnga_get, pnga_nbget, pnga_get_field, pnga_nbget_field)


//***************************************************************************
// 
//***************************************************************************

MONITOR_EXT_DECLARE_REAL_FN(ga_brdcst_fn_t, real_pnga_brdcst);

void
MONITOR_EXT_WRAP_NAME(pnga_brdcst)(Integer type, void *buf, Integer len, Integer originator)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_brdcst, pnga_brdcst);

  int metricId = hpcrun_ga_metricId_bytes();
  const uint64_t metricIncr = 1;

  SAMPLE_CALLPATH_BEFORE(smpl, metricId, metricIncr);

  real_pnga_brdcst(type, buf, len, originator);

  SAMPLE_CALLPATH_AFTER(smpl);
}



MONITOR_EXT_DECLARE_REAL_FN(ga_getput_fn_t, real_pnga_get);

void
MONITOR_EXT_WRAP_NAME(pnga_get)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_get, pnga_get);

  int metricId = hpcrun_ga_metricId_get();
  const uint64_t metricIncr = 1;

  TMSG(GA, "nga_get: %d", metricId);

  SAMPLE_CALLPATH_BEFORE(smpl, metricId, metricIncr);

  real_pnga_get(g_a, lo, hi, buf, ld);

  SAMPLE_CALLPATH_AFTER(smpl);
}


MONITOR_EXT_DECLARE_REAL_FN(ga_getput_fn_t, real_pnga_put);

void
MONITOR_EXT_WRAP_NAME(pnga_put)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld)
{
  MONITOR_EXT_GET_NAME_WRAP(real_pnga_put, pnga_put);

  int metricId = hpcrun_ga_metricId_put();
  const uint64_t metricIncr = 1;

  SAMPLE_CALLPATH_BEFORE(smpl, metricId, metricIncr);

  real_pnga_put(g_a, lo, hi, buf, ld);

  SAMPLE_CALLPATH_AFTER(smpl);
}

