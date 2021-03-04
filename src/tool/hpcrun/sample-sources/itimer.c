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
// Copyright ((c)) 2002-2021, Rice University
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
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */
#include <time.h>
#include <unistd.h>

#include <include/hpctoolkit-config.h>
#ifdef ENABLE_CLOCK_REALTIME
#include <sys/syscall.h>
#endif


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "sample_source_obj.h"
#include "common.h"
#include "sample-filters.h"
#include "itimer.h"
#include "ss-errno.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>

#include <hpcrun/main.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/ompt/ompt-region.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>

#include <sample-sources/blame-shift/blame-shift.h>



/******************************************************************************
 * macros
 *****************************************************************************/

// Note: it's not clear if CLOCK_THREAD_CPUTIME_ID or
// CLOCK_PROCESS_CPUTIME_ID is a better clock for CPUTIME.  The docs
// would say THREAD, but experiments suggest that PROCESS plus
// SIGEV_THREAD_ID is also thread-specific and it seems that THREAD is
// limited by the kernel Hz rate.

#define IDLE_METRIC_NAME     "idleness (sec)"

#define REALTIME_EVENT_NAME   "REALTIME"
#define REALTIME_METRIC_NAME  "REALTIME (sec)"
#define REALTIME_SIGNAL       (SIGRTMIN + 3)

#define REALTIME_CLOCK_TYPE     CLOCK_REALTIME
#define REALTIME_NOTIFY_METHOD  SIGEV_THREAD_ID

#define CPUTIME_EVENT_NAME    "CPUTIME"
#define CPUTIME_METRIC_NAME   "CPUTIME (sec)"
#define CPUTIME_CLOCK_TYPE     CLOCK_THREAD_CPUTIME_ID

// the man pages cite sigev_notify_thread_id in struct sigevent,
// but often the only name is a hidden union name.
#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id  _sigev_un._tid
#endif

#define sample_period 1

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

static bool use_realtime = false;
static bool use_cputime = false;

static char *the_event_name = "unknown";
static char *the_metric_name = "unknown";
static int   the_signal_num = 0;

static long period = DEFAULT_PERIOD;

static struct itimerval itval_start;
static struct itimerval itval_stop;

static struct itimerspec itspec_start;
static struct itimerspec itspec_stop;

static sigset_t timer_mask;

static __thread bool wallclock_ok = false;

// ****************************************************************************
// * public helper function
// ****************************************************************************

void hpcrun_itimer_wallclock_ok(bool flag)
{
  wallclock_ok = flag;
}

/******************************************************************************
 * internal helper functions
 *****************************************************************************/

// Helper functions return the result of the syscall, so 0 on success,
// or else -1 on failure.  We handle errors in the caller.

static int
hpcrun_create_real_timer(thread_data_t *td)
{
  int ret = 0;

#ifdef ENABLE_CLOCK_REALTIME
  if (! td->timer_init) {
    memset(&td->sigev, 0, sizeof(td->sigev));
    td->sigev.sigev_notify = REALTIME_NOTIFY_METHOD;
    td->sigev.sigev_signo = REALTIME_SIGNAL;
    td->sigev.sigev_value.sival_ptr = &td->timerid;
    td->sigev.sigev_notify_thread_id = syscall(SYS_gettid);

    clockid_t clock = REALTIME_CLOCK_TYPE;
#ifdef ENABLE_CLOCK_CPUTIME
    if (use_cputime) {
      clock = CPUTIME_CLOCK_TYPE;
    }
#endif
    ret = timer_create(clock, &td->sigev, &td->timerid);
    if (ret == 0) {
      td->timer_init = true;
    }
  }
#endif

  return ret;
}

static int
hpcrun_delete_real_timer(thread_data_t *td)
{
  int ret = 0;

#ifdef ENABLE_CLOCK_REALTIME
  if (td->timer_init) {
    ret = timer_delete(td->timerid);
    td->timerid = NULL;
  }
  td->timer_init = false;
#endif

  return ret;
}

// While handling a sample, the shutdown signal handler may asynchronously 
// delete a thread's timer and set it to NULL. Calling timer_settime on a 
// deleted timer will return an error. Calling timer_settime on a NULL
// timer will SEGV. For that reason, calling timer_settime(td->timerid, ...) 
// is unsafe as td->timerid can be set to NULL immediately before it is 
// loaded as an argument.  To avoid a SEGV, copy the timer into a local 
// variable, test it, and only call timer_settime with a non-NULL timer. 
static int
hpcrun_settime(thread_data_t *td, struct itimerspec *spec)
{
  if (!td->timer_init) return -1; // fail: timer not initialized

  timer_t mytimer = td->timerid;
  if (mytimer == NULL) return -1; // fail: timer deleted

  return timer_settime(mytimer, 0, spec, NULL);
}

static int
hpcrun_start_timer(thread_data_t *td)
{
#ifdef ENABLE_CLOCK_REALTIME
  if (use_realtime || use_cputime) {
    return hpcrun_settime(td, &itspec_start);
  }
#else
  EEMSG("start_timer: neither clock nor realtime Linux timer requested.\n");
#endif
  return -1;
}

static int
hpcrun_stop_timer(thread_data_t *td)
{
#ifdef ENABLE_CLOCK_REALTIME
  if (use_realtime || use_cputime) {
    int ret = hpcrun_settime(td, &itspec_stop);

    // If timer is invalid, it is not active; treat it as stopped
    // and ignore any error code that may have been returned by
    // hpcrun_settime
    if ((!td->timer_init) || (!td->timerid)) return 0;

    return ret;
  }
#else
  EEMSG("stop_itimer: neither clock nor realtime Linux timer requested.\n");
#endif
  return -1;
}

// Factor out the body of the start method so we can restart itimer
// without messages in the case that we are interrupting our own code.
// safe = 1 if not inside our code, so ok to print debug messages
//
static void
hpcrun_restart_timer(sample_source_t *self, int safe)
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
  thread_data_t *td = hpcrun_get_thread_data();

  if (safe) {
    TMSG(ITIMER_HANDLER, "starting %s: value = (%d,%d), interval = (%d,%d)",
	 the_event_name,
	 itval_start.it_value.tv_sec,
	 itval_start.it_value.tv_usec,
	 itval_start.it_interval.tv_sec,
	 itval_start.it_interval.tv_usec);
  }

  ret = hpcrun_start_timer(td);

  if (use_realtime || use_cputime) {
    if ((!td->timer_init) || (!td->timerid)) {
      // When multiple threads are present, a thread might receive a shutdown
      // signal while handling a sample. When a shutdown signal is received,
      // the thread's timer is deleted and set to uninitialized.
      // In this circumstance, restarting the timer will fail and that
      // is appropriate. Return without futher action or reporting an error.
      return;
    }
  }

  if (ret != 0) {
    if (safe) {
      TMSG(ITIMER_CTL, "setitimer failed to start!!");
      EMSG("setitimer failed (%d): %s", errno, strerror(errno));
    }
    hpcrun_ssfail_start("itimer");
  }

  // get the current value of the appropriate clock for this thread
  if (use_cputime) {
    ret = time_getTimeCPU(&TD_GET(last_time_us));
  } else {
    ret = time_getTimeReal(&TD_GET(last_time_us));
  }

  if (ret != 0) {
    EMSG("%s clock_gettime failed!", (use_cputime ? "time_getTimeCPU" : "time_getTimeReal") );
    monitor_real_abort();
  }

  TD_GET(ss_state)[self->sel_idx] = START;
}


/******************************************************************************
 * method definitions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  TMSG(ITIMER_CTL, "init");
  blame_shift_source_register(bs_type_timer);
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(ITIMER_CTL, "thread init");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(ITIMER_CTL, "thread init action");
}

static void
METHOD_FN(start)
{
  TMSG(ITIMER_CTL, "start %s", the_event_name);

  // the realtime clock needs an extra step to create the timer
  if (use_realtime || use_cputime) {
    thread_data_t *td = hpcrun_get_thread_data();
    if (hpcrun_create_real_timer(td) != 0) {
      EEMSG("Unable to create the timer for %s", the_event_name);
      hpcrun_ssfail_start(the_event_name);
    }
  }

  // Since we block a thread's timer signal when stopping, we
  // must unblock it when starting.
  monitor_real_pthread_sigmask(SIG_UNBLOCK, &timer_mask, NULL);

  hpcrun_restart_timer(self, 1);
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(ITIMER_CTL, "thread fini action");

  // Delete the realtime timer to avoid a timer leak.
  if (use_realtime || use_cputime) {
    thread_data_t *td = hpcrun_get_thread_data();
    hpcrun_delete_real_timer(td);
  }
}

static void
METHOD_FN(stop)
{
  TMSG(ITIMER_CTL, "stop %s", the_event_name);

  // We have observed thread-centric profiling signals 
  // (e.g., REALTIME) being delivered to a thread even after 
  // we have stopped the thread's timer.  During thread
  // finalization, this can cause a catastrophic error. 
  // For that reason, we always block the thread's timer 
  // signal when stopping. 
  monitor_real_pthread_sigmask(SIG_BLOCK, &timer_mask, NULL);

  thread_data_t *td = hpcrun_get_thread_data();
  int rc = hpcrun_stop_timer(td);
  if (rc != 0) {
    EMSG("stop %s failed, errno: %d", the_event_name, errno);
  }

  TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self, stop); // make sure stop has been called
  TMSG(ITIMER_CTL, "shutdown %s", the_event_name);
  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return hpcrun_ev_is(ev_str, CPUTIME_EVENT_NAME)
    || hpcrun_ev_is(ev_str, REALTIME_EVENT_NAME);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(ITIMER_CTL, "process event list, lush_metrics = %d", lush_metrics);

  // fetch the event string for the sample source
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);

  TMSG(ITIMER_CTL,"checking event spec = %s",event);

  if (hpcrun_ev_is(event, REALTIME_EVENT_NAME)) {
#ifdef ENABLE_CLOCK_REALTIME
    use_realtime = true;
    the_event_name = REALTIME_EVENT_NAME;
    the_metric_name = REALTIME_METRIC_NAME;
    the_signal_num = REALTIME_SIGNAL;
#else
    EEMSG("Event %s is not available on this system.", REALTIME_EVENT_NAME);
    hpcrun_ssfail_unknown(event);
#endif
  }

  if (hpcrun_ev_is(event, CPUTIME_EVENT_NAME)) {
#ifdef ENABLE_CLOCK_CPUTIME
    use_cputime = true;
    the_event_name = CPUTIME_EVENT_NAME;
    the_metric_name = CPUTIME_METRIC_NAME;
    the_signal_num = REALTIME_SIGNAL;
#else
    EEMSG("Event %s is not available on this system.", CPUTIME_EVENT_NAME);
    hpcrun_ssfail_unknown(event);
#endif
  }

  if (!use_realtime && !use_cputime) {
    // should never get here if supports_event is true
    hpcrun_ssfail_unknown(event);
  }

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);

  // store event threshold
  METHOD_CALL(self, store_event, ITIMER_EVENT, period);
  TMSG(OPTIONS,"Linux timer period set to %ld",period);

  // set up file local variables for sample source control
  int seconds = period / 1000000;
  int microseconds = period % 1000000;

  TMSG(ITIMER_CTL, "init %s sample_period = %ld, seconds = %d, usec = %d",
       the_event_name, period, seconds, microseconds);

  itval_start.it_value.tv_sec = seconds;
  itval_start.it_value.tv_usec = microseconds;
  itval_start.it_interval.tv_sec = 0;
  itval_start.it_interval.tv_usec = 0;

  itspec_start.it_value.tv_sec = seconds;
  itspec_start.it_value.tv_nsec = 1000 * microseconds;
  itspec_start.it_interval.tv_sec = 0;
  itspec_start.it_interval.tv_nsec = 0;

  memset(&itval_stop, 0, sizeof(itval_stop));
  memset(&itspec_stop, 0, sizeof(itspec_stop));

  sigemptyset(&timer_mask);
  sigaddset(&timer_mask, the_signal_num);

  // handle metric allocation
  hpcrun_pre_allocate_metrics(1 + lush_metrics);
  

  // set metric information in metric table
  TMSG(ITIMER_CTL, "setting metric timer period = %ld", sample_period);
  kind_info_t *timer_kind = hpcrun_metrics_new_kind();
  int metric_id =
    hpcrun_set_new_metric_info_and_period(timer_kind, the_metric_name, MetricFlags_ValFmt_Real,
					  sample_period, metric_property_time);
  METHOD_CALL(self, store_metric_id, ITIMER_EVENT, metric_id);
  if (lush_metrics == 1) {
    int mid_idleness = 
      hpcrun_set_new_metric_info_and_period(timer_kind, IDLE_METRIC_NAME,
					    MetricFlags_ValFmt_Real,
					    sample_period, metric_property_time);
    lush_agents->metric_time = metric_id;
    lush_agents->metric_idleness = mid_idleness;
  }
  hpcrun_close_kind(timer_kind);

  event = next_tok();
  if (more_tok()) {
    EEMSG("Can't use multiple timer events in the same run.");
    hpcrun_ssfail_conflict("timer", event);
  }
}

static void
METHOD_FN(finalize_event_list)
{
}

//
// There is only 1 event for itimer, hence the event "set" is always the same.
// The signal setup, however, is done here.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  monitor_sigaction(the_signal_num, &itimer_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available Timer events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\tReal clock time used by the thread in microseconds.\n"
	 "\t\tBased on the CLOCK_REALTIME timer with the SIGEV_THREAD_ID\n"
	 "\t\textension.  Includes time blocked in the kernel, and may\n"
	 "\t\tbreak the invocation of some syscalls that are sensitive to EINTR.\n",
	 REALTIME_EVENT_NAME);
#ifndef ENABLE_CLOCK_REALTIME
  printf("\t\tNot available on this system.\n");
#endif
  printf("\n");
  printf("%s  \tCPU clock time used by the thread in microseconds.  Based\n"
	 "\t\ton the CLOCK_THREAD_CPUTIME_ID timer with the SIGEV_THREAD_ID\n"
	 "\t\textension.\n",
	 CPUTIME_EVENT_NAME);
#ifndef ENABLE_CLOCK_CPUTIME
  printf("\t\tNot available on this system.\n");
#endif
  printf("\n");
  printf("Note: only one of the above timer events may be used in a run.\n");
  printf("\n");
}


/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name itimer
#define ss_cls SS_HARDWARE
#define ss_sort_order  20

#include "ss_obj.h"


/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
itimer_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  HPCTOOLKIT_APPLICATION_ERRNO_SAVE();

  static bool metrics_finalized = false;
  sample_source_t *self = &_itimer_obj;

  // if sampling is suppressed for this thread, restart timer, & exit
  if (hpcrun_suppress_sample() || sample_filters_apply()) {
    TMSG(ITIMER_HANDLER, "thread sampling suppressed");
    hpcrun_restart_timer(self, 1);

    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
  }

  // If we got a wallclock signal not meant for our thread, then drop the sample
  if (! wallclock_ok) {
    EMSG("Received Linux timer signal, but thread not initialized");
  }
  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  void* pc = hpcrun_context_pc(context);
  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    if (! hpcrun_is_sampling_disabled()) {
      hpcrun_restart_timer(self, 0);
    }

    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
  }

  // Ensure metrics are finalized.
  if (!metrics_finalized) {
    hpcrun_get_num_kind_metrics();
    metrics_finalized = true;
  }

  TMSG(ITIMER_HANDLER,"Itimer sample event");

  uint64_t metric_incr = 1; // default: one time unit

  // get the current time for the appropriate clock
  uint64_t cur_time_us = 0;
  int ret;

  if (use_cputime) {
    ret = time_getTimeCPU(&cur_time_us);
  } else {
    ret = time_getTimeReal(&cur_time_us);
  }

  if (ret != 0) {
    EMSG("%s clock_gettime failed!", (use_cputime ? "time_getTimeCPU" : "time_getTimeReal") );
    monitor_real_abort();
  }
  // compute the difference between it and the previous event on this thread
  metric_incr = cur_time_us - TD_GET(last_time_us);

  // convert microseconds to seconds
  hpcrun_metricVal_t metric_delta = {.r = metric_incr / 1.0e6}; 

  int metric_id = hpcrun_event2metric(self, ITIMER_EVENT);
  sample_val_t sv = hpcrun_sample_callpath(context, metric_id, metric_delta,
					    0/*skipInner*/, 0/*isSync*/, NULL);

  if(sv.sample_node) {
    blame_shift_apply(metric_id, sv.sample_node, metric_incr);
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(ITIMER_HANDLER, "No Linux timer restart due to disabled sampling");
  }
  else {
    hpcrun_restart_timer(self, 1);
  }

  hpcrun_safe_exit();

  HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

  return 0; // tell monitor that the signal has been handled
}
