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

#include <stdlib.h>
#include <string.h>

#include "sample_source_obj.h"
#include "common.h"

#include <messages/messages.h>
#include <hpcrun/thread_data.h>

void
CMETHOD_FN(add_event, const char *ev)
{
  char *evl = self->evl.evl_spec;

  TMSG(SS_COMMON,"add event %s to evl |%s|", ev, evl);
  strcat(evl, ev);
  strcat(evl," ");
  TMSG(SS_COMMON,"evl after event added = |%s|",evl);
}

void
CMETHOD_FN(store_event, int event_id, long thresh)
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
CMETHOD_FN(store_metric_id, int event_idx, int metric_id)
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
CMETHOD_FN(get_event_str)
{
  return (self->evl).evl_spec;
}

bool
CMETHOD_FN(started)
{
  return (TD_GET(ss_state)[self->sel_idx] == START);
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

// **********************************************************
// Sample Source Failure Messages (ssfail)
// **********************************************************

static char *prefix = "HPCToolkit fatal error";
static char *warning = "HPCToolkit warning";
static char *hpcrun_L = 
   "If running a dynamically-linked program with hpcrun, use 'hpcrun -L <program>' for a list of available events.\n\n"
   "If running a statically-linked program built with hpclink, set HPCRUN_EVENT_LIST=LIST in your environment and\n"
   "run your program to see a list of available events.\n\n"
   "Note: Either of the aforementioned methods will exit after listing available events. Arguments to your program\n"
   "will be ignored. Thus, an execution to list events can be run on a single core and it will execute for only a few\n"
   "seconds."; 
static int papi_error = 0;


// Special case to treat failure in PAPI init as a soft error.
// If PAPI_library_init() fails, delay printing an error message until
// after trying all other sample sources.
//
// This is useful on ranger where PAPI is not compiled into the kernel
// on the login nodes, but we can still run with WALLCLOCK there.
//
void
hpcrun_save_papi_error(int error)
{
  papi_error = error;
}

static void
hpcrun_display_papi_error(void)
{
  if (papi_error == HPCRUN_PAPI_ERROR_UNAVAIL) {
    STDERR_MSG("%s: PAPI_library_init() failed as unavailable.\n"
        "Probably, the kernel is missing a module for accessing the hardware\n"
        "performance counters (perf_events, perfmon or perfctr).\n",
	warning);
  }
  else if (papi_error == HPCRUN_PAPI_ERROR_VERSION) {
    STDERR_MSG("%s: PAPI_library_init() failed with version mismatch.\n"
        "Probably, HPCToolkit is out of sync with PAPI, or else PAPI is\n"
	"out of sync with the kernel.\n",
	warning);
  }
}

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
  hpcrun_display_papi_error();

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
hpcrun_ssfail_all_derived(char *source)
{
  EEMSG("%s: All %s events are derived.  To use proxy sampling,\n"
	"at least one event must support hardware overflow (eg, PAPI_TOT_CYC).\n",
	prefix, source);
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

