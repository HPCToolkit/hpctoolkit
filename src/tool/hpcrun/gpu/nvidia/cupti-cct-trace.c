#include "cupti-cct-trace.h"

#include <stdio.h>

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>

#include "cupti-cct-trace-map.h"

#define DEBUG

#ifdef DEBUG
#define TRACE_ASSERT(x) assert(x)
#define TRACE_MSG(...) TMSG(__VA_ARGS__)
#else
#define TRACE_ASSERT(x)
#define TRACE_MSG(...)
#endif


typedef enum {
  CUPTI_CCT_TRACE_NODE_TERMINAL = 0,
  CUPTI_CCT_TRACE_NODE_NON_TERMINAL = 1,
  CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF = 2,
  CUPTI_CCT_TRACE_NODE_FLUSH = 3,
  CUPTI_CCT_TRACE_NODE_COUNT = 4
} cupti_cct_trace_node_type_t;

struct cupti_cct_trace_node_s {
  cupti_cct_trace_node_type_t type;
  struct cupti_cct_trace_node_s *left;
  struct cupti_cct_trace_node_s *right;
  struct cupti_cct_trace_node_s *rule;

  struct cupti_cct_trace_node_s *ref_left;
  struct cupti_cct_trace_node_s *ref_right;
  uint64_t ref_count;

  uint64_t key;
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


static cupti_cct_trace_node_t *
trace_new
(
 cupti_cct_trace_node_type_t type,
 uint64_t key
)
{
  cupti_cct_trace_node_t *node = trace_alloc(&free_list);
  node->type = type;
  node->key = key;
  node->ref_count = 1;
  node->left = node;
  node->right = node;

  if (type == CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
    node->ref_left = node;
    node->ref_right = node;
  }

  return node;
}


static void
trace_init
(
)
{
  if (root == NULL) {
    root = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL, NULL);
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
}


static void
trace_ref_insert
(
 cupti_cct_trace_node_t *cur,
 cupti_cct_trace_node_t *next
)
{
  cur->ref_right->ref_left = next;
  next->ref_right = cur->ref_right;
  cur->ref_right = next;
  next->ref_left = cur;
}


static void
trace_ref_append
(
 cupti_cct_trace_node_t *head,
 cupti_cct_trace_node_t *next
)
{
  ++head->ref_count;
  next->rule = head;
  next->key = head;
  trace_ref_insert(head->ref_left, next);
}


// Splice two rules
static void
trace_splice
(
 cupti_cct_trace_node_t *to,
 cupti_cct_trace_node_t *from_head
)
{
  cupti_cct_trace_node_t *left = to;
  cupti_cct_trace_node_t *right = to->right;
  trace_insert(left, from_head->right);
  trace_insert(right, from_head->left);
}


static void
trace_unlink
(
 cupti_cct_trace_node_t *node
)
{
  node->left->right = node->right;
  node->right->left = node->left;

  node->left = node;
  node->right = node;
}


static void
trace_delete
(
 cupti_cct_trace_node_t *node
)
{
  node->left->right = node->right;
  node->right->left = node->left;

  if (node->type == CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF || 
    node->type == CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
    node->ref_left->ref_right = node->ref_right;
    node->ref_right->ref_left = node->ref_left;
  }

  trace_free(&free_list, node);
}


static cupti_cct_trace_map_entry_t *
trace_map_lookup
(
 uint64_t node1,
 uint64_t node2
)
{
  cct_trace_key_t key = {
    .node1 = node1,
    .node2 = node2
  };

  return cupti_cct_trace_map_lookup(key);
}


static void
trace_map_insert
(
 uint64_t node1,
 uint64_t node2,
 cupti_cct_trace_node_t *trace_node
)
{
  cct_trace_key_t key = {
    .node1 = node1,
    .node2 = node2
  };

  cupti_cct_trace_map_insert(key, trace_node);
}


static void
trace_map_delete
(
 uint64_t node1,
 uint64_t node2
)
{
  cct_trace_key_t key = {
    .node1 = node1,
    .node2 = node2
  };

  cupti_cct_trace_map_delete(key);
}


static void
trace_rule_delete
(
 cupti_cct_trace_node_t *rule
)
{
  if (rule != NULL && rule->ref_count == 1) {
    // There must be only one ref node only
    cupti_cct_trace_node_t *left = rule->ref_right->left;
    trace_delete(rule->ref_right);
    trace_splice(left, rule);
    trace_delete(rule);
  }
}


static bool
trace_shrink
(
)
{
  bool shrinked = false;

  // root->left is the last node
  cupti_cct_trace_node_t *current = root->left;

  // Don't merge across ranges
  while (current->left->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL && current->left->type != CUPTI_CCT_TRACE_NODE_FLUSH) {
    cupti_cct_trace_map_entry_t *entry = trace_map_lookup(current->left->key, current->key);


    if (entry == NULL) {
      // No such an pattern yet, we index it
      // No need to create a rule, so break out from the loop
      trace_map_insert(current->left->key, current->key, current->left);
      break;
    } 

    // Such a pattern exists
    // root->Aa...................Aa
    // prev_pattern_node    pattern_node
    // or
    // B->Aa
    // B->CAa
    cupti_cct_trace_node_t *prev_pattern_node = cupti_cct_trace_map_entry_cct_trace_node_get(entry);
    cupti_cct_trace_node_t *pattern_node = current->left;
    cupti_cct_trace_node_t *rule = NULL;

    if (prev_pattern_node->right == pattern_node) {
      TRACE_MSG(CUPTI_CCT_TRACE, "Trace overlapped (prev_pattern_node: %p, pattern_node: %p)", prev_pattern_node, pattern_node);
      // Overlapped
      // root->...aaa
      break;
    }

    if (prev_pattern_node->left->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
      // root->BAa...Aa
      // Delete the index of BA
      trace_map_delete(prev_pattern_node->left->key, prev_pattern_node->key);
    }

    bool contiguous = false;
    if (prev_pattern_node->key == prev_pattern_node->right->key &&
      prev_pattern_node->right->right == pattern_node) {
      // root->...aaaa
      // Don't delete the index of aa
      contiguous = true;
      TRACE_MSG(CUPTI_CCT_TRACE, "Trace contiguous (prev_pattern_node: %p, pattern_node: %p) (key: %p)",
        prev_pattern_node, pattern_node, prev_pattern_node->key);
    }

    if (!(prev_pattern_node->left == prev_pattern_node->right->right)) {
      // Not a two symbol rule node, needs replacement 
      // root->ab....ab, or
      // A->cab
      // Construct a rule using prev_pattern_nodes
      
      TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial before replacement (left: %p->%p->%p->%p) (key: %p->%p->%p->%p)",
        prev_pattern_node->left, prev_pattern_node, prev_pattern_node->right, prev_pattern_node->right->right,
        prev_pattern_node->left->key, prev_pattern_node->key, prev_pattern_node->right->key, prev_pattern_node->right->right->key);

      // 1. Replace prev_pattern_nodes with rule_ref
      cupti_cct_trace_node_t *rule_ref = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF, NULL); 
      cupti_cct_trace_node_t *left = prev_pattern_node->left;
      cupti_cct_trace_node_t *right = prev_pattern_node->right->right;
      cupti_cct_trace_node_t *node1 = prev_pattern_node;
      cupti_cct_trace_node_t *node2 = prev_pattern_node->right;
      trace_unlink(node1);
      trace_unlink(node2);
      trace_insert(left, rule_ref);

      // 2. Delete the index of the subsequent pattern
      // root->Aab
      // Delete the index of ab
      if (!contiguous) {
        // Not root->AaAa
        trace_map_delete(node2->key, right->key);
      }

      // Utility rule
      trace_rule_delete(node1->rule);

      // 3. Move prev_pattern nodes from the trace to the rule
      // root->Aa...................Aa
      rule = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL, NULL);
      trace_append(rule, node1);
      trace_append(rule, node2);
      trace_ref_append(rule, rule_ref);

      TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after replacement (left: %p->%p->%p->%p) (key: %p->%p->%p->%p)",
        left, left->right, left->right->right, left->right->right->right,
        left->key, left->right->key, left->right->right->key, left->right->right->right->key);
      TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial new (rule: %p->%p->%p) (key: %p->%p->%p)",
        rule, rule->right, rule->right->right,
        rule->key, rule->right->key, rule->right->right->key);
      TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial new (rule_ref: %p->%p) (key: %p->%p)",
        rule_ref->rule, rule->ref_right,
        rule->key, rule->ref_right->key);

      // 4. Index new sequences
      // root->...bAa...
      // Index bA and Aa
      if (left->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
        trace_map_insert(left->key, left->right->key, left);
      }
      if (left->right->right->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
        trace_map_insert(left->right->key, left->right->right->key, left->right);
      }
    } else {
      // B->Aa
      // Replace the current pattern appearance
      rule = prev_pattern_node->left;

      TRACE_ASSERT(rule->type == CUPTI_CCT_TRACE_NODE_NON_TERMINAL);

      TRACE_MSG(CUPTI_CCT_TRACE, "Trace existed pattern (rule: %p->%p->%p) (key: %p->%p->%p)",
        rule, rule->right, rule->right->right, rule->key, rule->right->key, rule->right->right->key);
    }

    cupti_cct_trace_node_t *pattern_node_left = pattern_node->left;
    if (!contiguous) {
      // root->...AbBAb
      // Delete BA
      // Don't delete B
      trace_map_delete(pattern_node_left->key, pattern_node->key);
    }

    TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial before delete (left: %p->%p->%p) (key: %p->%p->%p)",
      pattern_node_left, pattern_node_left->right, pattern_node_left->right->right,
      pattern_node_left->key, pattern_node_left->right->key, pattern_node_left->right->right->key);

    // Make a ref node
    cupti_cct_trace_node_t *rule_ref = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF, NULL);

    trace_delete(pattern_node->right);
    trace_delete(pattern_node);

    TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after delete (left: %p->%p->%p) (key: %p->%p->%p)",
      pattern_node_left, pattern_node_left->right, pattern_node_left->right->right,
      pattern_node_left->key, pattern_node_left->right->key, pattern_node_left->right->right->key);

    trace_append(root, rule_ref);
    trace_ref_append(rule, rule_ref);

    TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after append (left: %p->%p->%p) (key: %p->%p->%p)",
      pattern_node_left, pattern_node_left->right, pattern_node_left->right->right,
      pattern_node_left->key, pattern_node_left->right->key, pattern_node_left->right->right->key);

    // There must be another A, otherwise, A should have been replaced
    // No need to enfore utility rule
    current = root->left;

    shrinked = true;
  }

  return shrinked;
}


static bool
trace_condense
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
    return true;
  }

  return false;
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
  TRACE_MSG(CUPTI_CCT_TRACE, "Enter cupti_cct_trace_append %p", cct);

  trace_init();

  cupti_cct_trace_node_t *trace_node = trace_new(CUPTI_CCT_TRACE_NODE_TERMINAL, (uint64_t)cct);
  trace_append(root, trace_node);

  TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after append (root: %p<-%p<-%p<-%p) (key: %p<-%p<-%p<-%p)",
    root, root->left, root->left->left, root->left->left->left,
    root->key, root->left->key, root->left->left->key, root->left->left->left->key);

  bool ret = trace_shrink();

  TRACE_MSG(CUPTI_CCT_TRACE, "Exit cupti_cct_trace_append %p shrunk %d", cct, ret);

  return ret;
}


bool
cupti_cct_trace_flush
(
)
{
  TRACE_MSG(CUPTI_CCT_TRACE, "Enter flush key trace");

  trace_init();

  cupti_cct_trace_node_t *trace_node = trace_new(CUPTI_CCT_TRACE_NODE_FLUSH, NULL);
  trace_append(root, trace_node);

  bool condensed = trace_condense();

  TRACE_MSG(CUPTI_CCT_TRACE, "Exit flush key trace condensed %d", condensed);

  return condensed;
}


void
cupti_cct_trace_dump
(
)
{
  cupti_cct_trace_node_t *node = root->right;
  printf("root->");
  while (node != root) {
    printf("%p", node->key);
    if (node->right != root) {
      printf("->");
    } else {
      printf("\n");
    }
    node = node->right;
  }
  printf("\n");
}
