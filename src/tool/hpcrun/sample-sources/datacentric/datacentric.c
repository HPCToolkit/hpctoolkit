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
// Copyright ((c)) 2002-2017, Rice University
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include <main.h>
#include <hpcrun/thread_data.h>

#include <messages/messages.h>

#include "datacentric.h"
#include "data_tree.h"

#include "sample-sources/perf/event_custom.h"


/******************************************************************************
 * constants
 *****************************************************************************/
#define EVNAME_DATACENTRIC "DATACENTRIC"


/******************************************************************************
 * data structure
 *****************************************************************************/

/******************************************************************************
 * local variables
 *****************************************************************************/



static void
datacentric_handler(event_thread_t *current, void *context, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if ( (current == NULL)      ||  (mmap_data == NULL) ||
       (mmap_data->addr == 0) ||  (sv.sample_node == NULL))
    return;

  // memory information exists
  void *start, *end;
  cct_node_t *node = splay_lookup((void*) mmap_data->addr, &start, &end);

}


static void
datacentric_register(event_info_t *event_desc)
{
  struct event_threshold_s threshold = init_default_count();

  // ------------------------------------------
  // hardware-specific data centric setup (if supported)
  // ------------------------------------------
  int result = datacentric_hw_register(event_desc, &threshold);
  if (result == 0)
    return;

  // ------------------------------------------
  // create metric page-fault
  // ------------------------------------------
  int metric = hpcrun_new_metric();

  event_desc->metric = metric;
  event_desc->metric_desc = hpcrun_set_metric_info_and_period(
      metric, EVNAME_DATACENTRIC,
      MetricFlags_ValFmt_Int, 1, metric_property_none);
}


/******************************************************************************
 * method definitions
 *****************************************************************************/


void
datacentric_init()
{
  event_custom_t *event_datacentric = hpcrun_malloc(sizeof(event_custom_t));
  event_datacentric->name         = EVNAME_DATACENTRIC;
  event_datacentric->desc         = "Experimental counter: counting the memory latency.";
  event_datacentric->register_fn  = datacentric_register;   // call backs
  event_datacentric->handler_fn   = datacentric_handler;
  event_datacentric->metric_index = 0;        // these fields to be defined later
  event_datacentric->metric_desc  = NULL;     // these fields to be defined later
  event_datacentric->handle_type  = EXCLUSIVE;// call me only for my events

  event_custom_register(event_datacentric);
}




// ***************************************************************************
//  Interface functions
// ***************************************************************************

// increment the return count
//
// N.B. : This function is necessary to avoid exposing the retcnt_obj.
//        For the case of the retcnt sample source, the handler (the trampoline)
//        is separate from the sample source code.
//        Consequently, the interaction with metrics must be done procedurally
