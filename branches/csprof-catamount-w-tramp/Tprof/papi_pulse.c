#include <papi.h>
#include <unistd.h>
#include "structs.h"
#include "general.h"

#define M(s) write(2,s"\n",strlen(s)+1)

extern void t_sample_callstack(void *context);

void my_handler(int EventSet, void *pc, long long ovec, void *context)
{
  // M("In PAPI handler");
  t_sample_callstack(context);
}

static int eventSet = PAPI_NULL;

#define THRESHOLD   10000000

void papi_pulse_init(void){

    int ret;
    int threshold = THRESHOLD;

    ret = PAPI_library_init(PAPI_VER_CURRENT);
    MSG(1,"PAPI_library_init = %d\n", ret);
    MSG(1,"PAPI_VER_CURRENT =  %d\n", PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT){
      MSG(1, "Failed: PAPI_library_init\n");
      exit(1);
    }
    ret = PAPI_create_eventset(&eventSet);
    MSG(1,"PAPI_create_eventset = %d\n", ret);
    if (ret != PAPI_OK){
      MSG(1, "Failure: PAPI_create_eventset: %d", ret);
      exit(1);
    }

    ret = PAPI_add_event(eventSet, PAPI_TOT_CYC);
    MSG(1,"PAPI_add_event = %d\n", ret);
    if (ret != PAPI_OK){
      MSG(1, "Failure: PAPI_add_event: %d", ret);
      exit(1);
    }
    ret = PAPI_overflow(eventSet, PAPI_TOT_CYC, threshold, 0, my_handler);
    MSG(1,"PAPI_overflow = %d\n", ret);
    if (ret != PAPI_OK){
      MSG(1, "Failure: PAPI_overflow: %d", ret);
      exit(1);
    }
    PAPI_start(eventSet);
}

void papi_pulse_fini(void){
    long long values;

    PAPI_stop(eventSet, &values);
    MSG(1,"values = %lld\n", values);
}
