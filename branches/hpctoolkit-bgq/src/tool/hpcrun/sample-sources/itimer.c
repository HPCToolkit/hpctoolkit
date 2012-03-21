// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>

#include <sample-sources/blame-shift.h>

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

#define DEFAULT_PERIOD  5000L

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

static const struct itimerval zerotimer = {
  .it_interval = {
    .tv_sec  = 0L,
    .tv_usec = 0L
  },

  .it_value    = {
    .tv_sec  = 0L,
    .tv_usec = 0L
  }

};

static long period = DEFAULT_PERIOD;

static sigset_t sigset_itimer;

// ******* METHOD DEFINITIONS ***********
static void
METHOD_FN(init)
{
  TMSG(ITIMER_CTL, "setting up itimer interrupt");
  sigemptyset(&sigset_itimer);
  sigaddset(&sigset_itimer, HPCRUN_PROFILE_SIGNAL);

  int ret = monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_itimer, NULL);

  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d",ret);
  }
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(ITIMER_CTL, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(ITIMER_CTL, "thread action (noop)");
}

// Factor out the body of the start method so we can restart itimer
// without messages in the case that we are interrupting our own code.
// safe = 1 if not inside our code, so ok to print debug messages
//
static void
hpcrun_restart_itimer(sample_source_t *self, int safe)
{
  int ret;

  // if thread data is unavailable, assume that all sample source ops
  // are suspended.
  if (! hpcrun_td_avail()) {
    if (safe) {
      TMSG(ITIMER_CTL, "Thread data unavailable ==> sampling suspended");
    }
    return;
  }

  if (safe) {
    TMSG(ITIMER_CTL,"starting itimer: value = (%d,%d), interval = (%d,%d)",
	 itimer.it_value.tv_sec,
	 itimer.it_value.tv_usec,
	 itimer.it_interval.tv_sec,
	 itimer.it_interval.tv_usec);
  }

  ret = setitimer(HPCRUN_PROFILE_TIMER, &itimer, NULL);
  if (ret != 0) {
    if (safe) {
      TMSG(ITIMER_CTL, "setitimer failed to start!!");
      EMSG("setitimer failed (%d): %s", errno, strerror(errno));
    }
    hpcrun_ssfail_start("itimer");
  }

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  ret = time_getTimeReal(&TD_GET(last_time_us));
  if (ret != 0) {
    if (safe) {
      EMSG("time_getTimeReal (clock_gettime) failed!");
    }
    monitor_real_abort();
  }
#endif

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(start)
{
  hpcrun_restart_itimer(self, 1);
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(ITIMER_CTL, "thread action (noop)");
}

static void
METHOD_FN(stop)
{
  int rc;

  rc = setitimer(HPCRUN_PROFILE_TIMER, &zerotimer, NULL);

  TMSG(ITIMER_CTL,"stopping itimer, ret = %d", rc);
  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(ITIMER_CTL, "shutodown itimer");
  METHOD_CALL(self, stop); // make sure stop has been called

  int ret = monitor_real_sigprocmask(SIG_BLOCK, &sigset_itimer, NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGPROF, ret = %d",ret);
  }

  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"WALLCLOCK") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{

  TMSG(ITIMER_CTL, "process event list, lush_metrics = %d", lush_metrics);
  // fetch the event string for the sample source
  char* _p = METHOD_CALL(self, get_event_str);
  
  //
  // EVENT: Only 1 wallclock event
  //
  char* event = start_tok(_p);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(ITIMER_CTL,"checking event spec = %s",event);

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);

  // store event threshold
  METHOD_CALL(self, store_event, ITIMER_EVENT, period);
  TMSG(OPTIONS,"wallclock period set to %ld",period);

  // set up file local variables for sample source control
  int seconds = period / 1000000;
  int microseconds = period % 1000000;

  TMSG(OPTIONS,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
       period, seconds, microseconds);

  // signal once after the given delay
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  // macros define whether automatic restart or not
  itimer.it_interval.tv_sec  =  AUTOMATIC_ITIMER_RESET_SECONDS(seconds);
  itimer.it_interval.tv_usec =  AUTOMATIC_ITIMER_RESET_MICROSECONDS(microseconds);

  // handle metric allocation
  hpcrun_pre_allocate_metrics(1 + lush_metrics);
  
  int metric_id = hpcrun_new_metric();
  METHOD_CALL(self, store_metric_id, ITIMER_EVENT, metric_id);

  // set metric information in metric table

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
# define sample_period 1
#else
# define sample_period period
#endif

  TMSG(ITIMER_CTL, "setting metric itimer period = %ld", sample_period);
  hpcrun_set_metric_info_and_period(metric_id, "WALLCLOCK (us)",
				    MetricFlags_ValFmt_Int,
				    sample_period);
  if (lush_metrics == 1) {
    int mid_idleness = hpcrun_new_metric();
    lush_agents->metric_time = metric_id;
    lush_agents->metric_idleness = mid_idleness;

    hpcrun_set_metric_info_and_period(mid_idleness, "idleness (us)",
				      MetricFlags_ValFmt_Real,
				      sample_period);
  }

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE WALLCLOCK events detected! Using first event spec: %s");
  }
  // 
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// There is only 1 event for itimer, hence the event "set" is always the same.
// The signal setup, however, is done here.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
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
  sample_source_t *self = &_itimer_obj;

  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  void* pc = hpcrun_context_pc(context);
  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    if (! hpcrun_is_sampling_disabled()) {
      hpcrun_restart_itimer(self, 0);
    }
    // tell monitor the signal has been handled.
    return 0;
  }

  TMSG(ITIMER_HANDLER,"Itimer sample event");

  uint64_t metric_incr = 1; // default: one time unit

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  uint64_t cur_time_us = 0;
  int ret = time_getTimeReal(&cur_time_us);
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    monitor_real_abort();
  }
  metric_incr = cur_time_us - TD_GET(last_time_us);
#endif

  int metric_id = hpcrun_event2metric(self, ITIMER_EVENT);
  cct_node_t *node = hpcrun_sample_callpath(context, metric_id, metric_incr,
					    0/*skipInner*/, 0/*isSync*/);
  blame_shift_apply(node, metric_incr * sample_period);

  if (hpcrun_is_sampling_disabled()) {
    TMSG(SPECIAL, "No itimer restart, due to disabled sampling");
  }
  else {
#ifdef RESET_ITIMER_EACH_SAMPLE
    hpcrun_restart_itimer(self, 1);
#endif
  }

  hpcrun_safe_exit();

  return 0; /* tell monitor that the signal has been handled */
}
