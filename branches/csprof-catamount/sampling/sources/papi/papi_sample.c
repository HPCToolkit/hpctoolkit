#include <assert.h>
#include <ctype.h>
#include <papi.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>

#include "papi_sample.h"

#include "pmsg.h"
#include "sample_event.h"
#include "metrics.h"

#include "tokenize.h"


// #define OVERFLOW_MODE PAPI_OVERFLOW_FORCE_SW
#define OVERFLOW_MODE 0

#define THRESHOLD   10000000
#define MIN(a,b) ((a)<=(b))?a:b


#define WEIGHT_METRIC 0

#define M(s) write(2,s"\n",strlen(s)+1)

extern int status;


#define MAX_CSPROF_PAPI_EVENTS 16

typedef struct {
  int events[MAX_CSPROF_PAPI_EVENTS];
  long thresh[MAX_CSPROF_PAPI_EVENTS];
  int nevents;
} csprof_papi_events_t;

static csprof_papi_events_t csprof_papi_events;

#define NO_EVENT_INDEX -1

static void
csprof_papi_events_add_event(int event, int thresh)
{
  assert(csprof_papi_events.nevents < MAX_CSPROF_PAPI_EVENTS-1);
  csprof_papi_events.events[csprof_papi_events.nevents] = event;
  csprof_papi_events.thresh[csprof_papi_events.nevents++] = thresh;
}

static int
csprof_papi_events_initialized()
{
  return csprof_papi_events.nevents > 0;
}

#if 0
static int 
csprof_get_event_index(int event)
{
  int i;
  for (i = 0; i < csprof_papi_events.nevents; i++) {
    if (event == csprof_papi_events.events[i]) return i;
  }
  return NO_EVENT_INDEX;
}
#endif

void
csprof_papi_event_handler(int event_set, void *pc, long long ovec,
			  void *context)
{
  int i;
  int my_events[MAX_CSPROF_PAPI_EVENTS];
  int my_event_count = MAX_CSPROF_PAPI_EVENTS;

  PMSG(PAPI,"papi event happened, ovec = %ld",ovec);

  int retval = 
    PAPI_get_overflow_event_index(event_set, ovec, my_events, 
				  &my_event_count);

  assert(retval == PAPI_OK);

  for(i = 0; i < my_event_count; i++) {
#if 0
    int metric_id = csprof_get_event_index(my_events[i]);
    assert(metric_id != NO_EVENT_INDEX);
#endif
    int metric_id = my_events[i];
    csprof_sample_event(context, metric_id);
  }
}

// static int eventSet = PAPI_NULL;

void
papi_setup(void){

  PAPI_set_debug(0x3ff);

  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  PMSG(PAPI,"PAPI_library_init = %d", ret);
  PMSG(PAPI,"PAPI_VER_CURRENT =  %d", PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT){
    EMSG("Failed: PAPI_library_init\n");
    abort();
  }

  ret = PAPI_thread_init(pthread_self);
  PMSG(PAPI,"PAPI_thread_init = %d",ret);
  if(ret != PAPI_OK){
    EMSG("Failed: PAPI_thread_init");
    abort();
  }
}

static char evbuf[1024];

#define DEFAULT_THRESHOLD 1000000L
static void
extract_ev_thresh(char *in,int *ec,long *th)
{
  unsigned int len;
  PAPI_event_info_t info;

  char *dlm = strrchr(in,':');
  if (dlm) {
    if (isdigit(dlm[1])) { // assume this is the threshold
      len = MIN(dlm-in,sizeof(evbuf));
      strncpy(evbuf,in,len);
      evbuf[len] = '\0';
    }
    else {
      dlm = NULL;
    }
  }
  if (!dlm) {
    strncpy(evbuf,in,len);
    evbuf[len] = '\0';
  }
  *th = dlm ? strtol(dlm+1,(char **)NULL,10) : DEFAULT_THRESHOLD;
  int ret = PAPI_event_name_to_code(evbuf, ec);
  if (ret != PAPI_OK) {
    EMSG("event name to code failed with name = %s",evbuf);
    EMSG("event_code_to_name failed: %d",ret);
    abort();
#if 0
    *ec = PAPI_TOT_CYC;
    *th = 1000000L;
    return;
#endif
  }
  ret = PAPI_query_event(*ec);
  if (ret != PAPI_OK) {
    EMSG("PAPI query event failed: %d",ret);
    abort();
#if 0
    *ec = PAPI_TOT_CYC;
    *th = 1000000L;
    return;
#endif
  }
  ret = PAPI_get_event_info(*ec, &info);
  if (ret != PAPI_OK) {
    EMSG("PAPI_get_event_info failed :%d",ret);
    abort();
#if 0
    *ec = PAPI_TOT_CYC;
    *th = 1000000L;
    return;
#endif
  }
}

void
papi_parse_evlist(char *evlist)
{
  char *event;
  int i;

  for(event = start_tok(evlist); more_tok(); event = next_tok()){
    int evcode;
    long thresh;

    PMSG(PAPI,"checking event spec = %s",event);
    extract_ev_thresh(event,&evcode,&thresh);
    PMSG(PAPI,"got event code = %x, thresh = %ld",evcode,thresh);
    // PMSG(PAPI,"compare PAPI_TOT_CYC = %x",PAPI_TOT_CYC);
    csprof_papi_events_add_event(evcode, thresh);
  }

  csprof_set_max_metrics(csprof_papi_events.nevents);
  for(i=0; i < csprof_papi_events.nevents; i++){
    char buffer[PAPI_MAX_STR_LEN];
    int metric_id = csprof_new_metric(); /* weight */
    PAPI_event_code_to_name(csprof_papi_events.events[i], buffer);
    csprof_set_metric_info_and_period(metric_id, strdup(buffer),
				      CSPROF_METRIC_ASYNCHRONOUS,
				      csprof_papi_events.thresh[i]);
  }
}

int 
papi_event_init(void) //int *eventSet,char *evlist)
{
  int i;
  int ret;
  // long thresh = THRESHOLD;
  // int evcode  = PAPI_TOT_CYC;
  int eventSet;

  // int initialized = csprof_papi_events_initialized();

  eventSet = PAPI_NULL;
  // PMSG(PAPI,"INITIAL &eventSet = %p, eventSet value = %d",eventSet, *eventSet);
  PMSG(PAPI,"create event set");
  ret = PAPI_create_eventset(&eventSet);
  PMSG(PAPI,"PAPI_create_eventset = %d, eventSet = %d", ret,eventSet);
  if (ret != PAPI_OK){
    EMSG("Failure: PAPI_create_eventset: %d", ret);
    abort();
  }
  for (i=0; i < csprof_papi_events.nevents; i++){
    int evcode = csprof_papi_events.events[i];

    ret = PAPI_add_event(eventSet, evcode);
    if (ret != PAPI_OK){
      EMSG("Failure: PAPI_add_event: %d", ret);
      abort();
    }
  }
  for(i=0;i < csprof_papi_events.nevents;i++){
    int evcode  = csprof_papi_events.events[i];
    long thresh = csprof_papi_events.thresh[i];

    ret = PAPI_overflow(eventSet, evcode, thresh, OVERFLOW_MODE,
			csprof_papi_event_handler);

    PMSG(PAPI,"PAPI_overflow = %d", ret);
    if (ret != PAPI_OK){
      EMSG("Failure: PAPI_overflow: %d", ret);
      abort();
    }
  }
  return eventSet;
}
#if 0
  metric_id = csprof_new_metric(); /* calls */
  csprof_set_metric_info_and_period(metric_id, "# returns",
                                    CSPROF_METRIC_FLAGS_NIL, 1);
#endif
  
void
papi_pulse_init(int eventSet)
{
  PMSG(PAPI,"starting PAPI w event set %d",eventSet);
  int ret = PAPI_start(eventSet);
  if (ret != PAPI_OK){
    EMSG("Failed to start papi f eventset %, ret = %d",eventSet,ret);
    abort();
  }
}

void
papi_pulse_fini(int eventSet)
{
  long_long values;

  return;
  int ret = PAPI_stop(eventSet, &values);
  if (ret != PAPI_OK){
    EMSG("Failed to stop papi f eventset %, ret = %d",eventSet,ret);
    abort();
  }
  PMSG(PAPI,"values = %lld\n", values);
}
