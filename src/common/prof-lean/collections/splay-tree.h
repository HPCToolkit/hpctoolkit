#include "collections-util.h"
#include "splay-tree-entry-data.h"

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>


/* Check if splay tree prefix is defined */
#ifdef SPLAY_TREE_PREFIX
  #define SPLAY_TREE_MEMBER(NAME) JOIN2(SPLAY_TREE_PREFIX, NAME)
#else
  #error SPLAY_TREE_PREFIX is not defined
#endif


/* Check if SPLAY_TREE_KEY_(TYPE&FIELD) is defined */
#ifndef SPLAY_TREE_KEY_TYPE
  #error SPLAY_TREE_KEY_TYPE is not defined
#endif

#ifndef SPLAY_TREE_KEY_FIELD
  #error SPLAY_TREE_KEY_FIELD is not defined
#endif


/* Check if comparison functions are defined */
#ifndef SPLAY_TREE_LT
  #define SPLAY_TREE_LT(A, B) ((A) < (B))
#endif

#ifndef SPLAY_TREE_GT
  #define SPLAY_TREE_GT(A, B) ((A) > (B))
#endif


#if !defined(SPLAY_TREE_DECLARE) && !defined(SPLAY_TREE_DEFINE)
#define SPLAY_TREE_DEFINE_INPLACE
#endif

/* Define splay tree type */
#if defined(SPLAY_TREE_DECLARE) || defined(SPLAY_TREE_DEFINE_INPLACE)
  /* Check if SPLAY_TREE_ENTRY_TYPE is defined */
  #ifndef SPLAY_TREE_ENTRY_TYPE
    #error SPLAY_TREE_ENTRY_TYPE is not defined
  #endif

  /* Define SPLAY_TREE_TYPE */
  #define SPLAY_TREE_TYPE SPLAY_TREE_MEMBER(t)
  typedef struct SPLAY_TREE_TYPE {
    SPLAY_TREE_ENTRY_TYPE *root;
  } SPLAY_TREE_TYPE;

  #define SPLAY_TREE_INITIALIZER { .root = NULL };
#endif



#ifdef SPLAY_TREE_DECLARE

void
SPLAY_TREE_MEMBER(init_splay_tree)
(
 SPLAY_TREE_TYPE *splay_tree
);


bool
SPLAY_TREE_MEMBER(insert)
(
 SPLAY_TREE_TYPE *splay_tree,
 SPLAY_TREE_ENTRY_TYPE *entry
);


SPLAY_TREE_ENTRY_TYPE *
SPLAY_TREE_MEMBER(lookup)
(
 SPLAY_TREE_TYPE *splay_tree,
 SPLAY_TREE_KEY_TYPE key
);


SPLAY_TREE_ENTRY_TYPE *
SPLAY_TREE_MEMBER(delete)
(
 SPLAY_TREE_TYPE *splay_tree,
 SPLAY_TREE_KEY_TYPE key
);


void
SPLAY_TREE_MEMBER(for_each)
(
 SPLAY_TREE_TYPE *splay_tree,
 void (*fn)(SPLAY_TREE_ENTRY_TYPE *, void *),
 void *arg
);


bool
SPLAY_TREE_MEMBER(empty)
(
 SPLAY_TREE_TYPE *splay_tree
);


size_t
SPLAY_TREE_MEMBER(size)
(
 SPLAY_TREE_TYPE *splay_tree
);

#endif



#if defined(SPLAY_TREE_DEFINE) || defined(SPLAY_TREE_DEFINE_INPLACE)

#ifdef SPLAY_TREE_DEFINE_INPLACE
#define SPLAY_TREE_FN_MOD static
#else
#define SPLAY_TREE_FN_MOD
#endif


/* private functions*/
static void
SPLAY_TREE_MEMBER(splay)
(
 SPLAY_TREE_ENTRY_TYPE **entry,
 SPLAY_TREE_KEY_TYPE key
)
{
  SPLAY_TREE_ENTRY_TYPE *root = *entry;

  SPLAY_TREE_ENTRY_TYPE dummy_node;
  SPLAY_TREE_ENTRY_TYPE *ltree_max, *rtree_min, *yy;

  if (root != NULL) {
    ltree_max = rtree_min = &dummy_node;
    for (;;) {
      if (SPLAY_TREE_LT(key, root->SPLAY_TREE_KEY_FIELD)) {
        if ((yy = root->left) == NULL) {
          break;
        }

        if (SPLAY_TREE_LT(key, yy->SPLAY_TREE_KEY_FIELD)) {
          root->left = yy->right;
          yy->right = root;
          root = yy;
          if ((yy = root->left) == NULL) {
            break;
          }
        }

        rtree_min->left = root;
        rtree_min = root;
      } else if (SPLAY_TREE_GT(key, root->SPLAY_TREE_KEY_FIELD)) {
        if ((yy = root->right) == NULL) {
          break;
        }

        if (SPLAY_TREE_GT(key, yy->SPLAY_TREE_KEY_FIELD)) {
          root->right = yy->left;
          yy->left = root;
          root = yy;
          if ((yy = root->right) == NULL) {
            break;
          }
        }

        ltree_max->right = root;
        ltree_max = root;
      } else {
        break;
      }

      root = yy;
    }

    ltree_max->right = root->left;
    rtree_min->left = root->right;
    root->left = dummy_node.right;
    root->right = dummy_node.left;
  }

  *entry = root;
}


static SPLAY_TREE_ENTRY_TYPE *
SPLAY_TREE_MEMBER(delete_root)
(
 SPLAY_TREE_TYPE *splay_tree
)
{
  SPLAY_TREE_ENTRY_TYPE * const root = splay_tree->root;

  if (root->left == NULL) {
    splay_tree->root = root->right;
  } else {
    SPLAY_TREE_MEMBER(splay)(&root->left, root->SPLAY_TREE_KEY_FIELD);
    root->left->right = root->right;
    splay_tree->root = root->left;
  }

  root->left = NULL;
  root->right = NULL;
  return root;
}


static void
SPLAY_TREE_MEMBER(for_each_inorder)
(
 SPLAY_TREE_ENTRY_TYPE *entry,
 void (*fn)(SPLAY_TREE_ENTRY_TYPE *, void *),
 void *arg
)
{
  if (entry == NULL) {
    return;
  }

  SPLAY_TREE_MEMBER(for_each_inorder)(entry->left, fn, arg);
  fn(entry, arg);
  SPLAY_TREE_MEMBER(for_each_inorder)(entry->right, fn, arg);
}


static void
SPLAY_TREE_MEMBER(count_helper)
(
 SPLAY_TREE_ENTRY_TYPE *entry,
 void *arg
)
{
  size_t *count = (size_t *) arg;
  ++(*count);
}


/* public functions */

__attribute__((unused))
SPLAY_TREE_FN_MOD void
SPLAY_TREE_MEMBER(init_splay_tree)
(
 SPLAY_TREE_TYPE *splay_tree
)
{
  splay_tree->root = NULL;
}


__attribute__((unused))
SPLAY_TREE_FN_MOD bool
SPLAY_TREE_MEMBER(insert)
(
 SPLAY_TREE_TYPE *splay_tree,
 SPLAY_TREE_ENTRY_TYPE *entry
)
{
  entry->left = NULL;
  entry->right = NULL;

  SPLAY_TREE_ENTRY_TYPE *root = splay_tree->root;
  if (root != NULL) {
    SPLAY_TREE_MEMBER(splay)(&root, entry->SPLAY_TREE_KEY_FIELD);

    if (entry->SPLAY_TREE_KEY_FIELD < root->SPLAY_TREE_KEY_FIELD) {
      entry->left = root->left;
      entry->right = root;
      root->left = NULL;
    } else if (entry->SPLAY_TREE_KEY_FIELD > root->SPLAY_TREE_KEY_FIELD) {
      entry->left = root;
      entry->right = root->right;
      root->right = NULL;
    } else {
      /* insert failed - key already present in the tree */
      return 0;
    }
  }

  splay_tree->root = entry;
  return 1;
}


__attribute__((unused))
SPLAY_TREE_FN_MOD SPLAY_TREE_ENTRY_TYPE *
SPLAY_TREE_MEMBER(lookup)
(
 SPLAY_TREE_TYPE *splay_tree,
 SPLAY_TREE_KEY_TYPE key
)
{
  SPLAY_TREE_MEMBER(splay)(&splay_tree->root, key);

  return splay_tree->root != NULL
         && splay_tree->root->SPLAY_TREE_KEY_FIELD == key
          ? splay_tree->root
          : NULL;
}


__attribute__((unused))
SPLAY_TREE_FN_MOD SPLAY_TREE_ENTRY_TYPE *
SPLAY_TREE_MEMBER(delete)
(
 SPLAY_TREE_TYPE *splay_tree,
 SPLAY_TREE_KEY_TYPE key
)
{
  SPLAY_TREE_MEMBER(splay)(&splay_tree->root, key);

  return splay_tree->root != NULL
         && splay_tree->root->SPLAY_TREE_KEY_FIELD == key
           ? SPLAY_TREE_MEMBER(delete_root)(splay_tree)
           : NULL;
}


__attribute__((unused))
SPLAY_TREE_FN_MOD void
SPLAY_TREE_MEMBER(for_each)
(
 SPLAY_TREE_TYPE *splay_tree,
 void (*fn)(SPLAY_TREE_ENTRY_TYPE *, void *),
 void *arg
)
{
  SPLAY_TREE_MEMBER(for_each_inorder)(splay_tree->root, fn, arg);
}


__attribute__((unused))
SPLAY_TREE_FN_MOD bool
SPLAY_TREE_MEMBER(empty)
(
 SPLAY_TREE_TYPE *splay_tree
)
{
  return splay_tree->root == NULL;
}


__attribute__((unused))
SPLAY_TREE_FN_MOD size_t
SPLAY_TREE_MEMBER(size)
(
 SPLAY_TREE_TYPE *splay_tree
)
{
  size_t count = 0;
  SPLAY_TREE_MEMBER(for_each_inorder)(splay_tree->root,
                                      SPLAY_TREE_MEMBER(count_helper),
                                      &count);
  return count;
}

#endif


#undef SPLAY_TREE_PREFIX
#undef SPLAY_TREE_MEMBER
#undef SPLAY_TREE_TYPE
#undef SPLAY_TREE_KEY_TYPE
#undef SPLAY_TREE_KEY_FIELD
#undef SPLAY_TREE_DECLARE
#undef SPLAY_TREE_DEFINE
#undef SPLAY_TREE_DEFINE_INPLACE
