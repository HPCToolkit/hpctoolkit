#ifndef _OMPT_INTERFACE_H_
#define _OMPT_INTERFACE_H_

#include <ompt.h>

//------------------------------------------------------------------------------
// hpcrun wrappers for ompt interfaces
//------------------------------------------------------------------------------
extern ompt_parallel_id_t hpcrun_ompt_get_parallel_id(int level);
extern ompt_task_id_t hpcrun_ompt_get_task_id(int level);
extern ompt_state_t hpcrun_ompt_get_state(uint64_t *wait_id);
extern ompt_frame_t *hpcrun_ompt_get_task_frame(int level);
extern void *hpcrun_ompt_get_idle_frame();

extern int hpcrun_ompt_elide_frames();

extern ompt_parallel_id_t hpcrun_ompt_outermost_parallel_id();

//------------------------------------------------------------------------------
// function: 
//   hpcrun_ompt_state_is_overhead
// 
// description:
//   returns 1 if the current state represents a form of overhead
//------------------------------------------------------------------------------
extern int hpcrun_ompt_state_is_overhead();

//FIXME for tasks
#define task_map_insert(id, cct) ((uint64_t) id & (uint64_t) cct) // computation to avoid unused var warnings
#define task_map_delete(id, cct)
#define task_map_lookup(id) (id & 0) // computation to avoid unused var warnings


#endif // _OMPT_INTERFACE_H_
