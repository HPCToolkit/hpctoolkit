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
// express or implied warnanties, including, but not limited to, the
// implied warnanties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business internuption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdio.h>



//*****************************************************************************
// libmonitor
//*****************************************************************************

#include <monitor.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "ompt-region-debug.h"

#if REGION_DEBUG

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/queues.h>

#include <hpcrun/memory/hpcrun-malloc.h>




//*****************************************************************************
// types
//*****************************************************************************

typedef struct region_resolve_tracker_s {
  q_element_ptr_t next;
  ompt_region_data_t *region;
  uint64_t region_id;
  int thread_id;
  ompt_notification_t *notification;
} typed_queue_elem(rn);

typedef q_element_ptr_t typed_queue_elem_ptr(rn);



//*****************************************************************************
// variables
//*****************************************************************************

static spinlock_t debuginfo_lock = SPINLOCK_UNLOCKED;

ompt_region_data_t *global_region_list = 0;

// (pending) region notification list
typed_queue_elem_ptr(rn) rn_list;

// region notification freelist
typed_queue_elem_ptr(rn) rn_freelist;


typed_queue_elem_ptr(rn) rn_freelist;

//*****************************************************************************
// private operations
//*****************************************************************************

// instantiate a concurrent queue
typed_queue(rn, cqueue)

// instantiate a serial queue
typed_queue(rn, squeue)

static typed_queue_elem(rn) *
rn_alloc()
{
  typed_queue_elem(rn) *rn = 
    (typed_queue_elem(rn) *) hpcrun_malloc(sizeof(typed_queue_elem(rn)));

  return rn;
}


static typed_queue_elem(rn) *
rn_get()
{
  typed_queue_elem(rn) *rn = 
    (typed_queue_elem_ptr_get(rn, squeue)(&rn_freelist) ? 
     typed_queue_pop(rn, squeue)(&rn_freelist) : rn_alloc());

  return rn;
}


static void
rn_free
(
  typed_queue_elem(rn) *rn
)
{
  typed_queue_push(rn, squeue)(&rn_freelist, rn);
}


static int
rn_matches
(
  typed_queue_elem(rn) *rn,
  ompt_region_data_t *region,
  int thread_id
)
{
  return rn->region == region && rn->thread_id == thread_id;
}


void 
rn_print
(
 char *what,
 typed_queue_elem(rn) *rn
)
{
  printf("region %p id 0x%lx notification %p notification->next %14p thread %3d "
	 "region->region_id 0x%lx      %s\n", rn->region, 
	 rn->region_id, rn->notification, rn->notification->next.next, rn->thread_id, rn->region->region_id, 
	 what);
}
 

static void
rn_queue_drop
(
 ompt_notification_t *notification,
 int thread_id
)
{
  ompt_region_data_t *region = notification->region_data;

  // invariant: cur is pointer to container for next element
  typed_queue_elem_ptr(rn) *cur = &rn_list;

  // invariant: cur is pointer to container for next element
  typed_queue_elem(rn) *next;

  // for each element in the queue 
  for (; (next = typed_queue_elem_ptr_get(rn, squeue)(cur));) { 
    // if a match is found, remove and return it
    if (rn_matches(next, region, thread_id)) {
      typed_queue_elem(rn) *rn = typed_queue_pop(rn, squeue)(cur);

      rn_print("(notify received)", rn);
      rn_free(rn);

      return;
    }

    // preserve invariant that cur is pointer to container for next element
    cur = &typed_queue_elem_ptr_get(rn, squeue)(cur)->next; 
  }

  printf("rn_queue_drop failed: (region %p, id 0x%lx, thread %3d)", 
	 region, notification->region_id, thread_id);
}



//*****************************************************************************
// interface operations
//*****************************************************************************

void
ompt_region_debug_notify_needed
(
 ompt_notification_t *notification
)
{
  int tid = monitor_get_thread_num();
 
  spinlock_lock(&debuginfo_lock);

  typed_queue_elem(rn) *rn = rn_get();

  rn->region = notification->region_data;
  rn->region_id = notification->region_id;
  rn->thread_id = tid;
  rn->notification = notification;

  typed_queue_push(rn, cqueue)(&rn_list, rn);

  rn_print("(notify needed)", rn);

  spinlock_unlock(&debuginfo_lock);
}


void
ompt_region_debug_init
(
 void
)
{
  typed_queue_elem_ptr_set(rn, squeue)(&rn_freelist, 0);
  typed_queue_elem_ptr_set(rn, squeue)(&rn_list, 0);
}


void
ompt_region_debug_notify_received
(
  ompt_notification_t *notification
)
{
  spinlock_lock(&debuginfo_lock);

  int thread_id = monitor_get_thread_num();

  rn_queue_drop(notification, thread_id);

  spinlock_unlock(&debuginfo_lock);
}


void
ompt_region_debug_region_create
(
  ompt_region_data_t* r
)
{
  // region tracking for debugging
  spinlock_lock(&debuginfo_lock);

  r->next_region = global_region_list;
  global_region_list = r;

  spinlock_unlock(&debuginfo_lock);
}


int 
hpcrun_ompt_region_check
(
  void
)
{
   ompt_region_data_t *e = global_region_list;
   while (e) {
     printf("region %p region id 0x%lx call_path = %p queue head = %p\n", 
	    e, e->region_id, e->call_path, atomic_load(&e->queue.head));

     ompt_notification_t *n = (ompt_notification_t *) atomic_load(&e->queue.head);
     while(n) {
       printf("   notification %p region %p region_id 0x%lx threads_queue %p unresolved_cct %p next %p\n",
	      n, n->region_data, n->region_id, n->threads_queue, n->unresolved_cct, n->next.next);
       n = (ompt_notification_t *) n->next.next;
     }
     e = (ompt_region_data_t *) e->next_region;
   } 

   typed_queue_elem(rn) *rn = 
     typed_queue_elem_ptr_get(rn, squeue)(&rn_list);

   int result = rn != 0;

   while (rn) {
     rn_print("(notification pending)", rn);
     rn = typed_queue_elem_ptr_get(rn,squeue)(&rn->next); 
   }

   return result;
}


#endif
