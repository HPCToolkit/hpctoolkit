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

// list/queue library
#include <sys/queue.h>

// java headers
#include <jvmti.h>

// hpcrun headers
#include <memory/hpcrun-malloc.h>

#define ENTRY_MALLOC(Type) (struct Type *) hpcrun_malloc(sizeof(struct Type))

//***************************************************************************
// queue management
//***************************************************************************

TAILQ_HEAD(java_callstack, entry) head = TAILQ_HEAD_INITIALIZER(head);

struct entry {
  jmethodID method;
  TAILQ_ENTRY(entry) entries;
}; 

//***************************************************************************
// global variables
//***************************************************************************

static const int debug = 1;
struct java_callstack freelist = TAILQ_HEAD_INITIALIZER(freelist);

//***************************************************************************
// Helper functions
//***************************************************************************

/*
 * debugging purpose: printing the stored callstacks
 */ 
static void
js_print_stack()
{
  struct entry *np;

  TAILQ_FOREACH(np, &head, entries)
   {
     printf("%p ", np->method);
   }
   printf("\n");
}


/*
 * a new node is allocated iff the freelist is empty. 
 */
static struct entry*
js_malloc()
{
   struct entry *node = NULL;

   if (freelist.tqh_first != NULL) {
      // grab a new node from the free list
      node = freelist.tqh_first;

      // remove the node from the free list
      TAILQ_REMOVE(&freelist, freelist.tqh_first, entries);
   } else {
      // free list is empty. create a new one from hpcrun_malloc
      node = ENTRY_MALLOC(entry);
   }
   return node;
}

//***************************************************************************
// interface APIs
//***************************************************************************

/*
 * adding a new method into the stack
 */ 
void
js_add(jmethodID method)
{
  struct entry *node = js_malloc();
  if (node != NULL) 
  {
     node->method = method;
     TAILQ_INSERT_HEAD(&head, node, entries);
  }
  if (debug)
     js_print_stack();
}

/*
 * removing the latest method from the stack
 */ 
void
js_remove(jmethodID method)
{
  struct entry *node = head.tqh_first;
  if (node->method != method) {
    // if we reach here, it means there's inconsistency. 
    // it can be either race condition, or issues with the jvm   
    //  a lock isn't necessary protect this, so the only solution is to
   //   traverse the list which node we should remove which is O(n) unfortunately 
    fprintf(stderr,"Error: got %p, while expecting %p ! \n", node->method, method);
  }

  // remove the node from the stack
  TAILQ_REMOVE(&head, head.tqh_first, entries);

  // save the node into the free list  
  TAILQ_INSERT_HEAD(&freelist, node, entries);

  if (debug)
     js_print_stack();
}




