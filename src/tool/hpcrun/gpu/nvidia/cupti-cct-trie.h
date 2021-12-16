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

#define CUPTI_CCT_TRIE_UNWIND_ROOT 0
#define CUPTI_CCT_TRIE_COMPRESS_THRESHOLD 1000
#define CUPTI_CCT_TRIE_PTR_NULL (CUPTI_CCT_TRIE_COMPRESS_THRESHOLD + 1)

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
 bool active,
 bool logic
);

// Return the trie root of the querying thread
cupti_cct_trie_node_t *
cupti_cct_trie_root_get
(
);

// Process pending notifications
void
cupti_cct_trie_notification_process
(
);

// Unwind the trie for one layer
void
cupti_cct_trie_unwind
(
);

// Dump cct trie statistics
void
cupti_cct_trie_dump
(
);

// Compress single path
void
cupti_cct_trie_compress
(
);

// Get the number of contiguous parent nodes with a single path
uint32_t
cupti_cct_trie_single_path_length_get
(
);

#endif
