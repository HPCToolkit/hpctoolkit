#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-unwind-map.h"

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct splay_unwind_node_t {
  struct splay_unwind_node_t *left;
  struct splay_unwind_node_t *right;
  unwind_key_t key;
} splay_unwind_node_t;


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
 splay_unwind_node_t *node,
 splay_visit_t order,
 void *arg
);


bool unwind_cmp_lt(unwind_key_t left, unwind_key_t right);
bool unwind_cmp_gt(unwind_key_t left, unwind_key_t right);

#define unwind_builtin_lt(a, b) (unwind_cmp_lt(a, b))
#define unwind_builtin_gt(a, b) (unwind_cmp_gt(a, b))

#define UNWIND_SPLAY_TREE(type, root, key, value, left, right)	\
  GENERAL_SPLAY_TREE(type, root, key, value, value, left, right, unwind_builtin_lt, unwind_builtin_gt)

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
  splay_unwind ## _ ## op

#define typed_splay_node(type) \
  type ## _ ## splay_unwind_node_t

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
    return splay_op(insert)((splay_unwind_node_t **) root, (splay_unwind_node_t *) node); \
  }) \
\
  static typed_splay_node(type) * \
  typed_splay_lookup(type) \
  (typed_splay_node(type) **root, unwind_key_t key) \
  macro({ \
    typed_splay_node(type) *result = (typed_splay_node(type) *) \
      splay_op(lookup)((splay_unwind_node_t **) root, key); \
    return result; \
  })

#define typed_splay_alloc(free_list, splay_node_type)		\
  (splay_node_type *) splay_unwind_alloc_helper		\
  ((splay_unwind_node_t **) free_list, sizeof(splay_node_type))	

#define typed_splay_free(free_list, node)			\
  splay_unwind_free_helper					\
  ((splay_unwind_node_t **) free_list,				\
   (splay_unwind_node_t *) node)

//******************************************************************************
// interface operations
//******************************************************************************

bool
unwind_cmp_gt
(
 unwind_key_t left,
 unwind_key_t right
)
{
  if (left.stack_length > right.stack_length) {
    return true;
  } else if (left.stack_length == right.stack_length) {
    if (left.function_id.lm_id > right.function_id.lm_id) {
      return true;
    } else if (left.function_id.lm_id == right.function_id.lm_id) {
      if (left.function_id.lm_ip > right.function_id.lm_ip) {
        return true;
      } else if (left.function_id.lm_ip == right.function_id.lm_ip) {
        if (left.prev > right.prev) {
          return true;
        }
      }
    }
  }
  return false;
}


bool
unwind_cmp_lt
(
 unwind_key_t left,
 unwind_key_t right
)
{
  if (left.stack_length < right.stack_length) {
    return true;
  } else if (left.stack_length == right.stack_length) {
    if (left.function_id.lm_id < right.function_id.lm_id) {
      return true;
    } else if (left.function_id.lm_id == right.function_id.lm_id) {
      if (left.function_id.lm_ip < right.function_id.lm_ip) {
        return true;
      } else if (left.function_id.lm_ip == right.function_id.lm_ip) {
        if (left.prev < right.prev) {
          return true;
        }
      }
    }
  }
  return false;
}


bool
unwind_cmp_eq
(
 unwind_key_t left,
 unwind_key_t right
)
{
  return left.stack_length == right.stack_length && left.function_id.lm_id == right.function_id.lm_id &&
    left.function_id.lm_ip && right.function_id.lm_ip && left.prev == right.prev;
}


splay_unwind_node_t *
splay_splay
(
 splay_unwind_node_t *root,
 unwind_key_t splay_key
)
{
  UNWIND_SPLAY_TREE(splay_unwind_node_t, root, splay_key, key, left, right);

  return root;
}


bool
splay_unwind_insert
(
 splay_unwind_node_t **root,
 splay_unwind_node_t *node
)
{
  node->left = node->right = NULL;

  if (*root != NULL) {
    *root = splay_splay(*root, node->key);
    
    if (unwind_cmp_lt(node->key, (*root)->key)) {
      node->left = (*root)->left;
      node->right = *root;
      (*root)->left = NULL;
    } else if (unwind_cmp_gt(node->key, (*root)->key)) {
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


splay_unwind_node_t *
splay_unwind_lookup
(
 splay_unwind_node_t **root,
 unwind_key_t key
)
{
  splay_unwind_node_t *result = 0;

  *root = splay_splay(*root, key);

  if (*root && unwind_cmp_eq((*root)->key, key)) {
    result = *root;
  }

  return result;
}


splay_unwind_node_t *
splay_unwind_alloc_helper
(
 splay_unwind_node_t **free_list, 
 size_t size
)
{
  splay_unwind_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (splay_unwind_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


void
splay_unwind_free_helper
(
 splay_unwind_node_t **free_list, 
 splay_unwind_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}

//******************************************************************************
// private operations
//******************************************************************************

#define st_insert				\
  typed_splay_insert(unwind)

#define st_lookup				\
  typed_splay_lookup(unwind)

#define st_delete				\
  typed_splay_delete(unwind)

#define st_forall				\
  typed_splay_forall(unwind)

#define st_count				\
  typed_splay_count(unwind)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(unwind))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(unwind) cupti_unwind_map_entry_t

typedef struct cupti_unwind_map_entry_t {
  struct cupti_unwind_map_entry_t *left;
  struct cupti_unwind_map_entry_t *right;
  unwind_key_t key;
  cct_node_t *cct_node;
} cupti_unwind_map_entry_t;

static __thread cupti_unwind_map_entry_t *map_root = NULL;
static __thread cupti_unwind_map_entry_t *free_list = NULL;

typed_splay_impl(unwind)

static cupti_unwind_map_entry_t *
cupti_unwind_map_new
(
 unwind_key_t key,
 cct_node_t *cct_node
)
{
  cupti_unwind_map_entry_t *entry = st_alloc(&free_list);
  
  memset(entry, 0, sizeof(cupti_unwind_map_entry_t));

  entry->key = key;
  entry->cct_node = cct_node;

  return entry;
}

//******************************************************************************
// public operations
//******************************************************************************

bool
cupti_unwind_map_insert
(
 unwind_key_t key,
 cct_node_t *cct_node
)
{
  cupti_unwind_map_entry_t *entry = st_lookup(&map_root, key);

  bool ret = false;

  if (entry == NULL) {
    entry = cupti_unwind_map_new(key, cct_node);
    st_insert(&map_root, entry);
    ret = true;
  }

  return ret;
}


cct_node_t *
cupti_unwind_map_cct_node_lookup
(
 unwind_key_t key
)
{
  cupti_unwind_map_entry_t *entry = st_lookup(&map_root, key);

  if (entry == NULL) {
    return NULL; 
  }
  
  return entry->cct_node;
}
