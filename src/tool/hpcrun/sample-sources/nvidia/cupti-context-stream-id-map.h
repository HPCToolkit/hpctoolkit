//
// Created by ax4 on 8/1/19.
//

#ifndef _HPCTOOLKIT_CUPTI_CONTEXT_STREAM_ID_MAP_H_
#define _HPCTOOLKIT_CUPTI_CONTEXT_STREAM_ID_MAP_H_

#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/stdatomic.h>
#include <tool/hpcrun/cct/cct.h>
#include <tool/hpcrun/threadmgr.h>

#include "cupti-stream-trace.h"

typedef struct cupti_context_stream_id_map_entry_s cupti_context_stream_id_map_entry_t;

typedef void (*cupti_context_stream_id_map_fn_t)(stream_trace_t *stream_trace);


cupti_context_stream_id_map_entry_t *
cupti_context_stream_id_map_lookup
(
 uint32_t context_id,
 uint32_t stream_id
);


void
cupti_context_stream_id_map_insert
(
 uint32_t context_id,
 uint32_t stream_id
);


void
cupti_context_stream_id_map_delete
(
 uint32_t context_id,
 uint32_t stream_id
);


void
cupti_context_stream_id_map_append
(
 uint32_t context_id,
 uint32_t stream_id,
 uint64_t start,
 uint64_t end,
 cct_node_t *cct_node
);


void
cupti_context_stream_id_map_process
(
 cupti_context_stream_id_map_fn_t fn
);


#endif //HPCTOOLKIT_CUPTI_STREAM_ID_MAP_H
