/* $Id$ */
/*
  Copyright ((c)) 2002-2007, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
/*
  File: csprof_rbtree.c

  Purpose: red-black trees in the style of CLR.  more or less written
    only for collecting information on the function return counts, but
    could be changed easily to accomodate other datatypes.  somewhat
    low-level (the caller of these functions must sometimes be prepared
    to delve into the node structure itself).  the current implementation
    does not contain any provision for deleting entries.

  Description:
    csprof_rbtree_create_root() - create a new red-black tree
    csprof_rbtree_search() - find the node containing a certain key
    csprof_rbtree_insert() - create a new node in the tree
    csprof_rbtree_foreach() - execute a function on every node in the tree */

#include "csprof_rbtree.h"
#include "csprof_mem.h"

/* internal function prototypes */
static void csprof_rbtree_right_rotate(csprof_rbtree_root_t *, csprof_rbtree_node_t *);
static void csprof_rbtree_left_rotate(csprof_rbtree_root_t *, csprof_rbtree_node_t *);
static csprof_rbtree_node_t *csprof_rbtree_create_node(void *, csprof_rbtree_node_t *);
static void csprof_rbtree_foreach_rec(csprof_rbtree_node_t *, csprof_rbtree_foreach_func, void *);

csprof_rbtree_root_t *
csprof_rbtree_create_tree()
{
    csprof_rbtree_root_t *tree;

    tree = csprof_malloc(sizeof(csprof_rbtree_root_t));
    tree->root = NULL;
    tree->size = 0;

    return tree;
}

/* find the node containing `key'.  return NULL on failure */
csprof_rbtree_node_t *
csprof_rbtree_search(csprof_rbtree_root_t *root, void *key)
{
    csprof_rbtree_node_t *curr = root->root;
    int val;

    while (curr != NULL) {
        int val = (key == curr->key);

        if(val) {
            return curr;
        }
        else {
            if(key < curr->key) {
                curr = curr->left;
            }
            else {
                curr = curr->right;
            }
        }
    }

    return NULL;
}

/* Rotate the right child of parent upwards */
static void
csprof_rbtree_left_rotate(csprof_rbtree_root_t *root, csprof_rbtree_node_t *parent)
{
    csprof_rbtree_node_t  *curr, *gparent;

    curr = RIGHT(parent);
    gparent = parent->parent;

    if (curr != NULL) {
        parent->right = LEFT(curr);
        if (LEFT(curr) != NULL) {
            LEFT(curr)->parent = parent;
        }

        curr->parent = parent->parent;
        if (gparent == NULL) {
            root->root = curr;
        }
        else {
            int val = (parent == LEFT(gparent));

            gparent->children[val] = curr;
        }
        LEFT(curr) = parent;
        parent->parent = curr;
    }
}

/* Rotate the left child of parent upwards */
static void
csprof_rbtree_right_rotate(csprof_rbtree_root_t *root,  csprof_rbtree_node_t *parent)
{
    csprof_rbtree_node_t  *curr, *gparent;

    curr = LEFT(parent);
    gparent = parent->parent;

    if (curr != NULL) {
        parent->left = RIGHT(curr);
        if (RIGHT(curr) != NULL) {
            RIGHT(curr)->parent = parent;
        }
        curr->parent = parent->parent;
        if (gparent == NULL) {
            root->root = curr;
        }
        else {
            int val = (parent == LEFT(gparent));

            gparent->children[val] = curr;
        }
        RIGHT(curr) = parent;
        parent->parent = curr;
    }
}

/* inserts `key' into the red-black tree `root'.  returns the node
   containing the key, which may or may not be newly allocated,
   depending on whether or not `key' existed in the tree previously */
csprof_rbtree_node_t *
csprof_rbtree_insert(csprof_rbtree_root_t *root, void *key)
{
    csprof_rbtree_node_t *curr = root->root;
    csprof_rbtree_node_t *parent, *gparent, *prev = NULL, *x;
    int val;

    /* handle the empty tree case separately */
    if (root->root == NULL) {
        root->root = csprof_rbtree_create_node(key, NULL);
        root->size = 1;
        return root->root;
    }

    /* this part could just be `csprof_rbtree_search', except that
       we need a little extra information, like the previous node
       and which direction we've gone at what point */
    while (curr != NULL) {
        val = (curr->key == key);

        if(val) {
            return curr;
        }
        else {
            prev = curr;
            val = curr->key < key;
            if(val) {
                curr = curr->left;
                val = -1;
            }
            else {
                curr = curr->right;
                val = +1;
            }
        }
    }

    /* hmmm, didn't contain `key'.  do an actual insert */
    (root->size)++;

    if(val > 0) {
        x = curr = prev->right = csprof_rbtree_create_node(key, prev);
    }
    else {
        x = curr = prev->left = csprof_rbtree_create_node(key, prev);
    }

    /* rebalance */
    while ((parent = curr->parent) != NULL
           && (gparent = parent->parent) != NULL
           && curr->parent->color == RED) {
        if (parent == gparent->left) {
            if (gparent->right != NULL && gparent->right->color == RED) {
                parent->color = BLACK;
                gparent->right->color = BLACK;
                gparent->color = RED;
                curr = gparent;
            }
            else {
                if (curr == parent->right) {
                    curr = parent;
                    csprof_rbtree_left_rotate(root, curr);
                    parent = curr->parent;
                }
                parent->color = BLACK;
                if ((gparent = parent->parent) != NULL) {
                    gparent->color = RED;
                    csprof_rbtree_right_rotate(root, gparent);
                }
            }
        }
        /* mirror case */
        else {
            if (gparent->left != NULL && gparent->left->color == RED) {
                parent->color = BLACK;
                gparent->left->color = BLACK;
                gparent->color = RED;
                curr = gparent;
            }
            else {
                if (curr == parent->left) {
                    curr = parent;
                    csprof_rbtree_right_rotate(root, curr);
                    parent = curr->parent;
                }
                parent->color = BLACK;
                if ((gparent = parent->parent) != NULL) {
                    gparent->color = RED;
                    csprof_rbtree_left_rotate(root, gparent);
                }
            }
        }
    }
    if (curr->parent == NULL) {
        root->root = curr;
    }
    root->root->color = BLACK;

    /* return our new champion */
    return x;
}

static csprof_rbtree_node_t *
csprof_rbtree_create_node(void *key, csprof_rbtree_node_t *parent)
{
    csprof_rbtree_node_t *x;

    x = csprof_malloc(sizeof(csprof_rbtree_node_t));
    x->key = key;
    x->count = 0;
    x->left = NULL;
    x->right = NULL;
    x->parent = parent;
    x->color = RED;

    return x;
}

unsigned int
csprof_rbtree_size(csprof_rbtree_root_t *root)
{
    return root->size;
}

/* tree traversal */
void
csprof_rbtree_foreach(csprof_rbtree_root_t *root, csprof_rbtree_foreach_func func, void *data)
{
    csprof_rbtree_foreach_rec(root->root, func, data);
}

/* in-order, but it doesn't really matter */
static void
csprof_rbtree_foreach_rec(csprof_rbtree_node_t *node, csprof_rbtree_foreach_func func, void *data)
{
    if(node != NULL) {
        csprof_rbtree_foreach_rec(node->left, func, data);

        func(node, data);

        csprof_rbtree_foreach_rec(node->right, func, data);
    }
}
