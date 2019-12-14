#ifndef _HPCTOOLKIT_CUPTI_STREAM_ID_MAP_H_
#define _HPCTOOLKIT_CUPTI_STREAM_ID_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "cupti-channel.h"
#include "cupti-trace-api.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_stream_id_map_entry_s cupti_stream_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_stream_id_map_entry_t *
cupti_stream_id_map_lookup
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id
);


void
cupti_stream_id_map_delete
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id
);


void
cupti_stream_id_map_stream_process
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id,
 cupti_trace_fn_t fn,
 void *arg
);


// Two usage cases:
// 1. Append events to all active streams (cuContextSynchronize)
// 2. Signal all active streams
void
cupti_stream_id_map_context_process
(
 cupti_stream_id_map_entry_t **root,
 cupti_trace_fn_t fn,
 void *arg
);


cupti_stream_id_map_entry_t *
cupti_stream_id_map_entry_new
(
 uint32_t stream_id
);

#endif
