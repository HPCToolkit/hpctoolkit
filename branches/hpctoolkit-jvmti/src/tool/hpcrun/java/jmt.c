// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$ 
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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


#include <messages/messages.h>

#include "splay-general.h"
#include <memory/hpcrun-malloc.h>
#include <jvmti.h>

struct java_method_node_s {
  jmethodID method;
  void *addr_start;
  struct java_method_node_s *left;
  struct java_method_node_s *right;
};


static struct java_method_node_s *root = NULL;


static struct java_method_node_s*
jmt_get_node(struct java_method_node_s *root, jmethodID method)
{
    REGULAR_SPLAY_TREE(java_method_node_s, root, method, method, left, right);
    return root;
}


static struct java_method_node_s*
jmt_insert_method(jmethodID method, void *addr_start, struct java_method_node_s *node)
{
   root = jmt_get_node( node, method );
   if (root == NULL) {
      node->left  = NULL;
      node->right = NULL;
    }
   else if (node->method < root->method) {
       node->left = root->left;
       node->right = root;
       root->left = NULL;
    }
    else {
       node->left = root;
       node->right = root->right;
       root->right = NULL;
    }
    return node;
}

///////////////////////////////////////////////////////////////////////////
// interface
///////////////////////////////////////////////////////////////////////////

void *
jmt_get_address(jmethodID method)
{
    struct java_method_node_s *r = jmt_get_node(root, method);
    if (r != NULL && r->method==method)
	return r->addr_start;
    return NULL;
}

void
jmt_add_java_method(jmethodID method, const void *address)
{
    struct java_method_node_s *n = hpcrun_malloc(sizeof(struct java_method_node_s));
    n->method = method;
    n->addr_start = address;

    if (n != NULL)
       root = jmt_insert_method(method, address, n);
    TMSG(JAVA, "jmt add mt: %p addr: %p r: %p", method, address, root);
}
