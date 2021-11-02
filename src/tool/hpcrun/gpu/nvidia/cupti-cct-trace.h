#ifndef cupti_cct_trace_h
#define cupti_cct_trace_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>

typedef struct cupti_cct_trace_node_s cupti_cct_trace_node_t;

// Append a cct and shrink if necessary
// Returns true if shrink is performed
bool
cupti_cct_trace_append
(
 uint32_t range_id,
 cct_node_t *cct
);


// The previous ccts are marked as a range
// Return prev range_id
int32_t
cupti_cct_trace_flush
(
 uint32_t range_id,
 bool sampled,
 bool merge,
 bool logic
);


void
cupti_cct_trace_dump
(
);

#endif
