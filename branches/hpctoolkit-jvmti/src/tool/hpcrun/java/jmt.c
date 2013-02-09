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

#include <jni.h>
#include <jvmti.h>

#include <messages/messages.h>

#include "splay-general.h"
#include <memory/hpcrun-malloc.h>

#include "jmt.h"

#define RIGHT(n)   (n)->left
#define LEFT(n)    (n)->right

// data structure for <java method, code address> pair
struct java_method_db_node_s {
  jmethodID method;
  struct java_method_db_node_s *left;
  struct java_method_db_node_s *right;
};

// data structure for <java method, code address> pair
struct method_addr_pair_node_s {
  jmethodID method;
  void *addr_start;
  struct method_addr_pair_node_s *left;
  struct method_addr_pair_node_s *right;
};

// root for java-address pair
static struct method_addr_pair_node_s *method_addr_root = NULL;

static int num_method_db = 0;
static struct java_method_db_node_s *method_db_root = NULL;

static struct method_addr_pair_node_s*
jmt_get_method_addr_pair_node(struct method_addr_pair_node_s *root, jmethodID method)
{
    REGULAR_SPLAY_TREE(method_addr_pair_node_s, root, method, method, left, right);
    return root;
}

static struct java_method_db_node_s*
jmt_get_method_db_node(struct java_method_db_node_s *root, jmethodID method)
{
    REGULAR_SPLAY_TREE(java_method_db_node_s, root, method, method, left, right);
    return root;
}


static struct method_addr_pair_node_s*
jmt_insert_method(jmethodID method, struct method_addr_pair_node_s *node)
{
   struct method_addr_pair_node_s *root = jmt_get_method_addr_pair_node( node, method );
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
    method_addr_root = root;
    return node;
}

static void
jmt_print_method_db(jvmtiEnv * jvmti, struct java_method_db_node_s * node )
{
  if (node == NULL)
    return;

  // ---------------------------------------------------------------
  // go to the left child
  // ---------------------------------------------------------------
  jmt_print_method_db(jvmti, LEFT(node));

  // ---------------------------------------------------------------
  // get the name of the method
  // ---------------------------------------------------------------
  char * method_name = NULL;
  char * method_signature = NULL;
  jvmtiError err;

  err = (*jvmti)->GetMethodName(jvmti, node->method, &method_name,
                                &method_signature, NULL);
  if (err == JVMTI_ERROR_NONE)
  {
    TMSG(JAVA, "jmt-method %p: %s  %s", node->method, method_name, method_signature);
  }
  else
  {
    TMSG(JAVA, "jmt-method %p: %d", node->method, err);
  }
  // ---------------------------------------------------------------
  // clean up
  // ---------------------------------------------------------------
  (*jvmti)->Deallocate(jvmti, (unsigned char *)method_name);
  (*jvmti)->Deallocate(jvmti, (unsigned char *)method_signature);

  // ---------------------------------------------------------------
  // go to the right child
  // ---------------------------------------------------------------
  jmt_print_method_db(jvmti, RIGHT(node));
}

///////////////////////////////////////////////////////////////////////////
// interface
///////////////////////////////////////////////////////////////////////////

// jmt_get_address: get the code address from java method ID based on the database
void *
jmt_get_address(jmethodID method)
{
    struct method_addr_pair_node_s *r = jmt_get_method_addr_pair_node(method_addr_root, method);
    if (r != NULL && r->method==method)
	return r->addr_start;
    return NULL;
}

// jmt_add_java_method: storing the pair java-method-ID and its address to the database
void
jmt_add_java_method(jmethodID method, const void *address)
{
    struct method_addr_pair_node_s *n = hpcrun_malloc(sizeof(struct method_addr_pair_node_s));

    if (n != NULL) {
       n->method = method;
       n->addr_start = (void*)address;

       method_addr_root = jmt_insert_method(method, n);
    }
    TMSG(JAVA, "jmt add mt: %p addr: %p r: %p", method, address, method_addr_root);
}

// jmt_add_method_db: add a method into the database of methods
//  this function has to be called to store method ID of java callstack
int 
jmt_add_method_db(jmethodID method)
{
  method_db_root = jmt_get_method_db_node(method_db_root, method);
  if ( (method_db_root != NULL) && (method_db_root->method == method))
  {
    // the method is already in the database, nothing to do
    return 0;
  }
  // the java method doesn't exist, add it into the database
  struct java_method_db_node_s *n = hpcrun_malloc(sizeof(struct java_method_db_node_s));
  if (n != NULL) 
  {
    n->method = method;

    if (method_db_root == NULL) 
    {
      n->left = NULL;
      n->right = NULL;
    }
     else if (method < method_db_root->method) 
    {
      n->left = method_db_root->left;
      n->right = method_db_root;
      method_db_root->left = NULL;
    }
     else
    {
      n->left = method_db_root;
      n->right = method_db_root->right;
      method_db_root->right = NULL;
    }
  }
  method_db_root = n;

  return num_method_db++;
}

// jmt_get_all_methods_db: get the list of methods in the database
jmethodID* 
jmt_get_all_methods_db(jvmtiEnv * jvmti)
{
  jmt_print_method_db(jvmti, method_db_root);
  return NULL; 
}

