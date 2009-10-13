// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample_source_none.c $
// $Id: sample_source_none.c 2500 2009-09-21 20:07:42Z mfagan $
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include "hpcrun_options.h"
#include "metrics.h"
#include "sample_event.h"
#include "sample_source.h"
#include "sample_source_common.h"
#include "sample_sources_registered.h"
#include "simple_oo.h"
#include "thread_data.h"
#include <cct/cct.h>

#include <messages/messages.h>

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
  self->state = INIT; // no actual init actions necessary for RETCNT
}

static void
METHOD_FN(_start)
{
  TMSG(RETCNT_CTL,"starting RETCNT");

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(stop)
{
  TMSG(RETCNT_CTL,"stopping RETCNT");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}

static int
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"RETCNT") != NULL);
}
 

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int metric_id = hpcrun_new_metric();
  TMSG(RETCNT_CTL, "Setting up return counts(trampolines)");

  hpcrun_set_metric_info_and_period(metric_id, "RETCNT",
				    HPCRUN_MetricFlag_Async,
				    1);

  METHOD_CALL(self, store_event, RETCNT_EVENT, IRRELEVANT);
  METHOD_CALL(self, store_metric_id, RETCNT_EVENT, metric_id);

  // turn on trampoline processing
  ENABLE(USE_TRAMP);
}

//
// Event sets not truly relevant for this sample source,
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available return-count events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("RETCNT\teach time a procedure returns, the return count for that procedure is incremented\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _retcnt_obj = {
  // common methods

  .add_event     = hpcrun_ss_add_event,
  .store_event   = hpcrun_ss_store_event,
  .store_metric_id = hpcrun_ss_store_metric_id,
  .get_event_str = hpcrun_ss_get_event_str,
  .started       = hpcrun_ss_started,
  .start         = hpcrun_ss_start,

  // specific methods

  .init = init,
  ._start = _start,
  .stop  = stop,
  .shutdown = shutdown,
  .supports_event = supports_event,
  .process_event_list = process_event_list,
  .gen_event_set = gen_event_set,
  .display_events = display_events,

  // data
  .evl = {
    .evl_spec = {[0] = '\0'},
    .nevents = 0
  },
  .evset_idx = -1,
  .name = "RETCNT",
  .cls  = SS_SOFTWARE,
  .state = UNINIT
};


/******************************************************************************
 * constructor 
 *****************************************************************************/

static void retcnt_obj_reg(void) __attribute__ ((constructor));

static void
retcnt_obj_reg(void)
{
  hpcrun_ss_register(&_retcnt_obj);
}

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

  TMSG(TRAMP, "Increment retcnt (metric id = %d), by %d", metric_id, incr);
  cct_metric_data_increment(metric_id,
			    &node->metrics[metric_id],
			    (cct_metric_data_t){.i = incr});
}
