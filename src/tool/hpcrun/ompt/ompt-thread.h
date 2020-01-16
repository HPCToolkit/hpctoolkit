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
// Copyright ((c)) 2002-2020, Rice University
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

// thread's notifications queueus
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


// stack that contais all nested parallel region
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
