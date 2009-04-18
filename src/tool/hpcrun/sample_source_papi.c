// -*-Mode: C++;-*- // technically C99
// $Id$

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
#include <pthread.h>
#include <stdbool.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include "monitor.h"


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "csprof_options.h"
#include "hpcfile_csprof.h"
#include "metrics.h"
#include "pmsg.h"
#include "sample_source_common.h"
#include "sample_sources_registered.h"
#include "sample_event.h"
#include "sample_source.h"
#include "simple_oo.h"
#include "thread_data.h"
#include "tokenize.h"

#include "lush/lush-backtrace.h"


/******************************************************************************
 * macros
 *****************************************************************************/


#define OVERFLOW_MODE 0

#define THRESHOLD   10000000
#ifdef MIN
#  undef MIN
#endif
#define MIN(a,b) ((a)<=(b))?a:b


#define WEIGHT_METRIC 0

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void papi_event_handler(int event_set, void *pc, long long ovec, void *context);
static void extract_and_check_event(char *in,int *ec,long *th);
static void extract_ev_thresh(const char *in,int evlen,char *ev,long *th);
static bool event_name_to_code(char *evname,int *ec);

/******************************************************************************
 * local variables
 *****************************************************************************/

static void
METHOD_FN(init)
{
  PAPI_set_debug(0x3ff);

  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  TMSG(PAPI,"PAPI_library_init = %d", ret);
  TMSG(PAPI,"PAPI_VER_CURRENT =  %d", PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT){
    csprof_abort("Failed: PAPI_library_init. Looking for version %d, got version %d",PAPI_VER_CURRENT,ret);
  }
  self->state = INIT;
}

static void
METHOD_FN(_start)
{
  thread_data_t *td = csprof_get_thread_data();
  int eventSet = td->eventSet[self->evset_idx];

  TMSG(PAPI,"starting PAPI w event set %d",eventSet);
  int ret = PAPI_start(eventSet);
  if (ret != PAPI_OK){
    csprof_abort("Failed to start papi f eventset %d. Return code = %d ==> %s",eventSet,ret,
		 PAPI_strerror(ret));
  }

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(stop)
{
  thread_data_t *td = csprof_get_thread_data();

  int eventSet = td->eventSet[self->evset_idx];
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
    EMSG("Failed to stop papi f eventset %d. Return code = %d ==> %s",eventSet,ret,PAPI_strerror(ret));
  }

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  PAPI_shutdown();

  self->state = UNINIT;
}

static int
METHOD_FN(supports_event,const char *ev_str)
{
  if (self->state == UNINIT){
    METHOD_CALL(self,init);
  }
  
  char evtmp[1024];
  int ec;
  long th;

  extract_ev_thresh(ev_str,sizeof(evtmp),evtmp,&th);
  return event_name_to_code(evtmp,&ec);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  char *event;
  int i;

  int num_lush_metrics = 0;

  char *evlist = self->evl.evl_spec;
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    int evcode;
    long thresh;

    TMSG(PAPI,"checking event spec = %s",event);
    extract_and_check_event(event,&evcode,&thresh);
    
    // FIXME:LUSH: need a more flexible metric interface
    if (lush_metrics == 1 && strncmp(event, "PAPI_TOT_CYC", 12) == 0) {
      num_lush_metrics++;
    }

    TMSG(PAPI,"got event code = %x, thresh = %ld",evcode,thresh);
    METHOD_CALL(self,store_event,evcode,thresh);
  }
  int nevents = (self->evl).nevents;
  TMSG(PAPI,"nevents = %d", nevents);

  csprof_set_max_metrics(nevents + num_lush_metrics);

  for (i = 0; i < nevents; i++) {
    char buffer[PAPI_MAX_STR_LEN];
    int metric_id = csprof_new_metric(); /* weight */
    PAPI_event_code_to_name(self->evl.events[i].event, buffer);
    TMSG(PAPI, "metric for event %d = %s", i, buffer);
    csprof_set_metric_info_and_period(metric_id, strdup(buffer),
				      HPCFILE_METRIC_FLAG_ASYNC,
				      self->evl.events[i].thresh);

    // FIXME:LUSH: need a more flexible metric interface
    if (num_lush_metrics > 0 && strcmp(buffer, "PAPI_TOT_CYC") == 0) {
      // there should be one lush metric; its source is the last event
      assert(num_lush_metrics == 1 && (i == (nevents - 1)));
      int mid_idleness = csprof_new_metric();
      lush_agents->metric_time = metric_id;
      lush_agents->metric_idleness = mid_idleness;

      csprof_set_metric_info_and_period(mid_idleness, "idleness",
					HPCFILE_METRIC_FLAG_ASYNC | HPCFILE_METRIC_FLAG_REAL,
					self->evl.events[i].thresh);
    }
  }
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int i;
  int ret;
  int eventSet;

  eventSet = PAPI_NULL;
  TMSG(PAPI,"create event set");
  ret = PAPI_create_eventset(&eventSet);
  PMSG(PAPI,"PAPI_create_eventset = %d, eventSet = %d", ret, eventSet);
  if (ret != PAPI_OK) {
    csprof_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s", 
		 ret, PAPI_strerror(ret));
  }

  int nevents = (self->evl).nevents;
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    ret = PAPI_add_event(eventSet, evcode);
    if (ret != PAPI_OK) {
      char nm[256];
      PAPI_event_code_to_name(evcode,nm);

      csprof_abort("Failure: PAPI_add_event:, trying to add event %s, got ret code = %d ==> %s",
		   nm, ret, PAPI_strerror(ret));
    }
  }
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    long thresh = self->evl.events[i].thresh;

    ret = PAPI_overflow(eventSet, evcode, thresh, OVERFLOW_MODE,
			papi_event_handler);
    TMSG(PAPI,"PAPI_overflow = %d", ret);
    if (ret != PAPI_OK) {
      csprof_abort("Failure: PAPI_overflow.Return code = %d ==> %s", 
		   ret, PAPI_strerror(ret));
    }
  }
  thread_data_t *td = csprof_get_thread_data();
  td->eventSet[self->evset_idx] = eventSet;
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _papi_obj = {
  // common methods

  .add_event   = csprof_ss_add_event,
  .store_event = csprof_ss_store_event,
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
  .evset_idx = 1,
  .name = "papi",
  .state = UNINIT
};

/******************************************************************************
 * constructor 
 *****************************************************************************/
static void papi_obj_reg(void) __attribute__ ((constructor));

static void
papi_obj_reg(void)
{
  csprof_ss_register(&_papi_obj);
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

#define DEFAULT_THRESHOLD 1000000L

static void
extract_ev_thresh(const char *in,int evlen,char *ev,long *th)
{
  unsigned int len;

  char *dlm = strrchr(in,'@');
  if (dlm) {
    if (isdigit(dlm[1])){ // assume this is the threshold
      len = MIN(dlm-in,evlen);
      strncpy(ev,in,len);
      ev[len] = '\0';
    }
    else {
      dlm = NULL;
    }
  }
  if (!dlm) {
    len = strlen(in);
    strncpy(ev,in,len);
    ev[len] = '\0';
  }
  *th = dlm ? strtol(dlm+1,(char **)NULL,10) : DEFAULT_THRESHOLD;
}

// convert papi event name to code
// NOTE: return status is true if succeeded, false otherwise

static bool
event_name_to_code(char *evname,int *ec)
{
  PAPI_event_info_t info;

  int ret = PAPI_event_name_to_code(evname, ec);
  if (ret != PAPI_OK) {
    TMSG(PAPI_EVENT_NAME,"event name to code failed with name = %s",evname);
    TMSG(PAPI_EVENT_NAME,"event_code_to_name failed: %d",ret);
    TMSG(PAPI_EVENT_NAME,"PAPI_event_name_to_code fails:, errcode = %s",PAPI_strerror(ret));
    return false;
  }
  ret = PAPI_query_event(*ec);
  if (ret != PAPI_OK) {
    TMSG(PAPI_EVENT_NAME,"PAPI query event failed: %d",ret);
    TMSG(PAPI_EVENT_NAME,"PAPI_query_event fails:, errcode = %s",PAPI_strerror(ret));
    return false;
  }
  ret = PAPI_get_event_info(*ec, &info);
  if (ret != PAPI_OK) {
    TMSG(PAPI_EVENT_NAME,"PAPI_get_event_info failed :%d",ret);
    TMSG(PAPI_EVENT_NAME,"PAPI_get_event_info fails:, errcode = %s",PAPI_strerror(ret));
    return false;
  }
  return true;
}

static void
extract_and_check_event(char *in,int *ec,long *th)
{
  char evbuf[1024];

  extract_ev_thresh(in,sizeof(evbuf),evbuf,th);
  if (! event_name_to_code(evbuf,ec)){
    csprof_abort("Strange PAPI failure. Previously validated event spec %s now fails to produce a valid event code.",in);
  }
}

static void
papi_event_handler(int event_set, void *pc, long long ovec,
                   void *context)
{
  int i;
  int my_events[MAX_EVENTS];
  int my_event_count = MAX_EVENTS;

  // Must check for async block first and avoid any MSG if true.
  if (csprof_async_is_blocked()) {
    csprof_inc_samples_blocked_async();
    return;
  }

  TMSG(PAPI_SAMPLE,"papi event happened, ovec = %ld",ovec);

  int ret = PAPI_get_overflow_event_index(event_set, ovec, my_events, 
					  &my_event_count);
  if (ret != PAPI_OK) {
    csprof_abort("Failed inside papi_event_handler at get_overflow_event_index."
		 "Return code = %d ==> %s", ret, PAPI_strerror(ret));
  }
  for (i = 0; i < my_event_count; i++) {
    // FIXME: SUBTLE ERROR: metric_id may not be same from csprof_new_metric()!
    // This means lush's 'time' metric should be *last*
    int metric_id = my_events[i];
    csprof_sample_event(context, metric_id, 
			1 /*metric_units_consumed*/, 0 /*is_sync*/);
  }
}
