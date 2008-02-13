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

#include "tokenize.h"
#include "metrics_types.h"

#define THRESHOLD   10000000
#define MIN(a,b) ((a)<=(b))?a:b


#define M(s) write(2,s"\n",strlen(s)+1)

extern int status;

void
csprof_papi_event_handler(int EventSet, void *pc, long long ovec,void *context){
  PMSG(PAPI,"papi event happened, ovec = %ld",ovec);
  csprof_sample_event(context, WEIGHT_METRIC, 1);
}

static int eventSet = PAPI_NULL;

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
papi_event_init(int *eventSet,char *evlist)
{
  int ret;
  long thresh = THRESHOLD;
  int evcode  = PAPI_TOT_CYC;
  char *event;

  *eventSet = PAPI_NULL;
  PMSG(PAPI,"INITIAL &eventSet = %p, eventSet value = %d",eventSet, *eventSet);
  ret = PAPI_create_eventset(eventSet);
  PMSG(PAPI,"PAPI_create_eventset = %d, eventSet = %d", ret,*eventSet);
  if (ret != PAPI_OK){
    EMSG("Failure: PAPI_create_eventset: %d", ret);
    abort();
  }
  for(event = start_tok(evlist);more_tok();event = next_tok()){
    PMSG(PAPI,"checking event spec = %s",event);
    extract_ev_thresh(event,&evcode,&thresh);
    PMSG(PAPI,"got event code = %x, thresh = %ld",evcode,thresh);
    PMSG(PAPI,"compare PAPI_TOT_CYC = %x",PAPI_TOT_CYC);

    ret = PAPI_add_event(*eventSet, evcode);
    if (ret != PAPI_OK){
      EMSG("Failure: PAPI_add_event: %d", ret);
      abort();
    }
    ret = PAPI_overflow(*eventSet, evcode, thresh, PAPI_OVERFLOW_FORCE_SW, csprof_papi_event_handler);
    PMSG(PAPI,"PAPI_overflow = %d", ret);
    if (ret != PAPI_OK){
      EMSG("Failure: PAPI_overflow: %d", ret);
      abort();
    }
  }
}

void
papi_pulse_init(int eventSet)
{
  PMSG(PAPI,"starting PAPI w event set %d",eventSet);
  PAPI_start(eventSet);
}

void
papi_pulse_fini(void){
  long long values;

  PAPI_stop(eventSet, &values);
  PMSG(PAPI,"values = %lld\n", values);
}
