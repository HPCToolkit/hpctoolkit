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
// Copyright ((c)) 2002-2019, Rice University
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


//*****************************************************************************
// system includes
//*****************************************************************************

#include <assert.h>



//*****************************************************************************
// libmonitor
//*****************************************************************************

#include <monitor.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../lib/prof-lean/stdatomic.h"
#include "../safe-sampling.h"
#include "../thread_data.h"
#include "../memory/hpcrun-malloc.h"

#include <hpcrun/safe-sampling.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>
#include <hpcrun/sample_event.h>
#include <printf.h>

#include "ompt-callback.h"
#include "ompt-callstack.h"
#include "ompt-defer.h"
#include "ompt-interface.h"
#include "ompt-region.h"
#include "ompt-task.h"
#include "ompt-thread.h"
#include "ompt.h"



/******************************************************************************
 * macros
 *****************************************************************************/

// FIXME: should use eliding interface rather than skipping frames manually
#define LEVELS_TO_SKIP 2 // skip one level in enclosing OpenMP runtime

/******************************************************************************
 * variables
 *****************************************************************************/

// private freelist from which only thread owner can reused regions
static __thread ompt_data_t* private_region_freelist_head = NULL;



//*****************************************************************************
// debugging support
//*****************************************************************************

#define REGION_DEBUGGING 1

#if REGION_DEBUGGING

typedef struct region_resolve_tracker_s {
  struct region_resolve_tracker_s *next;
  ompt_region_data_t *region;
  int thread_id;
} rr_pending_t;

static spinlock_t debuginfo_lock = SPINLOCK_UNLOCKED;
ompt_region_data_t *global_region_list = 0;
rr_pending_t *rr_pending = 0;
rr_pending_t *rr_freelist = 0;

#endif

//*****************************************************************************
// forward declarations
//*****************************************************************************

static ompt_region_data_t * 
ompt_region_acquire
(
 void
);

static void
ompt_region_release
(
 ompt_region_data_t *r
);


//*****************************************************************************
// private operations
//*****************************************************************************

// initialize support for regions
void
ompt_regions_init
(
 void
)
{
  wfq_init(&public_region_freelist);
}


ompt_region_data_t *
ompt_region_data_new
(
 uint64_t region_id, 
 cct_node_t *call_path
)
{
  ompt_region_data_t* e = ompt_region_acquire();

  e->region_id = region_id;
  e->call_path = call_path;

  wfq_init(&e->queue);

  // parts for freelist
  OMPT_BASE_T_GET_NEXT(e) = NULL;
  e->thread_freelist = &public_region_freelist;
  e->depth = 0;

  return e;
}

//bool
//ompt_region_data_refcnt_update(ompt_region_data_t* region_data, int val)
//{
//  bool result = false;
//  spinlock_lock(&region_data->region_lock);
//  if (region_data){
//    region_data->refcnt += val;
//    // FIXME: TODO: Decide when to delete region data
//    result = true;
//  }
//  spinlock_unlock(&region_data->region_lock);
//  return result;
//}
//
//uint64_t
//ompt_region_data_refcnt_get(ompt_region_data_t* region_data){
//  spinlock_lock(&region_data->region_lock);
//  uint64_t refcnt = region_data->refcnt;
//  spinlock_unlock(&region_data->region_lock);
//  return refcnt;
//}


static void 
ompt_parallel_begin_internal
(
 ompt_data_t *parallel_data,
 int flags
) 
{
  hpcrun_safe_enter();


  ompt_region_data_t* region_data = ompt_region_data_new(hpcrun_ompt_get_unique_id(), NULL);
  parallel_data->ptr = region_data;

  uint64_t region_id = region_data->region_id;
  thread_data_t *td = hpcrun_get_thread_data();

  // old version
//  ompt_data_t *parent_region_info = NULL;
//  int team_size = 0;
//  // FIXED: if we put 0 as previous, it would return the current parallel_data which is inside this function always different than NULL
//  hpcrun_ompt_get_parallel_info(1, &parent_region_info, &team_size);
//  if (parent_region_info == NULL) {
//    // mark the master thread in the outermost region
//    // (the one that unwinds to FENCE_MAIN)
//    td->master = 1;
//  }

  // FIXME vi3: check if this is right
  // the region has not been changed yet
  // that's why we say that the parent region is hpcrun_ompt_get_current_region_data
  ompt_region_data_t *parent_region = hpcrun_ompt_get_current_region_data();
  if (!parent_region) {
    // mark the master thread in the outermost region
    // (the one that unwinds to FENCE_MAIN)
    td->master = 1;
    region_data->depth = 0;
  } else {
    region_data->depth = parent_region->depth + 1;
  }

  if (ompt_eager_context_p()){
     region_data->call_path =
       ompt_parallel_begin_context(region_id, 
				   flags & ompt_parallel_invoker_program);
  }

  hpcrun_safe_exit();

}


static void
ompt_parallel_end_internal
(
 ompt_data_t *parallel_data,    // data of parallel region
 int flags
)
{
  // printf("Passed to internal. \n");
  hpcrun_safe_enter();

  ompt_region_data_t* region_data = (ompt_region_data_t*)parallel_data->ptr;

  if (!ompt_eager_context_p()){
    // check if there is any thread registered that should be notified that region call path is available
    ompt_notification_t* to_notify = (ompt_notification_t*) wfq_dequeue_public(&region_data->queue);

    region_stack_el_t *stack_el = &region_stack[top_index + 1];
    ompt_notification_t *notification = stack_el->notification;
    if (notification->unresolved_cct) {
      // FIXME vi3: consider to combine this if with next
      // calling hpcrun_sample_callpath (by calling ompt_region_context and
      // ompt_region_context_end_region_not_eager) twice is obviously overhead
      ending_region = region_data;
      cct_node_t *prefix = ompt_region_context(region_data->region_id, ompt_scope_end,
                                               flags & ompt_parallel_invoker_program);
      // if combined this if branch with branch of next if
      // we will remove this line
      tmp_end_region_resolve(notification, prefix);
      ending_region = NULL;
    }

    if (to_notify){
      if (region_data->call_path == NULL){
        // FIXME vi3: this is one big hack
        // different function is call for providing callpaths
        // FIXME vi3: also check if this add some ccts somewhere
        // probably does because it call hpcrun_sample_callpath
        // these ccts should not be added underneath thread root
        // we must create them and send them as a region path,
        // but do not insert them in any tree
        ending_region = region_data;
        // need to provide call path, because master did not take a sample inside region
        ompt_region_context_end_region_not_eager(region_data->region_id, ompt_scope_end,
                                     flags & ompt_parallel_invoker_program);
        ending_region = NULL;
      }

      // notify next thread
      wfq_enqueue(OMPT_BASE_T_STAR(to_notify), to_notify->threads_queue);
    }else{
      // if none, you can reuse region
      // this thread is region creator, so it could add to private region's list
      // FIXME vi3: check if you are right
      ompt_region_release(region_data);
      // or should use this
      // wfq_enqueue((ompt_base_t*)region_data, &public_region_freelist);
    }
  }

  // FIXME: vi3: what is this?
  // FIXME: not using team_master but use another routine to
  // resolve team_master's tbd. Only with tasking, a team_master
  // need to resolve itself
  if (ompt_task_full_context_p()) {
    TD_GET(team_master) = 1;
    thread_data_t* td = hpcrun_get_thread_data();
    resolve_cntxt_fini(td);
    TD_GET(team_master) = 0;
  }

  // FIXME: vi3 do we really need to keep this line
  hpcrun_get_thread_data()->region_id = 0;
  hpcrun_safe_exit();

}



/******************************************************************************
 * interface operations
 *****************************************************************************/

void
ompt_parallel_begin
(
 ompt_data_t *parent_task_data,
 const ompt_frame_t *parent_frame,
 ompt_data_t *parallel_data,
 unsigned int requested_team_size,
 int flags,
 const void *codeptr_ra
)
{
  ompt_parallel_begin_internal(parallel_data, flags);
}


void
ompt_parallel_end
(
 ompt_data_t *parallel_data,
 ompt_data_t *task_data,
 int flag,
 const void *codeptr_ra
)
{
  uint64_t parallel_id = parallel_data->value;
  //printf("Parallel end... region id = %lx\n", parallel_id);
  uint64_t task_id = task_data->value;

  ompt_data_t *parent_region_info = NULL;
  int team_size = 0;
  hpcrun_ompt_get_parallel_info(0, &parent_region_info, &team_size);
  uint64_t parent_region_id = parent_region_info->value;

  hpcrun_safe_enter();
  TMSG(DEFER_CTXT, "team end   id=0x%lx task_id=%x ompt_get_parallel_id(0)=0x%lx", parallel_id, task_id,
       parent_region_id);
  hpcrun_safe_exit();
  ompt_parallel_end_internal(parallel_data, flag);
}


void
ompt_implicit_task_internal_begin
(
 ompt_data_t *parallel_data,
 ompt_data_t *task_data,
 unsigned int team_size,
 unsigned int thread_num
)
{
  task_data->ptr = NULL;

  ompt_region_data_t* region_data = (ompt_region_data_t*)parallel_data->ptr;
  cct_node_t *prefix = region_data->call_path;

  task_data->ptr = prefix;

  if (!ompt_eager_context_p()) {
    // FIXME vi3: check if this is fine
    // add current region
    add_region_and_ancestors_to_stack(region_data, thread_num==0);

    // FIXME vi3: move this to add_region_and_ancestors_to_stack
    // Memoization process vi3:
    if (thread_num != 0){
      not_master_region = region_data;
    }

#if 0
    region_stack[top_index].parent_frame = hpcrun_ompt_get_task_frame(1);
#endif
  }
}


void
ompt_implicit_task_internal_end
(
 ompt_data_t *parallel_data,
 ompt_data_t *task_data,
 unsigned int team_size,
 unsigned int thread_num
)
{
  if (!ompt_eager_context_p()) {
    // the only thing we could do (certainly) here is to pop element from the stack
    // pop element from the stack
    pop_region_stack();
  }

}


void
ompt_implicit_task
(
 ompt_scope_endpoint_t endpoint,
 ompt_data_t *parallel_data,
 ompt_data_t *task_data,
 unsigned int team_size,
 unsigned int thread_num
)
{
//    printf("---%p, %d\n", parallel_data, endpoint == ompt_scope_end);
 if (endpoint == ompt_scope_begin)
   ompt_implicit_task_internal_begin(parallel_data,task_data,team_size,thread_num);
 else if (endpoint == ompt_scope_end)
   ompt_implicit_task_internal_end(parallel_data,task_data,team_size,thread_num);
 else
   assert(0);
}


void 
ompt_parallel_region_register_callbacks
(
 ompt_set_callback_t ompt_set_callback_fn
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_callback_parallel_begin,
                    (ompt_callback_t)ompt_parallel_begin);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_callback_parallel_end,
                    (ompt_callback_t)ompt_parallel_end);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_callback_implicit_task,
                                (ompt_callback_t)ompt_implicit_task);
  assert(ompt_event_may_occur(retval));
}


ompt_region_data_t* 
ompt_region_alloc
(
 void
)
{
  ompt_region_data_t* r = (ompt_region_data_t*) hpcrun_malloc(sizeof(ompt_region_data_t));
  return r;
}


static ompt_region_data_t* 
ompt_region_freelist_get
(
 void
)
{
  // FIXME vi3: should in this situation call OMPT_REGION_DATA_T_STAR / Notification / TRL_EL
  // FIXME vi3: I think that call to wfq_dequeue_private in this case should be thread safe
  // but check this one more time
  ompt_region_data_t* r = 
    (ompt_region_data_t*) wfq_dequeue_private(&public_region_freelist,
					      OMPT_BASE_T_STAR_STAR(private_region_freelist_head));
  return r;
}


static void
ompt_region_freelist_put
(
 ompt_region_data_t *r 
)
{
  freelist_add_first(OMPT_BASE_T_STAR(r), OMPT_BASE_T_STAR_STAR(private_region_freelist_head));
}


#if REGION_DEBUGGING
void
ompt_region_debug_chain
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
#endif


ompt_region_data_t*
ompt_region_acquire
(
 void
)
{
  ompt_region_data_t* r = ompt_region_freelist_get();
  if (r == 0) {
    r = ompt_region_alloc();
#if REGION_DEBUGGING
    ompt_region_debug_chain(r);
#endif
  }
  return r;
}


static void
ompt_region_release
(
 ompt_region_data_t *r
)
{
  ompt_region_freelist_put(r);
}


void
rr_queue_push
(
  rr_pending_t **q,
  rr_pending_t *rr
)
{
  rr->next = *q;
  *q = rr;
}


rr_pending_t *
rr_queue_pop
(
  rr_pending_t **q
)
{
  rr_pending_t *rr = 0;

  if (q) {
    rr = *q;
    *q = rr->next;
    rr->next = 0;
  } 

  return rr;
}


rr_pending_t *
rr_alloc()
{
  rr_pending_t *rr = (rr_pending_t *) hpcrun_malloc(sizeof(rr_pending_t));
  return rr;
}


rr_pending_t *
rr_get()
{
  rr_pending_t *rr = rr_freelist ? rr_queue_pop(&rr_freelist) : rr_alloc();
  return rr;
}


void
rr_free
(
  rr_pending_t *rr
)
{
  rr_queue_push(&rr_freelist, rr);
}


int
rr_matches
(
  rr_pending_t *rr,
  ompt_region_data_t *region,
  int thread_id
)
{
  return rr->region == region && rr->thread_id == thread_id;
}


void
rr_queue_drop
(
  ompt_region_data_t *region,
  int thread_id
)
{
  // invariant: cur is pointer to next element
  rr_pending_t **cur = &rr_pending;

  // for each element in the queue 
  for (; *cur;) { 
    // if a match is found, remove and return it
    if (rr_matches(*cur, region, thread_id)) {
      rr_pending_t *rr = rr_queue_pop(cur);

      printf("rr_done region %p (id=0x%lx) thread %d\n", rr->region, region->region_id, rr->thread_id);

      rr_free(rr);

      return;
    }

    // preserve invariant for next element in the list
    cur = &((*cur)->next);
  }

  printf("region resolution queue drop failed q = %p, region = %p, thread = %d\n", &rr_pending, region, thread_id);
}


void
rr_needed
(
  ompt_region_data_t *region
)
{
#if 1
  spinlock_lock(&debuginfo_lock);

  rr_pending_t *rr = rr_get();

  rr->region = region;
  rr->thread_id = monitor_get_thread_num();

  rr_queue_push(&rr_pending, rr);

  printf("rr_needed region %p (id=0x%lx) thread %d\n", region, region->region_id, rr->thread_id);

  spinlock_unlock(&debuginfo_lock);
#endif
}


void
rr_done
(
  ompt_region_data_t *region
)
{
#if 1
  spinlock_lock(&debuginfo_lock);

  int thread_id = monitor_get_thread_num();

  rr_queue_drop(region, thread_id);

  spinlock_unlock(&debuginfo_lock);
#endif
}


void 
hpcrun_ompt_region_check
(
  void
)
{
   ompt_region_data_t *e = global_region_list;
   while (e) {
     printf("region ((ompt_region_data_t *) %p) call_path = %p queue head = %p\n", e, e->call_path, 
            atomic_load(&e->queue.head));
     e = (ompt_region_data_t *) e->next_region;
   } 

   rr_pending_t *rr = rr_pending;
   while (rr) {
     printf("pending region %p thread %d\n", rr->region, rr->thread_id);
     rr = rr->next;
   }
}


void
hpcrun_ompt_region_free
(
 ompt_region_data_t *region_data
)
{
  wfq_enqueue(OMPT_BASE_T_STAR(region_data), region_data->thread_freelist);
}

