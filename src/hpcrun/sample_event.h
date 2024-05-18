// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#include "cct/cct.h"
#include "thread_data.h"
#include "trampoline/common/trampoline.h"
#include "unwind/common/backtrace.h"
#include "messages/messages.h"


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

extern void hpcrun_trace_node(cct_node_t *node);

extern sample_val_t hpcrun_sample_callpath(void *context, int metricId,
                                   hpcrun_metricVal_t metricIncr,
                                   int skipInner, int isSync, sampling_info_t *data);

extern cct_node_t* hpcrun_gen_thread_ctxt(void *context);

extern cct_node_t* hpcrun_sample_callpath_w_bt(void *context,
                                               int metricId, uint64_t metricIncr,
                                               bt_mut_fn bt_fn, bt_fn_arg arg,
                                               int isSync);
#endif // sample_event_h
