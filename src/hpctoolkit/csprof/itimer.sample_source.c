//
// itimer sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */
#include <assert.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

/*---------------------------
 * monitor
 *--------------------------*/
#include "monitor.h"

/*---------------------------
 * csprof
 *--------------------------*/
#include "common.sample_source.h"
#include "csprof_options.h"
#include "metrics.h"
#include "pmsg.h"
#include "registered_sample_sources.h"
#include "sample_event.h"
#include "sample_source.h"
#include "simple_oo.h"


/******************************************************************************
 * macros
 *****************************************************************************/

#define CSPROF_PROFILE_SIGNAL SIGPROF
#define CSPROF_PROFILE_TIMER ITIMER_PROF

#ifdef CATAMOUNT
#define CSPROF_PROFILE_SIGNAL SIGALRM
#define CSPROF_PROFILE_TIMER ITIMER_REAL
#endif


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context);



/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;

static unsigned long sample_period = 0L;
static unsigned long seconds = 0L;
static unsigned long microseconds = 0L;

// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  ; // no init necessary for itimer
}

static void
METHOD_FN(start)
{
  TMSG(ITIMER_CTL,"starting itimer");
  int rv = setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  // return rv
}

static void
METHOD_FN(stop)
{
  struct itimerval itimer;
  int rc;

  timerclear(&itimer.it_value);
  timerclear(&itimer.it_interval);
  rc = setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  TMSG(ITIMER_CTL,"stopping itimer");
  // return rc;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
}

static int
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"WALLCLOCK") != NULL);
}
 
// (arbitrary) code for WALLCLOCK event
#define WALLCLOCK 0

static void
METHOD_FN(process_event_list)
{
  long period = 5000L;

  char *_p = strchr(METHOD_CALL(self,get_event_str),':');
  if ( _p) {
    period = strtol(_p+1,NULL,10);
  }
  else {
    TMSG(OPTIONS,"WALLCLOCK event default period (5000) selected");
  }
  
  METHOD_CALL(self,store_event,WALLCLOCK,period);
  TMSG(OPTIONS,"wallclock period set to %ld",period);

  int seconds = period / 1000000;
  int microseconds = period % 1000000;

  TMSG(OPTIONS,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
       sample_period, seconds, microseconds);

  /* signal once after the given delay */
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  /* no automatic restart */
  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 0;

}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int ret = csprof_set_max_metrics(1 + lush_metrics);
  
  if (ret > 0) {
    int metric_id = csprof_new_metric(); 
    csprof_set_metric_info_and_period(metric_id, "WALLCLOCK (ms)",
				      HPCFILE_METRIC_FLAG_ASYNC,
				      sample_period);
    
    if (lush_metrics == 1) {
      // FIXME: ASSUMES itimer!
      //   1. This only makes sense for time-like metrics (must check metrics)
      //   2. Should we have one for each time-like metric?
      //   3. Better located elsewhere?
      int lush_metric_id = csprof_new_metric();
      assert(lush_metric_id == 1);
      csprof_set_metric_info_and_period(lush_metric_id, "P non-work (ms)",
					HPCFILE_METRIC_FLAG_ASYNC | HPCFILE_METRIC_FLAG_REAL,
					sample_period);
    }
  }

  monitor_sigaction(CSPROF_PROFILE_SIGNAL, &csprof_itimer_signal_handler, 0, NULL);
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _itimer_obj = {
  // common methods

  .add_event   = csprof_ss_add_event,
  .store_event = csprof_ss_store_event,
  .get_event_str = csprof_ss_get_event_str,

  // specific methods

  .init = init,
  .start = start,
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
  .evset_idx = 0,
  .name = "itimer"
};

/******************************************************************************
 * constructor 
 *****************************************************************************/
static void itimer_ss_register(void) __attribute__ ((constructor));

static void
itimer_obj_reg(void)
{
  csprof_ss_register(&_itimer_obj);
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

#define ITIMER_METRIC_ID 0
static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  TMSG(ITIMER_HANDLER,"Itimer sample event");
  csprof_sample_event(context, ITIMER_METRIC_ID, 1 /*sample_count*/);
  METHOD_CALL(&_itimer_obj,start);

  return 0; /* tell monitor that the signal has been handled */
}
