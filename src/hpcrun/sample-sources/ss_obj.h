// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#include "../sample_sources_registered.h"

#undef _TPx
#undef _T3
#undef _make_id
#undef _st
#undef _st1

#define _TPx(a,b,c) a ## b ## c
#define _T3(a, b, c) _TPx(a, b, c)
#define _make_id(tpl) _T3 tpl

#define _st(n) # n
#define _st1(n) _st(n)

#undef obj_name
#undef ss_str
#undef reg_fn_name

#ifndef ss_sort_order
#define ss_sort_order  50
#endif

#include "ss-obj-name.h"

#define obj_name() SS_OBJ_NAME(ss_name)
#define ss_str  _st1(ss_name)
#define reg_fn_name _make_id((,ss_name,_obj_reg))

sample_source_t obj_name() = {
  // common methods

  .add_event     = hpcrun_ss_add_event,
  .store_event   = hpcrun_ss_store_event,
  .store_metric_id = hpcrun_ss_store_metric_id,
  .get_event_str = hpcrun_ss_get_event_str,
  .started       = hpcrun_ss_started,

  // specific methods

  .init = init,
  .thread_init = thread_init,
  .thread_init_action = thread_init_action,
  .start = start,
  .thread_fini_action = thread_fini_action,
  .stop  = stop,
  .shutdown = shutdown,
  .supports_event = supports_event,
  .process_event_list = process_event_list,
  .finalize_event_list = finalize_event_list,
  .gen_event_set = gen_event_set,
  .display_events = display_events,

  // data
  .evl = {
    .evl_spec = {[0] = '\0'},
    .nevents = 0
  },
  .sel_idx   = -1,
  .name = ss_str,
  .cls  = ss_cls,
  .state = UNINIT,
  .sort_order = ss_sort_order,
};


/******************************************************************************
 * constructor
 *****************************************************************************/

void
SS_OBJ_CONSTRUCTOR(ss_name)(void)
{
  hpcrun_ss_register(&obj_name());
}

#undef ss_str
#undef reg_fn_name

#undef _st
#undef _st1
