// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample_source_itimer.c $
// $Id: sample_source_itimer.c 2600 2009-10-11 22:22:41Z johnmc $
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

//
// itimer sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <errno.h>
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
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>

#include <hpcrun/metrics.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include "sample_source_obj.h"
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <lib/prof-lean/timer.h>
#include <unwind/common/unwind.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#if defined(CATAMOUNT)
#   define HPCRUN_PROFILE_SIGNAL           SIGALRM
#   define HPCRUN_PROFILE_TIMER            ITIMER_REAL
#else
#  define HPCRUN_PROFILE_SIGNAL            SIGPROF
#  define HPCRUN_PROFILE_TIMER             ITIMER_PROF
#endif

#define SECONDS_PER_HOUR                   3600

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
 * local constants
 *****************************************************************************/

enum _local_const {
  ITIMER_EVENT = 0    // itimer has only 1 event
};

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
itimer_signal_handler(int sig, siginfo_t *siginfo, void *context);


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
}

static void
METHOD_FN(start)
{
  if (! hpcrun_td_avail()){
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }
  TMSG(ITIMER_CTL,"starting itimer");
  if (setitimer(HPCRUN_PROFILE_TIMER, &itimer, NULL) != 0) {
    EMSG("setitimer failed (%d): %s", errno, strerror(errno));
    hpcrun_ssfail_start("itimer");
  }

  TD_GET(ss_state)[self->evset_idx] = START;

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  int ret = time_getTimeCPU(&TD_GET(last_time_us));
  if (ret != 0) {
    EMSG("time_getTimeCPU (clock_gettime) failed!");
    abort();
  }
#endif
}

static void
METHOD_FN(stop)
{
  struct itimerval itimer;
  int rc;

  timerclear(&itimer.it_value);
  timerclear(&itimer.it_interval);
  rc = setitimer(HPCRUN_PROFILE_TIMER, &itimer, NULL);
  TMSG(ITIMER_CTL,"stopping itimer");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self, stop); // make sure stop has been called
  self->state = UNINIT;
}

static int
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"WALLCLOCK") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  long period = 5000L;

  char *_p = strchr(METHOD_CALL(self,get_event_str),'@');
  if ( _p) {
    period = strtol(_p+1,NULL,10);
  }
  else {
    TMSG(OPTIONS,"WALLCLOCK event default period (5000) selected");
  }
  
  METHOD_CALL(self, store_event, ITIMER_EVENT, period);
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

  hpcrun_pre_allocate_metrics(1 + lush_metrics);
  
#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  long sample_period = 1;
#else
  long sample_period = self->evl.events[0].thresh;
#endif

  int metric_id = hpcrun_new_metric();
  METHOD_CALL(self, store_metric_id, ITIMER_EVENT, metric_id);
  TMSG(ITIMER_CTL, "setting metric ITIMER,period = %ld", sample_period);
  hpcrun_set_metric_info_and_period(metric_id, "WALLCLOCK (us)",
				    HPCRUN_MetricFlag_Async,
				    sample_period);
  if (lush_metrics == 1) {
    int mid_idleness = hpcrun_new_metric();
    lush_agents->metric_time = metric_id;
    lush_agents->metric_idleness = mid_idleness;

    hpcrun_set_metric_info_and_period(mid_idleness, "idleness (ms)",
				      HPCRUN_MetricFlag_Async | HPCRUN_MetricFlag_Real,
				      sample_period);
  }
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// Event "sets" not possible for this sample source.
// It has only 1 event.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  sigemptyset(&sigset_itimer);
  sigaddset(&sigset_itimer, HPCRUN_PROFILE_SIGNAL);
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &itimer_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available itimer events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("WALLCLOCK\tWall clock time used by the process in microseconds\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name itimer
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
itimer_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  // Must check for async block first and avoid any MSG if true.
  void* pc = context_pc(context);
  if (hpcrun_async_is_blocked(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
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

    int metric_id = hpcrun_event2metric(&_itimer_obj, ITIMER_EVENT);
    hpcrun_sample_callpath(context, metric_id, metric_incr,
			   0/*skipInner*/, 0/*isSync*/);
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(SPECIAL, "No itimer restart, due to disabled sampling");
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
