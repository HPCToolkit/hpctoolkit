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

#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#include "cct.h"
#include "thread_data.h"
#include <trampoline/common/trampoline.h>
#include <unwind/common/backtrace.h>
#include <messages/messages.h>


//***************************************************************************

// The async blocks are now superseded by the hpcrun_safe_enter() and
// hpcrun_safe_exit() functions.  See: safe-sampling.h

//***************************************************************************


extern bool private_hpcrun_sampling_disabled; // private!

static inline bool
hpcrun_is_sampling_disabled(void)
{
  return private_hpcrun_sampling_disabled;
}

static inline void
hpcrun_disable_sampling(void)
{
  TMSG(SPECIAL,"Sampling disabled");
  private_hpcrun_sampling_disabled = true;
}

static inline void
hpcrun_enable_sampling(void)
{
  TMSG(SPECIAL,"Sampling disabled");
  private_hpcrun_sampling_disabled = false;
}

extern void hpcrun_drop_sample(void);


typedef struct sample_val_s {
  cct_node_t* sample_node; // CCT leaf representing innermost call path frame
  cct_node_t* trace_node;  // CCT leaf representing trace record
} sample_val_t;


static inline void
hpcrun_sample_val_init(sample_val_t* x)
{
  //memset(x, 0, sizeof(*x));
  x->sample_node = 0;
  x->trace_node = 0;
}


extern sample_val_t hpcrun_sample_callpath(void *context, int metricId, uint64_t metricIncr, 
				   int skipInner, int isSync);

extern cct_node_t* hpcrun_gen_thread_ctxt(void *context);

extern cct_node_t* hpcrun_sample_callpath_w_bt(void *context,
					       int metricId, uint64_t metricIncr, 
					       bt_mut_fn bt_fn, bt_fn_arg arg,
					       int isSync);
#endif // sample_event_h
