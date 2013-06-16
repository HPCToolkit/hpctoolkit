#ifndef _OMPT_INTERFACE_H_
#define _OMPT_INTERFACE_H_

#include <ompt.h>

//------------------------------------------------------------------------------
// hpcrun wrappers for ompt interfaces
//------------------------------------------------------------------------------
extern int hpcrun_ompt_get_parallel_id(int level);
extern int hpcrun_ompt_get_state(uint64_t *wait_id);
extern ompt_frame_t *hpcrun_ompt_get_task_frame(int level);
extern ompt_data_t *hpcrun_ompt_get_task_data(int level);

extern int hpcrun_ompt_elide_frames();

extern int hpcrun_ompt_outermost_parallel_id();

//------------------------------------------------------------------------------
// function: 
//   hpcrun_ompt_state_is_overhead
// 
// description:
//   returns 1 if the current state represents a form of overhead
//------------------------------------------------------------------------------
extern int hpcrun_ompt_state_is_overhead();

#endif // _OMPT_INTERFACE_H_
