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
// Copyright ((c)) 2002-2022, Rice University
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

//******************************************************************************
// file: closure-registry.c
// purpose:
//   implementation of support for registering and applying a set of closures
//******************************************************************************

#include "closure-registry.h"

#include <stdio.h>

void closure_list_register(closure_list_t* l, closure_t* entry) {
  entry->next = l->head;
  l->head = entry;
}

int closure_list_deregister(closure_list_t* l, closure_t* entry) {
  closure_t** ptr_to_curr = &l->head;
  closure_t* curr = l->head;
  while (curr != 0) {
    if (curr == entry) {
      *ptr_to_curr = curr->next;
      return 0;
    }
    ptr_to_curr = &curr->next;
    curr = curr->next;
  }
  return -1;
}

void closure_list_apply(closure_list_t* l) {
  closure_t* entry = l->head;
  while (entry != 0) {
    entry->fn(entry->arg);
    entry = entry->next;
  }
}

void closure_list_dump(closure_list_t* l) {
  printf("closure_list_t * = %p (head = %p)\n", l, l->head);
  closure_t* entry = l->head;
  while (entry != 0) {
    printf("  closure_t* = %p {fn = %p, next = %p}\n", entry, entry->fn, entry->next);
    entry = entry->next;
  }
}

#undef UNIT_TEST_closure_registry
#ifdef UNIT_TEST_closure_registry

void c1_fn(void* arg) {
  printf("in closure 1\n");
}

void c2_fn(void* arg) {
  printf("in closure 2\n");
}

closure_t c1 = {.fn = c1_fn, .next = NULL, .arg = NULL};
closure_t c2 = {.fn = c2_fn, .next = NULL, .arg = NULL};
closure_list_t cl;

int main(int argc, char** argv) {
  closure_list_register(&cl, &c1);
  closure_list_dump(&cl);
  closure_list_apply(&cl);

  closure_list_register(&cl, &c2);
  closure_list_dump(&cl);
  closure_list_apply(&cl);

  closure_list_deregister(&cl, &c2);
  closure_list_dump(&cl);
  closure_list_apply(&cl);

  closure_list_deregister(&cl, &c1);
  closure_list_dump(&cl);
  closure_list_apply(&cl);
}

#endif
