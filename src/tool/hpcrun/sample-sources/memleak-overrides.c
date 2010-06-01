// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL:$
// $Id:$
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



/******************************************************************************
 * standard include files
 *****************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>



/******************************************************************************
 * local include files
 *****************************************************************************/

#include <sample-sources/memleak.h>
#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>



/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef struct {
  cct_node_t *context;
  size_t bytes;
} leakhdr_t;

typedef void *calloc_fcn(size_t, size_t);
typedef void  free_fcn(void *);
typedef void *malloc_fcn(size_t);
typedef void *realloc_fcn(void *, size_t);



/******************************************************************************
 * macros
 *****************************************************************************/
#define SKIP_MEM_FRAME 0

#ifdef HPCRUN_STATIC_LINK
#define real_calloc   __real_calloc
#define real_free     __real_free
#define real_malloc   __real_malloc
#define real_realloc  __real_realloc
#else
#define real_calloc   __libc_calloc
#define real_free     __libc_free
#define real_malloc   __libc_malloc
#define real_realloc  __libc_realloc
#endif

extern calloc_fcn  real_calloc;
extern free_fcn    real_free;
extern malloc_fcn  real_malloc;
extern realloc_fcn real_realloc;



/******************************************************************************
 * interface operations
 *****************************************************************************/


void *
MONITOR_EXT_WRAP_NAME(malloc)(size_t bytes)
{
  leakhdr_t *h = (leakhdr_t *)real_malloc(bytes + sizeof(leakhdr_t));
  h->bytes = bytes;

  if (hpcrun_memleak_alloc_id() >= 0) {
    ucontext_t uc;
    getcontext(&uc);

    // attribute malloc to call path 
    h->context = 
      hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), bytes, SKIP_MEM_FRAME, 1);
    TMSG(MEMLEAK, "malloc %d bytes (cct node %p)", h->bytes, h->context); 
  } else {
    h->context = 0;
    TMSG(MEMLEAK, "malloc %d bytes (call path not logged)", h->bytes); 
  }
  return h+1;
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
  leakhdr_t *h = (leakhdr_t *)real_calloc(1, nmemb * bytes + sizeof(leakhdr_t));
  h->bytes = nmemb * bytes;

  if (hpcrun_memleak_alloc_id() >= 0) {
    ucontext_t uc;
    getcontext(&uc);

    // attribute calloc to call path 
    h->context = 
      hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), 
			     nmemb * bytes, SKIP_MEM_FRAME, 1);
    TMSG(MEMLEAK, "calloc %d bytes (cct node %p)", h->bytes, h->context); 
  } else {
    h->context = 0;
    TMSG(MEMLEAK, "calloc %d bytes (call path not logged)", h->bytes); 
  }
  return h+1;
}


void *
MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t bytes)
{
  int notnull = (ptr != 0);
  leakhdr_t *h = notnull ? ((leakhdr_t *) ptr) - 1: 0;

  h = (leakhdr_t *)real_realloc(h, bytes + sizeof(leakhdr_t));

  if (hpcrun_memleak_alloc_id() >= 0) {
    ucontext_t uc;
    getcontext(&uc);

    // attribute free of old data (if any) to call path
    if (notnull) hpcrun_free_inc(h->context, h->bytes); 

    // attribute realloc to call path 
    h->context = 
      hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), bytes, SKIP_MEM_FRAME, 1);
    h->bytes = bytes;
    TMSG(MEMLEAK, "realloc %d bytes (cct node %p)", h->bytes, h->context); 
  } else {
    h->context = 0;
    h->bytes = bytes;
    TMSG(MEMLEAK, "realloc %d bytes (call path not logged)", h->bytes); 
  }
  return h+1;
}


void
MONITOR_EXT_WRAP_NAME(free)(void *ptr)
{
  if (ptr == 0) return;

  leakhdr_t *h = ((leakhdr_t *) ptr) - 1;

  if (hpcrun_memleak_alloc_id() >= 0) {
    // attribute free to call path 
    hpcrun_free_inc(h->context, h->bytes);
    TMSG(MEMLEAK, "free %d bytes (cct node %p)", h->bytes, h->context); 
  } else {
    TMSG(MEMLEAK, "free %d bytes (call path not logged)", h->bytes); 
  }
  real_free(h);
}
