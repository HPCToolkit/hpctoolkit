// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

//
// directed blame shifting for locks, critical sections, ...
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <ucontext.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpctoolkit.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>

#include <hpcrun/sample-sources/blame-shift/blame-map.h>
#include <hpcrun/sample-sources/blame-shift/directed.h>
#include <hpcrun/sample-sources/blame-shift/metric_info.h>



/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

void
directed_blame_accept(void *arg, uint64_t obj)
{
  directed_blame_info_t *bi = (directed_blame_info_t *) arg;

  if (hpctoolkit_sampling_is_active()) {
    uint64_t blame = blame_map_get_blame(bi->blame_table, obj);

    if (blame > 0) {
      // attribute blame to the current context
      ucontext_t uc;
      getcontext(&uc);
      hpcrun_safe_enter();
      hpcrun_sample_callpath(&uc, bi->blame_metric_id, blame, bi->levels_to_skip, 1);
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
