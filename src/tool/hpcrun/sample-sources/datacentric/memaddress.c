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
 * local includes
 ******************************************************************************/

#include <cct/cct.h>                          // cct_node_t

#include "sample-sources/perf/perf-util.h"    // event_info_t, perf_attr_init
#include "sample-sources/perf/event_custom.h" // event_custom_t

#include "data_tree.h"

#include <place_folder.h>

/******************************************************************************
 *  MACROs
 ******************************************************************************/

#define EVNAME_MEMORY_CENTRIC "MEMCENTRIC"


/******************************************************************************
 * function folder
 ******************************************************************************/

FUNCTION_FOLDER(first_touch)

/******************************************************************************
 * data structure
 *****************************************************************************/


/******************************************************************************
 * local variables
 *****************************************************************************/
static int metric_page_fault = -1;

static void
memcentric_handler(event_info_t *current, void *context, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if ( (current == NULL)      ||  (mmap_data == NULL) ||
       (mmap_data->addr == 0) ||  (sv.sample_node == NULL))
    return;

  // memory information exists
  void *start, *end;
  struct datainfo_s *info = splay_lookup((void*) mmap_data->addr, &start, &end);

  if (info && info->context) {
    cct_node_t *root   = hpcrun_cct_get_root(sv.sample_node);
    cct_node_t *cursor = hpcrun_insert_special_node(root, POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(first_touch));

    // copy the call path of the malloc
    cct_node_t *node   = info->context;
    cursor             = hpcrun_cct_insert_path_return_leaf(node, cursor);

    metric_set_t* mset = hpcrun_reify_metric_set(cursor);

    hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(mset, metric_page_fault);
    if (loc->i == 0) {
      loc->i = mmap_data->addr;
    }

    thread_data_t *td = hpcrun_get_thread_data();
    td->core_profile_trace_data.perf_event_info[metric_page_fault].num_samples++;

    TMSG(DATACENTRIC, "handling node %p, cct: %p, addr: %p", node, sv.sample_node, mmap_data->addr);
  }
}


static int
memcentric_register(sample_source_t *self, event_custom_t *event, struct event_threshold_s *period)
{
  event_info_t *event_info = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
  if (event_info == NULL)
    return -1;

  memset(event_info, 0, sizeof(event_info_t));

  event_info->metric_custom = event;

  struct event_threshold_s threshold;
  perf_util_get_default_threshold( &threshold );

  // ------------------------------------------
  // create metric page-fault
  // ------------------------------------------
  metric_page_fault = hpcrun_new_metric();

  hpcrun_set_metric_info_and_period(
      metric_page_fault, "PAGE-FAULTS",
      MetricFlags_ValFmt_Int, 1, metric_property_none);

  // ------------------------------------------
  // create custom metric page-address
  // ------------------------------------------
  int page_address     = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(
      page_address, "PAGE-ADDR",
      MetricFlags_ValFmt_Int, 1 /* frequency*/, metric_property_none);

  // ------------------------------------------
  // initialize perf attributes
  // ------------------------------------------
  u64 sample_type = PERF_SAMPLE_IP   | PERF_SAMPLE_TID       |
      PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN |
      PERF_SAMPLE_CPU  | PERF_SAMPLE_PERIOD;

  // set the default attributes for page fault
  struct perf_event_attr *attr = &(event_info->attr);
  attr->config = PERF_COUNT_SW_PAGE_FAULTS;
  attr->type   = PERF_TYPE_SOFTWARE;

  perf_util_attr_init(
      attr,
      true                      /* use_period*/,
      threshold.threshold_num   /* use the default */,
      sample_type               /* need additional info to gather memory address */
  );
  perf_util_set_max_precise_ip(attr);

  event_info->attr.sample_id_all = 1;

  METHOD_CALL(self, store_event_and_info,
              attr->config, 1, page_address, event_info);;
  return 1;
}


/******************************************************************************
 * method definitions
 *****************************************************************************/


void
memcentric_init()
{
  event_custom_t *event_memcentric = hpcrun_malloc(sizeof(event_custom_t));
  event_memcentric->name         = EVNAME_MEMORY_CENTRIC;
  event_memcentric->desc         = "Experimental counter: identifying first-touch memory address.";
  event_memcentric->register_fn  = memcentric_register;   // call backs
  event_memcentric->handler_fn   = memcentric_handler;
  event_memcentric->handle_type  = EXCLUSIVE;// call me only for my events

  event_custom_register(event_memcentric);
}




