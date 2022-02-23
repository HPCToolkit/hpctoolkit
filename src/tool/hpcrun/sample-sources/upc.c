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

/*
 * BG/P's UPC interface for overflow sampling.
 *
 *   https://wiki.alcf.anl.gov/index.php/Performance_Tools
 *   intrepid: /bgsys/drivers/ppcfloor/arch/include/spi/UPC.h
 *
 * At startup:
 *
 *   BGP_UPC_Initialize();
 *   BGP_UPC_Initialize_Counter_Config(BGP_UPC_MODE_0, BGP_UPC_CFG_EDGE_DEFAULT);
 *   for each event
 *      BGP_UPC_Set_Counter_Value(event, 0);
 *      BGP_UPC_Set_Counter_Threshold_Value(event, threshold);
 *   BGP_UPC_Start(0);
 *
 * Inside signal handler for SIGXCPU:
 *
 *   BGP_UPC_Stop();
 *   for each event
 *      BGP_UPC_Read_Counter_Value(event, BGP_UPC_READ_EXCLUSIVE);
 *      if (counter >= threshold)
 *         BGP_UPC_Set_Counter_Value(event, 0);
 *         BGP_UPC_Set_Counter_Threshold_Value(event, threshold);
 *   BGP_UPC_Start(0);
 *
 * Notes:
 *
 *   1. Set counter value to 0 and count up to threshold.
 *   2. Stop UPC before reading counters.
 *   3. May sample on any number of events simultaneously.
 *   4. There is no event for total cycles.
 *   5. Seems to allow a separate threshold for each event,
 *      although some reports say only one global threshold.
 *   6. Must run BGP_UPC_Initialize() in every process.
 *   7. Set every node to Mode 0.
 *   8. Don't use BGP_UPC_Monitor_Event().
 *
 * Note: all UPC interrupts go to core 0, so we sample core 0 and stay
 * blind to the other cores.
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spi/UPC.h>
#include <spi/UPC_Events.h>
#include <spi/kernel_interface.h>
#undef INIT

#include <monitor.h>

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "ss-errno.h"
 
#include <hpcrun/main.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>

#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>

/******************************************************************************
 * local variables
 *****************************************************************************/

// FIXME: there is no good way for sighandler to find self.
static sample_source_t *myself = NULL;

#define DEFAULT_THRESHOLD  1000000L

//----------------------------------------------------------------------
// Helper functions
//----------------------------------------------------------------------

/*
 * The UPC layer doesn't provide a name to event code function, so we
 * do linear search here.  There are about 1,000 possible events, but
 * it's a one-time hit at startup.
 *
 * Returns: BGP event code, or -1 on failure.
 */
#define EVENT_NAME_SIZE  (BGP_UPC_MAXIMUM_LENGTH_EVENT_NAME + 50)
static int
bgp_event_name_to_code(const char *name)
{
  char buf[EVENT_NAME_SIZE];
  int ev;

  for (ev = BGP_UPC_MINIMUM_EVENT_ID; ev <= BGP_UPC_MAXIMUM_EVENT_ID; ev++) {
    if (BGP_UPC_Get_Event_Name(ev, EVENT_NAME_SIZE, buf) >= 0
	&& strcmp(name, buf) == 0) {
      return ev;
    }
  }

  return -1;
}

/*
 * Trim the category from the event description.  The BGP description
 * begins with a category: which is somewhat redundant with the name
 * of the event.  For example:
 *    name                      description
 *    BGP_PU0_JPIPE_ADD_SUB     P0 CPU:  Add/Sub in J-pipe
 */
static char *
trim_event_desc(char *desc)
{
  char *new;

  new = strchr(desc, ':');
  if (new == NULL) {
    return desc;
  }
  new += 1;
  new += strspn(new, " \t");

  return new;
}

//----------------------------------------------------------------------
// Method functions and signal handler
//----------------------------------------------------------------------

static int
hpcrun_upc_handler(int sig, siginfo_t *info, void *context)
{
  HPCTOOLKIT_APPLICATION_ERRNO_SAVE();

  int64_t counter, threshold;
  int ev, k;

  

  // if sampling disabled explicitly for this thread, skip all processing
  if (hpcrun_suppress_sample()) {
    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
  }

  BGP_UPC_Stop();

  // If the interrupt came from inside our code, then drop the sample
  // and return and avoid any MSG.
  // FIXME: NULL is bogus here and should be the program counter.
  int safe = hpcrun_safe_enter_async(NULL);
  if (! safe) {
    hpcrun_stats_num_samples_blocked_async_inc();
  }

  for (k = 0; k < myself->evl.nevents; k++) {
    ev = myself->evl.events[k].event;
    counter = BGP_UPC_Read_Counter_Value(ev, BGP_UPC_READ_EXCLUSIVE);
    threshold = myself->evl.events[k].thresh;
    if (counter >= threshold) {
      if (safe) {
	hpcrun_sample_callpath(context, myself->evl.events[k].metric_id,
			       1, 0, 0, NULL);
      }
      BGP_UPC_Set_Counter_Value(ev, 0);
      BGP_UPC_Set_Counter_Threshold_Value(ev, threshold);
    }
  }

  if (! hpcrun_is_sampling_disabled()) {
    BGP_UPC_Start(0);
  }

  if (safe) {
    hpcrun_safe_exit();
  }

  HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

  return 0; // tell monitor that the signal has been handled
}

// Note: Must run BGP_UPC_Initialize() in every process,
// and set every node to Mode 0 (to count on cores 0 and 1).
static void
METHOD_FN(init)
{
  BGP_UPC_Initialize();
  if (Kernel_PhysicalProcessorID() == 0) {
    BGP_UPC_Initialize_Counter_Config(BGP_UPC_MODE_0, BGP_UPC_CFG_EDGE_DEFAULT);
  }
  self->state = INIT;
  TMSG(UPC, "BGP_UPC_Initialize");
}

static void
METHOD_FN(thread_init)
{
}

static void
METHOD_FN(thread_init_action)
{
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
  char buf[EVENT_NAME_SIZE];
  long threshold;

  if (self->state == UNINIT) {
    METHOD_CALL(self, init);
  }

  hpcrun_extract_ev_thresh(ev_str, EVENT_NAME_SIZE, buf, &threshold, DEFAULT_THRESHOLD);
  return bgp_event_name_to_code(buf) != -1;
}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  char *event;
  char name[EVENT_NAME_SIZE];
  long threshold;
  int k, code, metric_id, nevents;

  for (event = start_tok(self->evl.evl_spec); more_tok(); event = next_tok()) {
    hpcrun_extract_ev_thresh(event, EVENT_NAME_SIZE, name, &threshold, DEFAULT_THRESHOLD);
    code = bgp_event_name_to_code(name);
    if (code < 0) {
      EMSG("unexpected failure in UPC process_event_list(): "
	   "unable to find code for event %s", event);
      hpcrun_ssfail_unsupported("UPC", event);
    }
    METHOD_CALL(self, store_event, code, threshold);
  }

  nevents = self->evl.nevents;
  hpcrun_pre_allocate_metrics(nevents);

  kind_info_t *upc_kind = hpcrun_metrics_new_kind();

  for (k = 0; k < nevents; k++) {
    code = self->evl.events[k].event;
    threshold = self->evl.events[k].thresh;
    BGP_UPC_Get_Event_Name(code, EVENT_NAME_SIZE, name);
    metric_id = 
      hpcrun_set_new_metric_info_and_period(upc_kind, strdup(name),
        MetricFlags_ValFmt_Int, threshold, metric_property_none);
    self->evl.events[k].metric_id = metric_id;
    TMSG(UPC, "add event %s(%d), threshold %ld, metric %d",
	 name, code, threshold, metric_id);
  }

  hpcrun_close_kind(upc_kind);
}

static void
METHOD_FN(finalize_event_list)
{
}

/*
 * On BG/P, all UPC interrupts go to core 0, so we sample core 0 and
 * stay blind to the other cores.  We can sample on core 0 of every
 * node, but leave in the soft failure in case something unexpected
 * happens.
 */
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  char name[EVENT_NAME_SIZE];
  int k, ev, ret;

  if (Kernel_PhysicalProcessorID() != 0) {
    EMSG("Warning: unable to sample in this process/thread "
	 "due to BlueGene hardware limitations (not core 0).");
    return;
  }

  for (k = 0; k < self->evl.nevents; k++) {
    ev = self->evl.events[k].event;
    BGP_UPC_Get_Event_Name(ev, EVENT_NAME_SIZE, name);

    ret = BGP_UPC_Set_Counter_Value(ev, 0);
    if (ret < 0) {
      EMSG("Warning: unable to sample on this node "
	   "due to BlueGene hardware limitations.");
      return;
    }
    ret = BGP_UPC_Set_Counter_Threshold_Value(ev, self->evl.events[k].thresh);
    if (ret < 0) {
      EMSG("Warning: unable to sample on this node "
	   "due to BlueGene hardware limitations.");
      return;
    }
    TMSG(UPC, "monitor event %s(%d), threshold %ld",
	 name, ev, self->evl.events[k].thresh);
  }

  myself = self;
  ret = monitor_sigaction(SIGXCPU, &hpcrun_upc_handler, 0, NULL);
  if (ret < 0) {
    EEMSG("HPCToolkit fatal error: unable to install signal handler for SIGXCPU");
    exit(1);
  }
  TMSG(UPC, "installed signal handler for SIGXCPU");
}

static void
METHOD_FN(start)
{
  if (Kernel_PhysicalProcessorID() != 0)
    return;

  // Need to ignore failure here.
  BGP_UPC_Start(0);

  TD_GET(ss_state)[self->sel_idx] = START;
  TMSG(UPC, "BGP_UPC_Start on core 0");
}

static void
METHOD_FN(thread_fini_action)
{
}

static void
METHOD_FN(stop)
{
  if (Kernel_PhysicalProcessorID() != 0)
    return;

  BGP_UPC_Stop();
  TD_GET(ss_state)[self->sel_idx] = STOP;
  TMSG(UPC, "BGP_UPC_Stop on core 0");
}

static void
METHOD_FN(shutdown)
{
  if (Kernel_PhysicalProcessorID() != 0)
    return;

  METHOD_CALL(self, stop);
  self->state = UNINIT;
  TMSG(UPC, "shutdown on core 0");
}

static void
METHOD_FN(display_events)
{
  char name[EVENT_NAME_SIZE];
  char desc[2048];
  int ev, num_total;

  printf("===========================================================================\n");
  printf("Available BG/P UPC events on core 0\n");
  printf("===========================================================================\n");
  printf("Name\t\t\t\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");

  num_total = 0;
  for (ev = BGP_UPC_MINIMUM_EVENT_ID; ev <= BGP_UPC_MAXIMUM_EVENT_ID; ev++) {
    if (BGP_UPC_Get_Event_Name(ev, EVENT_NAME_SIZE, name) >= 0
	&& BGP_UPC_Get_Event_Description(ev, 2040, desc) >= 0
	&& strstr(name, "PU0") != NULL) {
      printf("%-35s\t%s\n", name, trim_event_desc(desc));
      num_total++;
    }
  }
  printf("UPC events on core 0: %d\n", num_total);
  printf("\n");

  printf("===========================================================================\n");
  printf("Other BG/P UPC events (not all available)\n");
  printf("===========================================================================\n");
  printf("Name\t\t\t\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");

  num_total = 0;
  for (ev = BGP_UPC_MINIMUM_EVENT_ID; ev <= BGP_UPC_MAXIMUM_EVENT_ID; ev++) {
    if (BGP_UPC_Get_Event_Name(ev, EVENT_NAME_SIZE, name) >= 0
	&& BGP_UPC_Get_Event_Description(ev, 2040, desc) >= 0
	&& strstr(name, "PU0") == NULL) {
      printf("%-35s\t%s\n", name, trim_event_desc(desc));
      num_total++;
    }
  }
  printf("Other UPC events: %d\n", num_total);
  printf("\n");
}


#define ss_name upc
#define ss_cls SS_HARDWARE

#include "ss_obj.h"
