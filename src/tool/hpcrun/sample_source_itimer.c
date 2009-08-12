// -*-Mode: C++;-*- // technically C99
// $Id$

//
// itimer sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "csprof_options.h"
#include "metrics.h"
#include "sample_event.h"
#include "sample_source.h"
#include "sample_source_common.h"
#include "sample_sources_registered.h"
#include "simple_oo.h"
#include "thread_data.h"

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <lib/prof-lean/timer.h>


/******************************************************************************
 * macros
 *****************************************************************************/

#if defined(CATAMOUNT)
#   define CSPROF_PROFILE_SIGNAL           SIGALRM
#   define CSPROF_PROFILE_TIMER            ITIMER_REAL
#else
#  define CSPROF_PROFILE_SIGNAL            SIGPROF
#  define CSPROF_PROFILE_TIMER             ITIMER_PROF
#endif

#define SECONDS_PER_HOUR                   3600

#define ITIMER_METRIC_ID 0  /* tallent:FIXME: land mine alert! */

#if !defined(HOST_SYSTEM_IBM_BLUEGENE)
#  define USE_ELAPSED_TIME_FOR_WALLCLOCK
#endif

#define RESET_ITIMER_EACH_SAMPLE

#if defined(RESET_ITIMER_EACH_SAMPLE)

#  if defined(HOST_SYSTEM_IBM_BLUEGENE)
  //--------------------------------------------------------------------------
  // Blue Gene/P compute node support for itimer incorrectly delivers SIGALRM
  // in one-shot mode. To sidestep this problem, we use itimer in 
  // interval mode, but with an interval so long that we never expect to get 
  // a repeat interrupt before resetting it. 
  //--------------------------------------------------------------------------
#    define AUTOMATIC_ITIMER_RESET_SECONDS(x)            (SECONDS_PER_HOUR) 
#    define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)       (0)
#  else  // !defined(HOST_SYSTEM_IBM_BLUEGENE)
#    define AUTOMATIC_ITIMER_RESET_SECONDS(x)            (0) 
#    define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)       (0)
#  endif // !defined(HOST_SYSTEM_IBM_BLUEGENE)

#else  // !defined(RESET_ITIMER_EACH_SAMPLE)

#  define AUTOMATIC_ITIMER_RESET_SECONDS(x)              (x)
#  define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)         (x)

#endif // !defined(RESET_ITIMER_EACH_SAMPLE)



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context);


/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;
static sigset_t sigset_itimer;

// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for itimer
  sigemptyset(&sigset_itimer);
  sigaddset(&sigset_itimer, CSPROF_PROFILE_SIGNAL);
}

static void
METHOD_FN(_start)
{
  if (! csprof_td_avail()){
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }
  TMSG(ITIMER_CTL,"starting itimer");
  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);

  TD_GET(ss_state)[self->evset_idx] = START;

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  int ret = time_getTimeCPU(&TD_GET(last_time_us));
  if (ret != 0) {
    EMSG("time_getTimeCPU (clock_gettime) failed!");
    abort();
  }
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

  char *_p = strchr(METHOD_CALL(self,get_event_str),'@');
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
  itimer.it_interval.tv_sec  =  AUTOMATIC_ITIMER_RESET_SECONDS(seconds);
  itimer.it_interval.tv_usec =  AUTOMATIC_ITIMER_RESET_MICROSECONDS(microseconds);
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int ret = csprof_set_max_metrics(1 + lush_metrics);
  
  if (ret > 0) {
#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
    long sample_period = 1;
#else
    long sample_period = self->evl.events[0].thresh;
#endif

    int metric_id = csprof_new_metric();
    TMSG(ITIMER_CTL,"setting metric id = %d,period = %ld",metric_id,sample_period);
    csprof_set_metric_info_and_period(metric_id, "WALLCLOCK (us)",
				      HPCFILE_METRIC_FLAG_ASYNC,
				      sample_period);
    
    // FIXME:LUSH: need a more flexible metric interface
    if (lush_metrics == 1) {
      int mid_idleness = csprof_new_metric();
      lush_agents->metric_time = metric_id;
      lush_agents->metric_idleness = mid_idleness;

      csprof_set_metric_info_and_period(mid_idleness, "idleness (ms)",
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

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  // Must check for async block first and avoid any MSG if true.
  if (hpcrun_async_is_blocked()) {
    csprof_inc_samples_blocked_async();
  }
  else {
    TMSG(ITIMER_HANDLER,"Itimer sample event");

    uint64_t metric_incr = 1; // default: one time unit
#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
    uint64_t cur_time_us;
    int ret = time_getTimeCPU(&cur_time_us);
    if (ret != 0) {
      EMSG("time_getTimeCPU (clock_gettime) failed!");
      abort();
    }
    metric_incr = cur_time_us - TD_GET(last_time_us);
#endif

    hpcrun_sample_callpath(context, ITIMER_METRIC_ID, metric_incr,
			   0/*skipInner*/, 0/*isSync*/);
  }
  if (sampling_is_disabled()) {
    TMSG(SPECIAL,"No itimer restart, due to disabled sampling");
    return 0;
  }

#ifdef RESET_ITIMER_EACH_SAMPLE
  METHOD_CALL(&_itimer_obj, start);
#endif

#ifdef HOST_SYSTEM_IBM_BLUEGENE
  //
  // On BG/P, siglongjmp() is broken.  If we catch a SEGV,
  // siglongjmp() out of it and return from the itimer handler, then
  // the itimer signal will still be blocked in the application
  // program, thus blocking further samples.  We can work around this
  // problem by resetting the signal mask here.
  //
  monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_itimer, NULL);
#endif

  return 0; /* tell monitor that the signal has been handled */
}

