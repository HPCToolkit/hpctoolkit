// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Implement an API for binary trees that supports
//   (1) building a balanced binary tree from an arbitrary imbalanced tree
//       (e.g., a list), and
//   (2) searching a binary tree for a matching node
//
//   The binarytree_node_t data type is meant to be used as a prefix to
//   other structures so that this code can be used to manipulate arbitrary
//   tree structures.  Macros support the (unsafe) up and down casting needed
//   to use the API on derived structures.
//
//******************************************************************************

//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "binarytree.h"


//******************************************************************************
// macros
//******************************************************************************

#define MAX_SUBTREE_STR 32768
#define MAX_LEFT_LEAD_STR 256

//******************************************************************************
// implementation type
//******************************************************************************

#if OPAQUE_TYPE

// opaque type not supported by gcc 4.4.*
typedef struct binarytree_s {
  struct binarytree_s *left;
  struct binarytree_s *right;
  void* val;
} binarytree_t;

#endif


//******************************************************************************
// private operations
//******************************************************************************

// hide the length of a fixed length string from the caller to prevent compiler
// whining
static char *
vlen_str(char *fixed_length_string)
{
  return fixed_length_string;
}

static void
subtree_tostr2(binarytree_t *subtree, val_tostr tostr, char valstr[],
               char* left_lead, char result[])
{
  if (subtree) {
    char Left_subtree_buff[MAX_SUBTREE_STR + 1];
    char Right_subtree_buff[MAX_SUBTREE_STR + 1];
    char Left_lead_buff[MAX_LEFT_LEAD_STR + 1];

    size_t new_left_lead_size = strlen(left_lead) + strlen("|  ") + 1;
    snprintf(Left_lead_buff, new_left_lead_size, "%s%s", left_lead, "|  ");
    binarytree_t * left = {subtree->left};
    subtree_tostr2(left, tostr, valstr, Left_lead_buff, Left_subtree_buff);
    snprintf(Left_lead_buff, new_left_lead_size, "%s%s", left_lead, "   ");
    binarytree_t * right = {subtree->right};
    subtree_tostr2(right, tostr, valstr, Left_lead_buff, Right_subtree_buff);
    tostr(subtree->val, valstr);
    snprintf(result, MAX_TREE_STR, "%s%s%s%s%s%s%s%s%s%s%s%s",
             "|_ ", valstr, "\n",
             left_lead, "|\n",
             left_lead, vlen_str(Left_subtree_buff), "\n",
             left_lead, "|\n",
             left_lead, vlen_str(Right_subtree_buff));
  }
  else {
    strcpy(result, "|_ {}");
  }
}

//******************************************************************************
// interface operations
//******************************************************************************


binarytree_t *
binarytree_new(size_t size, mem_alloc m_alloc)
{
  binarytree_t *node = (binarytree_t *)m_alloc(sizeof(binarytree_t) + size);
  node->right = NULL;
  node->left  = NULL;
  return node;
}

// destructor
void binarytree_del(binarytree_t **root, mem_free m_free)
{
  if (*root) {
    binarytree_del(&((*root)->left), m_free);
    binarytree_del(&((*root)->right), m_free);
    m_free(*root);
    *root = NULL;
  }
}

// return the value at the root
// pre-condition: tree != NULL
void*
binarytree_rootval(binarytree_t *tree)
{
  if (tree == NULL)
    abort();
  return tree->val;
}

// pre-condition: tree != NULL
binarytree_t*
binarytree_leftsubtree(binarytree_t *tree)
{
  if (tree == NULL)
    abort();
  return tree->left;
}

// pre-condition: tree != NULL
binarytree_t*
binarytree_rightsubtree(binarytree_t *tree)
{
  if (tree == NULL)
    abort();
  return tree->right;
}

void
binarytree_set_leftsubtree(
  binarytree_t *tree,
  binarytree_t* subtree)
{
  if (tree == NULL)
    abort();
  tree->left = subtree;
}

void
binarytree_set_rightsubtree(
  binarytree_t *tree,
  binarytree_t* subtree)
{
  if (tree == NULL)
    abort();
  tree->right = subtree;
}

int
binarytree_count(binarytree_t *tree)
{
  return tree?
         binarytree_count(tree->left) + binarytree_count(tree->right) + 1: 0;
}

binarytree_t *
binarytree_list_to_tree(binarytree_t ** head, int count)
{
  if (count == 0)
    return NULL;
  int mid = count >> 1;
  binarytree_t *left = binarytree_list_to_tree(head, mid);
  binarytree_t *root = *head;
  root->left = left;
  *head = (*head)->right;
  root->right = binarytree_list_to_tree(head, count - mid - 1);
  return root;
}

void
binarytree_listify_helper(binarytree_t *root, binarytree_t **tail)
{
  if (root != NULL) {
    binarytree_listify_helper(root->left, tail);
    root->left = NULL;
    *tail = root;
    tail = &root->right;
    binarytree_listify_helper(root->right, tail);
  }
}

binarytree_t *
binarytree_listify(binarytree_t *root)
{
  binarytree_t *head;
  binarytree_listify_helper(root, &head);
  return head;
}

binarytree_t *
binarytree_listalloc(size_t elt_size, int num_elts, mem_alloc m_alloc)
{
  binarytree_t *head;
  binarytree_t **tail = &head;
  while (num_elts--) {
    *tail = binarytree_new(elt_size, m_alloc);
    tail = &(*tail)->right;
  }
  return head;
}

binarytree_t *
binarytree_find(binarytree_t * root, val_cmp matches, void *val)
{
  while (root) {
    int cmp_status = matches(root->val, val);

    if (cmp_status == 0) return root; // subtree_root matches val

    // determine which subtree to search
    root = (cmp_status > 0) ? root->left : root->right;
  }
  return NULL;
}

void
binarytree_tostring(binarytree_t * tree, val_tostr tostr, char valstr[],
  char result[])
{
  binarytree_tostring_indent(tree, tostr, valstr, "", result);
}

void
binarytree_tostring_indent(binarytree_t * root, val_tostr tostr,
  char valstr[], char* indents, char result[])
{
  if (root) {
    char Left_subtree_buff[MAX_SUBTREE_STR + 1];
    char Right_subtree_buff[MAX_SUBTREE_STR + 1];
    char newindents[MAX_INDENTS+5];
    snprintf(newindents, MAX_INDENTS+4, "%s%s", indents, "|  ");
    subtree_tostr2(root->left, tostr, valstr, newindents, result);
    snprintf(Left_subtree_buff, MAX_SUBTREE_STR, "%s", result);
    snprintf(newindents, MAX_INDENTS+4, "%s%s", indents, "   ");
    subtree_tostr2(root->right, tostr, valstr, newindents, result);
    snprintf(Right_subtree_buff, MAX_SUBTREE_STR, "%s", result);
    tostr(root->val, valstr);
    snprintf(result, MAX_TREE_STR, "%s%s%s%s%s%s%s%s%s%s%s",
             valstr, "\n",
             indents,"|\n",
             indents, vlen_str(Left_subtree_buff), "\n",
             indents, "|\n",
             indents, vlen_str(Right_subtree_buff));
  }
  else {
    strcpy(result, "{}");
  }
}

int
binarytree_height(binarytree_t *root)
{
  if (root) {
    int left_height = binarytree_height(root->left);
    int right_height = binarytree_height(root->right);
    return (left_height > right_height)? left_height + 1: right_height + 1;
  }
  return 0;
}

binarytree_t *
binarytree_insert(binarytree_t *root, val_cmp compare, binarytree_t *key)
{
  if(!root) {
    return key;  // empty tree case
  }
  else if (compare(root->val, key->val) > 0) {
    root->left = binarytree_insert(root->left, compare, key);
  }
  else if (compare(root->val, key->val) < 0) {
    root->right = binarytree_insert(root->right, compare, key);
  }
  return root;
}
