// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// directed blame shifting for locks, critical sections, ...
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#define _GNU_SOURCE

#include <ucontext.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "../../cct/cct.h"
#include "../../safe-sampling.h"
#include "../../sample_event.h"
#include "../../start-stop.h"

#include "blame-map.h"
#include "directed.h"
#include "metric_info.h"



/******************************************************************************
 * interface operations for clients
 *****************************************************************************/

void
directed_blame_accept(void *arg, uint64_t obj)
{
  directed_blame_info_t *bi = (directed_blame_info_t *) arg;

  if (hpcrun_sampling_is_active()) {
    uint64_t blame = blame_map_get_blame(bi->blame_table, obj);

    if (blame > 0) {
      // attribute blame to the current context
      ucontext_t uc;
      getcontext(&uc);
      hpcrun_safe_enter();
      hpcrun_metricVal_t blame_incr = {.i = blame};
      hpcrun_sample_callpath(&uc, bi->blame_metric_id, blame_incr, bi->levels_to_skip, 1, NULL);
      hpcrun_safe_exit();
    }
  }
}


void
directed_blame_sample(void *arg, int metric_id, cct_node_t *node,
                      int metric_incr)
{
  directed_blame_info_t *bi = (directed_blame_info_t *) arg;
  int metric_period;

  if (!metric_is_timebase(metric_id, &metric_period)) return;

  uint64_t obj_to_blame = bi->get_blame_target();
  if (obj_to_blame) {
    uint32_t metric_value = (uint32_t) (metric_period * metric_incr);
    blame_map_add_blame(bi->blame_table, obj_to_blame, metric_value);
    if (bi->wait_metric_id) {
      cct_metric_data_increment(bi->wait_metric_id, node,
                                (cct_metric_data_t){.i = metric_value});
    }
  }
}
