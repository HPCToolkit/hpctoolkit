#ifndef __ompt_region_h
#define __ompt_region_h

#include <ompt.h>

void ompt_parallel_region_register_callbacks(ompt_set_callback_t ompt_set_callback_fn);

#if 0

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


cct_node_t *hpcrun_region_lookup(uint64_t id);

#endif

#endif
