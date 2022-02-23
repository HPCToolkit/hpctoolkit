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
#include "display.h"
#include "papi-c-extended-info.h"
#include "sample-filters.h"

#include <hpcrun/main.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/gpu-monitors.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/threadmgr.h>
#include <hpcrun/trace.h>

#include <sample-sources/blame-shift/blame-shift.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include "papi-c.h"
#include "tool_state.h"


/******************************************************************************
 * macros
 *****************************************************************************/

#define DEBUG 0

#include <hpcrun/gpu/gpu-print.h>
#include <gpu-monitors.h>

#define OVERFLOW_MODE 0
#define WEIGHT_METRIC 0
#define DEFAULT_THRESHOLD  2000000L


/******************************************************************************
 * forward declarations 
 *****************************************************************************/
static void papi_event_handler(int event_set, void *pc, long long ovec, void *context);
static void papi_monitor_enter(papi_component_info_t *ci, cct_node_t *cct_node);
static void papi_monitor_exit(papi_component_info_t *ci);

static int  event_is_derived(int ev_code);
static void event_fatal_error(int ev_code, int papi_ret);

/******************************************************************************
 * local variables
 *****************************************************************************/

// Support for derived events (proxy sampling).
static int derived[MAX_EVENTS];
static int some_overflow;


// Special case to make PAPI_library_init() a soft failure.
// Make sure that we call no other PAPI functions.
//
static int papi_unavail = 0;

//
// To accomodate GPU blame shifting, we must disable the cuda component
// Flag below controls this disabling
//
static bool disable_papi_cuda = false;

static kind_info_t *papi_kind;

static int hpcrun_cycles_metric_id = -1;
static uint64_t hpcrun_cycles_cmd_period = 0;


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


static int
evcode_to_component_id(papi_source_info_t* psi, int evcode)
{
  int cidx = PAPI_get_event_component(evcode);
  if (cidx < 0 || cidx >= psi->num_components) {
    hpcrun_abort("PAPI component index out of range [0,%d]: %d", psi->num_components, cidx);
  }
  return cidx;
}


//
// fetch a given component's event set. Create one if need be
//
int
get_component_event_set(papi_component_info_t* ci)
{
   if (!ci->inUse) {
     ci->get_event_set(&(ci->eventSet));
     ci->inUse = true;
  }
  return ci->eventSet;
}


//
// add an event to a component's event set
//
void
component_add_event(papi_source_info_t* psi, int evcode)
{
  int cidx = evcode_to_component_id(psi, evcode);
  papi_component_info_t* ci = &(psi->component_info[cidx]);
  int event_set = get_component_event_set(ci);

  ci->add_event(event_set, evcode);
  ci->some_derived |= event_is_derived(evcode);

  TMSG(PAPI, "Added event code %x to component %d", evcode, cidx);
  {
    char buffer[PAPI_MAX_STR_LEN];
    PAPI_event_code_to_name(evcode, buffer);
    TMSG(PAPI,
         "PAPI_add_event(eventSet=%%d, event_code=%x (event name %s)) component=%d",
    /* eventSet, */ evcode, buffer, cidx);
  }
}


static void
papi_register_events(papi_source_info_t *psi, evlist_t evl)
{
  int i;
  int nevents = evl.nevents;

  // add events to new event_sets
  for (i = 0; i < nevents; i++) {
    int evcode = evl.events[i].event;
    component_add_event(psi, evcode);

  }

  // finalize component event sets
  for (i = 0; i < psi->num_components; i++) {
    papi_component_info_t *ci = &(psi->component_info[i]);
    ci->finalize_event_set();
  }
}


static void
papi_register_sync_callback(papi_component_info_t *ci)
{
  gpu_monitor_node_t node;
  node.ci = ci;
  node.enter_fn = papi_monitor_enter;
  node.exit_fn = papi_monitor_exit;
  gpu_monitor_register(node);
}


static void
papi_register_overflow_callback(int eventSet, int evcode, long thresh)
{
  TMSG(PAPI, "PAPI_overflow(eventSet=%d, evcode=%x, thresh=%d) register",
       eventSet, evcode, thresh);

  int ret = PAPI_overflow(eventSet, evcode, thresh, OVERFLOW_MODE, papi_event_handler);
  if (ret != PAPI_OK) {
    EMSG("failure in PAPI gen_event_set(): PAPI_overflow() returned: %s (%d)",
         PAPI_strerror(ret), ret);
    event_fatal_error(evcode, ret);
  }
}


static void
papi_register_callbacks(papi_source_info_t *psi, evlist_t evl)
{
  int i;
  // set up overflow handling for asynchronous event sets for active components
  // set up synchronous handling for synchronous event sets for active compoents
  for (i = 0; i < evl.nevents; i++) {

    int evcode = evl.events[i].event;
    long thresh = evl.events[i].thresh;
    int cidx = evcode_to_component_id(psi, evcode);
    papi_component_info_t *ci = &(psi->component_info[cidx]);
    int eventSet = get_component_event_set(ci);

    // **** No overflow for synchronous events ****
    if (ci->is_gpu_sync) {
      TMSG(PAPI, "event code %d (component %d) is synchronous, so do NOT set overflow", evcode, cidx);
      TMSG(PAPI, "Set up papi_monitor_apply instead");
      TMSG(PAPI, "synchronous sample component index = %d", cidx);

      papi_register_sync_callback(ci);
    }
    else{
      if (! derived[i]) { // ***** Only set overflow if NOT derived event *****
        papi_register_overflow_callback(eventSet, evcode, thresh);
      }
    }
  }

}


static bool
thread_count_scaling_for_component(int cidx)
{
  const PAPI_component_info_t *pci = PAPI_get_component_info(cidx);
  if (strcmp(pci->name, "bgpm/L2Unit") == 0) return true;
  return 0;
}


/******************************************************************************
 * method functions
 *****************************************************************************/

// strip the prefix "papi::" from an event name, if exists.
// this allows forcing a papi event over a perf event.
// allow case-insensitive and any number of ':'
static const char *
strip_papi_prefix(const char *str)
{
  if (strncasecmp(str, "papi:", 5) == 0) {
    str = &str[5];

    while (str[0] == ':') {
      str = &str[1];
    }
  }

  return str;
}


static void
METHOD_FN(init)
{
  tool_enter();
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
  tool_exit();
}

static void
METHOD_FN(thread_init)
{
  tool_enter();
  TMSG(PAPI, "thread init");
  if (papi_unavail) { goto finish; }

  int retval = PAPI_thread_init(pthread_self);
  if (retval != PAPI_OK) {
    EEMSG("PAPI_thread_init NOT ok, retval = %d", retval);
    monitor_real_abort();
  }
  TMSG(PAPI, "thread init OK");

finish:
  tool_exit();
}

static void
METHOD_FN(thread_init_action)
{
  tool_enter();
  TMSG(PAPI, "register thread");
  if (papi_unavail) { goto finish; }

  int retval = PAPI_register_thread();
  if (retval != PAPI_OK) {
    EEMSG("PAPI_register_thread NOT ok, retval = %d", retval);
    monitor_real_abort();
  }
  TMSG(PAPI, "register thread ok");

finish:
  tool_exit();
}

static void
METHOD_FN(start)
{
  tool_enter();
  int cidx;
  TMSG(PAPI, "start");

  if (papi_unavail) {
    goto finish;
  }

  thread_data_t* td = hpcrun_get_thread_data();
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  // make PAPI start idempotent.  the application can turn on sampling
  // anywhere via the start-stop interface, so we can't control what
  // state PAPI is in.

  if (my_state == START) {
    TMSG(PAPI,"*NOTE* PAPI start called when already in state START");
    goto finish;
  }

  // for each active component, start its event set
  papi_source_info_t* psi = td->ss_info[self->sel_idx].ptr;
  for (cidx=0; cidx < psi->num_components; cidx++) {
    papi_component_info_t* ci = &(psi->component_info[cidx]);
    if (ci->inUse) {
      if (component_uses_sync_samples(cidx)) {
  TMSG(PAPI, "component %d is synchronous, use synchronous start", cidx);
  ci->start();
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

finish:
  tool_exit();
}

static void
METHOD_FN(thread_fini_action)
{
  tool_enter();
  TMSG(PAPI, "unregister thread");
  if (papi_unavail) { goto finish; }

  int retval = PAPI_unregister_thread();
  char msg[] = "!!NOT PAPI_OK!! (code = -9999999)\n";
  snprintf(msg, sizeof(msg)-1, "!!NOT PAPI_OK!! (code = %d)", retval);
  TMSG(PAPI, "unregister thread returns %s", retval == PAPI_OK? "PAPI_OK" : msg);
finish:
  tool_exit();
}


static void
METHOD_FN(stop)
{
  tool_enter();

  int cidx;

  TMSG(PAPI, "stop");
  if (papi_unavail) { goto finish; }

  thread_data_t *td = hpcrun_get_thread_data();
  int nevents = self->evl.nevents;
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  if (my_state == STOP) {
    TMSG(PAPI,"*NOTE* PAPI stop called when already in state STOP");
    goto finish;
  }

  if (my_state != START) {
    TMSG(PAPI,"*WARNING* PAPI stop called when not in state START");
    goto finish;
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
  //  long_long *values = (long_long *) alloca(sizeof(long_long) * (nevents+2));

    int ret = PAPI_stop(ci->eventSet, values);
    if (ret != PAPI_OK) {
      EMSG("Failed to stop PAPI for eventset %d. Return code = %d ==> %s",
           ci->eventSet, ret, PAPI_strerror(ret));
    }

      }
    }
  }

  TD_GET(ss_state)[self->sel_idx] = STOP;
finish:
  tool_exit();
}


static void
METHOD_FN(shutdown)
{
  tool_enter();
  TMSG(PAPI, "shutdown");
  if (papi_unavail) { goto finish; }

  do{
    METHOD_CALL(self, stop); // make sure stop has been called
  }while(0);
  // FIXME: add component shutdown code here

  PAPI_shutdown();

  self->state = UNINIT;
finish:
  tool_exit();
}

// Return true if PAPI recognizes the name, whether supported or not.
// We'll handle unsupported events later.
static bool
METHOD_FN(supports_event, const char *ev_str)
{
  tool_enter();
  bool ret;
  ev_str = strip_papi_prefix(ev_str);

  TMSG(PAPI, "supports event");
  if (papi_unavail) { ret = false; goto finish;}

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }

  char evtmp[1024];
  int ec;
  long th;

  hpcrun_extract_ev_thresh(ev_str, sizeof(evtmp), evtmp, &th, DEFAULT_THRESHOLD);

  // corner case: check if it isn't a misspelling event
  if (is_event_to_exclude(evtmp)) {
    return false;
  }
    
  ret = (PAPI_event_name_to_code(evtmp, &ec) == PAPI_OK);

finish:
  tool_exit();
  return ret;
}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  tool_enter();
  TMSG(PAPI, "process event list");
  if (papi_unavail) { goto finish; }

  char *event;
  int i, ret;
  int num_lush_metrics = 0;

  papi_kind = hpcrun_metrics_new_kind();

  char* evlist = METHOD_CALL(self, get_event_str);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char name[1024];
    int evcode;
    long thresh;

    event = (char *) strip_papi_prefix(event);

    TMSG(PAPI,"checking event spec = %s",event);
    // FIXME: restore checking will require deciding if the event is synchronous or not
#ifdef USE_PAPI_CHECKING
    int period_type = hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD);
    if (!period_type) {
      AMSG("WARNING: %s using default threshold %ld, "
     "better to use an explicit threshold.", name, DEFAULT_THRESHOLD);
    }
#else
    int period_type = hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD);
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

    if (strncmp(event, "PAPI_TOT_CYC", 12) == 0) {
      if (period_type == THRESH_FREQ) {
        // frequency is specified in samples per second
        hpcrun_cycles_cmd_period = (uint64_t) (1000000000.0 / thresh);
      } else {
        // default is period.
        // period is specified in the number of cycles per sample
        hpcrun_cycles_cmd_period = (uint64_t) (1000000000.0 * thresh / HPCRUN_CPU_FREQUENCY);
      }
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
    metric_desc_properties_t prop = metric_property_none;
    PAPI_event_code_to_name(self->evl.events[i].event, buffer);
    TMSG(PAPI, "metric for event %d = %s", i, buffer);

    int isCycles = 0;
    // blame shifting needs to know if there is a cycles metric
    if (strcmp(buffer, "PAPI_TOT_CYC") == 0) {
      prop = metric_property_cycles;
      blame_shift_source_register(bs_type_cycles);
      isCycles = 1;
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

    int metric_id = /* weight */
      hpcrun_set_new_metric_info_and_period(papi_kind, strdup(buffer),
              MetricFlags_ValFmt_Int,
              threshold, prop);
    METHOD_CALL(self, store_metric_id, i, metric_id);
    if (isCycles) {
      hpcrun_cycles_metric_id = metric_id;
      hpcrun_set_trace_metric(HPCRUN_CPU_TRACE_FLAG);
    }

    // FIXME:LUSH: need a more flexible metric interface
    if (num_lush_metrics > 0 && strcmp(buffer, "PAPI_TOT_CYC") == 0) {
      // there should be one lush metric; its source is the last event
      int mid_idleness =
  hpcrun_set_new_metric_info_and_period(papi_kind, "idleness",
                MetricFlags_ValFmt_Real,
                self->evl.events[i].thresh, prop);
      assert(num_lush_metrics == 1 && (i == (nevents - 1)));
      lush_agents->metric_time = metric_id;
      lush_agents->metric_idleness = mid_idleness;
    }
  }

  hpcrun_close_kind(papi_kind);

  if (! some_overflow) {
    hpcrun_ssfail_all_derived("PAPI");
  }

finish:
  tool_exit();
}

static void
METHOD_FN(finalize_event_list)
{
}

static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  tool_enter();
  thread_data_t *td = hpcrun_get_thread_data();
  int i;

  TMSG(PAPI, "generating all event sets for all components");
  if (papi_unavail) { goto finish; }

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
    ci->name = component_get_name(i);
    ci->inUse = false;
    ci->eventSet = PAPI_NULL;
    ci->state = INIT;
    ci->some_derived = 0;
    ci->get_event_set = component_get_event_set(i);
    ci->add_event = component_add_event_proc(i);
    ci->finalize_event_set = component_finalize_event_set(i);
    ci->scale_by_thread_count = thread_count_scaling_for_component(i);
    ci->is_gpu_sync = component_uses_sync_samples(i);
    ci->setup = sync_setup_for_component(i);
    ci->teardown = sync_teardown_for_component(i);
    ci->start = sync_start_for_component(i);
    ci->read = sync_read_for_component(i);
    ci->stop = sync_stop_for_component(i);
    memset(ci->prev_values, 0, sizeof(ci->prev_values));
  }

  // record the component state in thread state
  td->ss_info[self->sel_idx].ptr = psi;

  papi_register_events(psi, self->evl);

  papi_register_callbacks(psi, self->evl);

finish:
  tool_exit();
}

static void
METHOD_FN(display_events)
{
  tool_enter();
  PAPI_event_info_t info;
  int ev, ret, num_total, num_prof;
  int num_components, cidx;

  if (papi_unavail) {
    PRINT("PAPI is not available.  Probably, the kernel doesn't support PAPI,\n"
     "or else maybe HPCToolkit is out of sync with PAPI.\n\n");
    goto finish;
  }

  cidx = 0; // CPU component
  {
    const PAPI_component_info_t *component = PAPI_get_component_info(cidx);
    PRINT("===========================================================================\n");
    PRINT("Available PAPI preset events in component %s\n", component->name);
    PRINT("\n");
    PRINT("Name\t    Profilable\tDescription\n");
    PRINT("===========================================================================\n");

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
  PRINT("%-10s\t%s\t%s\n", info.symbol, prof, info.long_descr);
      }
      ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_EVENTS, cidx);
    }
    PRINT("---------------------------------------------------------------------------\n");
    PRINT("Total PAPI events: %d, able to profile: %d\n", num_total, num_prof);
    PRINT("\n\n");
  }

  num_components = PAPI_num_components();
  for(cidx = 0; cidx < num_components; cidx++) {
    const PAPI_component_info_t* component = PAPI_get_component_info(cidx);
    int cmp_event_count = 0;

    if (component->disabled) continue;

    PRINT("===========================================================================\n");
    PRINT("Native events in component %s\n", component->name);
    PRINT("\n");
    PRINT("Name  Description\n");
    PRINT("===========================================================================\n");

    ev = 0 | PAPI_NATIVE_MASK;
    ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_FIRST, cidx);
    while (ret == PAPI_OK) {
      memset(&info, 0, sizeof(info));
      if (PAPI_get_event_info(ev, &info) == PAPI_OK) {
  cmp_event_count++;
        display_event_info(stdout, info.symbol, info.long_descr);
        PRINT("---------------------------------------------------------------------------\n");
      }
      ret = PAPI_enum_cmp_event(&ev, PAPI_ENUM_EVENTS, cidx);
    }
    PRINT("Total native events for component %s: %d\n", component->name, cmp_event_count);
    PRINT("\n\n");
    num_total += cmp_event_count;
  }

  PRINT( "Total events reported: %d\n", num_total);
  PRINT("\n\n");
finish:
  tool_exit();
}


/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name papi
#define ss_cls SS_HARDWARE
#define ss_sort_order  70

#include "ss_obj.h"

// **************************************************************************
// * public operations
// **************************************************************************
void
hpcrun_disable_papi_cuda(void)
{
  tool_enter();
  disable_papi_cuda = true;
  tool_exit();
}

/******************************************************************************
 * private operations
 *****************************************************************************/

// Returns: 1 if the event code is a derived event.
// The papi_avail(1) utility shows how to do this.
static int
event_is_derived(int ev_code)
{
  tool_enter();
  int ret;
  PAPI_event_info_t info;

  // "Is derived" is kind of a bad thing, so if any unexpected failure
  // occurs, we'll return the "bad" answer.
  if (PAPI_get_event_info(ev_code, &info) != PAPI_OK
      || info.derived == NULL) {
    ret = 1;
    goto finish;
  }
  if (info.count == 1
      || strlen(info.derived) == 0
      || strcmp(info.derived, "NOT_DERIVED") == 0
      || strcmp(info.derived, "DERIVED_CMPD") == 0) {
    ret = 0;
    goto finish;
  }
  ret = 1;

finish:
  tool_exit();
  return ret;
}

static void
event_fatal_error(int ev_code, int papi_ret)
{
  tool_enter();
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

  tool_exit();
}

static void
papi_event_handler(int event_set, void *pc, long long ovec,
                   void *context)
{
  tool_enter();
  sample_source_t *self = &obj_name();
  long long values[MAX_EVENTS];
  int my_events[MAX_EVENTS];
  int my_events_number = MAX_EVENTS;
  int nevents  = self->evl.nevents;
  int i, ret;

  int my_events_code[MAX_EVENTS];
  int my_events_code_count = MAX_EVENTS;

  // if sampling disabled explicitly for this thread, skip all processing
  if (hpcrun_suppress_sample() || sample_filters_apply()) goto finish;

  if (!ovec) {
    TMSG(PAPI_SAMPLE, "papi overflow event: event set %d ovec = %ld",
   event_set, ovec);
    goto finish;
  }

  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    goto finish;
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
              &my_events_number);
  if (ret != PAPI_OK) {
    TMSG(PAPI_SAMPLE, "papi_event_handler: event set %d ovec %ld "
   "get_overflow_event_index return code = %d ==> %s",
   event_set, ovec, ret, PAPI_strerror(ret));
#ifdef DEBUG_PAPI_OVERFLOW
    ret = PAPI_list_events(event_set, my_events_code, &my_events_code_count);
    if (ret != PAPI_OK) {
      TMSG(PAPI_SAMPLE, "PAPI_list_events failed inside papi_event_handler."
     "Return code = %d ==> %s", ret, PAPI_strerror(ret));
    } else {
      for (i = 0; i < my_events_code_count; i++) {
        TMSG(PAPI_SAMPLE, "event set %d event code %d = %x\n",
       event_set, i, my_events_code[i]);
      }
    }
    TMSG(PAPI_SAMPLE, "get_overflow_event_index failure in papi_event_handler");
#endif
  }

  ret = PAPI_list_events(event_set, my_events_code, &my_events_code_count);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_list_events failed inside papi_event_handler."
     "Return code = %d ==> %s", ret, PAPI_strerror(ret));
  }

  for (i = 0; i < my_events_number; i++) {
    // FIXME: SUBTLE ERROR: metric_id may not be same from hpcrun_new_metric()!
    // This means lush's 'time' metric should be *last*

    TMSG(PAPI_SAMPLE,"handling papi overflow event: "
  "event set %d event index = %d event code = 0x%x",
  event_set, my_events[i], my_events_code[my_events[i]]);

    int event_index = get_event_index(self, my_events_code[my_events[i]]);

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

    int time_based_metric = (hpcrun_cycles_metric_id == metric_id) ? 1 : 0;

    sampling_info_t info = {
      .sample_clock = 0,
      .sample_data = NULL,
      .sampling_period = hpcrun_cycles_cmd_period,
      .is_time_based_metric = time_based_metric
    };
    sample_val_t sv = hpcrun_sample_callpath(context, metric_id, 
			(hpcrun_metricVal_t) {.i=metricIncrement},
			0/*skipInner*/, 0/*isSync*/, &info);

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
      (hpcrun_metricVal_t) {.i=values[i] - ci->prev_values[i]},
      0, 0, NULL);
      }
    }

    ret = PAPI_read(event_set, ci->prev_values);
    if (ret != PAPI_OK) {
      EMSG("PAPI_read failed with %s (%d)", PAPI_strerror(ret), ret);
    }
  }

finish:
  tool_exit();
  hpcrun_safe_exit();
}


static void
attribute_metric_to_cct
(
 int metric_id,
 cct_node_t *cct_node,
 long long value
)
{
  metric_data_list_t* metrics = hpcrun_reify_metric_set(cct_node, metric_id);

  hpcrun_metric_std_inc(metric_id,
                        metrics,
                        (cct_metric_data_t) {.i = value});
}


static void
attribute_counters(papi_component_info_t *ci, long long *collected_values, cct_node_t *cct_node)
{
  sample_source_t *self = &obj_name();
  int events_codes[MAX_EVENTS];
  int my_events_number = MAX_EVENTS;
  int ret;

  // Attribute collected metric to cct nodes
  ret = PAPI_list_events(ci->eventSet, events_codes, &my_events_number);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_list_events failed inside papi_event_handler."
                 "Return code = %d ==> %s", ret, PAPI_strerror(ret));
  }

  for (int eid = 0; eid < my_events_number; ++eid) {
    int event_index = get_event_index(self, events_codes[eid]);
    int metric_id = hpcrun_event2metric(self, event_index);
    long long int final_counts = collected_values[eid] - ci->prev_values[eid];


    blame_shift_apply(metric_id, cct_node, final_counts/*metricIncr*/);
    attribute_metric_to_cct(metric_id, cct_node, final_counts);

    PRINT("PAPI_EXIT:: %d Event = %x, event_index = %d, metric_id = %d || value = %lld - %lld == %lld\n",
          eid, events_codes[eid], event_index, metric_id,
          collected_values[eid], ci->prev_values[eid],
          final_counts);
  }
}


static void
papi_monitor_enter(papi_component_info_t *ci, cct_node_t *cct_node)
{
  tool_enter();
//  PRINT("|------->PAPI_MONITOR_ENTER | cct = %p\n", cct_node);

  // if sampling disabled explicitly for this thread, skip all processing
  if (hpcrun_suppress_sample() || sample_filters_apply()) goto finish;

  ci->cct_node = cct_node;

  // Save counts on the end so we could substract that from next call (we don't want to measure ourselves)

  if (ci->inUse) {
    ci->read(ci->prev_values);

    PRINT("PAPI_ENTER:: Component %s Event = %d, value = %lld   |  %p\n", ci->name, ci->eventSet, ci->prev_values[0], cct_node);
  }

finish:
  tool_exit();
}


static void
papi_monitor_exit(papi_component_info_t *ci)
{
  tool_enter();
  long long collected_values[MAX_EVENTS];

  // if sampling disabled explicitly for this thread, skip all processing
  if (hpcrun_suppress_sample() || sample_filters_apply()) goto finish;

  if (ci->inUse){
    ci->read(collected_values);
    attribute_counters(ci, collected_values, ci->cct_node);
  }


finish:
  tool_exit();
}
