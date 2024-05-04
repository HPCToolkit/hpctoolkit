// SPDX-FileCopyrightText: 2016-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef UNRESOLVED_H
#define UNRESOLVED_H
#define UNRESOLVED_ROOT -50
#define UNRESOLVED -100

#include "thread_data.h"
#include "cct/cct_bundle.h"
#include "epoch.h"

static inline cct_node_t**
hpcrun_get_tbd_cct(void)
{
  return &((hpcrun_get_thread_epoch()->csdata).unresolved_root);
}

static inline cct_node_t**
hpcrun_get_process_stop_cct(void)
{
  return &((hpcrun_get_thread_epoch()->csdata).tree_root);
}

static inline cct_node_t**
hpcrun_get_top_cct(void)
{
  return &((hpcrun_get_thread_epoch()->csdata).top);
}
#endif // UNRESOLVED_H
