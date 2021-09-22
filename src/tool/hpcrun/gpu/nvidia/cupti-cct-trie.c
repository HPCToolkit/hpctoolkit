#include "cupti-cct-trie.h"

typedef struct cupti_cct_trie_entry_s {
  // CCT structure
  struct cupti_cct_trie_entry_s *parent;
  struct cupti_cct_trie_entry_s *children;
  
  // A map to store all children
  struct cupti_cct_trie_entry_s *left;
  struct cupti_cct_trie_entry_s *right;
} cupti_cct_trie_entry_t;

static __thread cupti_cct_trie_entry_t *root = NULL;

// Return true if the node exists
bool
cupti_cct_trie_insert
(
 cct_node_t *cct
)
{
  return false;
}
