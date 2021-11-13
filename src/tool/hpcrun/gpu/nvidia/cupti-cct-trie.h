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

// Append a cct to the trie.
// Return true if a new range is encountered
bool
cupti_cct_trie_append
(
 uint32_t range_id,
 cct_node_t *cct
);

// The previous ccts from cur to logic_root are marked as a range.
// Return either the range_id at the end node or 
// GPU_RANGE_NULL to indicate this is a new range
uint32_t
cupti_cct_trie_flush
(
 uint32_t context_id,
 uint32_t range_id,
 bool logic
);

// Return the trie root of the querying thread
cupti_cct_trie_node_t *
cupti_cct_trie_root_get
(
);

// Return the trie current position of the querying thread
cupti_cct_trie_node_t **
cupti_cct_trie_cur_ptr_get
(
);

// Return the trie logic root position of the querying thread
cupti_cct_trie_node_t **
cupti_cct_trie_logic_root_ptr_get
(
);

// Unwind the trie one layer up
void
cupti_cct_trie_unwind
(
);

// Dump cct trie statistics
void
cupti_cct_trie_dump
(
);

#endif
