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
// Copyright ((c)) 2002-2022, Rice University
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

//******************************************************************************
// A generic sample source object file
//
// There are no actual "generic" sample sources.
// This file is provided as a template for those who wish to add
//  additional sample sources to the hpctoolkit.
//******************************************************************************


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>
#include <stdbool.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


// *****************************************************************************
// local includes
// *****************************************************************************

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "ss-errno.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>

// *****************************************************************************
// (file) local variables
// 
// *****************************************************************************

//
// Sample source specific control info
//  (example: itimer uses itimerval)
// Some sample sources may not need this kind of local variable
//
static struct SOME_STRUCT ss_specific_data;

//
// Almost all sample sources have events. The local data below keeps track of
// event info, and metric info.
// 
// NOTE: some very unusual sample sources may not have any events. (see 'none' sample source)
//       In this case, the event & metric tracking data structures can likely be dispensed with

//
// For sample sources that may have multiple events:
// You will likely need to keep a count of the specified events, plus thresholds
//  event codes, & other info
//
// The sample code below uses a simple array of a fixed size to give the general idea
// If you need dynamically sized event info, use hpcrun_malloc instead of regular malloc.
//

struct event_info { // Typical structure to hold event info
  int code;
  long thresh;
};

static int n_events = 0;                               // # events for this sample source
static const int MAX_EVENTS = SAMPLE_SOURCE_SPECIFIC;
static struct event_info local_event[MAX_EVENTS];      // event codes (derived from event names usually)
static const long DEFAULT_THRESHOLD = 1000000L;  // sample source specific

//
// Sample source events almost always get at least 1 metric slot.
//  Some events may have multiple metrics.
//
// The declarations below show a simple structure with MAX_METRICS per event,
// and the same MAX_EVENTS in the sample event data structure above
//
// NOTE 1: The metric info may be included in the event data structure
//         It is separated here for illustration
//
// NOTE 2: If a sample source has no events (highly unusual, but see 'none' sample source),
//         then the data structures below are likely unnecessary

int metrics [MAX_EVENTS] [MAX_METRICS];

//***********************************************************
// forward definitions:
//
//  see comments with the handler source
//   (further down in this file) for more info
//***********************************************************

static int
generic_signal_handler(int sig, siginfo_t* siginfo, void* context);

//******************************************************************************
//  The method definitions listed below are all required. They do not have to
//  actually do anything (except where noted), but they must all be present.
//******************************************************************************


// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  //
  // This method covers LIBRARY init operations only.
  // Other init takes place in subsequent operations
  //
  // There may be no library init actions for some sample sources
  //  (itimer for example).
  // See papi.c for an example of non-trivial library init requirements
  //

  // the following statement must always appears at the end of the init method
  self->state = INIT;
}

//
// Many sample sources require additional initialization when employed in
// threaded programs.
// The 'thread_init' method is the hook to supply the additional initialization
//

static void
METHOD_FN(thread_init)
{
  // sample source thread init code here
}

//
// Some sample sources may require each thread function to take some 'thread_init_action'
// whenever a thread is started
// [ For example, PAPI source uses 'PAPI_register_thread' function ]
//

static void
METHOD_FN(thread_init_action)
{
}

static void
METHOD_FN(start)
{

  // The following test MUST appear in all start methods

  if (! hpcrun_td_avail()){
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }


  // (Optional) Debug message
  //  You are not limited in any way here. You may liberally sprinkle various
  //  debug messages throughout this (and any other) method definition.
  //
  TMSG(YOUR_MSG_CLASS, "your debug message about the sample source being started");

  //
  // Sample-source specific code goes here.
  //
  // If there are any auxilliary data that the start mechanism needs,
  // declare some static local variables (see the local variables section
  // near the top of the file).
  // The values for these local variables can be set in either the 'gen_event_set'
  // method or the 'process_event_list' method
  // examples:
  //  itimer uses a 'struct itimerval' local variable


  // if your sample source returns some success/fail status, then you can
  // test it here, and log an error message
  //
  if (sample_source_start == FAIL) {
    EMSG("YOUR ERROR MESSAGE HERE: (%d): %s", YOUR_errno, YOUR_errorstring);
    hpcrun_ssfail_start("GENERIC");
  }

  // This line must always appear at the end of a start method
  TD_GET(ss_state)[self->sel_idx] = START;
}

//
// Some sample sources may require each thread function to take some 'thread_fini_action'
// whenever the thread is shut down.
// [ For example, PAPI source uses 'PAPI_unregister_thread' function ]
//

static void
METHOD_FN(thread_fini_action)
{
}

static void
METHOD_FN(stop)
{
  //
  // sample-source specific stopping code goes here
  //
  // Similar to the 'start' method, if the 'stop' method needs any auxilliary data
  // declare some static local variables (see the local variables section
  // near the top of the file).
  // The values for these local variables can be set in either the 'gen_event_set'
  // method or the 'process_event_list' method
  // examples:
  //  itimer uses a 'struct itimerval zerotimer' local variable to hold the relevant
  //  itimer information

  // if your sample source returns some success/fail status, then you can
  // test it here, and log an error message
  if (sample_source_stop == FAIL) {
    EMSG("YOUR ERROR MESSAGE HERE: (%d): %s", YOUR_errno, YOUR_errorstring);
    hpcrun_ssfail_start("GENERIC");
  }

  // (Optional) debug message
  TMSG(YOUR_MESSAGE_CLASS,"your stop message %p : %s", your_optional_arg1, your_optional_arg2);

  // This line must always appear at the end of a stop method
  TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  //
  // Sample source specific shutdown code goes here
  //
  //

  // debug messages and error messages can go here

  // These lines must always appear at the end of a shutdown method

  METHOD_CALL(self, stop); // make sure stop has been called
  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  //
  // The event string argument contains the event specifications from
  // the command line.
  //
  // NOTE: You are guaranteed that the 'init' method for your sample source
  // has been invoked before any 'supports_event' method calls are invoked
  //
  // For simple event specs, a call to "hpcrun_ev_is" is sufficient. (see below)
  // For a more involved check on events, see papi.c

  return hpcrun_ev_is(ev_str,"YOUR_SOURCE");
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  // The lush_metrics argument is always passed in.
  // If your sample source does not support lush, then ignore this argument
  // in your event_list_processing.
  //
  // When this method is invoked, previous sample source processing
  // has concatenated all of the event specs
  // for this sample_source into 1 string (event specs came from the command line)
  // The 'process_event_list' method takes care of several things:
  //   * Processing period information [ event specs are of the form NAME@PERIOD ]
  //   * Setting up local variables with the appropriate information for start/stop
  //     (although some of this may be done in 'gen_event_set' method)
  //   * Allocate metrics for each event [ including lush, if appropriate ]
  //   * Enter period, processing function, flags, name, etc
  //     for each metric of each event
  //     [ in general, each sample source MAY HAVE multiple events. Each event
  //       MAY HAVE multiple metrics associated with it ]
  //

  // fetch the event string for the sample source
  char* _p = METHOD_CALL(self, get_event_str);
  

  //  ** You MAY BE able to pre-allocate metrics for your sample source **
  //  if so, you should do that before the main event processing loop
  //
  //  Example of preallocation:
  //  hpcrun_pre_allocate_metrics(1 + lush_metrics);
  
  //
  // EVENT: for each event in the event spec
  //
  for (char* event = start_tok(_p); more_tok(); event = next_tok()) {
    // (loop) local variables for useful for decoding event specification strings
    char name[1024];
    long thresh;

    // extract event threshold
    hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, DEFAULT_THRESHOLD);

    // process events, store whatever is needed for starting/stopping
    // the sample source in the file local variable
    //
    // (The amt of processing may be very small (i.e. itimer) or more involved (i.e. papi)

    local_event[n_events].code   = YOUR_NAME_TO_CODE(event);
    local_event[n_events].thresh = thresh;

    kind_info_t *metric_kind = hpcrun_metrics_new_kind();

    // for each event, process metrics for that event
    // If there is only 1 metric for each event, then the loop is unnecessary
    //
    // **NOTE: EVERY sample source that has metrics MUST have file (local) storage
    //         to track the metric ids.
    //
    for (int i=0; i < NUM_METRICS_FOR_EVENT(event); i++) {
      int metric_id;
      const char *name = NAME_FOR_EVENT_METRIC(event, i);
      
      if (METRIC_IS_STANDARD) {
      // For a standard updating metric (add some counts at each sample time), use
      // hpcrun_set_metric_info_and_period routine, as shown below
      //
        metric_id = hpcrun_set_new_metric_info_and_period(
          metric_kind, name, HPCRUN_MetricFlag_Async, // This is the correct flag value for almost all events
          thresh, metric_property_none);
      }
      else { // NON STANDARD METRIC
        // For a metric that updates in a NON standard fashion, use
        // hpcrun_set_metric_info_w_fn, and pass the updating function as the last argument
        //
        metric_id = hpcrun_set_new_metric_info_w_fn(name, YOUR_FLAG_VALUES, thresh, YOUR_UPD_FN);
      }
      metrics[n_events++][i] = metric_id;
    }

    hpcrun_close_kind(metric_kind);
  }
  // NOTE: some lush-aware event list processing may need to be done here ...
}

static void
METHOD_FN(finalize_event_list)
{
}

static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  // The lush_metrics argument is always passed in.
  // If your sample source does not support lush, then ignore this argument
  // in your gen_event_set processing.
  //
  // When this method is invoked, the event list has been processed, and all of the
  // event list info is now in local variables.
  // This method sets up the handlers.
  // It is called once for each thread.
  //
  // Sample code below assumes the handler is a standard unix signal handler.
  // If that is not true for your sample source, then follow the appropriate protocol
  // for your source.
  //
  // NOTE: use 'monitor_sigaction' instead of the system 'sigaction' when setting up
  //       standard signal handlers.
  //
  sigemptyset(&sigset_generic);
  sigaddset(&sigset_generic, HPCRUN_GENERIC_SIGNAL);
  monitor_sigaction(HPCRUN_GENERIC_SIGNAL, &generic_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  //
  // This method displays descriptive text about the sample source
  // This text is printed whenever the '-L' argument is given to hpcrun
  //
  printf("===========================================================================\n");
  printf("Available generic events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("GENERIC EVENT\tWhat the generic event does\n");
  printf("\n");
}


//***************************************************************************
// object
//***************************************************************************

//
// To integrate a sample source into the simple object system in hpcrun,
// #define ss_name to whatever you want your sample source to be named.
// (retain the ss_cls definition, unless you have a VERY unusual sample source).
//
// Finally, retain the #include statement
//
// For the moment, the #include statement MUST appear after all of the
// method definitions. DO NOT MOVE IT.
//

#define ss_name generic
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

//******************************************************************************
// private operations 
// *****************************************************************************

//
// Whenever an event triggers, the event handler is invoked
// Typically, the event trigger is a unix signal, so a unix signal handler must be installed
//
// ***** ALL sample source event handlers MUST pass a proper (unix) context ****
// 
// NOTE: PAPI does NOT use a standard unix signal handler.
//
// The example handler below is a standard unix signal handler.
// See papi for an event handler that is NOT a standard unix signal handler
// 
static int
generic_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  HPCTOOLKIT_APPLICATION_ERRNO_SAVE();

  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  void* pc = hpcrun_context_pc(context);
  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
  }
  else {
    //
    // Determine which events caused this sample
    //
    for (EACH _EVENT e IN THIS SAMPLE) {
      for (EACH METRIC m FOR THIS EVENT) {
        //
        // get the relevant data for metric m for event e
        //
        cct_metric_data_t datum = MY_SOURCE_FETCH_DATA(e, m);
        //
        // Determine the metric id for (e, m)
        //  [ The metric_id for (e, m) should be determinable by the file local data]
        //
        int metric_id = MY_METRIC_ID(e, m);
        //
        // call hpcrun_sample_callpath with context, metric_id, & datum.
        // The last 2 arguments to hpcrun_sample_callpath are always 0
        //
        hpcrun_sample_callpath(context, metric_id, datum, 0/*skipInner*/, 0/*isSync*/, NULL);
      }
    }
  }
  // If sampling is disabled, return immediately
  //
  if (hpcrun_is_sampling_disabled()) {
    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
  }

  //
  // If the sample source must be restarted, then
  // put that code here.
  //
  // NOTE: if restart can be accomplished using the exact same code as the start method,
  //       then a method call will work. (See example below)
  //       Otherwise, put the special purpose restart code here
  //
  METHOD_CALL(&_generic_obj, start);

  //
  // If the sigprocmask must be manipulated to ensure continuing samples, then
  // be sure to use monitor_real_sigprocmask.
  // 
  // You do NOT normally have to do this. This manipulation is only called for
  // if the regular signal handling mechanism is buggy.
  //
  // An example of the monitor_real_sigprocmask is shown below, but you will
  // normally be able omit the example code.
  //
  monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_generic, NULL);

  HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

  return 0; // tell monitor that the signal has been handled
}

//
// sample custom metric update procedure
//

//
// replaces the metric data with the average of the datum and the previous entry
//
void
generic_special_metric_update_proc(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum)
{
  loc->i = (loc->i + datum) / 2;
}
