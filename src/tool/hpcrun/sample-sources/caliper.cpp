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
#include <stdbool.h>

#include <pthread.h>
*/

#include <string.h>
#include <ucontext.h>
#include <stdint.h>

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
  #include <hpcrun/thread_data.h>
  #include <hpcrun/safe-sampling.h>
  #include <hpcrun/sample_event.h>

  #include <utilities/tokenize.h>
  #include <messages/messages.h>
}

/*
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/sample_sources_registered.h>

#include <sample-sources/blame-shift/blame-shift.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>
*/

#include <caliper/Caliper.h>
using namespace cali;

#include <unordered_map>
using namespace std;

/******************************************************************************
 * macros
 *****************************************************************************/
#define DEFAULT_THRESHOLD 0

/******************************************************************************
 * type declarations
 *****************************************************************************/
typedef struct {
  uint64_t prev_values[MAX_EVENTS];
} caliper_info_t;

/******************************************************************************
 * caliper constants 
 *****************************************************************************/
const char counter_prefix[] = "counter_";

/******************************************************************************
 * local variables
 *****************************************************************************/
static int caliper_unavail = 0;

static sample_source_t* caliper_sample_obj = NULL;
static Caliper caliper = Caliper::instance();

static unordered_map<string, int> map;

/*
// Support for derived events (proxy sampling).
static int derived[MAX_EVENTS];
static int some_derived;
static int some_overflow;
*/
/******************************************************************************
 * external thread-local variables
 *****************************************************************************/
extern __thread bool hpcrun_thread_suppress_sample;

/******************************************************************************
 * callback functions
 *****************************************************************************/

static void
caliper_post_set_handler(Caliper* caliper, const Attribute& attr, const Variant& var)
{
  // if sampling disabled explicitly for this thread, skip all processing
  if (hpcrun_thread_suppress_sample) return;

  TMSG(CALIPER, "caliper event happened, name = ", attr.name().c_str());

  unordered_map<string, int>::const_iterator entry = map.find(attr.name());
  if (entry == map.end()) {
    return;
  }

  int event_idx = entry -> second;

  sample_source_t *self = caliper_sample_obj;
  thread_data_t *td = hpcrun_get_thread_data();
  caliper_info_t *info = td->ss_info[self->sel_idx].ptr;

  uint64_t prev_value = info -> prev_values[event_idx];
  uint64_t curr_value = (uint64_t)var.to_double();

  if (curr_value > prev_value) {
    info->prev_values[event_idx] = curr_value;
    
    ucontext_t uc;
    getcontext(&uc);
    
    if (!hpcrun_safe_enter()) {
      return;
    }
    
    hpcrun_sample_callpath(&uc, hpcrun_event2metric(self, event_idx), curr_value - prev_value,
        0, 0);
    hpcrun_safe_exit();
  }
}

/******************************************************************************
 * method functions
 *****************************************************************************/

void caliper_init(sample_source_t *self)
{
  if (!caliper) {
    caliper_unavail = 1;
    return;
  }
  caliper_sample_obj = self;
  self->state = INIT;
  caliper.events().post_set_evt.connect(&caliper_post_set_handler);
}

void caliper_thread_init(sample_source_t *self)
{
}

void caliper_thread_init_action(sample_source_t *self)
{
}

void caliper_start(sample_source_t *self)
{
  TMSG(CALIPER, "start");
  if (caliper_unavail) { return; }

  thread_data_t *td = hpcrun_get_thread_data();
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  if (my_state == START) {
    return;
  }

  TD_GET(ss_state)[self->sel_idx] = START;
}

void caliper_thread_fini_action(sample_source_t *self)
{
}

void caliper_stop(sample_source_t *self)
{
  TMSG(CALIPER, "stop");
  if (caliper_unavail) { return; }

  thread_data_t *td = hpcrun_get_thread_data();
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  if (my_state == STOP) {
    TMSG(CALIPER,"--stop called on an already stopped event set");
    return;
  }

  if (my_state != START) {
    TMSG(CALIPER,"*WARNING* Stop called on event set that has not been started");
    return;
  }

  TD_GET(ss_state)[self->sel_idx] = STOP;
}

void caliper_shutdown(sample_source_t *self)
{
  TMSG(CALIPER, "shutdown");
  if (caliper_unavail) { return; }

  if (TD_GET(ss_state)[self->sel_idx] != STOP) {
    caliper_stop(self);
  }

  self->state = UNINIT;
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
  static int num_events = 0;

  char* evlist = METHOD_CALL(self, get_event_str);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char name[1024];
    long thresh;

    TMSG(CALIPER, "checking event spec = %s",event);
    if (! hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD)) {
      AMSG("WARNING: %s using default threshold %ld, "
	   "better to use an explicit threshold.", name, DEFAULT_THRESHOLD);
    }

    char* metric = (char*) hpcrun_malloc(sizeof(char) * (strlen(name) + 1) );
    strcpy(metric, name);

    int metric_id = hpcrun_new_metric();
    hpcrun_set_metric_info(metric_id, metric);

	int event_idx = num_events++;
    METHOD_CALL(self, store_event, event_idx, thresh);
    METHOD_CALL(self, store_metric_id, event_idx, metric_id);	

    map[string(metric)]=event_idx;	
  }
}

void caliper_gen_event_set(sample_source_t *self, int lush_metrics)
{
  TMSG(CALIPER, "gen event list");
  if (caliper_unavail) { return; }
  
  caliper_info_t* info = (caliper_info_t*) hpcrun_malloc(sizeof(caliper_info_t));
  if (info == NULL) {
    hpcrun_abort("Failure to allocate space for Caliper sample source");
  }

  thread_data_t *td = hpcrun_get_thread_data();
  td->ss_info[self->sel_idx].ptr = info;

  memset(info->prev_values, 0, sizeof(info->prev_values));
}

void caliper_display_events(sample_source_t *self)
{
}

