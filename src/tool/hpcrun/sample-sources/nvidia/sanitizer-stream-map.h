#ifndef _HPCTOOLKIT_SANITIZER_STREAM_MAP_H_
#define _HPCTOOLKIT_SANITIZER_STREAM_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <cuda.h>

#include "sanitizer-record.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct sanitizer_stream_map_entry_s sanitizer_stream_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_stream_map_entry_t *
sanitizer_stream_map_lookup
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


void
sanitizer_stream_map_insert
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream,
 cstack_node_t *notification
);


void
sanitizer_stream_map_delete
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
);


void
sanitizer_stream_map_process_all
(
 sanitizer_stream_map_entry_t **root,
 sanitizer_record_fn_t fn
);


void
sanitizer_stream_map_process
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream,
 sanitizer_record_fn_t fn
);


sanitizer_stream_map_entry_t *
sanitizer_stream_map_entry_new
(
 CUstream stream
);

#endif

