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
// Copyright ((c)) 2002-2022, Rice University
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
// RETCNT sample source simple oo interface
//


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>
#include <cct/cct.h>

#if defined (HOST_CPU_x86_64) || defined (HOST_CPU_PPC)

#include <messages/messages.h>

#include <lib/prof-lean/hpcrun-metric.h>

extern void hpcrun_set_retain_recursion_mode(bool mode);

//***************************************************************************

// ******** Local Constants ****************

//
// There is only 1 "event" associated with a return count
// so the event id is necessarily 0.
//
static const int RETCNT_EVENT = 0;
//
// The "period" for a return count event
//  is irrelevant (so it is arbitrarily 0).
//
static const int IRRELEVANT = 0;


// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(RETCNT_CTL, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(RETCNT_CTL, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  TMSG(RETCNT_CTL,"starting " HPCRUN_METRIC_RetCnt);

  TD_GET(ss_state)[self->sel_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(RETCNT_CTL, "thread fini (no action needed");
}

static void
METHOD_FN(stop)
{
  TMSG(RETCNT_CTL,"stopping " HPCRUN_METRIC_RetCnt);
  TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return hpcrun_ev_is(ev_str, HPCRUN_METRIC_RetCnt);
}
 

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(RETCNT_CTL, "Setting up return counts(trampolines)");

  // FIXME: MetricFlags_Ty_Final
  kind_info_t *ret_kind = hpcrun_metrics_new_kind();
  int metric_id = hpcrun_set_new_metric_info(ret_kind, HPCRUN_METRIC_RetCnt);
  hpcrun_close_kind(ret_kind);

  METHOD_CALL(self, store_event, RETCNT_EVENT, IRRELEVANT);
  METHOD_CALL(self, store_metric_id, RETCNT_EVENT, metric_id);

  // turn on trampoline processing
  ENABLE(USE_TRAMP);
}

static void
METHOD_FN(finalize_event_list)
{
}

//
// Event sets not truly relevant for this sample source,
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  TMSG(REC_COMPRESS, "RETCNT event ==> retain recursion");
  hpcrun_set_retain_recursion_mode(true); // make sure all recursion elements are retained
                                          // whenever RETCNT is used.
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available return-count events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\t\tEach time a procedure returns, the return count for that\n"
	 "\t\tprocedure is incremented\n" 
         "(experimental feature, x86 only)\n", HPCRUN_METRIC_RetCnt);
  printf("\n");
}


#define ss_name retcnt
#define ss_cls SS_SOFTWARE
#define ss_sort_order 100

#include "ss_obj.h"

// ***************************************************************************
//  Interface functions
// ***************************************************************************

// increment the return count
//
// N.B. : This function is necessary to avoid exposing the retcnt_obj.
//        For the case of the retcnt sample source, the handler (the trampoline)
//        is separate from the sample source code.
//        Consequently, the interaction with metrics must be done procedurally

void
hpcrun_retcnt_inc(cct_node_t* node, int incr)
{
  int metric_id = hpcrun_event2metric(&_retcnt_obj, RETCNT_EVENT);

  if (metric_id == -1) return;
  
  TMSG(TRAMP, "Increment retcnt (metric id = %d), by %d", metric_id, incr);
  cct_metric_data_increment(metric_id,
			    node,
			    (cct_metric_data_t){.i = incr});
}

#else

void
hpcrun_retcnt_inc(cct_node_t* node, int incr)
{
}

#endif
