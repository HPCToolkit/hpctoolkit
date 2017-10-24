#ifndef _OMPT_INTERFACE_H_
#define _OMPT_INTERFACE_H_

#include <ompt.h>

//------------------------------------------------------------------------------
// hpcrun wrappers for ompt interfaces
//------------------------------------------------------------------------------

extern int hpcrun_ompt_get_parallel_info(
    int ancestor_level,
    ompt_data_t **parallel_data,
    int *team_size
);
extern uint64_t hpcrun_ompt_get_unique_id();
extern ompt_data_t* hpcrun_ompt_get_task_data(int level);
extern uint64_t hpcrun_ompt_outermost_parallel_id();
extern uint64_t hpcrun_ompt_get_parallel_info_id(int ancestor_level);
extern void hpcrun_ompt_get_parallel_info_id_pointer(int ancestor_level, uint64_t *region_id);


extern omp_state_t hpcrun_ompt_get_state(uint64_t *wait_id);
extern ompt_frame_t *hpcrun_ompt_get_task_frame(int level);
extern void *hpcrun_ompt_get_idle_frame();
extern uint64_t hpcrun_ompt_get_blame_target();

extern int hpcrun_ompt_elide_frames();


extern void ompt_mutex_blame_shift_enable();   // definition not found

extern void ompt_mutex_blame_shift_request();
extern void ompt_idle_blame_shift_request();

//------------------------------------------------------------------------------
// function: 
//   hpcrun_ompt_state_is_overhead
// 
// description:
//   returns 1 if the current state represents a form of overhead
//------------------------------------------------------------------------------
extern int hpcrun_omp_state_is_overhead();

extern ompt_idle_t ompt_idle_placeholder_fn;





struct cct_node_t* main_top_root;

#endif // _OMPT_INTERFACE_H_
