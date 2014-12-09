#include <stdint.h>

#include <cct/cct.h>

#include <ompt.h>

void ompt_callstack_register_handlers(void);

cct_node_t *
ompt_region_context(uint64_t region_id, 
		    int levels_to_skip);

cct_node_t *
ompt_parallel_begin_context(ompt_parallel_id_t region_id, 
			    int levels_to_skip);
