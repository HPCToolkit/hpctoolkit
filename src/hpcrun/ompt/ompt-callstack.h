// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_CALLSTACK_H__
#define __OMPT_CALLSTACK_H__


//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../cct/cct.h"

#include "omp-tools.h"



//******************************************************************************
// interface functions
//******************************************************************************

void ompt_callstack_init
(
 void
);


cct_node_t *
ompt_region_context_eager
(
 uint64_t region_id,
 ompt_scope_endpoint_t se_type,
 int adjust_callsite
);


cct_node_t *
ompt_parallel_begin_context
(
 ompt_id_t region_id,
 int adjust_callsite
);


cct_node_t *
ompt_region_root
(
 cct_node_t *_node
);


void
ompt_record_thread_type
(
 ompt_thread_t type
);


void
ompt_region_context_lazy
(
 uint64_t region_id,
 ompt_scope_endpoint_t se_type,
 int adjust_callsite
);


int
ompt_eager_context_p
(
 void
);

#endif
