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


static int leak_detection_enabled = 0; // default is off
static int leak_detection_uninit = 1;  // default is uninitialized

#define hpcrun_async_block()
#define hpcrun_async_unblock()

/******************************************************************************
 * private operations
 *****************************************************************************/

static void 
memleak_initialize(void)
{
  leak_detection_uninit = 0;
  leak_detection_enabled = 1; // unconditionally enable leak detection for now
}



/******************************************************************************
 * interface operations
 *****************************************************************************/


void *
MONITOR_EXT_WRAP_NAME(malloc)(size_t bytes)
{
  if (leak_detection_uninit) {
    memleak_initialize();
  }

  if (leak_detection_enabled) {
    leakhdr_t *h = (leakhdr_t *)real_malloc(bytes + sizeof(leakhdr_t));

    if (h == NULL) return h; // handle out of memory case

    h->bytes = bytes;

    if (hpcrun_memleak_active()) {
      ucontext_t uc;
      getcontext(&uc);

      // attribute malloc to call path 
      hpcrun_async_block();
      h->context = 
	hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), bytes, SKIP_MEM_FRAME, 1);
      hpcrun_async_unblock();

      TMSG(MEMLEAK, "malloc %d bytes (cct node %p)", h->bytes, h->context); 
    } else {
      h->context = 0;
      TMSG(MEMLEAK, "malloc %d bytes (call path not logged)", h->bytes); 
    }
    return h+1;
  } else {
    return real_malloc(bytes);
  }
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
  if (leak_detection_uninit) {
    memleak_initialize();
  }

  if (leak_detection_enabled) {

    leakhdr_t *h = (leakhdr_t *)real_calloc(1, nmemb * bytes + sizeof(leakhdr_t));

    if (h == NULL) return h; // handle out of memory case

    h->bytes = nmemb * bytes;

    if (hpcrun_memleak_active()) {
      ucontext_t uc;
      getcontext(&uc);

      // attribute calloc to call path 
      hpcrun_async_block();
      h->context = 
	hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), 
			       nmemb * bytes, SKIP_MEM_FRAME, 1);
      hpcrun_async_unblock();
      TMSG(MEMLEAK, "calloc %d bytes (cct node %p)", h->bytes, h->context); 
    } else {
      h->context = 0;
      TMSG(MEMLEAK, "calloc %d bytes (call path not logged)", h->bytes); 
    }
    return h+1;
  } else {
    return real_calloc(nmemb, bytes);
  }
}


void *
MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t bytes)
{
  if (leak_detection_uninit) {
    memleak_initialize();
  }

  if (leak_detection_enabled) {
    int notnull = (ptr != 0);
    leakhdr_t *h = NULL;
    leakhdr_t old_h;

    int prev_bytes = 0;
    cct_node_t *context = NULL;

    if (notnull) {
      h = ((leakhdr_t *) ptr) - 1;
      old_h = *h;
      h->context = NULL; 
    }
      
    int hdr_size = (bytes != 0) ? sizeof(leakhdr_t) : 0;

    h = (leakhdr_t *)real_realloc(h, bytes + hdr_size);

    if (bytes && h == NULL) return h; // handle out of memory case

    if (hpcrun_memleak_active()) {
      ucontext_t uc;

      // attribute free of old data (if any) to call path
      if (notnull) hpcrun_free_inc(old_h.context, old_h.bytes); 

      if (bytes == 0) return h; // handle realloc to 0 size case

      getcontext(&uc);

      // attribute realloc to call path 
      hpcrun_async_block();
      h->context = 
	hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), bytes, SKIP_MEM_FRAME, 1);
      hpcrun_async_unblock();
      h->bytes = bytes;
      TMSG(MEMLEAK, "realloc %d bytes (cct node %p)", h->bytes, h->context); 
    } else {
      h->context = 0;
      h->bytes = bytes;
      TMSG(MEMLEAK, "realloc %d bytes (call path not logged)", h->bytes); 
    }
    return h+1;
  } else {
    return real_realloc(ptr, bytes);
  }
}


void
MONITOR_EXT_WRAP_NAME(free)(void *ptr)
{
  if (ptr == 0) return;

  if (leak_detection_uninit) {
    memleak_initialize();
  }

  if (leak_detection_enabled) {

    leakhdr_t *h = ((leakhdr_t *) ptr) - 1;

    if (hpcrun_memleak_active()) {
      // attribute free to call path 
      hpcrun_free_inc(h->context, h->bytes);
      TMSG(MEMLEAK, "free %d bytes (cct node %p)", h->bytes, h->context); 
    } else {
      TMSG(MEMLEAK, "free %d bytes (call path not logged)", h->bytes); 
    }
    real_free(h);
  } else {
    real_free(ptr);
  }
}
