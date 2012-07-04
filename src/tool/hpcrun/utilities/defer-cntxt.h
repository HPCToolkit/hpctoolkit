#ifndef defer_cntxt_h
#define defer_cntxt_h

#include <hpcrun/thread_data.h>
/*
 * this interface is used for lazy callpath resolution 
*/

/* register the functions in runtime (OMP) to support lazy resolution */
void register_defer_callback();

/* check whether the lazy resolution is needed */
int  need_defer_cntxt();

/* resolve the contexts */
void resolve_cntxt();
void resolve_cntxt_fini();
void resolve_other_cntxt(thread_data_t *thread_data);

uint64_t is_partial_resolve(cct_node_t *prefix);

//deferred region ID assignment
void init_region_id();
#endif
