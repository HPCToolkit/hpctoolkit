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
// Copyright ((c)) 2002-2016, Rice University
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

#include "sample_sources_registered.h"

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
};


/******************************************************************************
 * constructor 
 *****************************************************************************/

static void reg_fn_name(void) __attribute__ ((constructor));

static void
reg_fn_name(void)
{
  hpcrun_ss_register(&obj_name());
}

#undef ss_str
#undef reg_fn_name

#undef _st
#undef _st1
