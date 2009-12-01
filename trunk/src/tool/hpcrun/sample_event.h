// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#include "cct.h"
#include "thread_data.h"
#include <trampoline/common/trampoline.h>
#include <unwind/common/backtrace.h>


//***************************************************************************

// Whenever a thread enters csprof synchronously via a monitor
// callback, don't allow it to reenter asynchronously via a signal
// handler.  Too much of csprof is not signal-handler safe to allow
// this.  For example, printing debug messages could deadlock if the
// signal hits while holding the MSG lock.
//
// This block is only needed per-thread, so the "suspend_sampling"
// thread data is a convenient place to store this.

static inline void
hpcrun_async_block(void)
{
  TD_GET(suspend_sampling) = 1;
}


static inline void
hpcrun_async_unblock(void)
{
  TD_GET(suspend_sampling) = 0;
}


static inline int
hpcrun_async_is_blocked(void* pc)
{
  return ( (! hpcrun_td_avail()) 
	   || (TD_GET(suspend_sampling) && !ENABLED(ASYNC_RISKY))
	   || hpcrun_trampoline_interior(pc)
	   || hpcrun_trampoline_at_entry(pc) );
}


//***************************************************************************

extern bool _hpcrun_sampling_disabled; // private!

static inline bool
hpcrun_is_sampling_disabled()
{
  return _hpcrun_sampling_disabled;
}


void
hpcrun_disable_sampling();

void
hpcrun_drop_sample();

cct_node_t* hpcrun_sample_callpath(void *context, int metricId, uint64_t metricIncr, 
				   int skipInner, int isSync);

#if 0
cct_node_t* hpcrun_sample_callpath_w_bt(void* context, int metric_id, cct_metric_data_t datum,
					bt_fn* get_bt)
#endif
#endif /* sample_event_h */
