
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


// added by vi3
int length_trl(ompt_trl_el_t* regions_list_head, int of_freelist);
int register_thread(int level);
void register_thread_to_all_regions();
void register_to_all_regions();
void try_resolve_context();

void resolve_one_region_context(ompt_region_data_t* region_data);
void resolving_all_remaining_context();

#endif