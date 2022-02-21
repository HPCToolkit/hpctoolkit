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
// NONE sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include "sample_source_obj.h"
#include "common.h"
#include <hpcrun/sample_sources_registered.h>
// #include "simple_oo.h"
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>

#include <messages/messages.h>



/******************************************************************************
 * method definitions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for NONE
}


static void
METHOD_FN(thread_init)
{
  TMSG(NONE_CTL, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(NONE_CTL, "thread init action (noop)");
}

static void
METHOD_FN(start)
{
  TMSG(NONE_CTL,"starting NONE");

  TD_GET(ss_state)[self->sel_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(NONE_CTL, "thread fini action (noop)");
}

static void
METHOD_FN(stop)
{
  TMSG(NONE_CTL,"stopping NONE");
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
  return hpcrun_ev_is(ev_str,"NONE");
}
 

//
// Special NONE protocol:
//  if event is NONE@xxx, then create log file and process the TMSG logging
//  if event is just plain NONE, then 
//     no log file or any other evidence of hpcrun
//
static void
METHOD_FN(process_event_list,int lush_metrics)
{
  char *event_str = METHOD_CALL(self,get_event_str);
  char *none_str = strstr(event_str,"NONE");
  if (none_str) {
    char *use_log = strchr(none_str,'@');
    if (use_log == NULL) {
      hpcrun_set_disabled(); 
    }
  }
}

static void
METHOD_FN(finalize_event_list)
{
}

//
// Event sets not relevant for this sample source
// It DOES NOT SAMPLE, so there are NO event sets
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


//
// There are no events defined for this sample source
//
static void
METHOD_FN(display_events)
{
}


/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name none
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * interface functions 
 *****************************************************************************/

void 
hpcrun_process_sample_source_none(void)
{
  sample_source_t *none = &_none_obj;
  
  METHOD_CALL(none, process_event_list, 0);

  if (getenv("SHOW_NONE") && hpcrun_get_disabled()) {
    static char none_msg[] = "NOTE: sample source NONE is specified\n";
    write(2, none_msg, strlen(none_msg));
  }
}
