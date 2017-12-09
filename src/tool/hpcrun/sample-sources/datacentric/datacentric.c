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
#include <hpcrun/thread_data.h>

#include <messages/messages.h>

#include "datacentric.h"
#include "data_tree.h"

#include "varmap.h"  // static variable mapping
#include "place_folder.h"

#include "sample-sources/perf/event_custom.h"


/******************************************************************************
 * macros 
 *****************************************************************************/
#define EVNAME_DATACENTRIC "DATACENTRIC"


/******************************************************************************
 * prototypes and forward declaration
 *****************************************************************************/

FUNCTION_FOLDER(static)

/******************************************************************************
 * data structure
 *****************************************************************************/

typedef union perf_mem_data_src perf_mem_data_src_t;

struct perf_data_src_mem_lvl_s {
  u32 ld_uncache, ld_io, ld_fbhit, ld_l1hit, ld_l2hit, lcl_hitm, ld_llchit; 
  u32 lcl_dram, ld_shared, rmt_dram, ld_excl;

  u64 loads, stores;
};

/******************************************************************************
 * local variables
 *****************************************************************************/

static struct perf_data_src_mem_lvl_s  __thread data_mem = {
  .ld_uncache = 0, .ld_io     = 0, .ld_fbhit = 0, .ld_l1hit  = 0, .ld_l2hit = 0, 
  .lcl_hitm   = 0, .ld_llchit = 0, .lcl_dram = 0, .ld_shared = 0, .rmt_dram = 0,
  .ld_excl    = 0, .loads     = 0, .stores   = 0 
};

static int metric_memload = -1;

/******************************************************************************
 * PRIVATE Function implementation
 *****************************************************************************/

#define P(a, b) PERF_MEM_##a##_##b

static void
datacentric_handler(event_thread_t *current, void *context, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if ( (current == NULL)      ||  (mmap_data == NULL) ||
       (sv.sample_node == NULL))
    return;
 
  cct_node_t *node = NULL;
  void *start, *end;

  if (mmap_data->addr) {
    // memory information exists
    node = splay_lookup((void*) mmap_data->addr, &start, &end);

    if (node == NULL) {
      return;
    }
    if (node == (void*)DATA_STATIC_CONTEXT) {
      // looking for the static variables
      cct_node_t *root   = hpcrun_cct_get_root(sv.sample_node);
      cct_node_t *cursor = hpcrun_insert_special_node(root, POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(static));
    }
  }

  if (mmap_data->data_src == 0) {
    return;
  }

  perf_mem_data_src_t data_src = (perf_mem_data_src_t)mmap_data->data_src;

  if (data_src.mem_op & P(OP, LOAD))
    data_mem.loads++;
  else if (data_src.mem_op & P(OP, STORE))
    data_mem.stores++;

  TMSG(DATACENTRIC, "data_src: %x, op: %d, lvl: %d, snoop: %d, loads: %d, stores: %d", 
		  data_src, data_src.mem_op, data_src.mem_lvl, data_src.mem_snoop, data_mem.loads, data_mem.stores);

  u64 lvl   = data_src.mem_lvl;
  u64 snoop = data_src.mem_snoop;

  if ( lvl & P(LVL, HIT) ) {
    
    cct_metric_data_increment(metric_memload, node,
      	                      (cct_metric_data_t){.i = 1});

    if (lvl & P(LVL, UNC)) data_mem.ld_uncache++;
    if (lvl & P(LVL, IO))  data_mem.ld_io++;
    if (lvl & P(LVL, LFB)) data_mem.ld_fbhit++;
    if (lvl & P(LVL, L1 )) data_mem.ld_l1hit++;
    if (lvl & P(LVL, L2 )) data_mem.ld_l2hit++;
    if (lvl & P(LVL, L3 )) {
      if (snoop & P(SNOOP, HITM))
        data_mem.lcl_hitm++;
      else
        data_mem.ld_llchit++;
    }

    if (lvl & P(LVL, LOC_RAM)) {
      data_mem.lcl_dram++;
      if (snoop & P(SNOOP, HIT))
        data_mem.ld_shared++;
      else
        data_mem.ld_excl++;
    }
#if 0
    bool mrem = 0; //data_src.mem_remote;
    if ((lvl & P(LVL, REM_RAM1)) ||
        (lvl & P(LVL, REM_RAM2)) ||
        mrem) {
      data_mem.rmt_dram++;
      if (snoop & P(SNOOP, HIT))
        data_mem.ld_shared++;
      else
        data_mem.ld_excl++;
    }
#endif

  }
}


static int
datacentric_register(event_custom_t *event)
{
  struct event_threshold_s threshold = init_default_count();
  event_info_t *event_desc = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
  if (event_desc == NULL)
    return -1;

  memset(event_desc, 0, sizeof(event_info_t));

  // ------------------------------------------
  // hardware-specific data centric setup (if supported)
  // ------------------------------------------
  int result = datacentric_hw_register(event_desc, &threshold);
  if (result == 0)
    return 0;

  // ------------------------------------------
  // Memory load metric
  // ------------------------------------------
  metric_memload = hpcrun_new_metric();
  hpcrun_set_metric_info(metric_memload, "MEM-LOAD");

  // ------------------------------------------
  // create metric data centric
  // ------------------------------------------
  int metric = hpcrun_new_metric();

  event_desc->metric        = metric;
  event_desc->metric_custom = event;
  event_desc->metric_desc   = hpcrun_set_metric_info_and_period(
        metric, EVNAME_DATACENTRIC,
        MetricFlags_ValFmt_Int, 1, metric_property_none);

  return event_desc_add(event_desc);
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
  event_datacentric->handler_fn   = datacentric_handler;
  event_datacentric->metric_index = 0;        // these fields to be defined later
  event_datacentric->metric_desc  = NULL;     // these fields to be defined later
  event_datacentric->handle_type  = EXCLUSIVE;// call me only for my events

  event_custom_register(event_datacentric);
}

