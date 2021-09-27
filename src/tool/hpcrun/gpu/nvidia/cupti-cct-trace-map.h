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
 cct_node_t *cct1,
 cct_node_t *cct2
} cct_trace_key_t;

// Look up a pair <cct1, cct2>
cupti_cct_trace_map_entry_t *
cupti_cct_trace_map_lookup
(
 cct_trace_key_t key
); 


void
cupti_cct_trace_map_insert
(
 cct_trace_key_t key,
 cupti_cct_trace_node_t *trace_node
);


void
cupti_cct_trace_map_delete
(
 cct_trace_key_t key
);


cupti_cct_trace_node_t *
cupti_cct_trace_map_entry_cct_trace_node_get
(
 cupti_cct_trace_map_entry_t *entry
);


#endif

