#ifndef defer_cntxt_h
#define defer_cntxt_h

#include <ompt.h>

#include <hpcrun/thread_data.h>
/*
 * this interface is used for lazy callpath resolution 
*/

/* check whether the lazy resolution is needed in an unwind */
int  need_defer_cntxt();

/* resolve the contexts */
void resolve_cntxt();
void resolve_cntxt_fini(thread_data_t*);
void resolve_other_cntxt(thread_data_t *thread_data);

uint64_t is_partial_resolve(cct_node_t *prefix);

//deferred region ID assignment
void init_region_id();

// export registration interfaces for ompt
void ompt_parallel_begin(ompt_task_id_t parent_task_id, 
                         ompt_frame_t *parent_task_frame,
		         ompt_parallel_id_t region_id, 
                         uint32_t requested_team_size, 
                         void *parallel_fn);

// export registration interfaces for ompt
void ompt_parallel_end(ompt_parallel_id_t parallel_id,    /* id of parallel region       */
  		       ompt_task_id_t task_id             /* id of task                  */ );

cct_node_t *hpcrun_region_lookup(uint64_t id);

#endif
