#ifndef _HPCTOOLKIT_CUBIN_MD5_MAP_H_
#define _HPCTOOLKIT_CUBIN_MD5_MAP_H_

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

typedef struct cubin_md5_map_entry_s cubin_md5_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_md5_map_entry_t *
cubin_md5_map_lookup
(
 uint64_t cubin_id
);


void
cubin_md5_map_insert
(
 uint64_t cubin_id,
 const void *cubin,
 size_t size
);


unsigned char *
cubin_md5_map_entry_md5_get
(
 cubin_md5_map_entry_t *entry
);

#endif
