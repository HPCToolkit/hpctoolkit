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
#include <sys/time.h>


/******************************************************************************
 * local include files
 *****************************************************************************/

#include <ibs_init.h>
#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>


/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef void *calloc_fcn(size_t, size_t);
typedef void  free_fcn(void *);
typedef void *malloc_fcn(size_t);
typedef void *realloc_fcn(void *, size_t);



/******************************************************************************
 * macros
 *****************************************************************************/

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

static int datacentric_enabled = 0; // default is off
static int datacentric_uninit = 1;  // default is uninitialized
pthread_mutex_t mutex_splay = PTHREAD_MUTEX_INITIALIZER;//lock for splay tree
//static struct splay_interval_s *root;

int insert_splay_tree(interval_tree_node*, void*, size_t, int32_t);
interval_tree_node* splaytree_lookup(void*);
/******************************************************************************
 * interface operations
 *****************************************************************************/

static void
datacentric_initialize(void)
{
  datacentric_uninit = 0;
  datacentric_enabled = 1; // unconditionally enable leak detection for now
}

void *
MONITOR_EXT_WRAP_NAME(malloc)(size_t bytes)
{
  void *h = real_malloc(bytes);
//  if(hpcrun_sync_is_blocked())
//  {
//    return h;
//  }
  if (hpcrun_datacentric_active()) {
    ucontext_t uc;
    getcontext(&uc);

    // attribute malloc to call path 
    hpcrun_async_block();
    cct_node_t* cct_node = 
      hpcrun_sample_callpath(&uc, hpcrun_dc_malloc_id(), 0, 0, 1);

    if (! cct_node ) {
      EMSG("cct node in malloc (datacentric-overrides.c) is NULL -- skipping the sample");
      return h;
    }

    TMSG(IBS_SAMPLE, "malloc %d bytes (ptr %p)", bytes, h); 
    interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
    if(node == NULL)
    {
      EMSG("unbelievable, node is NULL");
      exit(0);
    }
    pthread_mutex_lock(&mutex_splay);
    if(insert_splay_tree(node, h, bytes, cct_node->persistent_id)<0)
      EMSG("insert_splay_tree error");
    /*unblock async sampling*/
    pthread_mutex_unlock(&mutex_splay);
    hpcrun_async_unblock();
  } else {
    TMSG(IBS_SAMPLE, "malloc %d bytes (call path not logged)", bytes); 
  }
  return h;
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
  void *h = real_calloc(1, nmemb*bytes);
  if(hpcrun_sync_is_blocked())
    return h;

  if (hpcrun_datacentric_active()) {
    ucontext_t uc;
    getcontext(&uc);

    // attribute calloc to call path 
    hpcrun_async_block();
    cct_node_t* cct_node = 
      hpcrun_sample_callpath(&uc, hpcrun_dc_calloc_id(), 
			     0, 0, 1);
    TMSG(IBS_SAMPLE, "calloc %d bytes (ptr %p)", nmemb*bytes, h); 
    interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
    if(node == NULL)
    {
      TMSG(IBS_SAMPLE, "unbelievable, node is NULL");
      exit(0);
    }
    pthread_mutex_lock(&mutex_splay);
    if(insert_splay_tree(node, h, nmemb*bytes, cct_node->persistent_id)<0)
      TMSG(IBS_SAMPLE, "insert_splay_tree error");
    /*unblock async sampling*/
    pthread_mutex_unlock(&mutex_splay);
    hpcrun_async_unblock();
  } else {
    TMSG(IBS_SAMPLE, "calloc %d bytes (call path not logged)", nmemb*bytes); 
  }
  return h;
}


void *
MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t bytes)
{
  void *h = real_realloc(ptr, bytes);
  if(hpcrun_sync_is_blocked())
    return h;

  if (hpcrun_datacentric_active()) {
    ucontext_t uc;
    getcontext(&uc);

    // attribute realloc to call path 
    hpcrun_async_block();
    cct_node_t* cct_node = 
      hpcrun_sample_callpath(&uc, hpcrun_dc_realloc_id(), 0, 0, 1);
    TMSG(IBS_SAMPLE, "realloc %d bytes (ptr %p)", bytes, h); 
    interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
    if(node == NULL)
    {
      TMSG(IBS_SAMPLE, "unbelievable, node is NULL");
      exit(0);
    }
    pthread_mutex_lock(&mutex_splay);
    if(insert_splay_tree(node, h, bytes, cct_node->persistent_id)<0)
      TMSG(IBS_SAMPLE, "insert_splay_tree error");
    /*unblock async sampling*/
    pthread_mutex_unlock(&mutex_splay);
    hpcrun_async_unblock();
  } else {
    TMSG(IBS_SAMPLE, "realloc %d bytes (call path not logged)", bytes); 
  }
  return h;
}

/*int insert_splay_tree(interval_tree_node* node,  void* start, size_t size, int32_t id)
{ 
  interval_tree_node* dummy;
  
  memset(node, 0, sizeof(interval_tree_node));
  node->start = start;
  node->end = (void *)((unsigned long)start+size);
  node->cct_id = id;
*/
  /*first delete and then insert*/
/*  interval_tree_delete(&root, &dummy, node->start, node->end);
  if(interval_tree_insert(&root, node)!=0)
    return -1;//insert fail
  TMSG(IBS_SAMPLE, "splay insert: start(%p), end(%p)", node->start, node->end);
  return 0;
}

interval_tree_node* splaytree_lookup(void* p)
{
  interval_tree_node* result_node = hpcrun_malloc(sizeof(interval_tree_node));
  result_node = interval_tree_lookup(&root, p);
  return result_node;
}
*/
