#include "cupti-ip-norm-map.h"

#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "../gpu-metrics.h"

#define DEBUG
#ifdef DEBUG
#define TRACE_IP_NORM_MAP_MSG(...) TMSG(__VA_ARGS__)
#else
#define TRACE_IP_NORM_MAP_MSG(...)
#endif

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct splay_ip_norm_node_t {
  struct splay_ip_norm_node_t *left;
  struct splay_ip_norm_node_t *right;
  ip_normalized_t key;
} splay_ip_norm_node_t;


typedef enum splay_order_t {
  splay_inorder = 1,
  splay_allorder = 2
} splay_order_t;


typedef enum splay_visit_t {
  splay_preorder_visit = 1,
  splay_inorder_visit = 2,
  splay_postorder_visit = 3
} splay_visit_t;


typedef void (*splay_fn_t)
(
 splay_ip_norm_node_t *node,
 splay_visit_t order,
 void *arg
);


static bool ip_norm_cmp_lt(ip_normalized_t left, ip_normalized_t right);
static bool ip_norm_cmp_gt(ip_normalized_t left, ip_normalized_t right);

#define ip_norm_builtin_lt(a, b) (ip_norm_cmp_lt(a, b))
#define ip_norm_builtin_gt(a, b) (ip_norm_cmp_gt(a, b))

#define IP_NORM_SPLAY_TREE(type, root, key, value, left, right)	\
  GENERAL_SPLAY_TREE(type, root, key, value, value, left, right, ip_norm_builtin_lt, ip_norm_builtin_gt)

//*****************************************************************************
// macros 
//*****************************************************************************

#define splay_macro_body_ignore(x) ;
#define splay_macro_body_show(x) x

// declare typed interface functions 
#define typed_splay_declare(type)		\
  typed_splay(type, splay_macro_body_ignore)

// implementation of typed interface functions 
#define typed_splay_impl(type)			\
  typed_splay(type, splay_macro_body_show)

// routine name for a splay operation
#define splay_op(op) \
  splay_ip_norm ## _ ## op

#define typed_splay_node(type) \
  type ## _ ## splay_ip_norm_node_t

// routine name for a typed splay operation
#define typed_splay_op(type, op) \
  type ## _  ## op

// insert routine name for a typed splay 
#define typed_splay_splay(type) \
  typed_splay_op(type, splay)

// insert routine name for a typed splay 
#define typed_splay_insert(type) \
  typed_splay_op(type, insert)

// lookup routine name for a typed splay 
#define typed_splay_lookup(type) \
  typed_splay_op(type, lookup)

// delete routine name for a typed splay 
#define typed_splay_delete(type) \
  typed_splay_op(type, delete)

// forall routine name for a typed splay 
#define typed_splay_forall(type) \
  typed_splay_op(type, forall)

// count routine name for a typed splay 
#define typed_splay_count(type) \
  typed_splay_op(type, count)

// define typed wrappers for a splay type 
#define typed_splay(type, macro) \
  static bool \
  typed_splay_insert(type) \
  (typed_splay_node(type) **root, typed_splay_node(type) *node)	\
  macro({ \
    return splay_op(insert)((splay_ip_norm_node_t **) root, (splay_ip_norm_node_t *) node); \
  }) \
\
  static typed_splay_node(type) * \
  typed_splay_lookup(type) \
  (typed_splay_node(type) **root, ip_normalized_t key) \
  macro({ \
    typed_splay_node(type) *result = (typed_splay_node(type) *) \
      splay_op(lookup)((splay_ip_norm_node_t **) root, key); \
    return result; \
  }) \
\
  static void \
  typed_splay_forall(type) \
  (typed_splay_node(type) *root, splay_order_t order, void (*fn)(typed_splay_node(type) *, splay_visit_t visit, void *), void *arg) \
  macro({ \
    splay_op(forall)((splay_ip_norm_node_t *) root, order, (splay_fn_t) fn, arg); \
  }) 

#define typed_splay_alloc(free_list, splay_node_type)		\
  (splay_node_type *) splay_ip_norm_alloc_helper		\
  ((splay_ip_norm_node_t **) free_list, sizeof(splay_node_type))	

#define typed_splay_free(free_list, node)			\
  splay_ip_norm_free_helper					\
  ((splay_ip_norm_node_t **) free_list,				\
   (splay_ip_norm_node_t *) node)

//******************************************************************************
// interface operations
//******************************************************************************

static bool
ip_norm_cmp_gt
(
 ip_normalized_t left,
 ip_normalized_t right
)
{
  if (left.lm_id > right.lm_id) {
    return true;
  } else if (left.lm_id == right.lm_id) {
    if (left.lm_ip > right.lm_ip) {
      return true;
    } 
  }
  return false;
}


static bool
ip_norm_cmp_lt
(
 ip_normalized_t left,
 ip_normalized_t right
)
{
  if (left.lm_id < right.lm_id) {
    return true;
  } else if (left.lm_id == right.lm_id) {
    if (left.lm_ip < right.lm_ip) {
      return true;
    }
  }
  return false;
}


static bool
ip_norm_cmp_eq
(
 ip_normalized_t left,
 ip_normalized_t right
)
{
  return left.lm_id == right.lm_id && left.lm_ip == right.lm_ip;
}


static splay_ip_norm_node_t *
splay_ip_norm_splay
(
 splay_ip_norm_node_t *root,
 ip_normalized_t splay_key
)
{
  IP_NORM_SPLAY_TREE(splay_ip_norm_node_t, root, splay_key, key, left, right);

  return root;
}


static bool
splay_ip_norm_insert
(
 splay_ip_norm_node_t **root,
 splay_ip_norm_node_t *node
)
{
  node->left = node->right = NULL;

  if (*root != NULL) {
    *root = splay_ip_norm_splay(*root, node->key);
    
    if (ip_norm_cmp_lt(node->key, (*root)->key)) {
      node->left = (*root)->left;
      node->right = *root;
      (*root)->left = NULL;
    } else if (ip_norm_cmp_gt(node->key, (*root)->key)) {
      node->left = *root;
      node->right = (*root)->right;
      (*root)->right = NULL;
    } else {
      // key already present in the tree
      return true; // insert failed 
    }
  } 

  *root = node;

  return true; // insert succeeded 
}


static splay_ip_norm_node_t *
splay_ip_norm_lookup
(
 splay_ip_norm_node_t **root,
 ip_normalized_t key
)
{
  splay_ip_norm_node_t *result = 0;

  *root = splay_ip_norm_splay(*root, key);

  if (*root && ip_norm_cmp_eq((*root)->key, key)) {
    result = *root;
  }

  return result;
}


static void
splay_forall_allorder
(
 splay_ip_norm_node_t *node,
 splay_fn_t fn,
 void *arg
)
{
  if (node) {
    fn(node, splay_preorder_visit, arg);
    splay_forall_allorder(node->left, fn, arg);
    fn(node, splay_inorder_visit, arg);
    splay_forall_allorder(node->right, fn, arg);
    fn(node, splay_postorder_visit, arg);
  }
}


static void
splay_ip_norm_forall
(
 splay_ip_norm_node_t *root,
 splay_order_t order,
 splay_fn_t fn,
 void *arg
)
{
  switch (order) {
    case splay_allorder: 
      splay_forall_allorder(root, fn, arg);
      break;
    default:
      break;
  }
}


static splay_ip_norm_node_t *
splay_ip_norm_alloc_helper
(
 splay_ip_norm_node_t **free_list, 
 size_t size
)
{
  splay_ip_norm_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (splay_ip_norm_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


static void
splay_ip_norm_free_helper
(
 splay_ip_norm_node_t **free_list, 
 splay_ip_norm_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}


//******************************************************************************
// macros
//******************************************************************************

#define st_insert				\
  typed_splay_insert(ip_norm)

#define st_lookup				\
  typed_splay_lookup(ip_norm)

#define st_delete				\
  typed_splay_delete(ip_norm)

#define st_forall				\
  typed_splay_forall(ip_norm)

#define st_count				\
  typed_splay_count(ip_norm)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(ip_norm))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(ip_norm) cupti_ip_norm_map_entry_t 

struct cupti_ip_norm_map_entry_s {
  struct cupti_ip_norm_map_entry_s *left;
  struct cupti_ip_norm_map_entry_s *right;
  ip_normalized_t ip_norm;
  cct_node_t *cct;
};

static __thread cupti_ip_norm_map_entry_t *map_root = NULL;
static __thread cupti_ip_norm_map_entry_t *free_list = NULL;


typed_splay_impl(ip_norm)


static void
flush_fn_helper
(
 cupti_ip_norm_map_entry_t *entry,
 splay_visit_t visit_type,
 void *args
)
{
  if (visit_type == splay_postorder_visit) {
    st_free(&free_list, entry);
  }
}


typedef struct merge_args_s {
 uint32_t prev_range_id;
 uint32_t range_id;
 bool sampled;
} merge_args_t;


static void
merge_fn_helper
(
 cupti_ip_norm_map_entry_t *entry,
 splay_visit_t visit_type,
 void *args
)
{
  if (visit_type == splay_postorder_visit) {
    merge_args_t *merge_args = (merge_args_t *)args;

    ip_normalized_t prev_range_ip = { .lm_id = HPCRUN_FMT_GPU_RANGE_NODE, .lm_ip = merge_args->prev_range_id };
    ip_normalized_t range_ip = { .lm_id = HPCRUN_FMT_GPU_RANGE_NODE, .lm_ip = merge_args->range_id };

    cct_node_t *api_node = entry->cct;
    cct_node_t *kernel_ip = hpcrun_cct_children(api_node);
    cct_node_t *context = hpcrun_cct_children(kernel_ip);

    // Mutate functions
    cct_node_t *prev_range_node = hpcrun_cct_insert_ip_norm(context, prev_range_ip);
    cct_node_t *range_node = hpcrun_cct_insert_ip_norm(context, range_ip);
    
    uint64_t kernel_count = gpu_metrics_get_kernel_count(range_node);
    uint64_t sampled_kernel_count = merge_args->sampled ? kernel_count : 0;

    gpu_metrics_attribute_kernel_count(prev_range_node, sampled_kernel_count, kernel_count);
    // XXX(Keren): is this function stable?
    hpcrun_cct_delete_self(range_node);
  }
}


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm,
 cct_node_t *cct
)
{
  cupti_ip_norm_map_entry_t *result = st_lookup(root, ip_norm);

  cupti_ip_norm_map_ret_t ret;
  if (result == NULL) {
    ret = CUPTI_IP_NORM_MAP_NOT_EXIST;
  } else if (result->cct != cct) {
    ret = CUPTI_IP_NORM_MAP_DUPLICATE;
  } else {
    ret = CUPTI_IP_NORM_MAP_EXIST;
  }

  TRACE_IP_NORM_MAP_MSG(CUPTI_CCT_TRACE, "IP norm map lookup (lm_id: %d, lm_ip: %p, cct: %p)->(ret: %d)",
    ip_norm.lm_id, ip_norm.lm_ip, cct, ret);

  return ret;
}


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup_thread
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
)
{
  cupti_ip_norm_map_lookup(&map_root, ip_norm, cct);
}


void
cupti_ip_norm_map_insert
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm,
 cct_node_t *cct
)
{
  cupti_ip_norm_map_entry_t *entry = st_lookup(root, ip_norm);

  if (entry == NULL) {
    entry = st_alloc(&free_list);
    entry->ip_norm = ip_norm;
    entry->cct = cct;
    st_insert(root, entry);
  }

  TRACE_IP_NORM_MAP_MSG(CUPTI_CCT_TRACE, "IP norm map insert (lm_id: %d, lm_ip: %p, cct: %p)->(entry: %p)",
    ip_norm.lm_id, ip_norm.lm_ip, cct, entry);
}


void
cupti_ip_norm_map_insert_thread
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
)
{
  cupti_ip_norm_map_insert(&map_root, ip_norm, cct);
}


void
cupti_ip_norm_map_clear
(
 cupti_ip_norm_map_entry_t **root
)
{
  st_forall((*root), splay_allorder, flush_fn_helper, NULL);
  (*root) = NULL;
}


void
cupti_ip_norm_map_clear_thread
(
)
{
  cupti_ip_norm_map_clear(&map_root);
}


void
cupti_ip_norm_map_merge
(
 cupti_ip_norm_map_entry_t **root,
 uint32_t prev_range_id,
 uint32_t range_id,
 bool sampled
)
{
  merge_args_t merge_args = {
    .prev_range_id = prev_range_id,
    .range_id = range_id,
    .sampled = sampled
  };
  st_forall((*root), splay_allorder, merge_fn_helper, &merge_args);
}


void
cupti_ip_norm_map_merge_thread
(
 uint32_t prev_range_id,
 uint32_t range_id,
 bool sampled
)
{
  cupti_ip_norm_map_merge(&map_root, prev_range_id, range_id, sampled);
}
