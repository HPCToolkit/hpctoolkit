#include "cupti-cct-trace.h"

#include <stdio.h>

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>

#include "cupti-cct-trace-map.h"
#include "cupti-ip-norm-map.h"
#include "cupti-range-thread-list.h"

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

static __thread cupti_cct_trace_node_t *thread_root = NULL;
static __thread cupti_cct_trace_node_t *free_list = NULL;

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
trace_init
(
)
{
  if (thread_root == NULL) {
    thread_root = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL, 0);
    cupti_cct_trace_node_t *flush_node = trace_new(CUPTI_CCT_TRACE_NODE_FLUSH, 0);
    trace_append(thread_root, flush_node);
  }
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
  next->key = (uint64_t)head;
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

  return cupti_cct_trace_map_lookup_thread(key);
}


static void
trace_map_insert
(
 uint64_t node1,
 uint64_t node2,
 cupti_cct_trace_node_t *trace_node,
 uint32_t range_id
)
{
  cct_trace_key_t key = {
    .node1 = node1,
    .node2 = node2
  };

  cupti_cct_trace_map_insert_thread(key, trace_node, range_id);
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

  cupti_cct_trace_map_delete_thread(key);
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
 uint32_t range_id
)
{
  bool shrunk = false;

  // root->left is the last node
  cupti_cct_trace_node_t *current = thread_root->left;

  // Don't merge across ranges
  while (current->left->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
    cupti_cct_trace_map_entry_t *entry = trace_map_lookup(current->left->key, current->key);

    if (entry == NULL) {
      // No such an pattern yet, we index it
      // No need to create a rule, so break out from the loop
      trace_map_insert(current->left->key, current->key, current->left, range_id);
      break;
    } else {
      shrunk = true;
    }

    if (current->left->type == CUPTI_CCT_TRACE_NODE_FLUSH) {
      // Don't compress |A
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
      TRACE_MSG(CUPTI_CCT_TRACE, "Trace overlapped (prev_pattern_node: %p, pattern_node: %p), (key: %p, %p)",
        prev_pattern_node, pattern_node, prev_pattern_node->key, pattern_node->key);
      // Overlapped
      // root->...aaa
      break;
    }

    if (prev_pattern_node->left->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL &&
      prev_pattern_node->left->type != CUPTI_CCT_TRACE_NODE_FLUSH) {
      // root->...BAa...Aa
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
      cupti_cct_trace_node_t *rule_ref = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF, 0); 
      cupti_cct_trace_node_t *left = prev_pattern_node->left;
      cupti_cct_trace_node_t *right = prev_pattern_node->right->right;
      cupti_cct_trace_node_t *node1 = prev_pattern_node;
      cupti_cct_trace_node_t *node2 = prev_pattern_node->right;
      trace_unlink(node1);
      trace_unlink(node2);
      trace_insert(left, rule_ref);

      // 2. Delete the index of the subsequent pattern
      // root->Aab
      // Current pattern Aa
      // Delete the index of ab
      if (!contiguous) {
        // Not root->AaAa
        trace_map_delete(node2->key, right->key);
      }

      // Utility rule
      trace_rule_delete(node1->rule);
      trace_rule_delete(node2->rule);

      // 3. Move prev_pattern nodes from the trace to the rule
      // root->Aa...................Aa
      rule = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL, 0);
      rule_ref->key = (uint64_t)rule;
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
        trace_map_insert(left->key, left->right->key, left, range_id);
      }
      if (left->right->right->type != CUPTI_CCT_TRACE_NODE_NON_TERMINAL) {
        trace_map_insert(left->right->key, left->right->right->key, left->right, range_id);
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
    if (!contiguous && pattern_node_left->type != CUPTI_CCT_TRACE_NODE_FLUSH) {
      // root->...AbBAb
      // Current pattern Ab
      // Delete BA
      // Don't delete B
      trace_map_delete(pattern_node_left->key, pattern_node->key);
    }

    TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial before delete (left: %p->%p->%p) (key: %p->%p->%p)",
      pattern_node_left, pattern_node_left->right, pattern_node_left->right->right,
      pattern_node_left->key, pattern_node_left->right->key, pattern_node_left->right->right->key);

    // Make a ref node
    cupti_cct_trace_node_t *rule_ref = trace_new(CUPTI_CCT_TRACE_NODE_NON_TERMINAL_REF, 0);
    rule_ref->key = (uint64_t)rule;

    trace_delete(pattern_node->right);
    trace_delete(pattern_node);

    TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after delete (left: %p->%p->%p) (key: %p->%p->%p)",
      pattern_node_left, pattern_node_left->right, pattern_node_left->right->right,
      pattern_node_left->key, pattern_node_left->right->key, pattern_node_left->right->right->key);

    trace_append(thread_root, rule_ref);
    trace_ref_append(rule, rule_ref);

    TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after append (left: %p->%p->%p) (key: %p->%p->%p)",
      pattern_node_left, pattern_node_left->right, pattern_node_left->right->right,
      pattern_node_left->key, pattern_node_left->right->key, pattern_node_left->right->right->key);

    // There must be another A, otherwise, A should have been replaced
    // No need to enfore utility rule
    current = thread_root->left;

    shrunk = true;
  }

  return shrunk;
}


static int32_t
trace_condense
(
 uint32_t range_id,
 bool sampled,
 bool merge
)
{
  int32_t prev_range_id = -1;
  cupti_cct_trace_node_t *current = thread_root->left;

  // |A|
  if (current->left->left->type == CUPTI_CCT_TRACE_NODE_FLUSH) {
    // Single node, must be repeated with another range
    cupti_cct_trace_map_entry_t *entry = trace_map_lookup(current->left->left->key, current->left->key);
    assert(entry != NULL);

    if (merge) {
      prev_range_id = cupti_cct_trace_map_entry_range_id_get(entry);
      uint32_t num_threads = cupti_range_thread_list_num_threads();
      cupti_ip_norm_map_merge_thread(prev_range_id, range_id, sampled, num_threads);
    }

    cupti_cct_trace_node_t *trace_node = cupti_cct_trace_map_entry_cct_trace_node_get(entry);
    if (trace_node != current->left->left) {
      // Not the current range, I can take it out
      cupti_cct_trace_node_t *node1 = current->left->left;
      cupti_cct_trace_node_t *node2 = current->left;

      trace_delete(node1);
      trace_delete(node2);
    }
  }
  
  return prev_range_id;
}

//*****************************************************************************
// interface operations
//*****************************************************************************

bool
cupti_cct_trace_append
(
 uint32_t range_id,
 cct_node_t *cct
)
{
  // 1. Compress trace
  // Aa | Aa
  // 2. Index without compression
  // |A ... |A
  TRACE_MSG(CUPTI_CCT_TRACE, "Enter cupti_cct_trace_append %p", cct);

  trace_init();

  cupti_cct_trace_node_t *trace_node = trace_new(CUPTI_CCT_TRACE_NODE_TERMINAL, (uint64_t)cct);
  trace_append(thread_root, trace_node);

  TRACE_MSG(CUPTI_CCT_TRACE, "Trace partial after append (thread_root: %p<-%p<-%p<-%p) (key: %p<-%p<-%p<-%p)",
    thread_root, thread_root->left, thread_root->left->left, thread_root->left->left->left,
    thread_root->key, thread_root->left->key, thread_root->left->left->key, thread_root->left->left->left->key);

  bool ret = trace_shrink(range_id);

  TRACE_MSG(CUPTI_CCT_TRACE, "Exit cupti_cct_trace_append %p shrunk %d", cct, ret);

  return ret;
}


int32_t
cupti_cct_trace_flush
(
 uint32_t range_id,
 bool sampled,
 bool merge
)
{
  TRACE_MSG(CUPTI_CCT_TRACE, "Enter flush key trace");

  trace_init();

  cupti_cct_trace_node_t *trace_node = trace_new(CUPTI_CCT_TRACE_NODE_FLUSH, 0);
  trace_append(thread_root, trace_node);
  // Don't index A|
  
  int32_t prev_range_id = trace_condense(range_id, sampled, merge);

  TRACE_MSG(CUPTI_CCT_TRACE, "Exit flush key trace");

  return prev_range_id;
}


void
cupti_cct_trace_dump
(
)
{
  if (!thread_root) {
    return;
  }

  cupti_cct_trace_node_t *node = thread_root->right;
  printf("thread_root->");
  while (node != thread_root) {
    printf("%p", (void *)node->key);
    if (node->right != thread_root) {
      printf("->");
    } else {
      printf("\n");
    }
    node = node->right;
  }
  printf("\n");
}
