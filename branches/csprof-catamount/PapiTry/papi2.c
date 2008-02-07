#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <papi.h>

#include "pmsg.h"
#include "tokenize.h"

#define MIN(a,b) ((a)<=(b))?a:b

static long counts[5] = {0,0,0,0,0};

static void
handler(int es, void *pc, long long ovec, void *context)
{
  printf("got papi overflow w ovec = %ld\n",ovec);
  counts[0]++;
}

static char evbuf[1024];
static int evcode;
static long thresh;

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
    *ec = PAPI_TOT_CYC;
    *th = 1000000L;
    return;
  }
  ret = PAPI_query_event(*ec);
  if (ret != PAPI_OK) {
    EMSG("PAPI query event failed: %d",ret);
    *ec = PAPI_TOT_CYC;
    *th = 1000000L;
    return;
  }
  ret = PAPI_get_event_info(*ec, &info);
  if (ret != PAPI_OK) {
    EMSG("PAPI_get_event_info failed :%d",ret);
    *ec = PAPI_TOT_CYC;
    *th = 1000000L;
    return;
  }
}

static int eventSet = PAPI_NULL;

static int
papi_setup(char *ev_list)
{
  int  ecode     = PAPI_TOT_CYC;
  long threshold = 10000000L;

  int ret = PAPI_library_init(PAPI_VER_CURRENT);

  if (ret != PAPI_VER_CURRENT){
    EMSG("Failed: PAPI_library_init: %d",ret);
    abort();
  }

  ret = PAPI_thread_init(pthread_self);
  if(ret != PAPI_OK){
    EMSG("Failed: PAPI_thread_init: %d",ret);
    abort();
  }

  eventSet = PAPI_NULL;
  ret = PAPI_create_eventset(&eventSet);
  if (ret != PAPI_OK){
    EMSG("Failure: PAPI_create_eventset: %d", ret);
    abort();
  }
  printf("event set = %d\n",eventSet);

  char *event = NULL;
  for(event = start_tok(ev_list);more_tok();event = next_tok()){
    printf("checking event spec = %s\n",event);
    extract_ev_thresh(event,&evcode,&thresh);
    printf("got event code = %x, thresh = %ld\n",evcode,thresh);

    printf("compare PAPI_TOT_CYC = %x\n",PAPI_TOT_CYC);
    ret = PAPI_add_event(eventSet, evcode);
    if (ret != PAPI_OK){
      EMSG("Failure: PAPI_add_event: %d", ret);
      abort();
    }

    ret = PAPI_overflow(eventSet, evcode, thresh, PAPI_OVERFLOW_FORCE_SW, handler);
    if (ret != PAPI_OK){
      EMSG("Failure: PAPI_overflow: %d", ret);
      abort();
    }
  }
  return eventSet;
}

extern void tfn(void);

int
main(int argc, char *argv[])
{
  int ev;
  long long values;

  printf("Hello world papi test\n");
  char *tmp = (argc < 2) ? "PAPI_TOT_CYC:1000000" : argv[1];
  ev = papi_setup(tmp);
  PAPI_start(ev);
  tp();
  PAPI_stop(ev,&values);
  printf("final counts = %ld  %ld  %ld  %ld  %ld\n",counts[0],counts[1],counts[2],counts[3],counts[4]);
  return 0;
}
