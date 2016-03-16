// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/papi.c $
// $Id: papi.c 4027 2012-11-28 20:03:03Z krentel $
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

//
// PAPI-C (Component PAPI) sample source simple oo interface
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
#include <stdint.h>
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
#include "papi-c-extended-info.h"
#include "sample-filters.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/threadmgr.h>

#include <sample-sources/blame-shift/blame-shift.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>



/******************************************************************************
 * macros
 *****************************************************************************/

#define OVERFLOW_MODE 0
#define WEIGHT_METRIC 0
#define DEFAULT_THRESHOLD  2000000L

#include "papi-c.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/
static void papi_event_handler(int event_set, void *pc, long long ovec, void *context);
static int  event_is_derived(int ev_code);
static void event_fatal_error(int ev_code, int papi_ret);

/******************************************************************************
 * local variables
 *****************************************************************************/


// Special case to make PAPI_library_init() a soft failure.
// Make sure that we call no other PAPI functions.
//
static int papi_unavail = 0;

//
// To accomodate GPU blame shifting, we must disable the cuda component
// Flag below controls this disabling
//
static bool disable_papi_cuda = false;

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
get_event_index(sample_source_t *self, int event_code)
{
  int i;
  int nevents = self->evl.nevents;
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    if (event_code == evcode) return i;
  }
  assert(0);
}

//
// fetch a given component's event set. Create one if need be
//
int 
get_component_event_set(papi_source_info_t* psi, int cidx)
{
   if (cidx < 0 || cidx >= psi->num_components) {
    hpcrun_abort("PAPI component index out of range [0,%d]: %d", psi->num_components, cidx);
   }

   papi_component_info_t* ci = &(psi->component_info[cidx]);

   if (!ci->inUse) {
     ci->get_event_set(&(ci->eventSet));
     ci->inUse = true;
  }
  return ci->eventSet;
}

//
// add an event to a component's event set
//
int
component_add_event(papi_source_info_t* psi, int cidx, int evcode)
{
  int event_set = get_component_event_set(psi, cidx);
  papi_component_info_t* ci = &(psi->component_info[cidx]);
  return ci->add_event(event_set, evcode);
}

static bool
thread_count_scaling_for_component(int cidx)
{
  const PAPI_component_info_t *pci = PAPI_get_component_info(cidx);
  if (strcmp(pci->name, "bgpm/L2Unit") == 0) return true;
  return 0;
}

//-----------------------------------------------------------
// function print_desc
//   wrap lines for native event descriptions to contain 
//   four white space followed by BUFLEN characters
//-----------------------------------------------------------
static void 
print_desc(char *s)
{
#define BUFLEN 68 
  char buffer[BUFLEN];
  char *s_end = s + strlen(s);
  while (s < s_end) {
    int i;
    char *cur = s;
           
    //-------------------------------------------------
    // peel off at most the next BUFLEN characters from
    // the string. break the text at last white 
    // space if a word will extend beyond the boundary. 
    //-------------------------------------------------
    int last_blank = BUFLEN-1;
    for(i=0;i<BUFLEN; i++) {
      buffer[i] = *cur;
      if (*cur == ' ') {
	// remember last white space
	last_blank = i;
      }
      if (*cur == '\n'|| *cur == 0) { 
	// break at newline or end of string
	last_blank = i;
	break;
      }
      cur++;
    }
    buffer[last_blank] = 0;
    s += last_blank + 1;

    printf("      %s\n", buffer);
  }
}

/******************************************************************************
 * sample source registration
 *****************************************************************************/

// Support for derived events (proxy sampling).
static int derived[MAX_EVENTS];
static int some_overflow;


/******************************************************************************
 * external thread-local variables
 *****************************************************************************/
extern __thread bool hpcrun_thread_suppress_sample;

/******************************************************************************
 * method functions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  // PAPI_set_debug(0x3ff);

  // **NOTE: some papi components may start threads, so
  //         hpcrun must ignore these threads to ensure that PAPI_library_init
  //         succeeds
  //
  monitor_disable_new_threads();
  if (disable_papi_cuda) {
    TMSG(PAPI_C, "Will disable PAPI cuda component (if component is active)");
    int cidx = PAPI_get_component_index("cuda");
    if (cidx) {
      int res = PAPI_disable_component(cidx);
      if (res == PAPI_OK) {
	TMSG(PAPI, "PAPI cuda component disabled");
      }
      else {
	EMSG("*** PAPI cuda component could not be disabled!!!");
      }
    }
  }
  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  monitor_enable_new_threads();

  TMSG(PAPI_C,"PAPI_library_init = %d", ret);
  TMSG(PAPI_C,"PAPI_VER_CURRENT =  %d", PAPI_VER_CURRENT);

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
  int cidx;
  TMSG(PAPI, "start");

  if (papi_unavail) { 
    return; 
  }

  thread_data_t* td = hpcrun_get_thread_data();
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  // make PAPI start idempotent.  the application can turn on sampling
  // anywhere via the start-stop interface, so we can't control what
  // state PAPI is in.

  if (my_state == START) {
    TMSG(PAPI,"*NOTE* PAPI start called when already in state START");
    return;
  }

  // for each active component, start its event set
  papi_source_info_t* psi = td->ss_info[self->sel_idx].ptr;
  for (cidx=0; cidx < psi->num_components; cidx++) {
    papi_component_info_t* ci = &(psi->component_info[cidx]);
    if (ci->inUse) {
      if (component_uses_sync_samples(cidx)) {
	TMSG(PAPI, "component %d is synchronous, use synchronous start", cidx);
	ci->sync_start();
      }
      else {
	TMSG(PAPI,"starting PAPI event set %d for component %d", ci->eventSet, cidx);
	int ret = PAPI_start(ci->eventSet);
	if (ret == PAPI_EISRUN) {
	  // this case should not happen, but maybe it's not fatal
	  EMSG("PAPI returned EISRUN for event set %d component %d", ci->eventSet, cidx);
	}
	else if (ret != PAPI_OK) {
	  EMSG("PAPI_start failed with %s (%d) for event set %d component %d ", 
	       PAPI_strerror(ret), ret, ci->eventSet, cidx);
	  hpcrun_ssfail_start("PAPI");
	}

	if (ci->some_derived) {
	  ret = PAPI_read(ci->eventSet, ci->prev_values);
	  if (ret != PAPI_OK) {
	    EMSG("PAPI_read of event set %d for component %d failed with %s (%d)", 
		 ci->eventSet, cidx, PAPI_strerror(ret), ret);
	  }
	}
      }
    }
  }
  td->ss_state[self->sel_idx] = START;
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
  int cidx;

  TMSG(PAPI, "stop");
  if (papi_unavail) { return; }

  thread_data_t *td = hpcrun_get_thread_data();
  int nevents = self->evl.nevents;
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  if (my_state == STOP) {
    TMSG(PAPI,"*NOTE* PAPI stop called when already in state STOP");
    return;
  }

  if (my_state != START) {
    TMSG(PAPI,"*WARNING* PAPI stop called when not in state START");
    return;
  }

  papi_source_info_t *psi = td->ss_info[self->sel_idx].ptr;
  for (cidx=0; cidx < psi->num_components; cidx++) {
    papi_component_info_t *ci = &(psi->component_info[cidx]);
    if (ci->inUse) {
      if (component_uses_sync_samples(cidx)) {
	TMSG(PAPI, "component %d is synchronous, stop is trivial", cidx);
      }
      else {
	TMSG(PAPI,"stop w event set = %d", ci->eventSet);
	long_long values[nevents+2];
	//	long_long *values = (long_long *) alloca(sizeof(long_long) * (nevents+2));
	int ret = PAPI_stop(ci->eventSet, values);
	if (ret != PAPI_OK){
	  EMSG("Failed to stop PAPI for eventset %d. Return code = %d ==> %s",
	       ci->eventSet, ret, PAPI_strerror(ret));
	}
      }
    }
  }

  TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(PAPI, "shutdown");
  if (papi_unavail) { return; }

  METHOD_CALL(self, stop); // make sure stop has been called
  // FIXME: add component shutdown code here
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
    // FIXME: restore checking will require deciding if the event is synchronous or not
#ifdef USE_PAPI_CHECKING
    if (! hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD)) {
      AMSG("WARNING: %s using default threshold %ld, "
	   "better to use an explicit threshold.", name, DEFAULT_THRESHOLD);
    }
#else
    hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD);
#endif // USE_PAPI_CHECKING
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

    TMSG(PAPI,"event %s -> event code = %x, thresh = %ld", event, evcode, thresh);
    METHOD_CALL(self, store_event, evcode, thresh);
  }
  int nevents = (self->evl).nevents;
  TMSG(PAPI,"nevents = %d", nevents);

  hpcrun_pre_allocate_metrics(nevents + num_lush_metrics);

  some_overflow = 0;
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
	|| self->evl.events[i].thresh == 0) {
      TMSG(PAPI, "using proxy sampling for event %s", buffer);
      strcat(buffer, " (proxy)");
      self->evl.events[i].thresh = 1;
      derived[i] = 1;
    }
    else {
      derived[i] = 0;
      some_overflow = 1;
    }

    int cidx = PAPI_get_event_component(self->evl.events[i].event);
    int threshold;
    if (thread_count_scaling_for_component(cidx)) {
      threshold = 1;
    }
    else {
      threshold = self->evl.events[i].thresh;
    }

    if (component_uses_sync_samples(cidx))
      TMSG(PAPI, "Event %s from synchronous component", buffer);
    hpcrun_set_metric_info_and_period(metric_id, strdup(buffer),
				      MetricFlags_ValFmt_Int,
				      threshold, prop);

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
METHOD_FN(gen_event_set, int lush_metrics)
{
  thread_data_t *td = hpcrun_get_thread_data();
  int i;
  int ret;

  TMSG(PAPI, "generating all event sets for all components");
  if (papi_unavail) { return; }

  int num_components = PAPI_num_components();
  int ss_info_size = sizeof(papi_source_info_t) + 
    num_components * sizeof(papi_component_info_t);

  TMSG(PAPI, "Num components = %d", num_components);
  papi_source_info_t* psi = hpcrun_malloc(ss_info_size);
  if (psi == NULL) {
    hpcrun_abort("Failure to allocate vector for PAPI components");
  }

  // initialize state for each component
  psi->num_components = num_components;
  for (i = 0; i < num_components; i++) {
    papi_component_info_t *ci = &(psi->component_info[i]);
    ci->inUse = false;  
    ci->eventSet = PAPI_NULL;
    ci->state = INIT;
    ci->some_derived = 0;
    ci->get_event_set = component_get_event_set(i);
    ci->add_event = component_add_event_proc(i);
    ci->finalize_event_set = component_finalize_event_set(i);
    ci->scale_by_thread_count = thread_count_scaling_for_component(i);
    ci->is_sync = component_uses_sync_samples(i);
    ci->sync_setup = sync_setup_for_component(i);
    ci->sync_teardown = sync_teardown_for_component(i);
    ci->sync_start = sync_start_for_component(i);
    ci->sync_stop = sync_stop_for_component(i);
    memset(ci->prev_values, 0, sizeof(ci->prev_values));
  }

  // record the component state in thread state
  td->ss_info[self->sel_idx].ptr = psi;

  int nevents = (self->evl).nevents;
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    int cidx = PAPI_get_event_component(evcode);
    
    ret = component_add_event(psi, cidx, evcode);
    psi->component_info[cidx].some_derived |= event_is_derived(evcode);
    TMSG(PAPI, "Added event code %x to component %d", evcode, cidx);
    {
      char buffer[PAPI_MAX_STR_LEN];
      PAPI_event_code_to_name(evcode, buffer);
      TMSG(PAPI, 
	   "PAPI_add_event(eventSet=%%d, event_code=%x (event name %s)) component=%d", 
	   /* eventSet, */ evcode, buffer, cidx);
    }
    if (ret != PAPI_OK) {
      EMSG("failure in PAPI gen_event_set(): PAPI_add_event() returned: %s (%d)",
	   PAPI_strerror(ret), ret);
      event_fatal_error(evcode, ret);
    }
  }

  // finalize component event sets
    for (i = 0; i < num_components; i++) {
      papi_component_info_t *ci = &(psi->component_info[i]);
      ci->finalize_event_set();
    }

  // set up overflow handling for asynchronous event sets for active components
  // set up synchronous handling for synchronous event sets for active compoents
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    long thresh = self->evl.events[i].thresh;
    int cidx = PAPI_get_event_component(evcode);
    int eventSet = get_component_event_set(psi, cidx);
    
    // **** No overflow for synchronous events ****
    // **** Use component-specific setup for synchronous events ****
    if (component_uses_sync_samples(cidx)) {
      TMSG(PAPI, "event code %d (component %d) is synchronous, so do NOT set overflow", evcode, cidx);
      TMSG(PAPI, "Set up sync handler instead");
      TMSG(PAPI, "synchronous sample component index = %d", cidx);
      sync_setup_for_component(cidx)();
      continue;
    }
    // ***** Only set overflow if NOT derived event *****
    if (! derived[i]) {
      ret = PAPI_overflow(eventSet, evcode, thresh, OVERFLOW_MODE,
			  papi_event_handler);
      TMSG(PAPI, "PAPI_overflow(eventSet=%d, evcode=%x, thresh=%d) = %d", 
	   eventSet, evcode, thresh, ret);
      if (ret != PAPI_OK) {
	EMSG("failure in PAPI gen_event_set(): PAPI_overflow() returned: %s (%d)",
	     PAPI_strerror(ret), ret);
	event_fatal_error(evcode, ret);
      }
    }
  }
}

static void
METHOD_FN(display_events)
{
  PAPI_event_info_t info;
  int ev, ret, num_total, num_prof;
  int num_components, cidx;

  if (papi_unavail) {
    printf("PAPI is not available.  Probably, the kernel doesn't support PAPI,\n"
	   "or else maybe HPCToolkit is out of sync with PAPI.\n\n");
    return;
  }

  cidx = 0; // CPU component
  {
    const PAPI_component_info_t *component = PAPI_get_component_info(cidx);
    printf("===========================================================================\n");
    printf("Available PAPI preset events in component %s\n", component->name);
    printf("\n");
    printf("Name\t    Profilable\tDescription\n");
    printf("===========================================================================\n");

    num_total = 0;
    num_prof = 0;
    ev = 0| PAPI_PRESET_MASK;
    ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_FIRST, cidx);
    while (ret == PAPI_OK) {
      char *prof;
      memset(&info, 0, sizeof(info));
      if (PAPI_get_event_info(ev, &info) == PAPI_OK && info.count != 0) {
	if (event_is_derived(ev)) {
	  prof = "No";
	} else {
	  prof = "Yes";
	  num_prof++;
	}
	num_total++;
	printf("%-10s\t%s\t%s\n", info.symbol, prof, info.long_descr);
      }
      ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_EVENTS, cidx);
    }
    printf("---------------------------------------------------------------------------\n");
    printf("Total PAPI events: %d, able to profile: %d\n", num_total, num_prof);
    printf("\n\n");
  }

  num_components = PAPI_num_components(); 
  for(cidx = 0; cidx < num_components; cidx++) {
    const PAPI_component_info_t* component = PAPI_get_component_info(cidx);
    int cmp_event_count = 0;

    if (component->disabled) continue;

    printf("===========================================================================\n");
    printf("Native events in component %s\n", component->name);
    printf("\n");
    printf("Name  Description\n");
    printf("===========================================================================\n");
    
    ev = 0 | PAPI_NATIVE_MASK;
    ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_FIRST, cidx);
    while (ret == PAPI_OK) {
      memset(&info, 0, sizeof(info));
      if (PAPI_get_event_info(ev, &info) == PAPI_OK) {
	cmp_event_count++;
	printf("%-48s\n", info.symbol); 
        print_desc(info.long_descr);
        printf("---------------------------------------------------------------------------\n");
      }
      ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_EVENTS, cidx);
    }
    printf("Total native events for component %s: %d\n", component->name, cmp_event_count);
    printf("\n\n");
    num_total += cmp_event_count;
  }

  printf( "Total events reported: %d\n", num_total);
  printf("\n\n");
}


/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name papi
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

// **************************************************************************
// * public operations 
// **************************************************************************
void
hpcrun_disable_papi_cuda(void)
{
  disable_papi_cuda = true;
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

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
papi_event_handler(int event_set, void *pc, long long ovec,
                   void *context)
{
  sample_source_t *self = &obj_name();
  long long values[MAX_EVENTS];
  int my_events[MAX_EVENTS];
  int my_event_count = MAX_EVENTS;
  int nevents  = self->evl.nevents;
  int i, ret;

  int my_event_codes[MAX_EVENTS];
  int my_event_codes_count = MAX_EVENTS;

  // if sampling disabled explicitly for this thread, skip all processing
  if (hpcrun_thread_suppress_sample || sample_filters_apply()) return;

  if (!ovec) {
    TMSG(PAPI_SAMPLE, "papi overflow event: event set %d ovec = %ld",
	 event_set, ovec);
    return;
  }

  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    return;
  }

  int cidx = PAPI_get_eventset_component(event_set);
  thread_data_t *td = hpcrun_get_thread_data();
  papi_source_info_t *psi = td->ss_info[self->sel_idx].ptr;
  papi_component_info_t *ci = &(psi->component_info[cidx]);

  if (ci->some_derived) {
    ret = PAPI_read(event_set, values);
    if (ret != PAPI_OK) {
      EMSG("PAPI_read failed with %s (%d)", PAPI_strerror(ret), ret);
    }
  }

  ret = PAPI_get_overflow_event_index(event_set, ovec, my_events, 
				      &my_event_count);
  if (ret != PAPI_OK) {
    TMSG(PAPI_SAMPLE, "papi_event_handler: event set %d ovec %ld "
	 "get_overflow_event_index return code = %d ==> %s", 
	 event_set, ovec, ret, PAPI_strerror(ret));
#ifdef DEBUG_PAPI_OVERFLOW
    ret = PAPI_list_events(event_set, my_event_codes, &my_event_codes_count);
    if (ret != PAPI_OK) {
      TMSG(PAPI_SAMPLE, "PAPI_list_events failed inside papi_event_handler."
	   "Return code = %d ==> %s", ret, PAPI_strerror(ret));
    } else {
      for (i = 0; i < my_event_codes_count; i++) {
        TMSG(PAPI_SAMPLE, "event set %d event code %d = %x\n", 
	     event_set, i, my_event_codes[i]);
      }
    }
    TMSG(PAPI_SAMPLE, "get_overflow_event_index failure in papi_event_handler");
#endif
  }

  ret = PAPI_list_events(event_set, my_event_codes, &my_event_codes_count);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_list_events failed inside papi_event_handler."
		 "Return code = %d ==> %s", ret, PAPI_strerror(ret));
  }

  for (i = 0; i < my_event_count; i++) {
    // FIXME: SUBTLE ERROR: metric_id may not be same from hpcrun_new_metric()!
    // This means lush's 'time' metric should be *last*

    TMSG(PAPI_SAMPLE,"handling papi overflow event: "
	"event set %d event index = %d event code = 0x%x", 
	event_set, my_events[i], my_event_codes[my_events[i]]);

    int event_index = get_event_index(self, my_event_codes[my_events[i]]);

    int metric_id = hpcrun_event2metric(self, event_index);

    TMSG(PAPI_SAMPLE,"sampling call path for metric_id = %d", metric_id);

    uint64_t metricIncrement;
    if (ci->scale_by_thread_count) {
      float liveThreads = (float) hpcrun_threadmgr_thread_count();
      float myShare = 1.0 / liveThreads;
      metricIncrement = self->evl.events[i].thresh * myShare;
    } else {
      metricIncrement = 1;
    }

    sample_val_t sv = hpcrun_sample_callpath(context, metric_id, metricIncrement,
			   0/*skipInner*/, 0/*isSync*/);

    blame_shift_apply(metric_id, sv.sample_node, 1 /*metricIncr*/);
  }

  // Add metric values for derived events by the difference in counter
  // values.  Some samples can take a long time (e.g., analyzing a new
  // load module), so read the counters both on entry and exit to
  // avoid counting our work.

  if (ci->some_derived) {
    for (i = 0; i < nevents; i++) {
      if (derived[i]) {
	hpcrun_sample_callpath(context, hpcrun_event2metric(self, i),
			       values[i] - ci->prev_values[i], 0, 0);
      }
    }

    ret = PAPI_read(event_set, ci->prev_values);
    if (ret != PAPI_OK) {
      EMSG("PAPI_read failed with %s (%d)", PAPI_strerror(ret), ret);
    }
  }

  hpcrun_safe_exit();
}
