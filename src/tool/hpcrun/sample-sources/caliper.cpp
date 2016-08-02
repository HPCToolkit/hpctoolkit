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

// The implementation of caliper sample source.

/******************************************************************************
 * system includes
 *****************************************************************************/
/*
#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>
*/

#include <string.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

//#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

extern "C" {
  #include "common.h"
  #include "sample_source_obj.h"

  #include "caliper-stub.h"

  #include <hpcrun/metrics.h>

  #include <utilities/tokenize.h>
  #include <messages/messages.h>
}

/*
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>

#include <sample-sources/blame-shift/blame-shift.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>
*/

#include <caliper/Caliper.h>
using namespace cali;


/******************************************************************************
 * macros
 *****************************************************************************/
#define DEFAULT_THRESHOLD 0

/******************************************************************************
 * forward declarations 
 *****************************************************************************/
static void calier_event_handler(); //TODO

/******************************************************************************
 * caliper constants 
 *****************************************************************************/
const char counter_prefix[] = "counter_";

/******************************************************************************
 * local variables
 *****************************************************************************/
static int caliper_unavail = 0;

static int caliper_total_events = 0;

/*
// Support for derived events (proxy sampling).
static int derived[MAX_EVENTS];
static int some_derived;
static int some_overflow;
*/
/******************************************************************************
 * external thread-local variables
 *****************************************************************************/
//ddextern __thread bool hpcrun_thread_suppress_sample;

/******************************************************************************
 * method functions
 *****************************************************************************/

void caliper_init(sample_source_t *self)
{
   Caliper caliper = Caliper::instance();

}

void caliper_thread_init(sample_source_t *self)
{
}

void caliper_thread_init_action(sample_source_t *self)
{
}

void caliper_start(sample_source_t *self)
{
}

void caliper_thread_fini_action(sample_source_t *self)
{
}

void caliper_stop(sample_source_t *self)
{
}

void caliper_shutdown(sample_source_t *self)
{
}

bool caliper_supports_event(sample_source_t *self, const char *ev_str)
{
  TMSG(CALIPER, "supports event");
  if (caliper_unavail) { return false; }

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }
  
  char evtmp[1024];
  int ec;
  long th;

  hpcrun_extract_ev_thresh(ev_str, sizeof(evtmp), evtmp, &th, DEFAULT_THRESHOLD);

  char* loc = strstr(evtmp, counter_prefix);

  return loc == evtmp;
}
 
void caliper_process_event_list(sample_source_t *self, int lush_metrics)
{
  TMSG(CALIPER, "process event list");
  if (caliper_unavail) { return; }

  char *event;
  int i, ret;
  int num_lush_metrics = 0;

  char* evlist = METHOD_CALL(self, get_event_str);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char name[1024];
    long thresh;

    TMSG(CALIPER, "checking event spec = %s",event);
    if (! hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD)) {
      AMSG("WARNING: %s using default threshold %ld, "
	   "better to use an explicit threshold.", name, DEFAULT_THRESHOLD);
    }

    int metric_id = hpcrun_new_metric();
    hpcrun_set_metric_info(metric_id, name);

	int event_idx = caliper_total_events++;
    METHOD_CALL(self, store_event, event_idx, thresh);
    METHOD_CALL(self, store_metric_id, event_idx, metric_id);	
  
	printf("name = %s, event_idx = %d, metric_id = %d.\n", name, event_idx, metric_id);
  }
}

void caliper_gen_event_set(sample_source_t *self, int lush_metrics)
{
}

void caliper_display_events(sample_source_t *self)
{
}

/******************************************************************************
 * public operations
 *****************************************************************************/
static void
caliper_event_handler()
{
}
