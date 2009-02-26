// -*-Mode: C++;-*- // technically C99
// $Id$

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
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "csprof_options.h"
#include "metrics.h"
#include "pmsg.h"
#include "sample_event.h"
#include "sample_source.h"
#include "sample_source_common.h"
#include "sample_sources_registered.h"
#include "simple_oo.h"
#include "thread_data.h"


/******************************************************************************
 * macros
 *****************************************************************************/

#define CSPROF_PROFILE_SIGNAL SIGPROF
#define CSPROF_PROFILE_TIMER ITIMER_PROF

#ifdef CATAMOUNT
# define CSPROF_PROFILE_SIGNAL SIGALRM
# define CSPROF_PROFILE_TIMER ITIMER_REAL
#endif

#define SECONDS_TO_MICROSECONDS(s) ((s) * 1000000)

#define ITIMER_METRIC_ID 0

#define USE_THREAD_USAGE_FOR_WALLCLOCK
#define RESET_ITIMER_EACH_SAMPLE

//#define BGP

#ifdef BGP
# undef USE_THREAD_USAGE_FOR_WALLCLOCK
# undef RESET_ITIMER_EACH_SAMPLE
#endif

#ifdef RESET_ITIMER_EACH_SAMPLE
# define AUTOMATIC_ITIMER_RESET(x)      0
#else
# define AUTOMATIC_ITIMER_RESET(x)      x
#endif


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context);

static unsigned long long thread_usage_in_microseconds();



/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;

// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for itimer
}

static void
METHOD_FN(_start)
{
  TMSG(ITIMER_CTL,"starting itimer");
  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);

  TD_GET(ss_state)[self->evset_idx] = START;

#ifdef USE_THREAD_USAGE_FOR_WALLCLOCK
  TD_GET(last_us_usage) = thread_usage_in_microseconds();
#endif

  // int rv = setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
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
  TD_GET(ss_state)[self->evset_idx] = STOP;
  // return rc;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}

static int
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"WALLCLOCK") != NULL);
}
 
// (arbitrary) code for WALLCLOCK event
#define WALLCLOCK 0

static void
METHOD_FN(process_event_list,int lush_metrics)
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
       period, seconds, microseconds);

  /* signal once after the given delay */
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  /* macros define whether automatic restart or not */
  itimer.it_interval.tv_sec = AUTOMATIC_ITIMER_RESET(seconds);
  itimer.it_interval.tv_usec = AUTOMATIC_ITIMER_RESET(microseconds);
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int ret = csprof_set_max_metrics(1 + lush_metrics);
  
  if (ret > 0) {
#ifdef USE_THREAD_USAGE_FOR_WALLCLOCK
    long sample_period = 1;
#else
    long sample_period = self->evl.events[0].thresh;
#endif

    int metric_id = csprof_new_metric();
    TMSG(ITIMER_CTL,"setting metric id = %d,period = %ld",metric_id,sample_period);
    csprof_set_metric_info_and_period(metric_id, "WALLCLOCK (us)",
				      HPCFILE_METRIC_FLAG_ASYNC,
				      sample_period);
    
    // FIXME:LUSH: inadequacy compounded by inadequacy of metric
    // interface.  Cf. papi version.
    if (lush_metrics == 1) {
      int lush_metric_id = csprof_new_metric();
      assert(lush_metric_id == 1);
      csprof_set_metric_info_and_period(lush_metric_id, "idleness (ms)",
					HPCFILE_METRIC_FLAG_ASYNC | HPCFILE_METRIC_FLAG_REAL,
					sample_period);
    }
  }
  thread_data_t *td = csprof_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD; // Event sets not relevant for itimer

  monitor_sigaction(CSPROF_PROFILE_SIGNAL, &csprof_itimer_signal_handler, 0, NULL);
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _itimer_obj = {
  // common methods

  .add_event     = csprof_ss_add_event,
  .store_event   = csprof_ss_store_event,
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
  .evset_idx = 0,
  .name = "itimer",
  .state = UNINIT
};


/******************************************************************************
 * constructor 
 *****************************************************************************/

static void itimer_obj_reg(void) __attribute__ ((constructor));

static void
itimer_obj_reg(void)
{
  csprof_ss_register(&_itimer_obj);
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static unsigned long long
thread_usage_in_microseconds()
{
  struct timespec ts;
  long notime = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);

  if(notime != 0) {
    EMSG("thread_usage_in_microseconds: clock_gettime failed!"); 
    abort();
  }

  unsigned long long usage = (ts.tv_nsec/1000) + 
    SECONDS_TO_MICROSECONDS(ts.tv_sec);

  return usage;
}

extern int sampling_is_disabled(void);

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  if (!csprof_handling_synchronous_sample_p()) {
    TMSG(ITIMER_HANDLER,"Itimer sample event");

#ifdef USE_THREAD_USAGE_FOR_WALLCLOCK
    unsigned long long current_us_usage = thread_usage_in_microseconds();
    unsigned long long time_value = current_us_usage - TD_GET(last_us_usage);  /* time in us */
#else
    unsigned long long time_value = 1; /* time in samples */
#endif

    csprof_sample_event(context, ITIMER_METRIC_ID, time_value);
  }
  if (sampling_is_disabled()){
    TMSG(SPECIAL,"No itimer restart, due to disabled sampling");
    return 0;
  }

#ifdef RESET_ITIMER_EACH_SAMPLE
  METHOD_CALL(&_itimer_obj,start);
#endif
    

  return 0; /* tell monitor that the signal has been handled */
}

