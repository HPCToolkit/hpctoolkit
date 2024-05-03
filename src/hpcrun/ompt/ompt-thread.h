// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_THREAD_H__
#define __OMPT_THREAD_H__


//******************************************************************************
// local includes
//******************************************************************************

#include "ompt-types.h"



//******************************************************************************
// macros
//******************************************************************************

#define MAX_NESTING_LEVELS 128


//******************************************************************************
// global data
//******************************************************************************

// list of regions for which a thread is registered that are not yet resolved
extern __thread ompt_trl_el_t* registered_regions;

// thread's notifications queues
//
// public thread's notification queue
extern __thread ompt_wfq_t threads_queue;

// private thread's notification queue
extern __thread ompt_data_t* private_threads_queue;

// freelists

// thread's list of notification that can be reused
extern __thread ompt_notification_t* notification_freelist_head;

// thread's list of region's where thread was registered and resolved them
extern __thread ompt_trl_el_t* thread_region_freelist_head;

// region's free lists

// public freelist where all thread's can enqueue region_data to be reused
extern __thread ompt_wfq_t public_region_freelist;


// stack that contains all nested parallel region
// FIXME vi3: 128 levels are supported
extern __thread region_stack_el_t region_stack[];
extern  __thread int top_index;

// Memoization process vi3:
extern __thread ompt_region_data_t *not_master_region;
extern __thread cct_node_t *cct_not_master_region;


// FIXME vi3: just a temp solution
extern __thread ompt_region_data_t *ending_region;
extern __thread ompt_frame_t *top_ancestor_frame;

// number of unresolved regions
extern __thread int unresolved_cnt;


//******************************************************************************
// interface operations
//******************************************************************************

void
ompt_thread_type_set
(
  ompt_thread_t ttype
);


ompt_thread_t
ompt_thread_type_get
(
  void
);


_Bool
ompt_thread_computes
(
 void
);


region_stack_el_t *
top_region_stack
(
 void
);


region_stack_el_t *
pop_region_stack
(
 void
);


void push_region_stack
(
 ompt_notification_t* notification,
 bool took_sample,
 bool team_master
);


void
clear_region_stack
(
 void
);


int
is_empty_region_stack
(
 void
);

#endif
