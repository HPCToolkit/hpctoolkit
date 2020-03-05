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
// Copyright ((c)) 2002-2019, Rice University
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

#include <linux/perf_event.h>

#include <hpcrun/metrics.h>
#include <hpcrun/cct2metrics.h>

#include <hpcrun/cct/cct.h>

#include <hpcrun/sample-sources/perf/perf-util.h>

#define MAX_FORMULA_CHAR  100

/******************************************************************************
 * data structure
 *****************************************************************************/

typedef union perf_mem_data_src perf_mem_data_src_t;


struct perf_mem_metric {
  int locks;            /* count of 'lock' transactions       */
  int nomap;            /* count of load/stores with no phys adrs */

  int memload;          /* count of all loads in trace        */
  int memload_miss;     /* loads miss                         */

  int memld_fbhit;      /* count of loads hitting Fill Buffer */
  int memld_l1hit;      /* count of loads that hit L1D        */
  int memld_l2hit;      /* count of loads that hit L2D        */
  int memld_uncache;    /* loads to uncacheable address       */
  int memld_io;         /* loads to io address                */
  int memld_excl;       /* exclusive loads, rmt/lcl DRAM - snp none/miss */

  int memld_shared;     /* shared loads, rmt/lcl DRAM - snp hit */

  int memstore;         /* count of all stores in trace       */
  int memstore_l1_hit;  /* count of stores that hit L1D       */
  int memstore_l1_miss; /* count of stores that miss L1D      */
  int memstore_uncache; /* stores to uncacheable address      */

  int mem_lcl_hitm;     /* count of loads with local HITM     */
  int mem_rmt_hitm;     /* count of loads with remote HITM    */

  int mem_rmt_hit;      /* count of loads with remote hit clean; */

  int miss_dram_lcl;    /* count of loads miss to local DRAM */
  int miss_dram_rmt;    /* count of loads miss to remote DRAM */

  int memllc_hit;      /* llc hit */
  int memllc_miss;     /* llc mis  (derived metrics) */
};


/******************************************************************************
 * local variables
 *****************************************************************************/

static struct perf_mem_metric metric;
static kind_info_t *data_kind;

#if 0
static char formula_llc_miss[MAX_FORMULA_CHAR];
static char formula_percent_l1_hit[MAX_FORMULA_CHAR];
static char formula_percent_l2_hit[MAX_FORMULA_CHAR];
static char formula_percent_l3_hit[MAX_FORMULA_CHAR];

static char FORMAT_PERCENT[] = "6.2f \%";
#endif


/******************************************************************************
 * PRIVATE Function implementation
 *****************************************************************************/

#define P(a, b) PERF_MEM_##a##_##b


static void
datacentric_record_metric(int metric_id, cct_node_t *cct_node, cct_node_t *cct_datacentric,
                          cct_metric_data_t value)
{
  cct_metric_data_increment(metric_id, cct_node, value);
  cct_metric_data_increment(metric_id, cct_datacentric, value);

  thread_data_t *td = hpcrun_get_thread_data();
  metric_aux_info_t *info_aux = &(td->core_profile_trace_data.perf_event_info[metric_id]);
  info_aux->num_samples++;
}

static void
datacentric_record_load_mem(cct_node_t *node, cct_node_t *datacentric_node,
                        perf_mem_data_src_t *data_src)
{
  u64 lvl   = data_src->mem_lvl;
  u64 snoop = data_src->mem_snoop;

  // ---------------------------------------------------
  // number of load operations
  // ---------------------------------------------------
  cct_metric_data_t value = (cct_metric_data_t){.i = 1};
  datacentric_record_metric(metric.memload, node, datacentric_node, value );

  // ---------------------------------------------------
  // local load hit
  // ---------------------------------------------------
  if ( lvl & P(LVL, HIT) ) {

    if (lvl & P(LVL, UNC)) {
      // uncached memory
      datacentric_record_metric(metric.memld_uncache, node, datacentric_node, value);
    }
    if (lvl & P(LVL, IO))  {
      // I/O memory
      datacentric_record_metric(metric.memld_io, node, datacentric_node, value);
    }
    if (lvl & P(LVL, LFB)) {
      // life fill buffer
      datacentric_record_metric(metric.memld_fbhit, node, datacentric_node, value);
    }
    if (lvl & P(LVL, L1 )) {
      // level 1 cache
      datacentric_record_metric(metric.memld_l1hit, node, datacentric_node, value);
    }
    if (lvl & P(LVL, L2 )) {
      // level 2 cache
      datacentric_record_metric(metric.memld_l2hit, node, datacentric_node, value);
    }
    if (lvl & P(LVL, L3 )) {                      // level 3 cache
      if (snoop & P(SNOOP, HITM)) {
        // loads with local HITM
        datacentric_record_metric(metric.mem_lcl_hitm, node, datacentric_node, value);
      } else {
        // loads that hit LLC
        datacentric_record_metric(metric.memllc_hit, node, datacentric_node, value);
      }
    }

    if (lvl & P(LVL, LOC_RAM)) {
      // loads miss to local DRAM
      datacentric_record_metric(metric.miss_dram_lcl, node, datacentric_node, value);

      if (snoop & P(SNOOP, HIT)) {
        datacentric_record_metric(metric.memld_shared, node, datacentric_node, value);
      }else {
        // exclusive loads, rmt/lcl DRAM - snp none/miss
        datacentric_record_metric(metric.memld_excl, node, datacentric_node, value);
      }
    }

    if ((lvl & P(LVL, REM_RAM1)) ||
        (lvl & P(LVL, REM_RAM2))) {

      // loads miss to remote DRAM
      datacentric_record_metric(metric.miss_dram_rmt, node, datacentric_node, value);

      if (snoop & P(SNOOP, HIT)) {
        datacentric_record_metric(metric.memld_shared, node, datacentric_node, value);
      }
      else {
        // exclusive loads, rmt/lcl DRAM - snp none/miss
        datacentric_record_metric(metric.memld_excl, node, datacentric_node, value);
      }
    }
  }

  // ---------------------------------------------------
  // remote load hit
  // ---------------------------------------------------
  if ((lvl & P(LVL, REM_CCE1)) ||
      (lvl & P(LVL, REM_CCE2))) {
    if (snoop & P(SNOOP, HIT)) {
      datacentric_record_metric(metric.mem_rmt_hit, node, datacentric_node, value);
    }
    else if (snoop & P(SNOOP, HITM)) {
      datacentric_record_metric(metric.mem_rmt_hitm, node, datacentric_node, value);
    }
  }

  // ---------------------------------------------------
  // load miss
  // ---------------------------------------------------
  if ((lvl & P(LVL, MISS))) {
    datacentric_record_metric(metric.memload_miss, node, datacentric_node, value);
  }
}

static void
datacentric_record_store_mem( cct_node_t *node, cct_node_t *datacentric_node,
                          perf_mem_data_src_t *data_src)
{
  cct_metric_data_t value = (cct_metric_data_t){.i = 1};
  datacentric_record_metric(metric.memstore, node, datacentric_node, value);

  u64 lvl   = data_src->mem_lvl;

  if (lvl & P(LVL, HIT)) {
    if (lvl & P(LVL, UNC)) {
      datacentric_record_metric(metric.memstore_uncache, node, datacentric_node,
                                  value);
    }
    if (lvl & P(LVL, L1 )) {
      datacentric_record_metric(metric.memstore_l1_hit, node, datacentric_node,
                                  value);
    }
  }
  if (lvl & P(LVL, MISS))
    if (lvl & P(LVL, L1)) {
      datacentric_record_metric(metric.memstore_l1_miss, node, datacentric_node,
                                  value);
    }
}



void
pmu_handler_init()
{
  data_kind = hpcrun_metrics_new_kind();

  // ------------------------------------------
  // Memory load metric
  // ------------------------------------------
  metric.memload = hpcrun_set_new_metric_info(data_kind, "MEM-Load");

  metric.memld_fbhit = hpcrun_set_new_metric_info_and_period(data_kind,  "MEM-Load-FBhit",
      MetricFlags_ValFmt_Int, 1, metric_property_none );

  metric.memld_l1hit = hpcrun_set_new_metric_info_and_period(data_kind,  "MEM-Load-L1hit",
      MetricFlags_ValFmt_Int, 1, metric_property_none );

  metric.memld_l2hit = hpcrun_set_new_metric_info_and_period(data_kind,  "MEM-Load-L2hit",
      MetricFlags_ValFmt_Int, 1, metric_property_none );

  // ------------------------------------------
  // percent of cache l1 and l2 hit
  // ------------------------------------------
#if 0
  int percent_l1_hit = hpcrun_new_metric();
  metric_desc_t* metric_l1_hit_desc = hpcrun_set_metric_and_attributes(percent_l1_hit,  "% Load L1 Hit",
                      MetricFlags_ValFmt_Real, 1, metric_property_none, true /* disable show*/, false );

  snprintf(formula_percent_l1_hit, MAX_FORMULA_CHAR, "100 * $%d / $%d", metric.memld_l1hit, metric.memload);
  metric_l1_hit_desc->formula = formula_percent_l1_hit;
  metric_l1_hit_desc->format  = FORMAT_PERCENT;

  int percent_l2_hit = hpcrun_new_metric();
  metric_desc_t* metric_l2_hit_desc = hpcrun_set_metric_and_attributes(percent_l2_hit,  "% Load L2 Hit",
                      MetricFlags_ValFmt_Real, 1, metric_property_none, true /* disable show*/, false );

  snprintf(formula_percent_l2_hit, MAX_FORMULA_CHAR, "100 * $%d / ($%d-$%d)",
                                      metric.memld_l2hit, metric.memload, metric.memld_l1hit);
  metric_l2_hit_desc->formula = formula_percent_l2_hit;
  metric_l2_hit_desc->format  = FORMAT_PERCENT;
#endif

  // ------------------------------------------
  // last level cache (llc) hit
  // ------------------------------------------

  metric.memllc_hit = hpcrun_set_new_metric_info_and_period(data_kind,  "MEM-LLC-hit",
        MetricFlags_ValFmt_Int, 1, metric_property_none );

  // ------------------------------------------
  // hitm local
  // ------------------------------------------

  metric.mem_lcl_hitm = hpcrun_set_new_metric_info_and_period(data_kind, "MEM-hitm-local",
      MetricFlags_ValFmt_Int, 1, metric_property_none );

  // ------------------------------------------
  // percent l3 hit
  // ------------------------------------------

#if 0
  int percent_llc_hit = hpcrun_new_metric();
  metric_desc_t* metric_llc_hit_desc = hpcrun_set_metric_and_attributes(percent_llc_hit,  "% Load LLC Hit",
                      MetricFlags_ValFmt_Real, 1, metric_property_none, true /* disable show*/, false );

  snprintf(formula_percent_l3_hit, MAX_FORMULA_CHAR, "100 * ($%d+$%d) / ($%d-$%d-$%d)",
                                      metric.mem_lcl_hitm, metric.memllc_hit,
                                      metric.memload, metric.memld_l1hit, metric.memld_l2hit);
  metric_llc_hit_desc->formula = formula_percent_l3_hit;
  metric_llc_hit_desc->format  = FORMAT_PERCENT;
#endif

  // ------------------------------------------
  // Local and remote memory
  // ------------------------------------------

  metric.mem_rmt_hitm = hpcrun_set_new_metric_info(data_kind, "MEM-hitm-rmt");

  // ------------------------------------------
  // Remote hit
  // ------------------------------------------
  metric.mem_rmt_hit = hpcrun_set_new_metric_info(data_kind, "MEM-rmt-hit");

  // ------------------------------------------
  // DRAM
  // ------------------------------------------
  metric.miss_dram_lcl = hpcrun_set_new_metric_info(data_kind, "DRAM-miss-to-lcl");

  metric.miss_dram_rmt = hpcrun_set_new_metric_info(data_kind, "DRAM-miss-to-rmt");

  // ------------------------------------------
  // Memory load miss metric
  // ------------------------------------------
  metric.memload_miss = hpcrun_set_new_metric_info(data_kind, "MEM-Load-miss");

  // ------------------------------------------
  // llc miss
  // ------------------------------------------

  /**
   * llc_miss =
   *   = miss_dram_lcl + miss_dram_rmt + mem_rmt_hit + mem_rmt_hitm
   */
#if 0
  metric.memllc_miss = hpcrun_new_metric();

  metric_desc_t* metric_desc = hpcrun_set_metric_info(metric.memllc_miss, "MEM-LLC-miss");

  snprintf(formula_llc_miss, MAX_FORMULA_CHAR, "$%d+$%d+$%d+$%d",
                            metric.miss_dram_lcl, metric.miss_dram_rmt,
                            metric.mem_rmt_hit, metric.mem_rmt_hitm);
  metric_desc->formula = formula_llc_miss;
  metric_desc->format  = FORMAT_PERCENT;
#endif
  // ------------------------------------------
  // misc
  // ------------------------------------------
  metric.memld_io = hpcrun_set_new_metric_info_and_period(data_kind, "MEM-Load I/O Address",
      MetricFlags_ValFmt_Int, 1, metric_property_none);

  metric.memld_shared = hpcrun_set_new_metric_info_and_period(data_kind, "MEM-Load MESI State Shared",
      MetricFlags_ValFmt_Int, 1, metric_property_none);

  metric.memld_uncache = hpcrun_set_new_metric_info_and_period(data_kind, "MEM-Load Uncache",
      MetricFlags_ValFmt_Int, 1, metric_property_none);

  metric.memld_excl = hpcrun_set_new_metric_info_and_period(data_kind, "MEM-Load Exclusive",
      MetricFlags_ValFmt_Int, 1, metric_property_none);

  // ------------------------------------------
  // Memory store metric
  // ------------------------------------------
  metric.memstore = hpcrun_set_new_metric_info(data_kind, "MEM-Store");

  metric.memstore_l1_hit = hpcrun_set_new_metric_info(data_kind, "MEM-Store-L1hit");

  metric.memstore_l1_miss = hpcrun_set_new_metric_info(data_kind, "MEM-Store-L1miss");

  metric.memstore_uncache = hpcrun_set_new_metric_info(data_kind, "MEM-Store-Uncache");

  hpcrun_close_kind(data_kind);
}


// called when a sample occurs
void
pmu_handler_callback(perf_mmap_data_t *mmap_data,
                       cct_node_t *datacentric_node,
                       cct_node_t *sample_node)
{
  if (mmap_data->data_src == 0) {
    return ;
  }

  // ---------------------------------------------------------
  // data source information exist:
  // - add metrics about load and store of the memory
  // ---------------------------------------------------------

  perf_mem_data_src_t data_src = (perf_mem_data_src_t)mmap_data->data_src;

  if (data_src.mem_op & P(OP, LOAD)) {
    datacentric_record_load_mem( datacentric_node, sample_node, &data_src );
  }
  if (data_src.mem_op & P(OP, STORE)) {
    datacentric_record_store_mem( datacentric_node, sample_node, &data_src );
  }
}


