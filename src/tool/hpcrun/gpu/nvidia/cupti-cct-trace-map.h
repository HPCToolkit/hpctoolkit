#ifndef cupti_cct_trace_map_h
#define cupti_cct_trace_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>
#include "cupti-cct-trace.h"

//*****************************************************************************
// interface operations
//*****************************************************************************


typedef struct cupti_cct_trace_map_entry_s cupti_cct_trace_map_entry_t;

typedef struct cct_trace_key_t {
 uint64_t node1;
 uint32_t range1;
 uint64_t node2;
 uint32_t range2;
} cct_trace_key_t;

// Look up a pair <node1, node2>
cupti_cct_trace_map_entry_t *
cupti_cct_trace_map_lookup_thread
(
 cct_trace_key_t key
); 


cupti_cct_trace_map_entry_t *
cupti_cct_trace_map_lookup
(
 cupti_cct_trace_map_entry_t **map_root,
 cct_trace_key_t key
); 


void
cupti_cct_trace_map_insert_thread
(
 cct_trace_key_t key,
 cupti_cct_trace_node_t *trace_node,
 uint32_t range_id
);


void
cupti_cct_trace_map_insert
(
 cupti_cct_trace_map_entry_t **map_root,
 cct_trace_key_t key,
 cupti_cct_trace_node_t *trace_node,
 uint32_t range_id
);


void
cupti_cct_trace_map_delete_thread
(
 cct_trace_key_t key
);


// Logical delete, the index still maintains
void
cupti_cct_trace_map_delete
(
 cupti_cct_trace_map_entry_t **map_root,
 cct_trace_key_t key
);


cupti_cct_trace_node_t *
cupti_cct_trace_map_entry_cct_trace_node_get
(
 cupti_cct_trace_map_entry_t *entry
);


uint32_t
cupti_cct_trace_map_entry_range_id_get
(
 cupti_cct_trace_map_entry_t *entry
);


#endif

