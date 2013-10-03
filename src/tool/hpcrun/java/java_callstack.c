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

// standard libraries
#include <stdio.h>
#include <string.h>

// java headers
#include <jvmti.h>

// hpcrun headers
#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>
#include <splay.h>
#include <lib/prof-lean/spinlock.h>
#include <thread_data.h>


// hpcrun java headers
#include <java/stacktraces.h>
#include <java/splay-general.h>


//***************************************************************************
// macros
//***************************************************************************

#define JAVA_CS_LOCK  do {  \
  TD_GET(splay_lock) = 0;        \
  spinlock_lock(&java_tree_lock);  \
  TD_GET(splay_lock) = 1;        \
} while (0)

#define JAVA_CS_UNLOCK  do {       \
  spinlock_unlock(&java_tree_lock);  \
  TD_GET(splay_lock) = 0;          \
} while (0)


//***************************************************************************
// data structure
//***************************************************************************

struct splay_methodID_s {
  jmethodID method;
  interval_tree_node *interval;

  struct splay_methodID_s *left;
  struct splay_methodID_s *right;
};

typedef struct splay_methodID_s splay_methodID_t;


//***************************************************************************
// global variables
//***************************************************************************

static spinlock_t java_tree_lock;

static ASGCTType asgct;
static splay_methodID_t *root;


//***************************************************************************
// Helper functions
//***************************************************************************

/***
 * loading AsyncGetCallTrace from libjvm.so
 * if the loading fails, there's no use to get java's callpath from jvm
 ***/
static void
js_set_asgct()
{
   void *handle = dlopen("libjvm.so", RTLD_LAZY);

   if (handle != NULL) {
      asgct = (ASGCTType) dlsym(handle, "AsyncGetCallTrace");
   } 
   if (asgct == NULL) {
      TMSG(JAVA, "[Error] Unable to load AsyncGetCallTrace from libjvm.so");
   } 
}

static splay_methodID_t*
js_get_method_node(jmethodID method)
{
  REGULAR_SPLAY_TREE(splay_methodID_s, root, method, method, left, right);
  return root;
}

static splay_methodID_t *
js_insert_method(jmethodID method, splay_methodID_t *newnode)
{
  splay_methodID_t * node = js_get_method_node(method);
  if (node == NULL) {
      newnode->left  = NULL;
      newnode->right = NULL;
  }
  else if (newnode->method < node->method) {
      newnode->left = node->left;
      newnode->right = node;
      node->left = NULL;
  }
  else {
      newnode->left = node;
      newnode->right = node->right;
      node->right = NULL;
  }
  return newnode;
}

//***************************************************************************
// interface APIs
//***************************************************************************

/**
 * initializing variables and methods
 */
void 
js_init()
{
  js_set_asgct();
}


void
js_add_method(jmethodID method, interval_tree_node *interval_node)
{
#if 0
  JAVA_CS_LOCK;
  
  //check if the method is already in the tree
  splay_methodID_t *node = js_get_method_node(method);
  if (node != NULL) {
    if (node->method == method)
       // method already exists in the tree
       return;
  }

  node = hpcrun_malloc(sizeof(splay_methodID_t));
   
  if (node != NULL) {
    node->method = method;
    node->interval = interval_node;
    
    root = js_insert_method(method, node);
    TMSG(JAVA, "js  add mt: %p addr: %p r: %p", method, node->interval->start, root);
  }

  JAVA_CS_UNLOCK;
#endif
}

