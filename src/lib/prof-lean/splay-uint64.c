//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0



//******************************************************************************
// local includes
//******************************************************************************

#include "splay-macros.h"
#include "splay-uint64.h"



//******************************************************************************
// private operations
//******************************************************************************

static splay_uint64_node_t *
splay_splay
(
 splay_uint64_node_t *root,
 uint64_t splay_key
)
{
  REGULAR_SPLAY_TREE(splay_uint64_node_t, root, splay_key, key, left, right);

  return root;
}


static void
splay_delete_root
(
 splay_uint64_node_t **root
)
{
  splay_uint64_node_t *map_root = *root;

  if (map_root->left == NULL) {
    map_root = map_root->right;
  } else {
    map_root->left = splay_splay(map_root->left, map_root->key);
    map_root->left->right = map_root->right;
    map_root = map_root->left;
  }

  // detach deleted node from others
  (*root)->left = (*root)->right = NULL; 

  // set new root
  *root = map_root; 
}



static void
splay_forall_inorder
(
 splay_uint64_node_t *node,
 splay_fn_t fn,
 void *arg
)
{
  if (node) {
    splay_forall_inorder(node->left, fn, arg);
    fn(node, splay_inorder_visit, arg);
    splay_forall_inorder(node->right, fn, arg);
  }
}


static void
splay_forall_allorder
(
 splay_uint64_node_t *node,
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
splay_count_helper
(
 splay_uint64_node_t *node,
 splay_visit_t order,
 void *arg
)
{
  uint64_t *count = (uint64_t *) arg;
  (*count)++;
}



//******************************************************************************
// interface operations
//******************************************************************************

bool
splay_uint64_insert
(
 splay_uint64_node_t **root,
 splay_uint64_node_t *node
)
{
  node->left = node->right = NULL;

  if (*root != NULL) {
    *root = splay_splay(*root, node->key);
    
    if (node->key < (*root)->key) {
      node->left = (*root)->left;
      node->right = *root;
      (*root)->left = NULL;
    } else if (node->key > (*root)->key) {
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


splay_uint64_node_t *
splay_uint64_lookup
(
 splay_uint64_node_t **root,
 uint64_t key
)
{
  splay_uint64_node_t *result = 0;

  *root = splay_splay(*root, key);

  if (*root && (*root)->key == key) {
    result = *root;
  }

  return result;
}


splay_uint64_node_t *
splay_uint64_delete
(
 splay_uint64_node_t **root,
 uint64_t key
)
{
  splay_uint64_node_t *removed = NULL;
  
  if (*root) {
    *root = splay_splay(*root, key);

    if ((*root)->key == key) {
      removed = *root;
      splay_delete_root(root);
    }
  }

  return removed;
}


void
splay_uint64_forall
(
 splay_uint64_node_t *root,
 splay_order_t order,
 splay_fn_t fn,
 void *arg
)
{
  switch (order) {
  case splay_inorder: 
    splay_forall_inorder(root, fn, arg);
    break;
  case splay_allorder: 
    splay_forall_allorder(root, fn, arg);
    break;
  }
}


uint64_t 
splay_uint64_count
(
 splay_uint64_node_t *node
)
{
  uint64_t count = 0;
  splay_uint64_forall(node, splay_inorder, splay_count_helper, &count);
  return count;
}



//******************************************************************************
// unit test
//******************************************************************************

#if UNIT_TEST

#include <stdio.h>



#define N 20

#define st_insert				\
  typed_splay_insert(int)

#define st_lookup				\
  typed_splay_lookup(int)

#define st_delete				\
  typed_splay_delete(int)

#define st_forall				\
  typed_splay_forall(int)

#define st_count				\
  typed_splay_count(int)



typedef struct typed_splay_node(int) {
  struct typed_splay_node(int) *left;
  struct typed_splay_node(int) *right;
  uint64_t key;
  long mysquared;
} typed_splay_node(int);



typedef typed_splay_node(int) int_splay_t;



int_splay_t *root = 0;



typed_splay_impl(int)



int_splay_t *
splay_node
(
 uint64_t i
)
{
  int_splay_t *node = (int_splay_t *) malloc(sizeof(int_splay_t));
  node->left = node->right = NULL;
  node->key = i;
  node->mysquared = i * i;

  return node;
}


int
digits
(
 uint64_t n
)
{
  int val = 1;
  if (n > 10) val++;
  return val;
}

void
printnode
(
 int_splay_t *node,
 splay_visit_t order,
 void *arg
)
{
  int *depth = (int *) arg;

  if (order == splay_preorder_visit) (*depth)++;
  if (order == splay_inorder_visit) {
    printf("%*s%ld%*s%ld\n", *depth, "", node->key, 
	   60-*depth-digits(node->key), "", node->mysquared);
  }
  if (order == splay_postorder_visit) (*depth)--;
}


int
main
(
 int argc, 
 char **argv
)
{
  int i;
  uint64_t inserted = 0;

  srand(12345678);
  for (i = 0; i < N; i++) {
    int val = rand() % 100;
    if (st_lookup(&root, val) == NULL) {
      st_insert(&root, splay_node(val)); 
      inserted++;
    }
  }

  printf("node count: inserted %ld, count %ld\n", inserted, st_count(root));

  int depth = 0;

  printf("**** allorder ****\n");
  st_forall(root, splay_allorder, printnode, &depth);


  for (i = 0; i < N; i++) {
    int val = rand() % 100;
    if (st_lookup(&root, val) == NULL) {
      st_insert(&root, splay_node(val)); 
      inserted++;
    }
  }

  printf("node count: inserted %ld, count %ld\n", inserted, st_count(root));

  printf("**** allorder ****\n");
  st_forall(root, splay_allorder, printnode, &depth);

  printf("**** inorder ****\n");
  st_forall(root, splay_inorder, printnode, &depth);

  srand(12345678);
  for (i = 0; i < N; i++) {
    int val = rand() % 100;
    if (st_delete(&root, val)) {
      inserted--;
    }
  }

  printf("**** allorder ****\n");
  st_forall(root, splay_allorder, printnode, &depth);

  printf("**** inorder ****\n");
  st_forall(root, splay_inorder, printnode, &depth);

  for (i = 0; i < N; i++) {
    int val = rand() % 100;
    if (st_delete(&root, val)) {
      inserted--;
    }
  }

  printf("**** allorder ****\n");
  st_forall(root, splay_allorder, printnode, &depth);

  printf("**** inorder ****\n");
  st_forall(root, splay_inorder, printnode, &depth);
}

#endif
