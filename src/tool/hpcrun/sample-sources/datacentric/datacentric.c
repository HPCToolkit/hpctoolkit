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

  u32 st_uncache;          /* stores to uncacheable address */
  u32 st_noadrs;           /* cacheable store with no address */
  u32 st_l1hit;            /* count of stores that hit L1D */
  u32 st_l1miss;           /* count of stores that miss L1D */

  u64 loads, stores;
};

/******************************************************************************
 * local variables
 *****************************************************************************/

static struct perf_data_src_mem_lvl_s  __thread data_mem = {
  .ld_uncache = 0, .ld_io     = 0, .ld_fbhit = 0, .ld_l1hit  = 0, .ld_l2hit = 0, 
  .lcl_hitm   = 0, .ld_llchit = 0, .lcl_dram = 0, .ld_shared = 0, .rmt_dram = 0,
  .ld_excl    = 0, .loads     = 0,

  .st_uncache = 0, .st_noadrs = 0, .st_l1hit = 0, .st_l1miss = 0,
  .stores   = 0
};

static int metric_memload  = -1;
static int metric_memstore = -1;


/******************************************************************************
 * PRIVATE Function implementation
 *****************************************************************************/

#define P(a, b) PERF_MEM_##a##_##b

static void
datacentric_record_load_mem(cct_node_t *node,
                        perf_mem_data_src_t *data_src)
{

  u64 lvl   = data_src->mem_lvl;
  u64 snoop = data_src->mem_snoop;

  data_mem.loads++;

  cct_metric_data_increment(metric_memload, node,
                            (cct_metric_data_t){.i = 1});

  if ( lvl & P(LVL, HIT) ) {

    if (lvl & P(LVL, UNC)) data_mem.ld_uncache++; // uncached memory
    if (lvl & P(LVL, IO))  data_mem.ld_io++;      // I/O memory
    if (lvl & P(LVL, LFB)) data_mem.ld_fbhit++;   // life fill buffer
    if (lvl & P(LVL, L1 )) data_mem.ld_l1hit++;   // level 1 cacje
    if (lvl & P(LVL, L2 )) data_mem.ld_l2hit++;   // level 2 cache
    if (lvl & P(LVL, L3 )) {                      // level 3 cache
      if (snoop & P(SNOOP, HITM))
        data_mem.lcl_hitm++;                      // loads with local HITM
      else
        data_mem.ld_llchit++;                     // loads that hit LLC
    }

    if (lvl & P(LVL, LOC_RAM)) {
      data_mem.lcl_dram++;                        // loads miss to local DRAM
      if (snoop & P(SNOOP, HIT))
        data_mem.ld_shared++;                     // shared loads, rmt/lcl DRAM - snp hit
      else
        data_mem.ld_excl++;                       // exclusive loads, rmt/lcl DRAM - snp none/miss
    }
    bool mrem = 0; //data_src.mem_remote;
    if ((lvl & P(LVL, REM_RAM1)) ||
        (lvl & P(LVL, REM_RAM2)) ||
        mrem) {
      data_mem.rmt_dram++;                        // loads miss to remote DRAM
      if (snoop & P(SNOOP, HIT))
        data_mem.ld_shared++;
      else
        data_mem.ld_excl++;
    }
  }
}

static void
datacentric_record_store_mem( cct_node_t *node,
                          perf_mem_data_src_t *data_src)
{
  data_mem.stores++;

  cct_metric_data_increment(metric_memstore, node,
                            (cct_metric_data_t){.i = 1});

}

static void
datacentric_handler(event_info_t *current, void *context, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if ( (current == NULL)      ||  (mmap_data == NULL) ||
       (sv.sample_node == NULL))
    return;
 
  cct_node_t *node = NULL;
  void *start, *end;

  if (mmap_data->addr) {
    // memory information exists
    datainfo_t *info = splay_lookup((void*) mmap_data->addr, &start, &end);

    if (info == NULL) {
      // unknown
      return;
    }
    if (info->context == (void*)DATA_STATIC_CONTEXT) {
      // looking for the static variables
      cct_node_t *root   = hpcrun_cct_get_root(sv.sample_node);
      node = hpcrun_insert_special_node(root, POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(static));
    } else {
      node = info->context;
    }
  }

  if (mmap_data->data_src == 0) {
    return;
  }

  perf_mem_data_src_t data_src = (perf_mem_data_src_t)mmap_data->data_src;

  if (data_src.mem_op & P(OP, LOAD)) {
    datacentric_record_load_mem( node, &data_src );
  } else if (data_src.mem_op & P(OP, STORE)) {
    datacentric_record_store_mem( node, &data_src );
  }

  TMSG(DATACENTRIC, "data_src %p : %x, op: %d, lvl: %d, snoop: %d, loads: %d, stores: %d",
		  mmap_data->addr, data_src, data_src.mem_op, data_src.mem_lvl, data_src.mem_snoop, data_mem.loads, data_mem.stores);
}


static int
datacentric_register(sample_source_t *self,
                     event_custom_t  *event,
                     struct event_threshold_s *period)
{
  struct event_threshold_s threshold;
  perf_util_get_default_threshold( &threshold );

  // ------------------------------------------
  // hardware-specific data centric setup (if supported)
  // ------------------------------------------
  int result = datacentric_hw_register(self, event, &threshold);
  if (result == 0)
    return 0;

  // ------------------------------------------
  // Memory load metric
  // ------------------------------------------
  metric_memload = hpcrun_new_metric();
  hpcrun_set_metric_info(metric_memload, "MEM-LOAD");

  // ------------------------------------------
  // Memory store metric
  // ------------------------------------------
  metric_memstore = hpcrun_new_metric();
  hpcrun_set_metric_info(metric_memstore, "MEM-STORE");

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
  event_datacentric->handler_fn   = datacentric_handler;
  event_datacentric->handle_type  = EXCLUSIVE;// call me only for my events

  event_custom_register(event_datacentric);
}

