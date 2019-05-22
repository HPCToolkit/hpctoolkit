#ifndef _HPCTOOLKIT_MODULE_IGNORE_MAP_H_
#define _HPCTOOLKIT_MODULE_IGNORE_MAP_H_

#include <stdbool.h>
#include <hpcrun/loadmap.h>

void
module_ignore_map_init();


bool
module_ignore_map_module_lookup
(
 load_module_t *module
);


bool
module_ignore_map_inrange_lookup
(
 void *addr
);


bool
module_ignore_map_lookup
(
 void *start, 
 void *end
);


bool
module_ignore_map_id_lookup
(
 uint16_t module_id
);


bool
module_ignore_map_ignore
(
 void *start, 
 void *end
);


bool
module_ignore_map_delete
(
 void *start,
 void *end
);


#endif
