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
// Copyright ((c)) 2002-2016, Rice University
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

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stddef.h>
#include <stdio.h>
#include <string.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <sample-sources/simple_oo.h>
#include <sample-sources/sample_source_obj.h>
#include <sample-sources/common.h>
#include <sample-sources/io.h>

#include <hpcrun/metrics.h>
#include <hpcrun/thread_data.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>

/******************************************************************************
 * local variables
 *****************************************************************************/

static int metric_id_read = -1;
static int metric_id_write = -1;


/******************************************************************************
 * method definitions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  TMSG(IO, "init");
  self->state = INIT;
  metric_id_read = -1;
  metric_id_write = -1;
}


static void
METHOD_FN(thread_init)
{
  TMSG(IO, "thread init (no-op)");
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(IO, "thread init action (no-op)");
}


static void
METHOD_FN(start)
{
  TMSG(IO, "starting IO sample source");
  TD_GET(ss_state)[self->sel_idx] = START;
}


static void
METHOD_FN(thread_fini_action)
{
  TMSG(IO, "thread fini action (no-op)");
}


static void
METHOD_FN(stop)
{
  TMSG(IO, "stopping IO sample source");
  TD_GET(ss_state)[self->sel_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  TMSG(IO, "shutdown IO sample source");
  METHOD_CALL(self, stop);
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
  // FIXME: this message comes too early and goes to stderr instead of
  // the log file.
  // TMSG(IO, "test support event: %s", ev_str);
  return hpcrun_ev_is(ev_str, "IO");
}


// IO metrics: bytes read and bytes written.

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(IO, "create metrics for IO bytes read and bytes written");
  metric_id_read = hpcrun_new_metric();
  metric_id_write = hpcrun_new_metric();
  hpcrun_set_metric_info(metric_id_read,  "IO Bytes Read");
  hpcrun_set_metric_info(metric_id_write, "IO Bytes Written");
  TMSG(IO, "metric id read: %d, write: %d", metric_id_read, metric_id_write);
}


static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  TMSG(IO, "gen event set (no-op)");
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available IO events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("IO\t\tThe number of bytes read and written per dynamic context\n");
  printf("\n");
}


/***************************************************************************
 * object
 ***************************************************************************/

// sync class is "SS_SOFTWARE" so that both synchronous and
// asynchronous sampling is possible

#define ss_name io
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


// ***************************************************************************
//  Interface functions
// ***************************************************************************

int
hpcrun_metric_id_read(void)
{
  return metric_id_read;
}

int
hpcrun_metric_id_write(void)
{
  return metric_id_write;
}
