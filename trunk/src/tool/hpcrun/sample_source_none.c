// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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
// NONE sample source simple oo interface
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

#include "csprof_options.h"
#include "metrics.h"
#include "sample_event.h"
#include "sample_source.h"
#include "sample_source_none_event.h"
#include "sample_source_common.h"
#include "sample_sources_registered.h"
#include "simple_oo.h"
#include "thread_data.h"

#include <messages/messages.h>



// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for NONE
}

static void
METHOD_FN(_start)
{
  TMSG(NONE_CTL,"starting NONE");

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(stop)
{
  TMSG(NONE_CTL,"stopping NONE");
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
  return (strstr(ev_str,"NONE") != NULL);
}
 
static void
METHOD_FN(process_event_list,int lush_metrics)
{

  char *_use_log = strchr(METHOD_CALL(self,get_event_str),'@');
  if ( _use_log) {
    METHOD_CALL(self, store_event, NONE_USE_LOG, 1);
  }
  else {
    METHOD_CALL(self, store_event, NONE_USE_LOG, 0);
  }
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int ret = csprof_set_max_metrics(1 + lush_metrics);
  
  if (ret > 0) {
    int metric_id = csprof_new_metric();
    TMSG(ITIMER_CTL,"No event set for NONE sample source");
    csprof_set_metric_info_and_period(metric_id, "NONE",
				      HPCFILE_METRIC_FLAG_ASYNC,
				      1);
  }
  thread_data_t *td = csprof_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD; // Event sets not relevant for itimer
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _none_obj = {
  // common methods

  .add_event     = csprof_ss_add_event,
  .store_event   = csprof_ss_store_event,
  .get_event_str = csprof_ss_get_event_str,
  .started       = csprof_ss_started,
  .start         = csprof_ss_start,

  // specific methods

  .init = init,
  ._start = _start,
  .stop  = stop,
  .shutdown = shutdown,
  .supports_event = supports_event,
  .process_event_list = process_event_list,
  .gen_event_set = gen_event_set,

  // data
  .evl = {
    .evl_spec = {[0] = '\0'},
    .nevents = 0
  },
  .evset_idx = 2,
  .name = "NONE",
  .state = UNINIT
};


/******************************************************************************
 * constructor 
 *****************************************************************************/

static void none_obj_reg(void) __attribute__ ((constructor));

static void
none_obj_reg(void)
{
  csprof_ss_register(&_none_obj);
}
