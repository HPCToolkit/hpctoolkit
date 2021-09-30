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
 cct_node_t *cct
);


// The previous ccts are marked as a range
// The same range can be taken out
// Returns true if the trace is condensed
bool
cupti_cct_trace_flush
(
);


void
cupti_cct_trace_dump
(
);

#endif
