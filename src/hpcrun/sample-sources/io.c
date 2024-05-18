// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * system includes
 *****************************************************************************/

#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <string.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"
#include "io.h"

#include "../metrics.h"
#include "../thread_data.h"
#include "../messages/messages.h"

#include "../utilities/tokenize.h"

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
  kind_info_t *io_kind = hpcrun_metrics_new_kind();
  metric_id_read = hpcrun_set_new_metric_info(io_kind, "IO Bytes Read");
  metric_id_write = hpcrun_set_new_metric_info(io_kind, "IO Bytes Written");
  hpcrun_close_kind(io_kind);
  TMSG(IO, "metric id read: %d, write: %d", metric_id_read, metric_id_write);
}

static void
METHOD_FN(finalize_event_list)
{
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
  printf("IO\t\t" "The number of bytes read and written per dynamic context\n");
  printf("\n");
}


/***************************************************************************
 * object
 ***************************************************************************/

// sync class is "SS_SOFTWARE" so that both synchronous and
// asynchronous sampling is possible

#define ss_name io
#define ss_cls SS_SOFTWARE
#define ss_sort_order  40

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
