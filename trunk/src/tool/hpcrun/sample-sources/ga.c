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

//***************************************************************************
// system includes
//***************************************************************************

#include <stddef.h>
#include <stdio.h>
#include <string.h>


//***************************************************************************
// local includes
//***************************************************************************

#include <sample-sources/simple_oo.h>
#include <sample-sources/sample_source_obj.h>
#include <sample-sources/common.h>
#include <sample-sources/ga.h>

#include <hpcrun/metrics.h>
#include <hpcrun/thread_data.h>
#include <messages/messages.h>


//***************************************************************************
// local variables
//***************************************************************************

static int metricId_bytesXfer  = -1;
static int metricId_onesidedOp = -1;
static int metricId_collectiveOp = -1;


//***************************************************************************
// method definitions
//***************************************************************************

static void
METHOD_FN(init)
{
  TMSG(GA, "init");
  self->state = INIT;
  metricId_bytesXfer = -1;
  metricId_onesidedOp = -1;
  metricId_collectiveOp = -1;
}


static void
METHOD_FN(thread_init)
{
  TMSG(GA, "thread init (no-op)");
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(GA, "thread init action (no-op)");
}


static void
METHOD_FN(start)
{
  TMSG(GA, "starting GA sample source");
  TD_GET(ss_state)[self->evset_idx] = START;
}


static void
METHOD_FN(thread_fini_action)
{
  TMSG(GA, "thread fini action (no-op)");
}


static void
METHOD_FN(stop)
{
  TMSG(GA, "stopping GA sample source");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  TMSG(GA, "shutdown GA sample source");
  METHOD_CALL(self, stop);
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
  // FIXME: this message comes too early and goes to stderr instead of
  // the log file.
  // TMSG(GA, "test support event: %s", ev_str);
  return strncasecmp(ev_str, "GA", 2) == 0;
}


// GA metrics: bytes read and bytes written.

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(GA, "create GA metrics");
  metricId_bytesXfer = hpcrun_new_metric();
  metricId_onesidedOp = hpcrun_new_metric();
  metricId_collectiveOp = hpcrun_new_metric();
  hpcrun_set_metric_info(metricId_bytesXfer, "GA bytes xfr");
  hpcrun_set_metric_info(metricId_onesidedOp, "GA #onesided");
  hpcrun_set_metric_info(metricId_collectiveOp, "GA #collective");
}


static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  TMSG(GA, "gen event set (no-op)");
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available Global Arrays events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("GA\t\tCollect Global Arrays metrics\n");
  printf("\n");
}


//***************************************************************************
// object
//***************************************************************************

// sync class is "SS_SOFTWARE" so that both synchronous and
// asynchronous sampling is possible

#define ss_name ga
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


//***************************************************************************
// interface functions
//***************************************************************************

int
hpcrun_ga_metricId_bytesXfer()
{
  return metricId_bytesXfer;
}

int
hpcrun_ga_metricId_onesidedOp()
{
  return metricId_onesidedOp;
}

int
hpcrun_ga_metricId_collectiveOp()
{
  return metricId_collectiveOp;
}
