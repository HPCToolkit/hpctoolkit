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

//*********************************************************************
// global includes 
//*********************************************************************

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>


//*********************************************************************
// local includes 
//*********************************************************************

#include <include/hpctoolkit-config.h>

#include "disabled.h"
#include "env.h"
#include "files.h"
#include "monitor.h"
#include "rank.h"
#include "string.h"
#include "trace.h"
#include "thread_data.h"
#include "sample_prob.h"

#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcio-buffer.h>


//*********************************************************************
// type declarations
//*********************************************************************



//*********************************************************************
// forward declarations 
//*********************************************************************

static void hpcrun_trace_file_validate(int valid, char *op);
static inline void hpcrun_trace_append_with_time_real(core_profile_trace_data_t *cptd, unsigned int call_path_id, uint metric_id, uint32_t dLCA, uint64_t nanotime);


//*********************************************************************
// local variables 
//*********************************************************************

static int tracing = 0;

//*********************************************************************
// interface operations
//*********************************************************************

int
hpcrun_trace_isactive()
{
  return tracing;
}


void
hpcrun_trace_init()
{
  tracing = hpcrun_get_env_bool(HPCRUN_TRACE);
  TMSG(TRACE, "Tracing is %s", (tracing ? "ON" : "OFF"));
}


void
hpcrun_trace_open(core_profile_trace_data_t * cptd)
{
  if (hpcrun_get_disabled()) {
    tracing = 0;
    return;
  }

  TMSG(TRACE, "Trace open called");
  // With fractional sampling, if this process is inactive, then don't
  // open an output file, not even /dev/null.
  if (tracing && hpcrun_sample_prob_active()) {
	
    TMSG(TRACE, "Hit active portion");
    int fd, ret;

    // I think unlocked is ok here (we don't overlap any system
    // locks).  At any rate, locks only protect against threads, they
    // don't help with signal handlers (that's much harder).
    fd = hpcrun_open_trace_file(cptd->id);
    hpcrun_trace_file_validate(fd >= 0, "open");
    cptd->trace_buffer = hpcrun_malloc(HPCRUN_TraceBufferSz);
    ret = hpcio_outbuf_attach(&cptd->trace_outbuf, fd, cptd->trace_buffer,
			      HPCRUN_TraceBufferSz, HPCIO_OUTBUF_UNLOCKED,
                              hpcrun_malloc);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "open");

    hpctrace_hdr_flags_t flags = hpctrace_hdr_flags_NULL;
#ifdef DATACENTRIC_TRACE
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS, true);
#else
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS, false);
#endif

#if defined(LCA_TRACE) && (defined (HOST_CPU_x86_64) || defined (HOST_CPU_PPC))
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_LCA_RECORDED_BIT_POS, true);
    ENABLE(USE_TRAMP);
#else
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_LCA_RECORDED_BIT_POS, false);
#endif
    
    ret = hpctrace_fmt_hdr_outbuf(flags, cptd->trace_outbuf);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "write header to");
  }
  TMSG(TRACE, "Trace open done");
}


void
hpcrun_trace_append_with_time(core_profile_trace_data_t *st, unsigned int call_path_id, uint metric_id, uint64_t nanotime)
{
	if (tracing && hpcrun_sample_prob_active()) {
        hpcrun_trace_append_with_time_real(st, call_path_id, metric_id, INT_MAX, nanotime);
	}
}


void
hpcrun_trace_append(core_profile_trace_data_t *cptd, cct_node_t* node, uint metric_id, uint32_t dLCA)
{
  if (tracing && hpcrun_sample_prob_active()) {
    struct timeval tv;
    int ret = gettimeofday(&tv, NULL);
    assert(ret == 0 && "in trace_append: gettimeofday failed!");
    uint64_t nanotime = ((uint64_t)tv.tv_usec
			  + (((uint64_t)tv.tv_sec) * 1000000)) * 1000;

    // mark the leaf of a call path recorded in a trace record for retention
    // so that the call path associated with the trace record can be recovered.
    hpcrun_cct_retain(node);

    int32_t call_path_id = hpcrun_cct_persistent_id(node);

    hpcrun_trace_append_with_time_real(cptd, call_path_id, metric_id, dLCA, nanotime);
  }
}


void
hpcrun_trace_append_stream(core_profile_trace_data_t *cptd, cct_node_t* node,
  uint metric_id, uint32_t dLCA, uint64_t nanotime)
{
  if (tracing && hpcrun_sample_prob_active()) {
    // mark the leaf of a call path recorded in a trace record for retention
    // so that the call path associated with the trace record can be recovered.
    hpcrun_cct_retain(node);

    int32_t call_path_id = hpcrun_cct_persistent_id(node);

    hpcrun_trace_append_with_time_real(cptd, call_path_id, metric_id, dLCA, nanotime);
  }
}


void
hpcrun_trace_close(core_profile_trace_data_t * cptd)
{
  TMSG(TRACE, "Trace close called");
  if (tracing && hpcrun_sample_prob_active()) {

    TMSG(TRACE, "Trace active close code");
    int ret = hpcio_outbuf_close(&cptd->trace_outbuf);
    if (ret != HPCFMT_OK) {
      EMSG("unable to flush and close trace file");
    }

    int rank = hpcrun_get_rank();
    if (rank >= 0) {
      hpcrun_rename_trace_file(rank, cptd->id);
    }
  }
  TMSG(TRACE, "trace close done");
}

//*********************************************************************
// private operations
//*********************************************************************

static inline void hpcrun_trace_append_with_time_real(core_profile_trace_data_t *cptd, unsigned int call_path_id, uint metric_id, uint32_t dLCA, uint64_t nanotime)
{
    if (cptd->trace_min_time_us == 0) {
        cptd->trace_min_time_us = nanotime;
    }
    
    // TODO: should we need this check???
    if(cptd->trace_max_time_us < nanotime) {
        cptd->trace_max_time_us = nanotime;
    }
    
    hpctrace_fmt_datum_t trace_datum;
    trace_datum.cpId = (uint32_t)call_path_id;
    //TODO: was not in GPU version
    trace_datum.metricId = (uint32_t)metric_id;
    if (dLCA > HPCTRACE_FMT_DLCA_NULL)
      dLCA = HPCTRACE_FMT_DLCA_NULL;

#if defined(LCA_TRACE)
    HPCTRACE_FMT_SET_TIME(trace_datum.comp, nanotime);
    HPCTRACE_FMT_SET_DLCA(trace_datum.comp, dLCA);
#else
    trace_datum.comp = nanotime;
#endif
    
    hpctrace_hdr_flags_t flags = hpctrace_hdr_flags_NULL;
#ifdef DATACENTRIC_TRACE
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS, true);
#else
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_DATA_CENTRIC_BIT_POS, false);
#endif
    
#if defined(LCA_TRACE) && (defined (HOST_CPU_x86_64) || defined (HOST_CPU_PPC))
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_LCA_RECORDED_BIT_POS, true);
#else
    HPCTRACE_HDR_FLAGS_SET_BIT(flags, HPCTRACE_HDR_FLAGS_LCA_RECORDED_BIT_POS, false);
#endif
    
    int ret = hpctrace_fmt_datum_outbuf(&trace_datum, flags, cptd->trace_outbuf);
    hpcrun_trace_file_validate(ret == HPCFMT_OK, "append");
}


static void
hpcrun_trace_file_validate(int valid, char *op)
{
  if (!valid) {
    EMSG("unable to %s trace file\n", op);
    monitor_real_abort();
  }
}
