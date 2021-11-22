#include "cupti-cct-trie.h"

#include <stdio.h>

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>
#include <lib/prof-lean/splay-macros.h>

#include "cupti-range.h"
#include "cupti-ip-norm-map.h"
#include "cupti-range-thread-list.h"
#include "../gpu-metrics.h"
#include "../gpu-range.h"
#include "../gpu-op-placeholders.h"

struct cupti_cct_trie_node_s {
  cct_node_t *key;
  uint32_t range_id;

  struct cupti_cct_trie_node_s* parent;
  struct cupti_cct_trie_node_s* children;

  struct cupti_cct_trie_node_s* left;
  struct cupti_cct_trie_node_s* right;
};

static __thread cupti_cct_trie_node_t *trie_root = NULL;
static __thread cupti_cct_trie_node_t *trie_logic_root = NULL;
static __thread cupti_cct_trie_node_t *free_list = NULL;
static __thread cupti_cct_trie_node_t *trie_cur = NULL;

//***********************************
// splay
//***********************************
static bool cct_trie_cmp_lt(cct_node_t *left, cct_node_t *right) {
  return left < right;
}

static bool cct_trie_cmp_gt(cct_node_t *left, cct_node_t *right) {
  return left > right;
}

#define l_lt(a, b) cct_trie_cmp_lt(a, b)
#define l_gt(a, b) cct_trie_cmp_gt(a, b)

static cupti_cct_trie_node_t *
splay(cupti_cct_trie_node_t *node, cct_node_t *key)
{
  GENERAL_SPLAY_TREE(cupti_cct_trie_node_s, node, key, key, key, left, right, l_lt, l_gt);
  return node;
}

//***********************************
// allocator
//***********************************

static cupti_cct_trie_node_t *
cupti_cct_trie_alloc_helper
(
 cupti_cct_trie_node_t **free_list, 
 size_t size
)
{
  cupti_cct_trie_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (cupti_cct_trie_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


static void
cupti_cct_trie_free_helper
(
 cupti_cct_trie_node_t **free_list, 
 cupti_cct_trie_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}

#define trie_alloc(free_list)		\
  (cupti_cct_trie_node_t *) cupti_cct_trie_alloc_helper		\
  ((cupti_cct_trie_node_t **) free_list, sizeof(cupti_cct_trie_node_t))	

#define trie_free(free_list, node)			\
  cupti_cct_trie_free_helper					\
  ((cupti_cct_trie_node_t **) free_list,				\
   (cupti_cct_trie_node_t *) node)


static cupti_cct_trie_node_t *
cct_trie_new
(
 cupti_cct_trie_node_t *parent,
 cct_node_t *key
)
{
  cupti_cct_trie_node_t *trie_node = trie_alloc(&free_list);
  trie_node->range_id = GPU_RANGE_NULL;
  trie_node->key = key;
  trie_node->parent = parent;
  trie_node->children = NULL;
  trie_node->left = NULL;
  trie_node->right = NULL;

  return trie_node;
}


static void
cct_trie_init
(
 void
)
{
  if (trie_root == NULL) {
    trie_root = cct_trie_new(NULL, NULL);
    trie_logic_root = trie_root;
    trie_cur = trie_root;
  }
}

//***********************************
// interface
//***********************************

bool
cupti_cct_trie_append
(
 uint32_t range_id,
 cct_node_t *cct
)
{
  cct_trie_init();

  cupti_cct_trie_node_t *found = splay(trie_cur->children, cct);
  trie_cur->children = found;

  if (found && found->key == cct) {
    trie_cur = found;
    return true;
  }

  cupti_cct_trie_node_t *new = cct_trie_new(trie_cur, cct);
  trie_cur->children = new;
  trie_cur = new;
  // Only assign range_id when a node is created
  trie_cur->range_id = range_id;

  if (!found) {
    // Do nothing
  } else if (cct < found->key) {
    new->left = found->left;
    new->right = found;
    found->left = NULL;
  } else {
    new->left = found;
    new->right = found->right;
    found->right = NULL;
  }

  return false;
}


void
cupti_cct_trie_unwind
(
)
{
  if (trie_cur != trie_root) {
    trie_cur = trie_cur->parent;
  }
}


static void
cct_trie_merge_thread
(
 uint32_t context_id,
 uint32_t range_id,
 bool active,
 bool logic
)
{
  cupti_cct_trie_node_t *cur = trie_cur;
  ip_normalized_t kernel_ip = gpu_op_placeholder_ip(gpu_placeholder_type_kernel);

  while (cur != trie_logic_root) {
    cct_node_t *kernel_ph = hpcrun_cct_insert_ip_norm(cur->key, kernel_ip);
    cct_node_t *kernel_ph_children = hpcrun_cct_children(kernel_ph);
    cct_node_t *context_node = hpcrun_cct_insert_context(kernel_ph_children, context_id);

    cct_node_t *prev_range_node = hpcrun_cct_insert_range(context_node, range_id);
    uint64_t sampled_kernel_count = active ? 1 : 0;
    gpu_metrics_attribute_kernel_count(prev_range_node, sampled_kernel_count, 1);

    cur = cur->parent;
  }

  if (logic) {
    trie_logic_root = trie_cur;
  } else {
    trie_logic_root = trie_root;
    trie_cur = trie_root;
  }
}


typedef struct cct_trie_args_s {
  uint32_t context_id;
  uint32_t range_id;
  bool active;
  bool logic;
  bool progress;
} cct_trie_args_t;


void
cct_trie_notify_fn
(
 int thread_id,
 void *entry,
 void *args
)
{
  if (thread_id == cupti_range_thread_list_id_get()) {
    // We don't notify the starting thread
    return;
  }

  cct_trie_args_t *cct_trie_args = (cct_trie_args_t *)args;
  uint32_t context_id = cct_trie_args->context_id;
  uint32_t range_id = cct_trie_args->range_id;
  bool active = cct_trie_args->active;
  bool logic = cct_trie_args->logic;

  cupti_range_thread_list_notification_update(entry, context_id, range_id, active, logic);
}


uint32_t
cupti_cct_trie_flush
(
 uint32_t context_id,
 uint32_t range_id,
 bool active,
 bool logic
)
{
  cct_trie_init();

  uint32_t prev_range_id = trie_cur->range_id;

  // Traverse up and use original range_id to merge
  cct_trie_merge_thread(context_id, prev_range_id, active, logic);
    
  // Notify the other threads
  cct_trie_args_t args = {
    .context_id = context_id,
    .range_id = prev_range_id,
    .active = active,
    .logic = true
  }; 
  cupti_range_thread_list_apply(cct_trie_notify_fn, &args);

  return prev_range_id;
}


void
cupti_cct_trie_notification_process
(
)
{
  cct_trie_init();

  uint32_t context_id = cupti_range_thread_list_notification_context_id_get();
  uint32_t range_id = cupti_range_thread_list_notification_range_id_get();
  bool active = cupti_range_thread_list_notification_active_get();
  bool logic = cupti_range_thread_list_notification_logic_get();

  if (trie_cur == trie_logic_root || range_id == GPU_RANGE_NULL) {
    return;
  }

  // Traverse up and use original range_id to merge
  cct_trie_merge_thread(context_id, range_id, active, logic);
  cupti_range_thread_list_notification_clear();
}


cupti_cct_trie_node_t *
cupti_cct_trie_root_get
(
)
{
  cct_trie_init();
  return trie_root;
}


cupti_cct_trie_node_t **
cupti_cct_trie_cur_ptr_get
(
)
{
  cct_trie_init();
  return &trie_cur;
}


cupti_cct_trie_node_t **
cupti_cct_trie_logic_root_ptr_get
(
)
{
  cct_trie_init();
  return &trie_logic_root;
}

//**********************************************
// Debuging
//**********************************************

static void
cct_trie_walk(cupti_cct_trie_node_t* trie_node, int *num_nodes, int *single_path_nodes);


static void
cct_trie_walk_child(cupti_cct_trie_node_t* trie_node, int *num_nodes, int *single_path_nodes)
{
  if (!trie_node) return;

  cct_trie_walk_child(trie_node->left, num_nodes, single_path_nodes);
  cct_trie_walk_child(trie_node->right, num_nodes, single_path_nodes);
  printf("cct %p, parent %p, range_id %d\n", trie_node->key, trie_node->parent->key, trie_node->range_id);
  cct_trie_walk(trie_node, num_nodes, single_path_nodes);
}


static void
cct_trie_walk(cupti_cct_trie_node_t* trie_node, int *num_nodes, int *single_path_nodes)
{
  if (!trie_node) return;
  ++(*num_nodes);
  if (trie_node->children != NULL && trie_node->children->left == NULL && trie_node->children->right == NULL) {
    ++(*single_path_nodes);
  }
  cct_trie_walk_child(trie_node->children, num_nodes, single_path_nodes);
}


void
cupti_cct_trie_dump
(
)
{
  int num_nodes = 0;
  int single_path_nodes = 0;
  cct_trie_walk(trie_root, &num_nodes, &single_path_nodes);
  printf("num_nodes %d, single_path_nodes %d\n", num_nodes, single_path_nodes);
}
