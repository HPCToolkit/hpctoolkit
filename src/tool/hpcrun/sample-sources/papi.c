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
// Copyright ((c)) 2002-2013, Rice University
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
// PAPI sample source simple oo interface
//


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <papi.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

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
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>

#include <sample-sources/blame-shift.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <datacentric.h>
#include <numa.h>
#include <numaif.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#define OVERFLOW_MODE 0
#define WEIGHT_METRIC 0
#define DEFAULT_THRESHOLD  2000000L


/******************************************************************************
 * type declarations 
 *****************************************************************************/

typedef struct {
  int eventSet;
  long long prev_values[MAX_EVENTS];
} papi_source_info_t;


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

void
hpcrun_metric_min(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr);
void
hpcrun_metric_max(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr);

static void papi_event_handler(int event_set, void *pc, void *data_addr, unsigned long t, unsigned long cpu, long long ovec, void *context);
static int  event_is_derived(int ev_code);
static void event_fatal_error(int ev_code, int papi_ret);


/******************************************************************************
 * local variables
 *****************************************************************************/

static int cyc_metric_id = -1; /* initialized to an illegal metric id */

// Special case to make PAPI_library_init() a soft failure.
// Make sure that we call no other PAPI functions.
//
static int papi_unavail = 0;

// Support for derived events (proxy sampling).
static int derived[MAX_EVENTS];
static int some_derived;
static int some_overflow;

// Metrics for numa related metrics
static int numa_match_metric_id = -1;
static int numa_mismatch_metric_id = -1;

static int low_offset_metric_id = -1;
static int high_offset_metric_id = -1;

static int *location_metric_id;

/******************************************************************************
 * method functions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  PAPI_set_debug(0x3ff);

  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  TMSG(PAPI,"PAPI_library_init = %d", ret);
  TMSG(PAPI,"PAPI_VER_CURRENT =  %d", PAPI_VER_CURRENT);

  // Delay reporting PAPI_library_init() errors.  This allows running
  // with other events if PAPI is not available.
  if (ret < 0) {
    hpcrun_save_papi_error(HPCRUN_PAPI_ERROR_UNAVAIL);
    papi_unavail = 1;
  } else if (ret != PAPI_VER_CURRENT) {
    hpcrun_save_papi_error(HPCRUN_PAPI_ERROR_VERSION);
    papi_unavail = 1;
  }

  // Tell PAPI to count events in all contexts (user, kernel, etc).
  // FIXME: PAPI_DOM_ALL causes some syscalls to fail which then
  // breaks some applications.  For example, this breaks some Gemini
  // (GNI) functions called from inside gasnet_init() or MPI_Init() on
  // the Cray XE (hopper).
  //
  if (ENABLED(SYSCALL_RISKY)) {
    ret = PAPI_set_domain(PAPI_DOM_ALL);
    if (ret != PAPI_OK) {
      EMSG("warning: PAPI_set_domain(PAPI_DOM_ALL) failed: %d", ret);
    }
  }

  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(PAPI, "thread init");
  if (papi_unavail) { return; }

  int retval = PAPI_thread_init(pthread_self);
  if (retval != PAPI_OK) {
    EEMSG("PAPI_thread_init NOT ok, retval = %d", retval);
    monitor_real_abort();
  }
  TMSG(PAPI, "thread init OK");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(PAPI, "register thread");
  if (papi_unavail) { return; }

  int retval = PAPI_register_thread();
  if (retval != PAPI_OK) {
    EEMSG("PAPI_register_thread NOT ok, retval = %d", retval);
    monitor_real_abort();
  }
  TMSG(PAPI, "register thread ok");
}

static void
METHOD_FN(start)
{
  TMSG(PAPI, "start");
  if (papi_unavail) { return; }

  thread_data_t *td = hpcrun_get_thread_data();
  papi_source_info_t *psi = td->ss_info[self->evset_idx].ptr;
  int eventSet = psi->eventSet;
  source_state_t my_state = TD_GET(ss_state)[self->evset_idx];

  // make PAPI start idempotent.  the application can turn on sampling
  // anywhere via the start-stop interface, so we can't control what
  // state PAPI is in.

  if (my_state == START) {
    return;
  }

  TMSG(PAPI,"starting PAPI w event set %d",eventSet);
  int ret = PAPI_start(eventSet);
  if (ret == PAPI_EISRUN) {
    // this case should not happen, but maybe it's not fatal
    EMSG("PAPI returned EISRUN, but state was not START");
  }
  else if (ret != PAPI_OK) {
    EMSG("PAPI_start failed with %s (%d)", PAPI_strerror(ret), ret);
    hpcrun_ssfail_start("PAPI");
  }

  if (some_derived) {
    ret = PAPI_read(eventSet, psi->prev_values);
    if (ret != PAPI_OK) {
      EMSG("PAPI_read failed with %s (%d)", PAPI_strerror(ret), ret);
    }
  }

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(PAPI, "unregister thread");
  if (papi_unavail) { return; }

  int retval = PAPI_unregister_thread();
  char msg[] = "!!NOT PAPI_OK!! (code = -9999999)\n";
  snprintf(msg, sizeof(msg)-1, "!!NOT PAPI_OK!! (code = %d)", retval);
  TMSG(PAPI, "unregister thread returns %s", retval == PAPI_OK? "PAPI_OK" : msg);
}

static void
METHOD_FN(stop)
{
  TMSG(PAPI, "stop");
  if (papi_unavail) { return; }

  thread_data_t *td = hpcrun_get_thread_data();
  papi_source_info_t *psi = td->ss_info[self->evset_idx].ptr;
  int eventSet = psi->eventSet;
  int nevents  = self->evl.nevents;
  source_state_t my_state = TD_GET(ss_state)[self->evset_idx];

  if (my_state == STOP) {
    TMSG(PAPI,"--stop called on an already stopped event set %d",eventSet);
    return;
  }

  if (my_state != START) {
    TMSG(PAPI,"*WARNING* Stop called on event set that has not been started");
    return;
  }

  TMSG(PAPI,"stop w event set = %d",eventSet);
  long_long *values = (long_long *) alloca(sizeof(long_long) * (nevents+2));
  int ret = PAPI_stop(eventSet, values);
  if (ret != PAPI_OK){
    EMSG("Failed to stop papi f eventset %d. Return code = %d ==> %s",
	 eventSet,ret,PAPI_strerror(ret));
  }

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(PAPI, "shutdown");
  if (papi_unavail) { return; }

  METHOD_CALL(self, stop); // make sure stop has been called
  PAPI_shutdown();

  self->state = UNINIT;
}

// Return true if PAPI recognizes the name, whether supported or not.
// We'll handle unsupported events later.
static bool
METHOD_FN(supports_event, const char *ev_str)
{
  TMSG(PAPI, "supports event");
  if (papi_unavail) { return false; }

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }
  
  char evtmp[1024];
  int ec;
  long th;

  hpcrun_extract_ev_thresh(ev_str, sizeof(evtmp), evtmp, &th, DEFAULT_THRESHOLD);
  return PAPI_event_name_to_code(evtmp, &ec) == PAPI_OK;
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(PAPI, "process event list");
  if (papi_unavail) { return; }

  char *event;
  int i, ret;
  int num_lush_metrics = 0;

  char* evlist = METHOD_CALL(self, get_event_str);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char name[1024];
    int evcode;
    long thresh;

    TMSG(PAPI,"checking event spec = %s",event);
    if (! hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD)) {
      AMSG("WARNING: %s using default threshold %ld, "
	   "better to use an explicit threshold.", name, DEFAULT_THRESHOLD);
    }
    ret = PAPI_event_name_to_code(name, &evcode);
    if (ret != PAPI_OK) {
      EMSG("unexpected failure in PAPI process_event_list(): "
	   "PAPI_event_name_to_code() returned %s (%d)",
	   PAPI_strerror(ret), ret);
      hpcrun_ssfail_unsupported("PAPI", name);
    }
    if (PAPI_query_event(evcode) != PAPI_OK) {
      hpcrun_ssfail_unsupported("PAPI", name);
    }

    // FIXME:LUSH: need a more flexible metric interface
    if (lush_metrics == 1 && strncmp(event, "PAPI_TOT_CYC", 12) == 0) {
      num_lush_metrics++;
    }

    TMSG(PAPI,"got event code = %x, thresh = %ld", evcode, thresh);
    METHOD_CALL(self, store_event, evcode, thresh);
  }
  int nevents = (self->evl).nevents;
  TMSG(PAPI,"nevents = %d", nevents);

  hpcrun_pre_allocate_metrics(nevents + num_lush_metrics);

  some_derived = 0;
  some_overflow = 0;

  // set up NUMA related metrics
  numa_match_metric_id = hpcrun_new_metric(); /* create numa metric id (access matches data location in NUMA node) */
  numa_mismatch_metric_id = hpcrun_new_metric(); /* create numa metric id (access does not match data location in NUMA node) */
  low_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (low offset) */
  high_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (high offset) */

  // create data location (numa node) metrics
  int numa_node_num = numa_num_configured_nodes();
  location_metric_id = (int *) hpcrun_malloc(numa_node_num * sizeof(int));
  for (i = 0; i < numa_node_num; i++) {
    location_metric_id[i] = hpcrun_new_metric();
  }

  for (i = 0; i < nevents; i++) {
    char buffer[PAPI_MAX_STR_LEN + 10];
    int metric_id = hpcrun_new_metric(); /* weight */
    metric_desc_properties_t prop = metric_property_none;
    METHOD_CALL(self, store_metric_id, i, metric_id);
    PAPI_event_code_to_name(self->evl.events[i].event, buffer);
    TMSG(PAPI, "metric for event %d = %s", i, buffer);
    // blame shifting needs to know if there is a cycles metric
    if (strcmp(buffer, "PAPI_TOT_CYC") == 0) {
      prop = metric_property_cycles;
      blame_shift_source_register(bs_type_cycles);
    } 

    // allow derived events (proxy sampling), as long as some event
    // supports hardware overflow.  use threshold = 0 to force proxy
    // sampling (for testing).
    if (event_is_derived(self->evl.events[i].event)
	|| self->evl.events[i].thresh == 0)
    {
      TMSG(PAPI, "using proxy sampling for event %s", buffer);
      strcat(buffer, " (proxy)");
      self->evl.events[i].thresh = 1;
      derived[i] = 1;
      some_derived = 1;
    } else {
      derived[i] = 0;
      some_overflow = 1;
    }

    hpcrun_set_metric_info_and_period(metric_id, strdup(buffer),
				      MetricFlags_ValFmt_Int,
				      self->evl.events[i].thresh, prop);
    int threshold = self->evl.events[i].thresh;
    hpcrun_set_metric_info_and_period(numa_match_metric_id, "NUMA_MATCH",
                                      MetricFlags_ValFmt_Int,
                                      threshold, metric_property_none);
    hpcrun_set_metric_info_and_period(numa_mismatch_metric_id, "NUMA_MISMATCH",
                                      MetricFlags_ValFmt_Int,
                                      threshold, metric_property_none);
    hpcrun_set_metric_info_w_fn(low_offset_metric_id, "LOW_OFFSET",
                                      MetricFlags_ValFmt_Real,
                                      1, hpcrun_metric_min, metric_property_none);
    hpcrun_set_metric_info_w_fn(high_offset_metric_id, "HIGH_OFFSET",
                                      MetricFlags_ValFmt_Real,
                                      1, hpcrun_metric_max, metric_property_none);

    // set numa location metrics
    int j;
    for (j = 0; j < numa_node_num; j++) {
      char metric_name[128];
      sprintf(metric_name, "NUMA_NODE%d", j);
      hpcrun_set_metric_info_and_period(location_metric_id[j], strdup(metric_name),
                                         MetricFlags_ValFmt_Int,
                                         threshold, metric_property_none);
    }

    // FIXME:LUSH: need a more flexible metric interface
    if (num_lush_metrics > 0 && strcmp(buffer, "PAPI_TOT_CYC") == 0) {
      // there should be one lush metric; its source is the last event
      assert(num_lush_metrics == 1 && (i == (nevents - 1)));
      int mid_idleness = hpcrun_new_metric();
      lush_agents->metric_time = metric_id;
      lush_agents->metric_idleness = mid_idleness;

      hpcrun_set_metric_info_and_period(mid_idleness, "idleness",
					MetricFlags_ValFmt_Real,
					self->evl.events[i].thresh, prop);
    }
  }

  if (! some_overflow) {
    hpcrun_ssfail_all_derived("PAPI");
  }
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int i;
  int ret;
  int eventSet;

  TMSG(PAPI, "gen event set");
  if (papi_unavail) { return; }

  int ss_info_size = sizeof(papi_source_info_t);
  papi_source_info_t *psi = hpcrun_malloc(ss_info_size);
  if (psi == NULL) {
    hpcrun_abort("Failure to allocate space for PAPI sample source");
  }

  psi->eventSet = PAPI_NULL;
  memset(psi->prev_values,0, sizeof(psi->prev_values));

  // record the component state in thread state
  thread_data_t *td = hpcrun_get_thread_data();
  td->ss_info[self->evset_idx].ptr = psi;


  eventSet = PAPI_NULL;
  ret = PAPI_create_eventset(&eventSet);
  TMSG(PAPI,"PAPI_create_eventset = %d, eventSet = %d", ret, eventSet);
  if (ret != PAPI_OK) {
    hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s", 
		 ret, PAPI_strerror(ret));
  }

  int nevents = (self->evl).nevents;
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    ret = PAPI_add_event(eventSet, evcode);
    TMSG(PAPI, "PAPI_add_event(eventSet=%d, event_code=%x)", eventSet, evcode);
    if (ret != PAPI_OK) {
      EMSG("failure in PAPI gen_event_set(): PAPI_add_event() returned: %s (%d)",
	   PAPI_strerror(ret), ret);
      event_fatal_error(evcode, ret);
    }
  }

  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    long thresh = self->evl.events[i].thresh;

    if (! derived[i]) {
      TMSG(PAPI, "PAPI_overflow(eventSet=%d, evcode=%x, thresh=%d)",
           eventSet, evcode, thresh);
      ret = PAPI_overflow(eventSet, evcode, thresh, OVERFLOW_MODE,
                          papi_event_handler);
      TMSG(PAPI, "PAPI_overflow = %d", ret);
      if (ret != PAPI_OK) {
	EMSG("failure in PAPI gen_event_set(): PAPI_overflow() returned: %s (%d)",
             PAPI_strerror(ret), ret);
        event_fatal_error(evcode, ret);
      }
    }
  }
  psi->eventSet= eventSet;
}

static void
METHOD_FN(display_events)
{
  PAPI_event_info_t info;
  char name[200], *prof;
  int ev, ret, num_total, num_prof;

  printf("===========================================================================\n");
  printf("Available PAPI preset events\n");
  printf("===========================================================================\n");
  printf("Name\t    Profilable\tDescription\n");
  printf("---------------------------------------------------------------------------\n");

  if (papi_unavail) {
    printf("PAPI is not available.  Probably, the kernel doesn't support PAPI,\n"
	   "or else maybe HPCToolkit is out of sync with PAPI.\n\n");
    return;
  }

  num_total = 0;
  num_prof = 0;
  ev = PAPI_PRESET_MASK;
  ret = PAPI_OK;
#ifdef PAPI_ENUM_FIRST
  ret = PAPI_enum_event(&ev, PAPI_ENUM_FIRST);
#endif
  while (ret == PAPI_OK) {
    if (PAPI_query_event(ev) == PAPI_OK) {
      PAPI_event_code_to_name(ev, name);
      PAPI_get_event_info(ev, &info);
      if (event_is_derived(ev)) {
	prof = "No";
      } else {
	prof = "Yes";
	num_prof++;
      }
      num_total++;
      printf("%-10s\t%s\t%s\n", name, prof, info.long_descr);
    }
    ret = PAPI_enum_event(&ev, PAPI_ENUM_EVENTS);
  }
  printf("Total PAPI events: %d, able to profile: %d\n",
	 num_total, num_prof);
  printf("\n");

  printf("===========================================================================\n");
  printf("Available native events\n");
  printf("===========================================================================\n");
  printf("Name\t\t\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");

  num_total = 0;
  ev = PAPI_NATIVE_MASK;
  ret = PAPI_OK;
#ifdef PAPI_ENUM_FIRST
  ret = PAPI_enum_event(&ev, PAPI_ENUM_FIRST);
#endif
  while (ret == PAPI_OK) {
    if (PAPI_query_event(ev) == PAPI_OK) {
      PAPI_event_code_to_name(ev, name);
      PAPI_get_event_info(ev, &info);
      num_total++;
      printf("%-30s\t%s\n", name, info.long_descr);
    }
    ret = PAPI_enum_event(&ev, PAPI_ENUM_EVENTS);
  }
  printf("Total native events: %d\n", num_total);
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name papi
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

void
hpcrun_metric_min(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if(loc->i > incr.i || loc->i == 0) loc->i = incr.i; break;
    case MetricFlags_ValFmt_Real:
      if(loc->r > incr.r || loc->r == 0.0) loc->r = incr.r; break;
    default:
      assert(false);
  }
}

void
hpcrun_metric_max(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if(loc->i < incr.i || loc->i == 0) loc->i = incr.i; break;
    case MetricFlags_ValFmt_Real:
      if(loc->r < incr.r || loc->r == 0.0) loc->r = incr.r; break;
    default:
      assert(false);
  }
}

// Returns: 1 if the event code is a derived event.
// The papi_avail(1) utility shows how to do this.
static int
event_is_derived(int ev_code)
{
  PAPI_event_info_t info;

  // "Is derived" is kind of a bad thing, so if any unexpected failure
  // occurs, we'll return the "bad" answer.
  if (PAPI_get_event_info(ev_code, &info) != PAPI_OK
      || info.derived == NULL) {
    return 1;
  }
  if (info.count == 1
      || strlen(info.derived) == 0
      || strcmp(info.derived, "NOT_DERIVED") == 0
      || strcmp(info.derived, "DERIVED_CMPD") == 0) {
    return 0;
  }
  return 1;
}

static void
event_fatal_error(int ev_code, int papi_ret)
{
  char name[1024];

  PAPI_event_code_to_name(ev_code, name);
  if (PAPI_query_event(ev_code) != PAPI_OK) {
    hpcrun_ssfail_unsupported("PAPI", name);
  }
  if (event_is_derived(ev_code)) {
    hpcrun_ssfail_derived("PAPI", name);
  }
  if (papi_ret == PAPI_ECNFLCT) {
    hpcrun_ssfail_conflict("PAPI", name);
  }
  hpcrun_ssfail_unsupported("PAPI", name);
}

static void
papi_event_handler(int event_set, void *pc, void *data_addr,
		   unsigned long t, unsigned long cpu, long long ovec,
                   void *context)
{
  sample_source_t *self = &_papi_obj;
  long long values[MAX_EVENTS];
  int my_events[MAX_EVENTS];
  int my_event_count = MAX_EVENTS;
  int nevents  = self->evl.nevents;
  int i, ret;
  void *start = NULL;
  void *end = NULL;

  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    return;
  }

  TMSG(PAPI_SAMPLE,"papi event happened, ovec = %ld",ovec);

  if (some_derived) {
    ret = PAPI_read(event_set, values);
    if (ret != PAPI_OK) {
      EMSG("PAPI_read failed with %s (%d)", PAPI_strerror(ret), ret);
    }
  }

  ret = PAPI_get_overflow_event_index(event_set, ovec, my_events, &my_event_count);
  if (ret != PAPI_OK) {
    hpcrun_abort("Failed inside papi_event_handler at get_overflow_event_index."
		 "Return code = %d ==> %s", ret, PAPI_strerror(ret));
  }

  for (i = 0; i < my_event_count; i++) {
    // FIXME: SUBTLE ERROR: metric_id may not be same from hpcrun_new_metric()!
    // This means lush's 'time' metric should be *last*

    int metric_id = hpcrun_event2metric(&_papi_obj, my_events[i]);

    TMSG(PAPI_SAMPLE,"sampling call path for metric_id = %d", metric_id);

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
        load_module_t *lm = hpcrun_loadmap_findByAddr(pc, pc);
        if(lm && lm->dso_info) {
          void *static_data_addr = static_data_interval_splay_lookup(&(lm->dso_info->data_root), data_addr, &start, &end);
          if(static_data_addr) {
            TD_GET(lm_id) = lm->id;
            TD_GET(lm_ip) = (uintptr_t)static_data_addr;
          }
        }
      }
    }
    // FIXME: record data_node and precise ip into thread data
    TD_GET(data_node) = data_node;
    TD_GET(pc) = pc;
    sample_val_t sv = hpcrun_sample_callpath(context, metric_id, 1/*metricIncr*/, 
			   0/*skipInner*/, 0/*isSync*/);
    TD_GET(data_node) = NULL;
    TD_GET(pc) = NULL;
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
      hpcrun_metric_min(low_offset_metric_id, set, (cct_metric_data_t){.r = offset});
      if (end - start <= 8) // one access covers the whole range
        hpcrun_metric_max(high_offset_metric_id, set, (cct_metric_data_t){.r = 1.0});
      else
        hpcrun_metric_max(high_offset_metric_id, set, (cct_metric_data_t){.r = offset});
    }

    // compute numa-related metrics
    int numa_access_node = numa_node_of_cpu(cpu);
    int numa_location_node;
    void *addr = data_addr;
    int ret_code = move_pages(0, 1, &addr, NULL, &numa_location_node, 0);
    if(ret_code == 0 && numa_location_node >= 0) {
//printf("data_addr is %p, cpu is %u, numa_access_node is %d, numa_loaction_node is %d\n",data_addr, cpu, numa_access_node, numa_location_node);
      if(numa_access_node == numa_location_node)
        cct_metric_data_increment(numa_match_metric_id, sv.sample_node, (cct_metric_data_t){.i = 1});
      else
        cct_metric_data_increment(numa_mismatch_metric_id, sv.sample_node, (cct_metric_data_t){.i = 1});
      // location metric
      cct_metric_data_increment(location_metric_id[numa_location_node], sv.sample_node, (cct_metric_data_t){.i = 1});
    }

    blame_shift_apply(metric_id, sv.sample_node, 1 /*metricIncr*/);
  }

  // Add metric values for derived events by the difference in counter
  // values.  Some samples can take a long time (eg, analyzing a new
  // load module), so read the counters both on entry and exit to
  // avoid counting our work.

  if (some_derived) {
    thread_data_t *td = hpcrun_get_thread_data();
    papi_source_info_t *psi = td->ss_info[self->evset_idx].ptr;
    for (i = 0; i < nevents; i++) {
      if (derived[i]) {
	hpcrun_sample_callpath(context, hpcrun_event2metric(self, i),
			       values[i] - psi->prev_values[i], 0, 0);
      }
    }

    ret = PAPI_read(event_set, psi->prev_values);
    if (ret != PAPI_OK) {
      EMSG("PAPI_read failed with %s (%d)", PAPI_strerror(ret), ret);
    }
  }

  hpcrun_safe_exit();
}
