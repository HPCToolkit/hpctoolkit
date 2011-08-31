// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

/*
  Copyright ((c)) 2002, Rice University 
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
#ifndef CSPROF_RBTREE_H
#define CSPROF_RBTREE_H

/* datatypes */

typedef enum {
    RED, BLACK
} csprof_rbtree_color_t;

/* not very general, but hey */
typedef struct csprof_rbtree_node
{
    void *key;
    unsigned long count;
    struct csprof_rbtree_node *left, *right, *parent;
    csprof_rbtree_color_t color;
} csprof_rbtree_node_t;

typedef struct csprof_rbtree_root
{
    csprof_rbtree_node_t *root;
    unsigned int size;
} csprof_rbtree_root_t;

/* receives a pointer to a node, and some user-supplied data */
typedef void (*csprof_rbtree_foreach_func) (csprof_rbtree_node_t *, void *);

/* function prototypes */

csprof_rbtree_root_t *csprof_rbtree_create_tree();

/* FIXME: do we want to make these take instptr_t? */
csprof_rbtree_node_t *csprof_rbtree_search(csprof_rbtree_root_t *, void *);
csprof_rbtree_node_t *csprof_rbtree_insert(csprof_rbtree_root_t *, void *);

/* tree traversal */
void csprof_rbtree_foreach(csprof_rbtree_root_t *, csprof_rbtree_foreach_func, void *);

unsigned int csprof_rbtree_size(csprof_rbtree_root_t *);

#endif /* CSPROF_RBTREE_H */
