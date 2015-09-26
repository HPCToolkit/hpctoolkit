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

#include <hpcrun/safe-sampling.h>
#include <hpcrun/thread_data.h>



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



/******************************************************************************
 * private operations
 *****************************************************************************/

static void 
ompt_parallel_begin_internal(
  ompt_parallel_id_t region_id, 
  int levels_to_skip,
  ompt_invoker_t invoker
) 
{
  hpcrun_safe_enter();
  thread_data_t *td = hpcrun_get_thread_data();
  uint64_t parent_region_id = hpcrun_ompt_get_parallel_id(0);

  if (parent_region_id == 0) {
    // mark the master thread in the outermost region 
    // (the one that unwinds to FENCE_MAIN)
    td->master = 1;
  }

  cct_node_t *callpath = 
   ompt_parallel_begin_context(region_id, ++levels_to_skip, 
                               invoker == ompt_invoker_program);

  assert(region_id != 0);
  ompt_region_map_insert((uint64_t) region_id, callpath);

  if (!td->master) {
    if (td->outer_region_id == 0) {
      // this callback occurs in the context of the parent, so
      // the enclosing parallel region is available at level 0
      td->outer_region_id = parent_region_id;
      td->outer_region_context = NULL;
    } else {
      // check whether we should update the outermost id
      // if the outer-most region with td->outer_region_id is an 
      // outer region of current region,
      // then no need to update outer-most id in the td
      // else if it is not an outer region of the current region, 
      // we have to update the outer-most id 
      int i=0;
      uint64_t outer_id = 0;
  
      outer_id = hpcrun_ompt_get_parallel_id(i);
      while (outer_id > 0) {
        if (outer_id == td->outer_region_id) break;
  
        outer_id = hpcrun_ompt_get_parallel_id(++i);
      }
      if (outer_id == 0){
	// parent region id
        td->outer_region_id = hpcrun_ompt_get_parallel_id(0); 
        td->outer_region_context = 0;
        TMSG(DEFER_CTXT, "enter a new outer region 0x%lx (start team)", 
	     td->outer_region_id);
      }
    }
  }

  hpcrun_safe_exit();
}


static void 
ompt_parallel_end_internal( 
  ompt_parallel_id_t parallel_id,    /* id of parallel region       */
  int levels_to_skip,
  ompt_invoker_t invoker
)
{
  hpcrun_safe_enter();
  ompt_region_map_entry_t *record = ompt_region_map_lookup(parallel_id);
  if (record) {
    if (ompt_region_map_entry_refcnt_get(record) > 0) {
      // associate calling context with region if it is not already present
      if (ompt_region_map_entry_callpath_get(record) == NULL) {
	ompt_region_map_entry_callpath_set
            (record, 
             ompt_region_context(parallel_id, ompt_context_end, 
                                 ++levels_to_skip, invoker == ompt_invoker_program));
      }
    } else {
      ompt_region_map_refcnt_update(parallel_id, 0L);
    }
  } else {
    assert(0);
  }
  // FIXME: not using team_master but use another routine to 
  // resolve team_master's tbd. Only with tasking, a team_master
  // need to resolve itself
  if (ENABLED(OMPT_TASK_FULL_CTXT)) {
    TD_GET(team_master) = 1;
    thread_data_t* td = hpcrun_get_thread_data();
    resolve_cntxt_fini(td);
    TD_GET(team_master) = 0;
  }
  hpcrun_safe_exit();
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
  ompt_task_id_t parent_task_id, 
  ompt_frame_t *parent_task_frame,
  ompt_parallel_id_t region_id, 
  uint32_t requested_team_size, 
  void *parallel_fn,
  ompt_invoker_t invoker
)
{
#if 0
  hpcrun_safe_enter();
  TMSG(DEFER_CTXT, "team create  id=0x%lx parallel_fn=%p ompt_get_parallel_id(0)=0x%lx", region_id, parallel_fn, 
       hpcrun_ompt_get_parallel_id(0));
  hpcrun_safe_exit();
#endif
  int levels_to_skip = LEVELS_TO_SKIP;
  ompt_parallel_begin_internal(region_id, ++levels_to_skip, invoker); 
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
  ompt_parallel_id_t parallel_id,    /* id of parallel region       */
  ompt_task_id_t task_id,            /* id of task                  */ 
  ompt_invoker_t invoker             /* runtime invokes all tasks?  */
)
{
  hpcrun_safe_enter();
  TMSG(DEFER_CTXT, "team end   id=0x%lx task_id=%x ompt_get_parallel_id(0)=0x%lx", parallel_id, task_id, 
       hpcrun_ompt_get_parallel_id(0));
  hpcrun_safe_exit();
  int levels_to_skip = LEVELS_TO_SKIP;
  ompt_parallel_end_internal(parallel_id, ++levels_to_skip, invoker);
}
#endif


void 
ompt_parallel_region_register_callbacks(
  ompt_set_callback_t ompt_set_callback_fn
)
{
  int retval;
  retval = ompt_set_callback_fn(ompt_event_parallel_begin,
                    (ompt_callback_t)ompt_parallel_begin);
  assert(ompt_event_may_occur(retval));

  retval = ompt_set_callback_fn(ompt_event_parallel_end,
                    (ompt_callback_t)ompt_parallel_end);
  assert(ompt_event_may_occur(retval));
}
