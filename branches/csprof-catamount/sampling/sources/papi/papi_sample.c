#include <papi.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>

#include "pmsg.h"
#include "sample_event.h"

#include "papi_sample.h"

#define THRESHOLD   10000000

#define WEIGHT_METRIC 0

#define M(s) write(2,s"\n",strlen(s)+1)

extern int status;

void
csprof_papi_event_handler(int EventSet, void *pc, long long ovec,void *context){
  PMSG(PAPI,"papi event happened");
  csprof_sample_event(context);
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

void
papi_event_init(int *eventSet,int eventcode,long threshold)
{
  int ret;
  int threshold = THRESHOLD;

  *eventSet = PAPI_NULL;
  PMSG(PAPI,"INITIAL &eventSet = %p, eventSet value = %d",eventSet, *eventSet);
  ret = PAPI_create_eventset(eventSet);
  PMSG(PAPI,"PAPI_create_eventset = %d, eventSet = %d", ret,*eventSet);
  if (ret != PAPI_OK){
    EMSG("Failure: PAPI_create_eventset: %d", ret);
    abort();
  }

  ret = PAPI_add_event(*eventSet, PAPI_TOT_CYC);
  PMSG(PAPI,"PAPI_add_event = %d", ret);
  if (ret != PAPI_OK){
    EMSG("Failure: PAPI_add_event: %d", ret);
    abort();
  }
  ret = PAPI_overflow(*eventSet, PAPI_TOT_CYC, threshold, 0, csprof_papi_event_handler);
  PMSG(PAPI,"PAPI_overflow = %d", ret);
  if (ret != PAPI_OK){
    EMSG("Failure: PAPI_overflow: %d", ret);
    abort();
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

void
papi_event_info_from_opt(csprof_options_t *opts,int *code,
                         long *thresh)
{
  
}

