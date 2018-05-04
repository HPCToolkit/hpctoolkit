
#ifndef defer_cntxt_h
#define defer_cntxt_h

//#include <ompt.h>

#include <hpcrun/thread_data.h>
#include "ompt.h"

// support for deferred callstack resolution

// check whether the lazy resolution is needed in an unwind
int  need_defer_cntxt();

/* resolve the contexts */
void resolve_cntxt();
void resolve_cntxt_fini(thread_data_t*);
void resolve_other_cntxt(thread_data_t *thread_data);

uint64_t is_partial_resolve(cct_node_t *prefix);

//deferred region ID assignment
void init_region_id();

cct_node_t *hpcrun_region_lookup(uint64_t id);

#define IS_UNRESOLVED_ROOT(addr) (addr->ip_norm.lm_id == (uint16_t)UNRESOLVED)

void enqueue_region(ompt_region_data_t* region_data, ompt_notification_t* new);
ompt_notification_t* dequeue_region(ompt_region_data_t* region_data);

//void enqueue_thread(ompt_thread_data_t* thread_data, ompt_notification_t* new);

int length_trl(ompt_thread_regions_list_t* regions_list_head, int of_freelist);
int register_thread(int level);
void register_thread_to_all_regions();
void register_to_all_regions();
void try_resolve_context();

// FIXME: vi3: currently try to fix this
void top_most_match(cct_node_t* cct_a, cct_node_t* cct_b);
int find_match(cct_node_t *cct_a, cct_node_t *cct_b, cct_node_t **left, cct_node_t **right);

void resolve_one_region_context(ompt_region_data_t* region_data);
void resolving_all_remaining_context();


void enqueue_thread(ompt_threads_queue_t* threads_queue, ompt_notification_t* new);
ompt_notification_t* dequeue_thread(ompt_threads_queue_t* threads_queue);


#endif