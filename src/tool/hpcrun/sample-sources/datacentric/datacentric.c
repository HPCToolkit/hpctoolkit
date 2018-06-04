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

#include "include/queue.h"  // linked-list

#include "cct_addr.h"       // struct var_addr_s
#include "datacentric.h"
#include "data_tree.h"
#include "env.h"

#include "place_folder.h"

#include "sample-sources/perf/event_custom.h"


/******************************************************************************
 * macros 
 *****************************************************************************/


/******************************************************************************
 * prototypes and forward declaration
 *****************************************************************************/

//FUNCTION_FOLDER(static)

/******************************************************************************
 * data structure
 *****************************************************************************/

typedef union perf_mem_data_src perf_mem_data_src_t;

struct perf_data_src_mem_lvl_s {
  u32 nr_entries;

  u32 locks;               /* count of 'lock' transactions */
  u32 store;               /* count of all stores in trace */
  u32 st_uncache;          /* stores to uncacheable address */
  u32 st_noadrs;           /* cacheable store with no address */
  u32 st_l1hit;            /* count of stores that hit L1D */
  u32 st_l1miss;           /* count of stores that miss L1D */
  u32 load;                /* count of all loads in trace */
  u32 ld_excl;             /* exclusive loads, rmt/lcl DRAM - snp none/miss */
  u32 ld_shared;           /* shared loads, rmt/lcl DRAM - snp hit */
  u32 ld_uncache;          /* loads to uncacheable address */
  u32 ld_io;               /* loads to io address */
  u32 ld_miss;             /* loads miss */
  u32 ld_noadrs;           /* cacheable load with no address */
  u32 ld_fbhit;            /* count of loads hitting Fill Buffer */
  u32 ld_l1hit;            /* count of loads that hit L1D */
  u32 ld_l2hit;            /* count of loads that hit L2D */
  u32 ld_llchit;           /* count of loads that hit LLC */
  u32 lcl_hitm;            /* count of loads with local HITM  */
  u32 rmt_hitm;            /* count of loads with remote HITM */
  u32 tot_hitm;            /* count of loads with local and remote HITM */
  u32 rmt_hit;             /* count of loads with remote hit clean; */
  u32 lcl_dram;            /* count of loads miss to local DRAM */
  u32 rmt_dram;            /* count of loads miss to remote DRAM */
  u32 nomap;               /* count of load/stores with no phys adrs */
  u32 noparse;             /* count of unparsable data sources */
};

struct perf_mem_metric {
  int memload;
  int memload_miss;

  int memstore;
  int memstore_l1_hit;
  int memstore_l1_miss;

  int memllc_miss;
};

/******************************************************************************
 * local variables
 *****************************************************************************/


static struct perf_mem_metric metric;

/******************************************************************************
 * PRIVATE Function implementation
 *****************************************************************************/

#define P(a, b) PERF_MEM_##a##_##b

static void
datacentric_record_load_mem(cct_node_t *node,
                        perf_mem_data_src_t *data_src)
{
  struct perf_data_src_mem_lvl_s  data_mem;

  u64 lvl   = data_src->mem_lvl;
  u64 snoop = data_src->mem_snoop;

  memset(&data_mem, 0, sizeof(struct perf_data_src_mem_lvl_s));

  // ---------------------------------------------------
  // number of load operations
  // ---------------------------------------------------
  cct_metric_data_increment(metric.memload, node,
                            (cct_metric_data_t){.i = 1});

  // ---------------------------------------------------
  // local load hit
  // ---------------------------------------------------
  if ( lvl & P(LVL, HIT) ) {

    if (lvl & P(LVL, UNC)) data_mem.ld_uncache++; // uncached memory
    if (lvl & P(LVL, IO))  data_mem.ld_io++;      // I/O memory
    if (lvl & P(LVL, LFB)) data_mem.ld_fbhit++;   // life fill buffer
    if (lvl & P(LVL, L1 )) data_mem.ld_l1hit++;   // level 1 cache
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

    if ((lvl & P(LVL, REM_RAM1)) ||
        (lvl & P(LVL, REM_RAM2))) {

      data_mem.rmt_dram++;                        // loads miss to remote DRAM
      if (snoop & P(SNOOP, HIT))
        data_mem.ld_shared++;
      else
        data_mem.ld_excl++;
    }
  }

  // ---------------------------------------------------
  // remote load hit
  // ---------------------------------------------------
  if ((lvl & P(LVL, REM_CCE1)) ||
      (lvl & P(LVL, REM_CCE2))) {
    if (snoop & P(SNOOP, HIT)) {
      data_mem.rmt_hit++;
    }
    else if (snoop & P(SNOOP, HITM)) {
      data_mem.rmt_hitm++;
      data_mem.tot_hitm++;
    }
  }

  // ---------------------------------------------------
  // llc miss
  // ---------------------------------------------------
  u64 llc_miss =  data_mem.lcl_dram + data_mem.rmt_dram +
                  data_mem.rmt_hit  + data_mem.rmt_hitm ;
  if (llc_miss > 0) {
    cct_metric_data_increment(metric.memllc_miss, node,
                              (cct_metric_data_t){.i = llc_miss});
  }

  // ---------------------------------------------------
  // load miss
  // ---------------------------------------------------
  if ((lvl & P(LVL, MISS))) {
    cct_metric_data_increment(metric.memload_miss, node,
                              (cct_metric_data_t){.i = 1});
  }
}

static void
datacentric_record_store_mem( cct_node_t *node,
                          perf_mem_data_src_t *data_src)
{
  struct perf_data_src_mem_lvl_s  data_mem;

  memset(&data_mem, 0, sizeof(struct perf_data_src_mem_lvl_s));

  cct_metric_data_increment(metric.memstore, node,
                            (cct_metric_data_t){.i = 1});

  u64 lvl   = data_src->mem_lvl;

  if (lvl & P(LVL, HIT)) {
    if (lvl & P(LVL, UNC)) data_mem.st_uncache++;
    if (lvl & P(LVL, L1 )) {
      data_mem.st_l1hit++;
      cct_metric_data_increment(metric.memstore_l1_hit, node,
                                  (cct_metric_data_t){.i = 1});
    }
  }
  if (lvl & P(LVL, MISS))
    if (lvl & P(LVL, L1)) {
      data_mem.st_l1miss++;
      cct_metric_data_increment(metric.memstore_l1_miss, node,
                                  (cct_metric_data_t){.i = 1});
    }
}



/***
 * manage signal handler for datacentric event
 */
static void
datacentric_handler(event_info_t *current, void *context, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if ( (current == NULL)      ||  (mmap_data == NULL) ||
       (sv.sample_node == NULL))
    return;
 
  cct_node_t *node = sv.sample_node;
  void *start = NULL, *end = NULL;

  // ---------------------------------------------------------
  // memory information exists:
  // - check if we have this variable address in our database
  // - if it's in our database, add it to the cct node
  // ---------------------------------------------------------
  if (mmap_data->addr) {
    datatree_info_t *info = datatree_splay_lookup((void*) mmap_data->addr, &start, &end);

    if (info == NULL || start == NULL) {
      // unknown or not in our database
      return;
    }

    // if necessary, add the start of the variable address to the cct node
    hpcrun_cct_var_add(node, start, info->context);

    hpcrun_cct_set_node_memaccess(node);
  }

  if (mmap_data->data_src == 0) {
    return;
  }

  // ---------------------------------------------------------
  // data source information exist:
  // - add metrics about load and store of the memory
  // ---------------------------------------------------------

  perf_mem_data_src_t data_src = (perf_mem_data_src_t)mmap_data->data_src;

  if (data_src.mem_op & P(OP, LOAD)) {
    datacentric_record_load_mem( node, &data_src );
  }
  if (data_src.mem_op & P(OP, STORE)) {
    datacentric_record_store_mem( node, &data_src );
  }

  TMSG(DATACENTRIC, "data-fd: %d, lvl: %d, op: %d", current->attr.config, data_src.mem_lvl, data_src.mem_op );
}


/****
 * register datacentric event
 */
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
  metric.memload = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memload, "MEM-Load");

  // ------------------------------------------
  // Memory store metric
  // ------------------------------------------
  metric.memstore = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore, "MEM-Store");

  metric.memstore_l1_hit = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore_l1_hit, "MEM-Store-l1hit");

  metric.memstore_l1_miss = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore_l1_miss, "MEM-Store-l1miss");

  // ------------------------------------------
  // Memory load miss metric
  // ------------------------------------------
  metric.memload_miss = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memload_miss, "MEM-Load-miss");

  // ------------------------------------------
  // Memory llc load metric
  // ------------------------------------------
  metric.memllc_miss = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memllc_miss, "MEM-LLC-miss");



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

