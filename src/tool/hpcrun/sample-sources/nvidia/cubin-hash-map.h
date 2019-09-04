#ifndef _HPCTOOLKIT_CUBIN_HASH_MAP_H_
#define _HPCTOOLKIT_CUBIN_HASH_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cubin_hash_map_entry_s cubin_hash_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_hash_map_entry_t *
cubin_hash_map_lookup
(
 uint32_t cubin_id
);


void
cubin_hash_map_insert
(
 uint32_t cubin_id,
 const void *cubin,
 size_t size
);


unsigned char *
cubin_hash_map_entry_hash_get
(
 cubin_hash_map_entry_t *entry,
 unsigned int *len
);

#endif
