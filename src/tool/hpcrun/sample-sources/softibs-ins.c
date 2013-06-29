// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-omp/src/tool/hpcrun/sample-sources/itimer.c $
// $Id: itimer.c 3947 2012-09-21 21:28:49Z mfagan $
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

//
// itimer sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>
#include <monitor-exts/monitor_ext.h>


/******************************************************************************
 * local includes
 *****************************************************************************/
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>

#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>

#include <sample-sources/blame-shift.h>

#include <sys/mman.h>
#include <memory/hpcrun-malloc.h>
#include <lib/prof-lean/atomic.h>
#include <sample-sources/softibs.h>
#include <datacentric.h>
#include <loadmap.h>

/* override functions at each memory access instrumentation */

// trigger synchronous samples
void
MONITOR_EXT_WRAP_NAME(llvm_memory_profiling)(void *data_addr, unsigned id)
{
  // setup the threshold for memory access sampling
  thread_data_t *td = hpcrun_get_thread_data();
  if(++td->ma_count < ma_period) {
    return;
  }

  hpcrun_safe_enter();

  td->ma_count = 0;
  void *start = NULL;
  void *end = NULL;
  unsigned long cpu = (unsigned long) TD_GET(core_profile_trace_data).id;

  int metric_id = hpcrun_ma_metric_id();
  int metric_incr = 1;
  sample_val_t sv;

  // enable datacentric analysis
  cct_node_t *data_node = NULL;
  if(ENABLED(DATACENTRIC)) {
    if(data_addr) {
      TD_GET(ldst) = 1;
      TD_GET(ea) = data_addr;
    } 
    data_node = splay_lookup(data_addr, &start, &end);
    if (!data_node) { 
      // check the static data
      uint16_t lmid = 0;
      void *static_data_addr = static_data_lookup(data_addr, &start, &end, &lmid);
      if(static_data_addr) {
        TD_GET(lm_id) = lmid;
        TD_GET(lm_ip) = (uintptr_t)static_data_addr;
      }
    }
  }
  // FIXME: record data_node and precise ip into thread data
  TD_GET(data_node) = data_node;
  TD_GET(start) = start;
  TD_GET(end) = end;

  ucontext_t context;
  getcontext(&context);
  sv = hpcrun_sample_callpath(&context, metric_id, metric_incr,
				1/*skipInner*/, 1/*isSync*/);

  TD_GET(start) = NULL;
  TD_GET(end) = NULL;
  TD_GET(data_node) = NULL;
  TD_GET(ldst) = 0;
  TD_GET(lm_id) = 0;
  TD_GET(lm_ip) = 0;
  TD_GET(ea) = NULL;

  // compute data range info (offset %)
  if(start && end) {
    float offset = (float)(data_addr - start)/(end - start);
    if (! hpcrun_has_metric_set(sv.sample_node)) {
      cct2metrics_assoc(sv.sample_node, hpcrun_metric_set_new());
    }
    metric_set_t* set = hpcrun_get_metric_set(sv.sample_node);
    hpcrun_metric_min(hpcrun_low_offset_metric_id(), set, (cct_metric_data_t){.r = offset});
    if (end - start <= 8) // one access covers the whole range
      hpcrun_metric_max(hpcrun_high_offset_metric_id(), set, (cct_metric_data_t){.r = 1.0});
    else
      hpcrun_metric_max(hpcrun_high_offset_metric_id(), set, (cct_metric_data_t){.r = offset});
  }

  // compute numa-related metrics
  int numa_access_node = numa_node_of_cpu(cpu);
  int numa_location_node;
  void *addr = data_addr;
  int ret_code = move_pages(0, 1, &addr, NULL, &numa_location_node, 0);
  if(ret_code == 0 && numa_location_node >= 0) {
//printf("data_addr is %p, cpu is %u, numa_access_node is %d, numa_loaction_node is %d\n",data_addr, cpu, numa_access_node, numa_location_node);
    if(numa_access_node == numa_location_node)
      cct_metric_data_increment(hpcrun_numa_match_metric_id(), sv.sample_node, (cct_metric_data_t){.i = 1});
    else
      cct_metric_data_increment(hpcrun_numa_mismatch_metric_id(), sv.sample_node, (cct_metric_data_t){.i = 1});
    // location metric
    cct_metric_data_increment(hpcrun_location_metric_id()[numa_location_node], sv.sample_node, (cct_metric_data_t){.i = 1});
  }

  hpcrun_safe_exit();

}

