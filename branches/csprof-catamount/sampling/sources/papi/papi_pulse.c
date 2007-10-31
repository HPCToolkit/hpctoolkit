#include <papi.h>
#include <setjmp.h>
#include <unistd.h>
#include <ucontext.h>

#include "bad_unwind.h"
#include "driver.h"
#include "general.h"
#include "state.h"
#include "structs.h"

#include "backtrace.h"
#include "bad_unwind.h"
#include "last.h"

#define THRESHOLD   10000000

#define WEIGHT_METRIC 0

#define M(s) write(2,s"\n",strlen(s)+1)

int samples_taken = 0;
int bad_unwind_count    = 0;
void *unwind_pc;

void
csprof_take_profile_sample(csprof_state_t *state,void *context,void *pc){

  samples_taken++;
  MSG(1,"csprof take profile sample");
  DBGMSG_PUB(1, "Signalled at %#lx", pc);

  csprof_sample_callstack(state, WEIGHT_METRIC, 1, context);
  csprof_state_flag_clear(state,
			  CSPROF_TAIL_CALL | CSPROF_EPILOGUE_RA_RELOADED | CSPROF_EPILOGUE_SP_RESET);
}

extern int status;

void csprof_papi_event_handler(int EventSet, void *pc, long long ovec, 
			       void *context) 
{
  csprof_set_last_sample_addr((unsigned long) pc);
  csprof_increment_raw_sample_count();
  // FIXME: arrange to skip the unwind when variable is set
#if 0
  return;
#endif
  {
    _jb *it = get_bad_unwind();
    MSG(1,"In PAPI handler");
    if (!sigsetjmp(it->jb,1)){
      if(status != CSPROF_STATUS_FINI){

	csprof_state_t *state = csprof_get_state();

	if(state != NULL) {
	  csprof_take_profile_sample(state,(void *)(&(((ucontext_t *)context)->uc_mcontext)),pc);
	  csprof_state_flag_clear(state, CSPROF_THRU_TRAMP);
	}

      }
    }
    else {
      bad_unwind_count++;
      MSG(1,"got bad unwind");
    }
  }
}

static int eventSet = PAPI_NULL;

void papi_pulse_init(void){

    int ret;
    int threshold = THRESHOLD;

    PAPI_set_debug(0x3ff);

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
    ret = PAPI_overflow(eventSet, PAPI_TOT_CYC, threshold, 0, csprof_papi_event_handler);
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


#include "metrics.h"

extern void unw_init(void);

extern int setup_segv(void);
// extern double __ieee754_log();
void csprof_process_driver_init(csprof_options_t *opts){
    unw_init();
    // csprof_addr_to_interval((unsigned long)__ieee754_log + 5);
    setup_segv();
    {
      int metric_id;

      csprof_set_max_metrics(2);
      metric_id = csprof_new_metric(); /* weight */
      csprof_set_metric_info_and_period(metric_id, "# samples",
					CSPROF_METRIC_ASYNCHRONOUS,
					opts->sample_period);
      metric_id = csprof_new_metric(); /* calls */
      csprof_set_metric_info_and_period(metric_id, "# returns",
					CSPROF_METRIC_FLAGS_NIL, 1);
    }
    // FIXME: papi specific for now
    papi_pulse_init();
}

void csprof_process_driver_fini(void){
  papi_pulse_fini();
}
