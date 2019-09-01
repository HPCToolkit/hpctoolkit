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
// Copyright ((c)) 2002-2018, Rice University
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

#include <linux/version.h>

#include <hpcrun/messages/messages.h>
#include <utilities/arch/cpuid.h>

#include "sample-sources/perf/perf_event_open.h"
#include "sample-sources/perf/perf-util.h"
#include "sample-sources/perf/perf_skid.h"
#include "sample-sources/perf/perfmon-util.h"
#include "sample-sources/perf/event_custom.h"

#include "datacentric.h"
#include "pmu_x86.h"


#define MAX_FORMULA_CHAR  100

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
  u32 rmt_hit;             /* count of loads with remote hit clean; */
  u32 lcl_dram;            /* count of loads miss to local DRAM */
  u32 rmt_dram;            /* count of loads miss to remote DRAM */
  u32 nomap;               /* count of load/stores with no phys adrs */
  u32 noparse;             /* count of unparsable data sources */
};

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

static char formula_llc_miss[MAX_FORMULA_CHAR];
static char formula_percent_l1_hit[MAX_FORMULA_CHAR];
static char formula_percent_l2_hit[MAX_FORMULA_CHAR];
static char formula_percent_l3_hit[MAX_FORMULA_CHAR];

static char FORMAT_PERCENT[] = "6.2f \%";

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
  struct perf_data_src_mem_lvl_s  data_mem;

  u64 lvl   = data_src->mem_lvl;
  u64 snoop = data_src->mem_snoop;

  memset(&data_mem, 0, sizeof(struct perf_data_src_mem_lvl_s));

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
  struct perf_data_src_mem_lvl_s  data_mem;

  memset(&data_mem, 0, sizeof(struct perf_data_src_mem_lvl_s));

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

static void
create_metric_addons()
{
  // ------------------------------------------
  // Memory load metric
  // ------------------------------------------
  metric.memload = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memload, "MEM-Load");

  metric.memld_fbhit = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_fbhit,  "MEM-Load-FBhit",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true );

  metric.memld_l1hit = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_l1hit,  "MEM-Load-L1hit",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true );

  metric.memld_l2hit = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_l2hit,  "MEM-Load-L2hit",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true );

  // ------------------------------------------
  // percent of cache l1 and l2 hit
  // ------------------------------------------

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

  // ------------------------------------------
  // last level cache (llc) hit
  // ------------------------------------------

  metric.memllc_hit = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memllc_hit,  "MEM-LLC-hit",
        MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true );

  // ------------------------------------------
  // hitm local
  // ------------------------------------------

  metric.mem_lcl_hitm = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.mem_lcl_hitm, "MEM-hitm-local",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true );

  // ------------------------------------------
  // percent l3 hit
  // ------------------------------------------

  int percent_llc_hit = hpcrun_new_metric();
  metric_desc_t* metric_llc_hit_desc = hpcrun_set_metric_and_attributes(percent_llc_hit,  "% Load LLC Hit",
                      MetricFlags_ValFmt_Real, 1, metric_property_none, true /* disable show*/, false );

  snprintf(formula_percent_l3_hit, MAX_FORMULA_CHAR, "100 * ($%d+$%d) / ($%d-$%d-$%d)",
                                      metric.mem_lcl_hitm, metric.memllc_hit,
                                      metric.memload, metric.memld_l1hit, metric.memld_l2hit);
  metric_llc_hit_desc->formula = formula_percent_l3_hit;
  metric_llc_hit_desc->format  = FORMAT_PERCENT;

  // ------------------------------------------
  // Local and remote memory
  // ------------------------------------------

  metric.mem_rmt_hitm = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.mem_rmt_hitm, "MEM-hitm-rmt");

  // ------------------------------------------
  // Remote hit
  // ------------------------------------------
  metric.mem_rmt_hit = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.mem_rmt_hit, "MEM-rmt-hit");

  // ------------------------------------------
  // DRAM
  // ------------------------------------------
  metric.miss_dram_lcl = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.miss_dram_lcl, "DRAM-miss-to-lcl");

  metric.miss_dram_rmt = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.miss_dram_rmt, "DRAM-miss-to-rmt");

  // ------------------------------------------
  // Memory load miss metric
  // ------------------------------------------
  metric.memload_miss = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memload_miss, "MEM-Load-miss");

  // ------------------------------------------
  // llc miss
  // ------------------------------------------

  /**
   * llc_miss =
   *   = miss_dram_lcl + miss_dram_rmt + mem_rmt_hit + mem_rmt_hitm
   */
  metric.memllc_miss = hpcrun_new_metric();

  metric_desc_t* metric_desc = hpcrun_set_metric_info(metric.memllc_miss, "MEM-LLC-miss");

  snprintf(formula_llc_miss, MAX_FORMULA_CHAR, "$%d+$%d+$%d+$%d",
                            metric.miss_dram_lcl, metric.miss_dram_rmt,
                            metric.mem_rmt_hit, metric.mem_rmt_hitm);
  metric_desc->formula = formula_llc_miss;
  metric_desc->format  = FORMAT_PERCENT;

  // ------------------------------------------
  // misc
  // ------------------------------------------
  metric.memld_io = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_io, "Load I/O Address",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true);

  metric.memld_shared = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_shared, "Load MESI State Shared",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true);

  metric.memld_uncache = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_uncache, "Load Uncache",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true);

  metric.memld_excl = hpcrun_new_metric();
  hpcrun_set_metric_and_attributes(metric.memld_excl, "Load Exclusive",
      MetricFlags_ValFmt_Int, 1, metric_property_none, false /* disable show*/, true);

  // ------------------------------------------
  // Memory store metric
  // ------------------------------------------
  metric.memstore = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore, "MEM-Store");

  metric.memstore_l1_hit = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore_l1_hit, "MEM-Store-L1hit");

  metric.memstore_l1_miss = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore_l1_miss, "MEM-Store-L1miss");

  metric.memstore_uncache = hpcrun_new_metric();
  hpcrun_set_metric_info(metric.memstore_uncache, "MEM-Store-Uncache");
}


// called when a sample occurs
void
datacentric_hw_handler(perf_mmap_data_t *mmap_data,
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


// called to create events by the main datacentric plugin
int
datacentric_hw_register(sample_source_t *self, event_custom_t *event,
                        struct event_threshold_s *period)
{
  int size = sizeof(pmu_events)/sizeof(struct pmu_config_s);
  u64 sample_type =   PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME
                    | PERF_SAMPLE_IP     | PERF_SAMPLE_ADDR
                    | PERF_SAMPLE_CPU    | PERF_SAMPLE_TID
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                    | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT
#endif
           ;

  int num_pmu = 0;
  cpu_type_t cpu_type = get_cpuid();

  for(int i=0; i<size ; i++) {

    if (pmu_events[i].cpu != cpu_type)
      continue;

    struct perf_event_attr event_attr;
    memset(&event_attr, 0, sizeof(event_attr));

    if (pfmu_getEventAttribute(pmu_events[i].event, &event_attr) < 0) {
      EMSG("Cannot initialize event %s", pmu_events[i].event);
      continue;
    }

    //set_default_perf_event_attr(event_attr, period);
    bool is_period = period->threshold_type == PERIOD;
    perf_util_attr_init(pmu_events[i].event, &event_attr, is_period, period->threshold_num, sample_type);
    perf_skid_set_max_precise_ip(&event_attr);

    num_pmu++;

    // ------------------------------------------
    // create metric data centric
    // ------------------------------------------
    int metric = hpcrun_new_metric();
    hpcrun_set_metric_info_and_period(
          metric, pmu_events[i].event,
          MetricFlags_ValFmt_Real, 1, metric_property_none);

    // ------------------------------------------
    // Register the event to the global list
    // ------------------------------------------
    event_info_t *einfo  = (event_info_t*) hpcrun_malloc(sizeof(event_info_t));
    einfo->metric_custom = event;
    einfo->id            = pmu_events[i].event;

    memcpy(&einfo->attr, &event_attr, sizeof(struct perf_event_attr));

    METHOD_CALL(self, store_event_and_info,
                event_attr.config,     /* event id     */
                1,              /* threshold    */
                metric,         /* metric id    */
                einfo           /* info pointer */ );

  }
  if (num_pmu > 0)
    create_metric_addons();

  return num_pmu;
}
