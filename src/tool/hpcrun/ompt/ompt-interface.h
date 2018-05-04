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



// vi3: Part for Allocating
// FIXME: vi3 are names ok?
// freeing memory represents adding entity to freelist

// allocating and free region data
// here freeing memory represents adding to freelist
ompt_region_data_t* hpcrun_ompt_region_alloc();
void hpcrun_ompt_region_free(ompt_region_data_t *region_data);

// allocation and free notification
ompt_notification_t* hpcrun_ompt_notification_alloc();
void hpcrun_ompt_notification_free(ompt_notification_t *notification);

// allocating and free thread's regions
ompt_thread_regions_list_t* hpcrun_ompt_thread_region_alloc();
void hpcrun_ompt_thread_region_free(ompt_thread_regions_list_t *thread_region);


// vi3: Helper function to get region_data
ompt_region_data_t* hpcrun_ompt_get_region_data(int ancestor_level);
ompt_region_data_t* hpcrun_ompt_get_current_region_data();
ompt_region_data_t* hpcrun_ompt_get_parent_region_data();




#endif // _OMPT_INTERFACE_H_
