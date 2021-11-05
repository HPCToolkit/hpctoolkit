#ifndef cupti_cct_trie_h
#define cupti_cct_trie_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>

typedef struct cupti_cct_trie_node_s cupti_cct_trie_node_t;

// Append a cct and shrink if necessary
// Returns true if shrink is performed
bool
cupti_cct_trie_append
(
 uint32_t range_id,
 cct_node_t *cct
);


// The previous ccts are marked as a range
// Return prev range_id
int32_t
cupti_cct_trie_flush
(
 uint32_t range_id,
 bool sampled,
 bool merge,
 bool logic
);


void
cupti_cct_trie_cur_range_set
(
 uint32_t range_id
);


void
cupti_cct_trie_dump
(
);

#endif
