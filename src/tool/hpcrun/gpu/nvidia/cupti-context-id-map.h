#ifndef _HPCTOOLKIT_CUPTI_CONTEXT_ID_MAP_H_
#define _HPCTOOLKIT_CUPTI_CONTEXT_ID_MAP_H_

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

typedef struct cupti_context_id_map_entry_s cupti_context_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_context_id_map_entry_t *
cupti_context_id_map_lookup
(
 uint32_t context_id
);


void
cupti_context_id_map_context_delete
(
 uint32_t context_id
);


void
cupti_context_id_map_stream_delete
(
 uint32_t context_id,
 uint32_t stream_id
);


void
cupti_context_id_map_stream_process
(
 uint32_t context_id,
 uint32_t stream_id,
 cupti_trace_fn_t fn,
 void *arg
);


void
cupti_context_id_map_context_process
(
 uint32_t context_id,
 cupti_trace_fn_t fn,
 void *arg
);


void
cupti_context_id_map_device_process
(
 cupti_trace_fn_t fn,
 void *arg
);

#endif

