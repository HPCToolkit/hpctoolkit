#ifndef _OMPT_INTERFACE_H_
#define _OMPT_INTERFACE_H_

#include <ompt.h>


#include "../cct/cct.h"




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


#define OMPT_BASE_T_STAR(ptr) (ompt_base_t*)ptr
#define OMPT_BASE_T_STAR_STAR(ptr) (ompt_base_t**)&ptr
#define OMPT_BASE_T_GET_NEXT(ptr) ptr->next.next

#define OMPT_REGION_DATA_T_START(ptr) (ompt_region_data_t*)ptr
#define OMPT_NOTIFICATION_T_START(ptr) (ompt_notification_t*)ptr
#define OMPT_TRL_EL_T_START(ptr) (ompt_trl_el_t*)ptr


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
ompt_trl_el_t* hpcrun_ompt_trl_el_alloc();
void hpcrun_ompt_trl_el_free(ompt_trl_el_t *thread_region);


// vi3: Helper function to get region_data
ompt_region_data_t* hpcrun_ompt_get_region_data(int ancestor_level);
ompt_region_data_t* hpcrun_ompt_get_current_region_data();
ompt_region_data_t* hpcrun_ompt_get_parent_region_data();

// helper functions for freelist manipulation
ompt_base_t* freelist_remove_first(ompt_base_t **head);
void freelist_add_first(ompt_base_t *new, ompt_base_t **head);

// wait free queue
void wfq_set_next_pending(ompt_base_t *element);
ompt_base_t* wfq_get_next(ompt_base_t *element);
void wfq_init(ompt_wfq_t *queue);
void wfq_enqueue(ompt_base_t *new, ompt_wfq_t *queue);
ompt_base_t* wfq_dequeue_public(ompt_wfq_t *public_queue);
ompt_base_t* wfq_dequeue_private(ompt_wfq_t *public_queue, ompt_base_t **private_queue);

#endif // _OMPT_INTERFACE_H_
