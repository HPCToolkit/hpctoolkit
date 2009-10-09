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

#include <stdlib.h>
#include <string.h>

#include "sample_source.h"
#include "sample_source_common.h"

#include <messages/messages.h>

int
METHOD_FN(csprof_ss_started)
{
  NMSG(SS_COMMON,"check start for %s = %d",self->name,self->state);
  return (self->state == START);
}

void
METHOD_FN(csprof_ss_add_event, const char *ev)
{
  char *evl = self->evl.evl_spec;

  NMSG(SS_COMMON,"add event %s to evl |%s|",ev,evl);
  strcat(evl, ev);
  strcat(evl," ");
  NMSG(SS_COMMON,"evl after event added = |%s|",evl);
}

void
METHOD_FN(csprof_ss_store_event, int event_id, long thresh)
{
  TMSG(SAMPLE_SOURCE,"%s: store event %d thresh = %ld", self->name, event_id, thresh);
  evlist_t *_p = &(self->evl);
  int* ev      = &(_p->nevents);
  if (*ev >= MAX_EVENTS) {
    EMSG("Too many events entered for sample source. Event code %d ignored", event_id);
    return;
  }
  _ev_t* current_event  = &(_p->events[*ev]);

  current_event->event     = event_id;
  current_event->thresh    = thresh;

  (*ev)++;
  TMSG(SAMPLE_SOURCE,"%s now has %d events", self->name, *ev);
}

void
METHOD_FN(csprof_ss_store_metric_id, int event_idx, int metric_id)
{

  TMSG(SAMPLE_SOURCE, "%s event[%d] = metric_id %d", self->name, event_idx, metric_id);
  evlist_t *_p = &(self->evl);
  int n_events = _p->nevents;

  if (event_idx >= n_events) {
    EMSG("Trying to store metric_id(=%d) for an invalid event index(=%d)."
	 "Only %d events recorded for sample source %s", metric_id, event_idx, n_events, self->name);
    return;
  }
  _ev_t* current_event  = &(_p->events[event_idx]);

  current_event->metric_id = metric_id;
}


char* 
METHOD_FN(csprof_ss_get_event_str)
{
  return (self->evl).evl_spec;
}

void
METHOD_FN(csprof_ss_start)
{
  if (self->state != HARD_STOP){
    METHOD_CALL(self,_start);
  }
}

void
METHOD_FN(csprof_ss_hard_stop)
{
  METHOD_CALL(self,stop);
  self->state = HARD_STOP;
}

//------------------------------------------------------------
// Sample Source Failure Messages (ssfail)
//------------------------------------------------------------

static char *prefix = "HPCToolkit fatal error";
static char *hpcrun_L = "See 'hpcrun -L <program>' for a list of available events.";

// The none and unknown event failures happen before we set up the log
// file, so we can only write to stderr.
void
hpcrun_ssfail_none(void)
{
  STDERR_MSG("%s: no sampling source specified.\n"
      "Set HPCRUN_EVENT_LIST to a comma-separated list of EVENT@PERIOD pairs\n"
      "and rerun your program.  If you truly want to run with no events, then\n"
      "set HPCRUN_EVENT_LIST=NONE.\n%s",
      prefix, hpcrun_L);
  exit(1);
}

void
hpcrun_ssfail_unknown(char *event)
{
  STDERR_MSG("%s: event %s is unknown or unsupported.\n%s",
	     prefix, event, hpcrun_L);
  exit(1);
}

void
hpcrun_ssfail_unsupported(char *source, char *event)
{
  EEMSG("%s: %s event %s is not supported on this platform.\n%s",
	prefix, source, event, hpcrun_L);
  exit(1);
}

void
hpcrun_ssfail_derived(char *source, char *event)
{
  EEMSG("%s: %s event %s is a derived event and thus cannot be profiled.\n%s",
	prefix, source, event, hpcrun_L);
  exit(1);
}

void
hpcrun_ssfail_conflict(char *source, char *event)
{
  EEMSG("%s: %s event %s cannot be profiled in this sequence.\n"
	"Either it conflicts with another event, or it exceeds the number of\n"
	"hardware counters.\n%s",
	prefix, source, event, hpcrun_L);
  exit(1);
}

void
hpcrun_ssfail_start(char *source)
{
  EEMSG("%s: sample source %s failed to start.\n"
	"Check the event list and the HPCToolkit installation and try again.\n%s",
	prefix, source, hpcrun_L);
  exit(1);
}

// **********************************************************
// Interface (NON method) functions
// **********************************************************

int
hpcrun_event2metric(sample_source_t* ss, int event_idx)
{
  TMSG(SAMPLE_SOURCE, "%s fetching metric_id for event %d", ss->name, event_idx);
  evlist_t *_p = &(ss->evl);
  int n_events = _p->nevents;

  if (event_idx >= n_events) {
    EMSG("Trying to fetch metric id an invalid event index."
	 "Only %d events recorded for sample source %s. Returning 0", event_idx, n_events, ss->name);
    return 0;
  }
  _ev_t* current_event  = &(_p->events[event_idx]);

  TMSG(SAMPLE_SOURCE, "Fetched metric id = %d", current_event->metric_id);
  return current_event->metric_id;
}
