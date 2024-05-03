// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef OMPT_QUEUES_H
#define OMPT_QUEUES_H

//*****************************************************************************
// Description:
//
//   interface for sequential and concurrent LIFO queues (AKA stacks)
//
//*****************************************************************************



//*****************************************************************************
// macros types
//*****************************************************************************

#include "../../common/lean/queues.h"

#include "ompt-types.h"



//*****************************************************************************
// macros types
//*****************************************************************************

//*****************************************************************************
// interface functions
//*****************************************************************************

//-----------------------------------------------------------------------------
// wait-free queue manipulation
//-----------------------------------------------------------------------------

void
wfq_set_next_pending
(
 ompt_base_t *element
);


ompt_base_t *
wfq_get_next
(
 ompt_base_t *element
);


void
wfq_init
(
 ompt_wfq_t *queue
);


void
wfq_enqueue
(
 ompt_base_t *new,
 ompt_wfq_t *queue
);


ompt_base_t *
wfq_dequeue_public
(
 ompt_wfq_t *public_queue
);


ompt_base_t *
wfq_dequeue_private
(
 ompt_wfq_t *public_queue,
 ompt_base_t **private_queue
);


#endif
