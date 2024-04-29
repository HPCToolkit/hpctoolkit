#include "../splay-tree-entry-data.h"

#define KEY_TYPE      unsigned long
#define VALUE_TYPE    int


typedef struct my_splay_tree_entry_t {
  SPLAY_TREE_ENTRY_DATA(struct my_splay_tree_entry_t);
  KEY_TYPE key;
  VALUE_TYPE value;
} my_splay_tree_entry_t;


/* Instantiate splay_tree */
#define SPLAY_TREE_PREFIX         my_splay_tree
#define SPLAY_TREE_KEY_TYPE       KEY_TYPE
#define SPLAY_TREE_KEY_FIELD      key
#define SPLAY_TREE_ENTRY_TYPE     my_splay_tree_entry_t
#define SPLAY_TREE_DEFINE_INPLACE
#include "../splay-tree.h"


#include "splay-tree-test.h"
