// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/itimer.c $
// $Id: itimer.c 2657 2009-10-23 17:00:03Z mfagan $
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
// _tst sample source simple oo interface
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
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>

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
  _TST_EVENT = 0    // _tst has only 1 event
};

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
_tst_signal_handler(int sig, siginfo_t *siginfo, void *context);


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

static long period = 5000L;

static sigset_t sigset_itimer;

// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for _tst
}

static void
METHOD_FN(start)
{
  if (! hpcrun_td_avail()){
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }

  TMSG(_TST_CTL,"starting _tst w value = (%d,%d), interval = (%d,%d)",
       itimer.it_interval.tv_sec,
       itimer.it_interval.tv_usec,
       itimer.it_value.tv_sec,
       itimer.it_value.tv_usec);

  if (setitimer(HPCRUN_PROFILE_TIMER, &itimer, NULL) != 0) {
    EMSG("setitimer failed (%d): %s", errno, strerror(errno));
    hpcrun_ssfail_start("_tst");
  }

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  int ret = time_getTimeCPU(&TD_GET(last_time_us));
  if (ret != 0) {
    EMSG("time_getTimeCPU (clock_gettime) failed!");
    abort();
  }
#endif

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(stop)
{
  int rc;

  rc = setitimer(HPCRUN_PROFILE_TIMER, &zerotimer, NULL);

  TMSG(ITIMER_CTL,"stopping _tst");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self, stop); // make sure stop has been called
  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"_TST") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{

  // fetch the event string for the sample source
  char* _p = METHOD_CALL(self, get_event_str);
  
  //
  // EVENT: Only 1 wallclock event
  //
  char* event = start_tok(_p);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(_TST_CTL,"checking event spec = %s",event);

  // extract event threshold
  extract_ev_thresh(event, sizeof(name), name, &period);

  // store event threshold
  METHOD_CALL(self, store_event, _TST_EVENT, period);
  TMSG(OPTIONS,"_TST period set to %ld",period);

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
  METHOD_CALL(self, store_metric_id, _TST_EVENT, metric_id);

  // set metric information in metric table

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
# define sample_period 1
#else
# define sample_period period
#endif

  TMSG(_TST_CTL, "setting metric _TST, period = %ld", sample_period);
  hpcrun_set_metric_info_and_period(metric_id, "_TST",
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

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE _TST events detected! Using first event spec: %s");
  }
  // 
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// Event "sets" not possible for this sample source.
// It has only 1 event.
// Initialize the sigset and installing the signal handler are
// the only actions
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  sigemptyset(&sigset_itimer);
  sigaddset(&sigset_itimer, HPCRUN_PROFILE_SIGNAL);
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &_tst_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  //
  // normally _tst does not display events
  // DEVELOPER ONLY: modify this method in your sandbox as needed
  //

#ifdef DISPLAY_TEST_EVENTS
  printf("===========================================================================\n");
  printf("Available _tst events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("WALLCLOCK\tWall clock time used by the process in microseconds\n");
  printf("\n");
#endif
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name _tst
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
_tst_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  // Must check for async block first and avoid any MSG if true.
  void* pc = context_pc(context);
  if (hpcrun_async_is_blocked(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
  }
  else {
    TMSG(_TST_HANDLER,"_Tst sample event");

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

    int metric_id = hpcrun_event2metric(&__tst_obj, _TST_EVENT);
    hpcrun_sample_callpath_w_bt(context, metric_id, metric_incr, NULL, NULL, 0);
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(SPECIAL, "No _tst restart, due to disabled sampling");
    return 0;
  }

#ifdef RESET_ITIMER_EACH_SAMPLE
  METHOD_CALL(&__tst_obj, start);
#endif

  return 0; /* tell monitor that the signal has been handled */
}
