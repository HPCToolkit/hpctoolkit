#ifndef _HPCTOOLKIT_CUBIN_ID_MAP_H_
#define _HPCTOOLKIT_CUBIN_ID_MAP_H_

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

typedef struct cubin_id_map_entry_s cubin_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_id_map_entry_t *
cubin_id_map_lookup
(
 uint32_t cubin_id
); 


void
cubin_id_map_insert
(
 uint32_t cubin_id, 
 uint32_t hpctoolkit_module_id, 
 Elf_SymbolVector *vector
);


void
cubin_id_map_delete
(
 uint32_t cubin_id
);


uint32_t
cubin_id_map_entry_hpctoolkit_id_get
(
 cubin_id_map_entry_t *entry
);


Elf_SymbolVector *
cubin_id_map_entry_elf_vector_get
(
 cubin_id_map_entry_t *entry
);


ip_normalized_t
cubin_id_transform
(
 uint32_t cubin_id, 
 uint32_t function_id, 
 int64_t offset
);

#endif
