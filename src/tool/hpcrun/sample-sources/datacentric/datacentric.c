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

#include <linux/perf_event.h> // perf_mem_data_src

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
#include <loadmap.h>
#include <hpcrun/thread_data.h>

#include <messages/messages.h>

#include <cct/cct_bundle.h>
#include <cct/cct.h>
#include <cct/cct_addr.h>   // struct var_addr_s

#include "include/queue.h"  // linked-list

#include "datacentric.h"
#include "data_tree.h"
#include "env.h"

#include "sample-sources/perf/event_custom.h"
#include "sample-sources/perf/perfmon-util.h"
#include "sample-sources/perf/perf_skid.h"


/******************************************************************************
 * macros 
 *****************************************************************************/

const char *EVNAME_ADDRESS_CENTRIC = "FAULTS";

/******************************************************************************
 * prototypes and forward declaration
 *****************************************************************************/



/******************************************************************************
 * data structure
 *****************************************************************************/

enum datacentric_status_e { UNINITIALIZED, INITIALIZED };


/******************************************************************************
 * variables
 *****************************************************************************/

static enum datacentric_status_e plugin_status = UNINITIALIZED;

static int metric_variable_size = -1;


/******************************************************************************
 * Place forlder
 *****************************************************************************/
//
// "Special" routine to serve as a placeholder for "dynamic" allocation
//

static void
DATACENTRIC_Dynamic(void)
{}

static void
DATACENTRIC_MemoryAccess(void)
{}

//
// "Special" routine to serve as a placeholder for "static" global variables
//

static void
DATACENTRIC_Static(void)
{}

//
// "Special" routine to serve as a placeholder for any variables that
//  are not static nor dynamic
//
static void
DATACENTRIC_Unknown(void)
{}

/******************************************************************************
 * PRIVATE Function implementation
 *****************************************************************************/

static cct_node_t*
datacentric_create_root_node(cct_node_t *root, uint16_t lm_id,
                             uintptr_t addr_start, uintptr_t addr_end)
{
  // create a node for this variable

  ip_normalized_t npc;
  cct_addr_t addr;

  memset(&npc,  0, sizeof(ip_normalized_t));
  memset(&addr, 0, sizeof(cct_addr_t));

  // create a node with the ip = memory address
  // this node will be translated by hpcprof and name it with
  //  the static variable that matches with the memory address

  npc.lm_ip    = addr_start + 1; // hpcprof sometimes decrements the value of IP.
  npc.lm_id    = lm_id;
  addr.ip_norm = npc;

  cct_node_t *context = hpcrun_cct_insert_addr(root, &addr);

#if SUPPORT_FOR_ADDRESS_CENTRIC
  // assign the start address and the end address metric
  // for this node
  int metric_id      = datacentric_get_metric_addr_start();
  metric_set_t *mset = hpcrun_reify_metric_set(context);

  hpcrun_metricVal_t value;
  value.p = (void*)addr_start;
  hpcrun_metric_std_min(metric_id, mset, value);

  metric_id = datacentric_get_metric_addr_end();
  value.p   = (void*)addr_end;
  hpcrun_metric_std_max(metric_id, mset, value);
#endif

  return context;
}



/***
 * manage pre signal handler for datacentric event
 * we only accept the event if:
 * - it contains memory references,
 * - case for address centric: the reference is known
 */
static event_accept_type_t
datacentric_pre_handler(event_handler_arg_t *args)
{
  if ( (args->current == NULL)  ||  (args->data == NULL) )
    return REJECT_EVENT;

  perf_mmap_data_t *mmap_data = args->data;
  if (mmap_data->addr == 0)
    return REJECT_EVENT;

  // for data centric event (not address centric event), we don't care
  // if the address is known or not. If it's unknown, we will attribute it
  // to "unknown attribute" root node

#if SUPPORT_FOR_ADDRESS_CENTRIC
  if (args->current->id == EVNAME_ADDRESS_CENTRIC) {

    // for address centric event, the address has to be known in the database
    // otherwise, it's just a waste of cpu time

    void *start = NULL, *end = NULL;

    datatree_info_t *info   = datatree_splay_lookup((void*) mmap_data->addr, &start, &end);
    if (info == NULL || info->status == DATATREE_INFO_HANDLED)
      return REJECT_EVENT;  // the address is NOT in the database
                            // or the memory has been handled
  }
#endif

  return ACCEPT_EVENT;
}



/***
 * manage post signal handler for datacentric event
 */
static event_accept_type_t
datacentric_post_handler(event_handler_arg_t *args)
{
  if ( (args->current == NULL)  ||  (args->data == NULL) ||
       (args->sample->sample_node == NULL))
    return REJECT_EVENT;

  perf_mmap_data_t *mmap_data = args->data;

  cct_node_t *sample_node = args->sample->sample_node;

  // ---------------------------------------------------------
  // memory information exists:
  // - check if we have this variable address in our database
  //   - if it's in our database (either static or dynamic), add it to the cct node
  //   - otherwise, it must be unknown variable. Not sure what we should do with this
  // ---------------------------------------------------------
  if (mmap_data->addr == 0) return REJECT_EVENT;

  // --------------------------------------------------------------
  // store the memory address reported by the hardware counter to the metric
  // even if the address is outside the range of recognized variable (see
  //    datatree_splay_lookup() function which returns if the address is recognized)
  // we may need to keep the information (?).
  // another solution is to create a sibling node to avoid to step over the
  //  recognized metric.
  // --------------------------------------------------------------

  void *start = NULL, *end = NULL;

  datatree_info_t *info   = datatree_splay_lookup((void*) mmap_data->addr, &start, &end);
  cct_node_t *var_context = NULL;
  thread_data_t* td       = hpcrun_get_thread_data();
  cct_bundle_t *bundle    = &(td->core_profile_trace_data.epoch->csdata);

  // --------------------------------------------------------------
  // try to find the accessed memory address from the database
  // if the accessed memory is within the range (found in the database), then it must be
  // either static variable or heap allocation
  // otherwise, we encounter an unknown variable
  // --------------------------------------------------------------
  cct_node_t* var_node = NULL;

  if (info) {
    var_context = info->context;

    if (info->magic == DATA_STATIC_MAGIC) {
      // attach this node of static variable to the datacentric root

      cct_node_t *datacentric_root = hpcrun_cct_bundle_init_datacentric_node(bundle);
      cct_node_t *variable_root    = hpcrun_insert_special_node(datacentric_root, DATACENTRIC_Static);
      cct_addr_t *addr             = hpcrun_cct_addr(sample_node);

      var_context = datacentric_create_root_node(variable_root, addr->ip_norm.lm_id,
                      (uintptr_t)info->memblock, (uintptr_t)info->rmemblock);

      var_node    = var_context;

      // mark that this is a special node for global variable
      // hpcprof will treat specially to print the name of the variable to xml file
      // (if hpcstruct provides the information)

      hpcrun_cct_set_node_variable(var_context);

    } else {
      // dynamic allocation
      cct_node_t *datacentric_root = hpcrun_cct_bundle_init_datacentric_node(bundle);
      cct_node_t *variable_root    = hpcrun_insert_special_node(datacentric_root, DATACENTRIC_Dynamic);

      var_node = hpcrun_cct_insert_path_return_leaf(var_context, variable_root);
      hpcrun_cct_set_node_allocation(var_node);

      // add artificial root for memory-access call-path
      var_context = hpcrun_insert_special_node(var_node, DATACENTRIC_MemoryAccess);

      hpcrun_cct_set_node_memaccess_root(var_context);
    }

    // record the size of the variable
    metric_set_t *mset       = hpcrun_reify_metric_set(var_node);
    const int size_in_bytes  = info->rmemblock - info->memblock;
    hpcrun_metricVal_t value = {.i = size_in_bytes};

    hpcrun_metric_std_set( datacentric_get_metric_variable_size(), mset, value );

    info->status = DATATREE_INFO_HANDLED;

  } else {
    // unknown variable
    cct_node_t *datacentric_root = hpcrun_cct_bundle_init_datacentric_node(bundle);
    var_context                  = hpcrun_insert_special_node(datacentric_root, DATACENTRIC_Unknown);

    hpcrun_cct_set_node_unknown_attribute(var_context);
    var_node = var_context;
  }

  // copy the callpath of the sample to the variable context
  cct_node_t *node = hpcrun_cct_insert_path_return_leaf(sample_node, var_context);

#if 0
  // sample node will point to this var node.
  // we need this node to keep the id so that hpcprof/hpcviewer will not lose the pointer
  hpcrun_cct_retain(var_node);
  hpcrun_cct_link_source_memaccess(sample_node, var_node);
#endif

  metric_set_t *mset = hpcrun_reify_metric_set(node);

#if SUPPORT_FOR_ADDRESS_CENTRIC
  // variable address is store in the database
  // record the interval of this access

  hpcrun_metricVal_t val_addr;
  val_addr.p = (void *)mmap_data->addr;

  // record the lower offset address
  int metric_id = datacentric_get_metric_addr_start();
  hpcrun_metric_std_min(metric_id, mset, val_addr);

  // record the upper offset address
  metric_id = datacentric_get_metric_addr_end();
  val_addr.p++;
  hpcrun_metric_std_max(metric_id, mset, val_addr);
#endif

  // re-record metric event for this node with the same value as linux_perf
  // the total metric of hpcrun cct and datacentric has to be the same for this metric
  //
  // this is important so that users can see the aggregate metrics across variables
  // while datacentric shows the distribution between variables

  hpcrun_metric_std_inc(args->metric, mset, (hpcrun_metricVal_t) {.r=args->metric_value});

  hpcrun_cct_set_node_memaccess(node);

  // hardware specific event handler
  // e.g. Intel PEBS provides additional information in the mmapped buffer
  ///     for memory latency (see perf c2c and mem tool)
  datacentric_hw_handler(mmap_data, node, sample_node);

  return ACCEPT_EVENT;
}


/*************
 * Include address-centric with data-centric
 *
 * To record the first touch, we use perf_events' PAGE-FAULTS event
 * which is delivered when a page fault occurs (by the kernel).
 *
 * It seems this software event has the same effect with mprotect()
 * but without requiring to handle sigsegv. We need to avoid handling sigsegv
 * since it interferes with hpcrun sigsegv.
 *************/
static int
datacentric_register_address_centric(sample_source_t *self,
    event_custom_t  *event,
    struct event_threshold_s *period)
{
  event_info_t *event_info = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
  if (event_info == NULL)
    return -1;

  memset(event_info, 0, sizeof(event_info_t));

  event_info->id = EVNAME_ADDRESS_CENTRIC;
  event_info->metric_custom = event;

  u64 sample_type = PERF_SAMPLE_IP   | PERF_SAMPLE_TID    |
                    PERF_SAMPLE_TIME | PERF_SAMPLE_PERIOD |
                    PERF_SAMPLE_CPU  | PERF_SAMPLE_ADDR
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                    | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
#endif
  ;

  struct perf_event_attr *attr = &(event_info->attr);

  if (pfmu_getEventAttribute(EVNAME_ADDRESS_CENTRIC, attr) < 0) {
    EMSG("Cannot initialize event %s", EVNAME_ADDRESS_CENTRIC);
    return 0; // the kernel has no support for page-faults event
  }

  // set the default attributes for page fault
  perf_util_attr_init(
      EVNAME_ADDRESS_CENTRIC,
      attr,
      period->threshold_type == PERIOD ,  /* use_period      */
      period->threshold_num,              /* should we deliver sample for every page fault ?*/
      sample_type     /* need additional info to gather memory address */
  );
  perf_skid_set_max_precise_ip(attr);

  // ------------------------------------------
  // create metric page-fault
  // ------------------------------------------
  int metric_page_fault = hpcrun_new_metric();

  hpcrun_set_metric_info_and_period(
      metric_page_fault, EVNAME_ADDRESS_CENTRIC,
      MetricFlags_ValFmt_Real, 1, metric_property_none);

  // ------------------------------------------
  // register the event to be created later
  // ------------------------------------------

  METHOD_CALL(self, store_event_and_info,
              attr->config,       /* event id     */
              1,                  /* threshold    */
              metric_page_fault,  /* metric id    */
              event_info          /* info pointer */ );

  return 1;
}



/****
 * register datacentric event
 * the method is designed to be called by the main thread or at least by one thread,
 *  during initialization of the process (before creation of OpenMP threads).
 *
 *  This method will create metrics for different memory events and then the
 *  metrics are read-only, it will not modified by others.
 *  All child threads will use this metric descriptors to record the memory activities.
 */
static int
datacentric_register(sample_source_t *self,
                     event_custom_t  *event,
                     struct event_threshold_s *period)
{
  // ------------------------------------------
  // metric for variable size (in bytes)
  // ------------------------------------------
  metric_variable_size = hpcrun_new_metric();

  hpcrun_set_metric_and_attributes(metric_variable_size,  DATACENTRIC_METRIC_PREFIX  "Size (byte)",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true );

  // ------------------------------------------
  // hardware-specific data centric setup (if supported)
  // ------------------------------------------
  struct event_threshold_s threshold;
  perf_util_get_default_threshold( &threshold );

  int result = datacentric_hw_register(self, event, &threshold);
  if (result == 0)
    return 0;

  // ------------------------------------------
  // Support for address centric (page fault event)
  // ------------------------------------------
  datacentric_register_address_centric(self, event, period);

  // ------------------------------------------
  // mark that data centric has been initialized, ready to be activated
  // ------------------------------------------
  plugin_status = INITIALIZED;

  return 1;
}


/******************************************************************************
 * PUBLIC method definitions
 *****************************************************************************/

void
datacentric_init()
{
  event_custom_t *event_datacentric = hpcrun_malloc(sizeof(event_custom_t));
  event_datacentric->name         = EVNAME_DATACENTRIC;
  event_datacentric->desc         = "Experimental counter: counting the memory latency.";
  event_datacentric->register_fn  = datacentric_register;   // call backs
  event_datacentric->handle_type  = EXCLUSIVE;// call me only for my events

  event_datacentric->post_handler_fn   = datacentric_post_handler;
  event_datacentric->pre_handler_fn    = datacentric_pre_handler;

  event_custom_register(event_datacentric);
}


int
datacentric_is_active()
{
  return (plugin_status == INITIALIZED);
}

int
datacentric_get_metric_variable_size()
{
  return metric_variable_size;
}

