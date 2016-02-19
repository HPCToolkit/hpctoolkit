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
// Copyright ((c)) 2002-2016, Rice University
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

#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <messages/messages.h>

extern void *__real_calloc(size_t nmemb, size_t size);
extern void *__real_malloc(size_t size);
extern void *__real_realloc(void *ptr, size_t size);


#if 0
extern int __real_sigprocmask(int, const sigset_t *, sigset_t *);
extern int __real_sigaction(int sig, const struct sigaction *act,
			  struct sigaction *oact);
extern sig_t __real_signal(int sig, sig_t func);

int __wrap_sigprocmask(int mask, const sigset_t *set1, sigset_t *set2){
  return __real_sigprocmask(mask,set1,set2);
}

int __wrap_sigaction(int sig, const struct sigaction *act,struct sigaction *oact){
  return __real_sigaction(sig,act,oact);
}

sig_t __wrap_signal(int sig, sig_t func){
  return __real_signal(sig,func);
}
#endif

void *__wrap_calloc(size_t nmemb, size_t size){
  return __real_calloc(nmemb,size);
}
void *__wrap_realloc(void *ptr, size_t size){
  return __real_realloc(ptr,size);
}

/* override various functions here */

int _hpcrun_in_malloc = 0;
int hpcrun_need_more = 0;

void *__wrap_malloc(size_t s){
  void *alloc;

  _hpcrun_in_malloc = 1;
  alloc = __real_malloc(s);
  if (hpcrun_need_more){
    assert(0);
    /* alloc more space here */
    hpcrun_need_more = 0;
  }
  _hpcrun_in_malloc = 0;
  return alloc;
}

void *__wrap_pthread_create(void *dc){
  EMSG("Called pthread_create!!");
  assert(0);
}
