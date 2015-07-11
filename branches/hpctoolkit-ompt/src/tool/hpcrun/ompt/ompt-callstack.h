#include <stdint.h>

#include <cct/cct.h>

#include <ompt.h>

typedef enum {
  ompt_context_begin,
  ompt_context_end
} ompt_context_type_t;

void ompt_callstack_register_handlers(void);

cct_node_t *
ompt_region_context(uint64_t region_id, 
                    ompt_context_type_t ctype,
		    int levels_to_skip);

cct_node_t *
ompt_parallel_begin_context(ompt_parallel_id_t region_id, 
			    int levels_to_skip);

cct_node_t *region_root(cct_node_t *_node);

