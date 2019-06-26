#ifndef _HPCTOOLKIT_CUBIN_MODULE_MAP_H_
#define _HPCTOOLKIT_CUBIN_MODULE_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/sample_event.h>
#include "cubin-symbols.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cubin_module_map_entry_s cubin_module_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_module_map_entry_t *
cubin_module_map_lookup
(
 const void *module
); 


void
cubin_module_map_insert
(
 const void *module,
 uint32_t hpctoolkit_module_id, 
 Elf_SymbolVector *vector
);


void
cubin_module_map_delete
(
 const void *module
);


uint32_t
cubin_module_map_entry_hpctoolkit_id_get
(
 cubin_module_map_entry_t *entry
);


Elf_SymbolVector *
cubin_module_map_entry_elf_vector_get
(
 cubin_module_map_entry_t *entry
);


ip_normalized_t
cubin_module_transform
(
 const void *cubin_module, 
 uint32_t function_index, 
 int64_t offset
);

#endif
