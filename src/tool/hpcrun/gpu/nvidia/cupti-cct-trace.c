#include "cupti-cct-trace.h"

typedef enum {
  CUPTI_CCT_TRACE_NODE_TERMINAL = 0,
  CUPTI_CCT_TRACE_NODE_NON_TERMINAL = 1,
  CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF = 2,
  CUPTI_CCT_TRACE_NODE_FLUSH = 3,
  CUPTI_CCT_TRACE_NODE_COUNT = 4
} cupti_cct_trace_node_type_t;

struct cupti_cct_trace_node_s {
  cupti_cct_trace_node_type_t type;
  struct cupti_cct_trace_s *left;
  struct cupti_cct_trace_s *right;
  struct cupti_cct_trace_s *rule;
  uint64_t ref_count;

  cct_node_t *cct;
};

__thread cupti_cct_trace_node_t *root = NULL;
__thread cupti_cct_trace_node_t *free_list = NULL;

//*****************************************************************************
// local operations
//*****************************************************************************

static cupti_cct_trace_node_t *
cupti_cct_trace_alloc_helper
(
 cupti_cct_trace_node_t **free_list, 
 size_t size
)
{
  cupti_cct_trace_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (cupti_cct_trace_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


void
cupti_cct_trace_free_helper
(
 cupti_cct_trace_node_t **free_list, 
 cupti_cct_trace_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}

#define trace_alloc(free_list)		\
  (cupti_cct_trace_node_t *) cupti_cct_trace_alloc_helper		\
  ((cupti_cct_trace_node_t **) free_list, sizeof(cupti_cct_trace_node_t))	

#define trace_free(free_list, node)			\
  cupti_cct_trace_free_helper					\
  ((cupti_cct_trace_node_t **) free_list,				\
   (cupti_cct_trace_node_t *) node)


static cupti_cct_trace_t *
trace_new
(
 cupti_cct_trace_node_type_t type,
 cct_node_t *cct
)
{
  cupti_cct_trace_node_t *node = trace_alloc(&free_list);
  node->type = type;
  node->cct = cct;
  node->ref_count = 1;
  node->left = node;
  node->right = node;
}


static void
trace_init
(
)
{
  if (root == NULL) {
    root = cupti_cct_trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL, NULL);
  }
}


static void
trace_insert
(
 cupti_cct_trace_node_t *cur,
 cupti_cct_trace_node_t *next
)
{
  cur->right->left = next;
  next->right = cur->right;
  cur->right = next;
  next->left = cur;
}


static void
trace_append
(
 cupti_cct_trace_node_t *head,
 cupti_cct_trace_node_t *next
)
{
  trace_insert(head->left, next);
  trace_insert(next, head);
}


// Splice two rules
static void
trace_splice
(
 cupti_cct_trace_node_t *to_head,
 cupti_cct_trace_node_t *from_head
)
{
  cupti_cct_trace_node_t *tail = from_head->left;
  trace_insert(to_head->left, from_head->right);
  trace_insert(tail, to_head);
}


static cupti_cct_trace_node_t *
trace_delete
(
 cupti_cct_trace_node_t *node
)
{
  node->left->right = node->right;
  node->right->left = node->left;
}


static void
trace_shrink
(
)
{
  cupti_cct_trace_node_t *current = root->left;

  while (current->left != CUPTI_CCT_TRACE_NODE_NON_TERMINAL &&
    current->left != CUPTI_CCT_TRACE_NODE_FLUSH) {
    cct_trace_key_t key = {
      .cct1 = current->left->cct,
      .cct2 = current->cct
    };

    cupti_cct_trace_map_entry_t *entry = cupti_cct_trace_map_lookup(key);

    if (entry == NULL) {
      // No such an pattern yet, we index it
      // No need to create a rule, so break out from the loop
      cupti_cct_trace_map_insert(key, current->left);
      current = rule_ref;
      break;
    } else {
      // Such a pattern exists
      // root->Aa...................Aa
      // prev_pattern_node    pattern_node
      // or
      // B->Aa
      // B->CAa
      cupti_cct_trace_node_t *prev_pattern_node = cupti_cct_trace_map_entry_cct_trace_node_get(entry);
      cupti_cct_trace_node_t *pattern_node = key.cct1;
      cupti_cct_trace_node_t *rule = NULL;

      if (prev_pattern_node->right == pattern_node) {
        // Overlapped
        // root->...aaa
        break;
      }

      bool continguous = false;
      if (prev_pattern_node->cct == prev_pattern_node->right->cct
          prev_pattern_node->right->cct == prev_pattern_node->right->right->cct) {
        // root->...aaaa
        contiguous = true;
      }

      if (!(prev_pattern_node->left == prev_pattern_node->right->right)) {
        // Not a two symbol rule node, needs replacement 
        // CAa
        // AaC
        // root->Aa
        // Construct a rule using prev_pattern_nodes

        // 1. Replace prev_pattern_node with rule_ref
        cupti_cct_trace_node_t *rule_ref = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF, NULL); 
        cupti_cct_trace_node_t *right = prev_pattern_node->right;
        trace_insert(prev_pattern_node->left, rule_ref);
        trace_insert(rule_ref, right);

        // 2. Delete the index of the subsequent pattern
        // root->Aab
        // Delete the index of ab
        cct_trace_key_t delete_key = {
          .cct1 = prev_pattern_node->right->cct,
          .cct2 = prev_pattern_node->right->right->cct
        };

        if (!continguous) {
          // ....aaaa...
          // ....Aaa....
          // Do not delete aa
          cupti_cct_trace_map_delete(delete_key);
        }

        // 3. Move the first Aa from the trace to the rule
        // root->Aa...................Aa
        // ref_count does not change
        rule = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL, NULL);

        // Utility rule
        cupti_cct_trace_node_t *nodes[] = { prev_pattern_node, prev_pattern_node->right };
        for (size_t i = 0; i < 2; ++i) {
          cupti_cct_trace_node_t *node = nodes[i];

          if (node->rule) {
            node->rule->ref_count -= 1;

            if (node->rule->ref_count == 1) {
              // this rule can be eliminated
              trace_splice(rule, node->rule);
              trace_delete(node->rule);
            } else {
              // append the rule symbol
              trace_append(rule, node);
            }
          }
        }
      } else {
        // B->Aa
        // Just replace the current pattern appearance
        rule = prev_pattern_node->left;
      }

      ++rule->ref_count;

      // root->...AbcAb
      // Delete cA
      cct_trace_key_t delete_key = {
        .cct1 = pattern_node->left->cct,
        .cct2 = pattern_node->cct
      };

      if (!contiguous) {
        // root->...aaaa
        // do not delete aa
        cupti_cct_trace_map_delete(delete_key);
      }

      cupti_cct_trace_node_t *rule_ref = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF, NULL);
      cupti_cct_trace_node_t *left = pattern_node->left;
      trace_delete(pattern_node);
      trace_delete(pattern_node->right);
      trace_append(left, rule_ref);

      // There must be another A, otherwise, A should have been replaced
      // No need to enfore utility rule

      current = root->left;
    }
  }
}


static void
trace_shrink
(
)
{
  cupti_cct_trace_node_t *current = root->left;
  // S->R|
  // S->....|R|
  if (current->left->left == root || current->left->left->type == CUPTI_CCT_TRACE_NODE_FLUSH) {
    // A range cannot be compressed further
    trace_delete(current->left);
    trace_delete(current);
    // Don't remove the rule itself
  }
}

//*****************************************************************************
// interface operations
//*****************************************************************************

bool
cupti_cct_trace_append
(
 cct_node_t *cct
)
{
  trace_init();

  cupti_cct_trace_node_t *trace_node = trace_new(CUPTI_CCT_TRACE_NODE_TERMINAL, cct);
  trace_append(root, trace_node);

  bool ret = trace_shrink();

  return ret;
}


void
cupti_cct_trace_flush
(
)
{
  trace_init();

  cupti_cct_trace_node_t *trace_node = trace_new(CUPTI_CCT_TRACE_NODE_FLUSH, cct);
  trace_append(root, trace_node);

  trace_condense();
}
