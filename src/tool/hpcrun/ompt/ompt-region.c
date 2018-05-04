/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "ompt-callback.h"
#include "ompt-callstack.h"
#include "ompt-defer.h"
#include "ompt-interface.h"
#include "ompt-region.h"
#include "ompt-region-map.h"
#include "ompt-task.h"
#include "ompt-thread.h"

#include <hpcrun/safe-sampling.h>
#include <hpcrun/thread_data.h>




#include <hpcrun/cct/cct.h>
#include <hpcrun/sample_event.h>


/******************************************************************************
 * macros
 *****************************************************************************/

// FIXME: should use eliding interface rather than skipping frames manually
#define LEVELS_TO_SKIP 2 // skip one level in enclosing OpenMP runtime


/******************************************************************************
 * external declarations 
 *****************************************************************************/

extern int omp_get_level(void);
extern int omp_get_thread_num(void);
extern int omp_get_max_threads(void);


/******************************************************************************
 * private operations
 *****************************************************************************/

ompt_region_data_t *
ompt_region_data_new(uint64_t region_id, cct_node_t *call_path)
{
  // old version of allocating
  // ompt_region_data_t *e;
  // e = (ompt_region_data_t *)hpcrun_malloc(sizeof(ompt_region_data_t));

  // new version
  ompt_region_data_t* e = hpcrun_ompt_region_alloc();

  e->region_id = region_id;
  e->refcnt = 0;
  e->call_path = call_path;
//  spinlock_unlock(&e->region_lock);
  atomic_init(&e->head, ompt_notification_null);
  // parts for freelist
  e->next_freelist = NULL;
  e->thread_freelist = &public_region_freelist;
  return e;
}

//bool
//ompt_region_data_refcnt_update(ompt_region_data_t* region_data, int val)
//{
//  bool result = false;
//  spinlock_lock(&region_data->region_lock);
//  if(region_data){
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
ompt_parallel_begin_internal(
  ompt_data_t *parallel_data,
  int levels_to_skip,
  ompt_invoker_t invoker
) 
{
  hpcrun_safe_enter();

  ompt_region_data_t* region_data = ompt_region_data_new(hpcrun_ompt_get_unique_id(), NULL);
  parallel_data->ptr = region_data;

  uint64_t region_id = region_data->region_id;
  thread_data_t *td = hpcrun_get_thread_data();

  ompt_data_t *parent_region_info = NULL;
  int team_size = 0;
  // FIXED: if we put 0 as previous, it would return the current parallel_data which is inside this function always different than NULL
  hpcrun_ompt_get_parallel_info(1, &parent_region_info, &team_size);
  if (parent_region_info == NULL) {
    // mark the master thread in the outermost region
    // (the one that unwinds to FENCE_MAIN)
    td->master = 1;
  }

  if(ompt_eager_context){
     region_data->call_path =
     ompt_parallel_begin_context(region_id, ++levels_to_skip,
                                 invoker == ompt_invoker_program);
  }

  hpcrun_safe_exit();

}


static void
ompt_parallel_end_internal(
    ompt_data_t *parallel_data,    /* data of parallel region       */
    int levels_to_skip,
    ompt_invoker_t invoker
)
{
//  printf("Passed to internal. \n");
  hpcrun_safe_enter();




//  ompt_region_map_entry_t *record = ompt_region_map_lookup(parallel_id);

  ompt_region_data_t* region_data = (ompt_region_data_t*)parallel_data->ptr;
  // If we want to print all registered notifications
//  ompt_notification_t* head = atomic_load(&region_data->head);
//  int i = 0;
//  for(; head && i<20; i++, head = head->next){
//    printf("%d. %p\n", i, head);
//  }

  if(!ompt_eager_context){
    // check if there is any thread registered
    if(atomic_load(&region_data->head)){
      region_data->call_path = ompt_region_context(region_data->region_id, ompt_context_end,
                                   ++levels_to_skip, invoker == ompt_invoker_program);
      // notify next thread
      ompt_notification_t* to_notify = dequeue_region(region_data);
      enqueue_thread(to_notify->threads_queue, to_notify);
//      printf("Current head: %p\n", atomic_load(&region_data->head));

    }
  }

  ompt_region_data_t* record =(ompt_region_data_t*)parallel_data->ptr;

//  if (record) {
//    if (ompt_region_data_refcnt_get(record) > 0 ) {
//      //printf("Refcnt = %d\n", ompt_region_map_entry_refcnt_get(record));
//      // associate calling context with region if it is not already present
//      if (record->call_path == NULL) {
//        record->call_path = ompt_region_context(record->region_id, ompt_context_end,
//                                 ++levels_to_skip, invoker == ompt_invoker_program);
//      }
//    } else {
//      ompt_region_data_refcnt_update(record, 0L);
//    }
//  } else {
//    assert(0);
//  }
//
//
  // FIXME: not using team_master but use another routine to
  // resolve team_master's tbd. Only with tasking, a team_master
  // need to resolve itself
  if (ompt_task_full_context) {
    TD_GET(team_master) = 1;
    thread_data_t* td = hpcrun_get_thread_data();
    resolve_cntxt_fini(td);
    TD_GET(team_master) = 0;
  }


  hpcrun_get_thread_data()->region_id = 0;
  hpcrun_safe_exit();

  //printf("Parallel region id: %lx.\n", parallel_data->value);
}




/******************************************************************************
 * interface operations
 *****************************************************************************/

#ifdef OMPT_V2013_07 
void 
ompt_parallel_begin(
  ompt_data_t  *parent_task_data,   /* tool data for parent task   */
  ompt_frame_t *parent_task_frame,  /* frame data of parent task   */
  ompt_parallel_id_t parallel_id    /* id of parallel region       */
)
{
  int levels_to_skip = LEVELS_TO_SKIP;
  ompt_parallel_begin_internal(parallel_id, ++level_to_skip, 0);
}
#else

void
ompt_parallel_begin(
  ompt_data_t *parent_task_data,
  const ompt_frame_t *parent_frame,
  ompt_data_t *parallel_data,
  unsigned int requested_team_size,
  ompt_invoker_t invoker,
  const void *codeptr_ra
)
{
  //printf("Parallel begin...\n");
  //parallel_data->value = hpcrun_ompt_get_unique_id();
  //printf("Parallel id = %d\n", parallel_data->value);

#if 0
  hpcrun_safe_enter();
  TMSG(DEFER_CTXT, "team create  id=0x%lx parallel_fn=%p ompt_get_parallel_id(0)=0x%lx", region_id, parallel_fn,
       hpcrun_ompt_get_parallel_info_id(0));
  hpcrun_safe_exit();
#endif
  int levels_to_skip = LEVELS_TO_SKIP;
  ompt_parallel_begin_internal(parallel_data, ++levels_to_skip, invoker);
}

#endif

#ifdef OMPT_V2013_07 
void 
ompt_parallel_end(
  ompt_data_t  *parent_task_data,   /* tool data for parent task   */
  ompt_frame_t *parent_task_frame,  /* frame data of parent task   */
  ompt_parallel_id_t parallel_id    /* id of parallel region       */
  )
{
  int levels_to_skip = LEVELS_TO_SKIP;
  ompt_parallel_end_internal(parallel_id, ++levels_to_skip);
}

#else



void
ompt_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  ompt_invoker_t invoker,
  const void *codeptr_ra
)
{
//  printf("Parallel end...\n");

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
  int levels_to_skip = LEVELS_TO_SKIP;
  ompt_parallel_end_internal(parallel_data, ++levels_to_skip, invoker);
}


#endif

void
ompt_implicit_task_internal_begin(
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  unsigned int team_size,
  unsigned int thread_num
)
{

  task_data->ptr = NULL;

//  thread_data_t *td = hpcrun_get_thread_data();

  ompt_region_data_t* region_data = (ompt_region_data_t*)parallel_data->ptr;
  cct_node_t *prefix = region_data->call_path;
  thread_data_t* td = hpcrun_get_thread_data();
  td->current_parallel_data = parallel_data;
//  if(!TD_GET(master)){
//    prefix = hpcrun_cct_insert_path_return_leaf(
//      td->core_profile_trace_data.epoch->csdata.tree_root,
//      prefix);
//  }

  task_data->ptr = prefix;



//  printf("REGION DATA IMPLICIT_TASK_BEGIN: %p\n", region_data);


//  td->region_id = parallel_data->value;
//  td->outer_region_context = prefix;

}

void
ompt_implicit_task_internal_end(
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  unsigned int team_size,
  unsigned int thread_num
)
{
  //thread_data_t* td = hpcrun_get_thread_data();
//  printf("Implicit task end...\n");

}


void
ompt_implicit_task(
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  unsigned int team_size,
  unsigned int thread_num)
{
 if(endpoint == ompt_scope_begin)
   ompt_implicit_task_internal_begin(parallel_data,task_data,team_size,thread_num);
 else if (endpoint == ompt_scope_end)
   ompt_implicit_task_internal_end(parallel_data,task_data,team_size,thread_num);
 else
   assert(0);
}


void 
ompt_parallel_region_register_callbacks(
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




