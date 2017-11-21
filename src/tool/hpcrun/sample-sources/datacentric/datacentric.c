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

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include <main.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>

#include <messages/messages.h>
#include <utilities/tokenize.h>
#include <safe-sampling.h>

#include "datacentric.h"
#include "data_tree.h"

#include "sample-sources/display.h" // show the list of events
#include "sample-sources/perf/event_custom.h"

#include <place_folder.h>

/******************************************************************************
 * constants
 *****************************************************************************/
#define EVNAME_DATACENTRIC "DATACENTRIC"


/******************************************************************************
 * data structure
 *****************************************************************************/
struct datacentric_info_s {
  cct_node_t *data_node;
  bool first_touch;
};

/******************************************************************************
 * local variables
 *****************************************************************************/

#define POINTER_TO_FUNCTION

#if defined(__PPC64__) || defined(HOST_CPU_IA64)
#define POINTER_TO_FUNCTION *(void**)
#endif


static cct_node_t *
datacentric_get_root(cct_node_t *node)
{
  cct_node_t *current = node;
  while(current && hpcrun_cct_parent(current)) {
      current = hpcrun_cct_parent(current);
  }
  return current;
}

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

  if (node) {
    cct_node_t *root   = datacentric_get_root(sv.sample_node);
    cct_node_t *cursor = hpcrun_insert_special_node(root, POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(first_touch));

    // copy the call path of the malloc
    cursor = hpcrun_cct_insert_path_return_leaf(node, cursor);

    metric_set_t* mset = hpcrun_reify_metric_set(cursor);
    hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(mset, current->event->metric_custom->metric_index);
    if (loc->i == 0) {
      loc->i = mmap_data->addr;
    }
    TMSG(DATACENTRIC, "handling node %p, cct: %p", node, sv.sample_node);
  }
}


static void
datacentric_register(event_info_t *event_desc)
{
  struct event_threshold_s threshold = init_default_count();

  // ------------------------------------------
  // create metric page-fault
  // ------------------------------------------
  int pagefault_metric = hpcrun_new_metric();

  event_desc->metric = pagefault_metric;
  event_desc->metric_desc = hpcrun_set_metric_info_and_period(
      pagefault_metric, "PAGE-FAULTS",
      MetricFlags_ValFmt_Int, 1, metric_property_none);

  // ------------------------------------------
  // create custom metric page-address
  // ------------------------------------------
  int page_address     = hpcrun_new_metric();
  event_desc->metric_custom->metric_index = page_address;
  event_desc->metric_custom->metric_desc  = hpcrun_set_metric_info_and_period(
      page_address, "PAGE-ADDR",
      MetricFlags_ValFmt_Int, 1 /* frequency*/, metric_property_none);

  // ------------------------------------------
  // initialize perf attributes
  // ------------------------------------------
  u64 sample_type = PERF_SAMPLE_IP   | PERF_SAMPLE_TID       |
      PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN |
      PERF_SAMPLE_CPU  | PERF_SAMPLE_PERIOD;

  // set the default attributes for page fault
  struct perf_event_attr *attr = &(event_desc->attr);
  attr->config = PERF_COUNT_SW_PAGE_FAULTS;
  attr->type   = PERF_TYPE_SOFTWARE;

  perf_attr_init(
      attr,
      true                      /* use_period*/,
      threshold.threshold_num   /* use the default */,
      sample_type               /* need additional info for sample type */
  );

  event_desc->attr.sample_id_all = 1;

  // ------------------------------------------
  // hardware-specific data centric setup (if supported)
  // ------------------------------------------
  datacentric_hw_register(event_desc, &threshold);
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
