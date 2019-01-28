
#ifndef defer_cntxt_h
#define defer_cntxt_h

#include <hpcrun/thread_data.h>
#include <unwind/common/backtrace_info.h>
#include "ompt.h"
#include "ompt-types.h"

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

int try_resolve_one_region_context();

void resolve_one_region_context(ompt_region_data_t* region_data);
void resolving_all_remaining_context();


// function which provides call path for regions where thread is the master
void provide_callpath_for_regions_if_needed(backtrace_info_t* bt, cct_node_t* cct);
void provide_callpath_for_end_of_the_region(backtrace_info_t *bt, cct_node_t *cct);


// vi3: stack reorganization
void add_region_and_ancestors_to_stack(ompt_region_data_t *region_data, bool team_master);

void tmp_end_region_resolve(ompt_notification_t *notification, cct_node_t* prefix);

#endif
