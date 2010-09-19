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
#include <ucontext.h>

/* definition for posix memalign */
#define _XOPEN_SOURCE 600
#include <stdlib.h>

/* definition for valloc, memalign */
#include <malloc.h>

/* definition for sysconf */
#include <unistd.h>

/* definition for _SC_PAGESIZE */
#include <sys/mman.h>



/******************************************************************************
 * local include files
 *****************************************************************************/

#include <sample-sources/memleak.h>
#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>



/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef struct leakfooter_s {
  long magic;
  cct_node_t *context;
  size_t bytes;
  void *memblock;
  struct leakfooter_s *left;
  struct leakfooter_s *right;
} leakfooter_t;

leakfooter_t leakfooter_NULL = { .magic = 0, .context = NULL, .bytes = 0 };

typedef int posix_memalign_fcn(void **, size_t, size_t);
typedef void *memalign_fcn(size_t, size_t);
typedef void *valloc_fcn(size_t);
typedef void *calloc_fcn(size_t, size_t);
typedef void  free_fcn(void *);
typedef void *malloc_fcn(size_t);
typedef void *realloc_fcn(void *, size_t);



/******************************************************************************
 * macros
 *****************************************************************************/
#define MEMLEAK_MAGIC 0x68706374

#define SKIP_MEM_FRAME 0
#define SKIP_MEMALIGN_HELPER_FRAME 1

#define DEBUG_WITH_CLEAR 1
#define NOSTUB 1

#ifdef HPCRUN_STATIC_LINK
#define real_posix_memalign   __real_posix_memalign
#define real_memalign   __real_memalign
#define real_valloc   __real_valloc
#define real_calloc   __real_calloc
#define real_free     __real_free
#define real_malloc   __real_malloc
#define real_realloc  __real_realloc
#else
#define real_posix_memalign   __libc_posix_memalign
#define real_memalign   __libc_memalign
#define real_valloc   __libc_valloc
#define real_calloc   __libc_calloc
#define real_free     __libc_free
#define real_malloc   __libc_malloc
#define real_realloc  __libc_realloc
#endif

extern posix_memalign_fcn real_posix_memalign;
extern memalign_fcn       real_memalign;
extern valloc_fcn         real_valloc;
extern calloc_fcn         real_calloc;
extern free_fcn           real_free;
extern malloc_fcn         real_malloc;
extern realloc_fcn        real_realloc;


static int leak_detection_enabled = 0; // default is off
static int leak_detection_uninit = 1;  // default is uninitialized



/******************************************************************************
 * private data
 *****************************************************************************/


static struct leakfooter_s *memleak_tree_root = NULL;
static spinlock_t memtree_lock = SPINLOCK_UNLOCKED;




/******************************************************************************
 * splay operations
 *****************************************************************************/


static struct leakfooter_s *
splay(struct leakfooter_s *root, void *key)
{
  REGULAR_SPLAY_TREE(leakfooter_s, root, key, memblock, left, right);
  return root;
}


static void
splay_insert(struct leakfooter_s *node)
{
  void *memblock = node->memblock;

  node->left = node->right = NULL;

  spinlock_lock(&memtree_lock);  
  if (memleak_tree_root != NULL) {
    memleak_tree_root = splay(memleak_tree_root, memblock);

    if (memblock < memleak_tree_root->memblock) {
      node->left = memleak_tree_root->left;
      node->right = memleak_tree_root;
      memleak_tree_root->left = NULL;
    } else if (memblock > memleak_tree_root->memblock) {
      node->left = memleak_tree_root;
      node->right = memleak_tree_root->right;
      memleak_tree_root->right = NULL;
    } else {
      TMSG(MEMLEAK, "memleak splay tree: unable to insert %p (already present)\n", 
	   node->memblock);
      assert(0);
    }
  }
  memleak_tree_root = node;
  spinlock_unlock(&memtree_lock);  
}


static struct leakfooter_s *
splay_delete(void *memblock)
{
  struct leakfooter_s *result = NULL;

  spinlock_lock(&memtree_lock);  
  if (memleak_tree_root == NULL) {
    spinlock_unlock(&memtree_lock);  
    TMSG(MEMLEAK, "memleak splay tree empty: unable to delete %p\n", memblock);
    return NULL;
  }

  memleak_tree_root = splay(memleak_tree_root, memblock);

  if (memblock != memleak_tree_root->memblock) {
    spinlock_unlock(&memtree_lock);  
    TMSG(MEMLEAK, "memleak splay tree: %p not in tree\n", memblock);
    return NULL;
  }

  result = memleak_tree_root;

  if (memleak_tree_root->left == NULL) {
    memleak_tree_root = memleak_tree_root->right;
    spinlock_unlock(&memtree_lock);  
    return result;
  }

  memleak_tree_root->left = splay(memleak_tree_root->left, memblock);
  memleak_tree_root->left->right = memleak_tree_root->right;
  memleak_tree_root =  memleak_tree_root->left;
  spinlock_unlock(&memtree_lock);  
  return result;
}



/******************************************************************************
 * private operations
 *****************************************************************************/


static void 
memleak_initialize(void)
{
  leak_detection_uninit = 0;
  leak_detection_enabled = 1; // unconditionally enable leak detection for now
}


static int
set_footer(char *memptr, size_t bytes, char *routine_name, int frames_to_skip)
{
  int use_callpath = hpcrun_memleak_active();

  // footer at end of memory block
  leakfooter_t *foot = (leakfooter_t *) (memptr + bytes);
      
  foot->magic   = MEMLEAK_MAGIC;
  foot->memblock     = memptr;
  foot->bytes   = bytes;
  foot->context = NULL;

  if (use_callpath) {
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_async_block();
    foot->context = hpcrun_sample_callpath(&uc, hpcrun_memleak_alloc_id(), bytes, 
					   frames_to_skip, 1);
    hpcrun_async_unblock();

    TMSG(MEMLEAK, "%s %d bytes (cct node %p) -> %p", 
	 routine_name, bytes, foot->context, memptr); 

    splay_insert(foot);
  }

  return use_callpath;
}


static int 
posix_memalign_helper(void **memptr, size_t alignment, 
	              size_t bytes, 
		      char *routine_name, uint32_t frames_to_skip)
{
  int footer_size;
  int success;

  if (leak_detection_uninit) {
    memleak_initialize();
  }

  footer_size = (leak_detection_enabled) ? sizeof(leakfooter_t) : 0;

  success = real_posix_memalign(memptr, alignment, bytes + footer_size);

  if (!(leak_detection_enabled && success == 0 && 
	set_footer(*memptr, bytes, routine_name, frames_to_skip + 1))) {

    TMSG(MEMLEAK, "%s %d bytes (call path not logged) -> %p", 
	 routine_name, bytes, *memptr); 
  }

#ifdef DEBUG_WITH_CLEAR
  memset(*memptr, 0, bytes);
#endif

  return success;
}


void * 
malloc_helper(size_t bytes, char *routine_name, uint32_t frames_to_skip)
{
  int footer_size;
  void *memptr;

  if (leak_detection_uninit) {
    memleak_initialize();
  }

  footer_size = (leak_detection_enabled) ? sizeof(leakfooter_t) : 0;
  memptr = real_malloc(bytes + footer_size);

  if (!(leak_detection_enabled && memptr && set_footer(memptr, bytes, routine_name,
						       frames_to_skip + 1))) {
    TMSG(MEMLEAK, "%s %d bytes (call path not logged) -> %p", routine_name, bytes, memptr); 
  }

  return memptr;
}


/******************************************************************************
 * interface operations
 *****************************************************************************/


void *
MONITOR_EXT_WRAP_NAME(valloc)(size_t bytes)
{
  static size_t alignment;
  int success;
  void *memptr;

  if (alignment == 0) {
    alignment = sysconf(_SC_PAGESIZE);
  }

  success = posix_memalign_helper(&memptr, alignment, bytes, "valloc", SKIP_MEM_FRAME + 1);

  if (success != 0) memptr = NULL;

  return memptr;
}


int
MONITOR_EXT_WRAP_NAME(posix_memalign)(void **memptr, size_t alignment, size_t bytes)
{
  int success = posix_memalign_helper(memptr, alignment, bytes, "posix_memalign", 
				      SKIP_MEM_FRAME + 1);
  return success;
}


void *
MONITOR_EXT_WRAP_NAME(memalign)(size_t boundary, size_t bytes)
{
  void *memptr;

  int success = 
    posix_memalign_helper(&memptr, boundary, bytes, "memalign", SKIP_MEM_FRAME + 1);

  if (success != 0) memptr = NULL;

  return memptr;
}


void *
MONITOR_EXT_WRAP_NAME(malloc)(size_t bytes)
{
#ifdef NOSTUB 
  void *memptr = malloc_helper(bytes, "malloc", SKIP_MEM_FRAME + 1);
#ifdef DEBUG_WITH_CLEAR
  memset(memptr, 0, bytes);
#endif
#else
  void *memptr = real_malloc(bytes);
#endif
  return memptr;
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
#ifdef NOSTUB
  void *memptr = malloc_helper(nmemb * bytes, "calloc", SKIP_MEM_FRAME + 1);
#else
  void *memptr = real_malloc(nmemb * bytes);
#endif
  memset(memptr, 0, nmemb * bytes);
  return memptr;
}


void
MONITOR_EXT_WRAP_NAME(free)(void *ptr)
{
#ifdef NOSTUB
  if (ptr == 0) return;

  if (leak_detection_uninit) {
    memleak_initialize();
  }

  if (leak_detection_enabled && hpcrun_memleak_active()) {
    leakfooter_t *f = splay_delete(ptr);
    // attribute free to call path 
    if (f && f->magic == MEMLEAK_MAGIC) {
      TMSG(MEMLEAK, "free(%p) %d bytes (cct node %p)", ptr, f->bytes, f->context); 
      if (f->context) hpcrun_free_inc(f->context, f->bytes);
      f->context = NULL;
      f->magic = 0;
    } else {
      // apparently no footer; let's free the original pointer instead
      // and take no destructive actions
      TMSG(MEMLEAK, "free(%p) without matching allocate!", ptr); 
    }
  } else {
    TMSG(MEMLEAK, "free(%p) (call path not logged)", ptr); 
  }
#endif
  real_free(ptr);
}


void *
MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t bytes)
{
#ifdef NOSTUB
  if (leak_detection_uninit) {
    memleak_initialize();
  }

  if (leak_detection_enabled) {
    int notnull = (ptr != 0);
    leakfooter_t *f = NULL;
    leakfooter_t old_f = leakfooter_NULL;

    if (notnull) {
      f = splay_delete(ptr);
      old_f = *f;
      f->context = NULL; 
      f->magic = 0; 

      if (old_f.context) {
        TMSG(MEMLEAK, "free(%p) %d bytes (cct node %p) [realloc]", ptr, 
	     old_f.bytes, old_f.context); 

	hpcrun_free_inc(old_f.context, old_f.bytes); 
      }
    }
      
    int footer_size = (bytes != 0) ? sizeof(leakfooter_t) : 0;

    ptr = (leakfooter_t *)real_realloc(ptr, bytes + footer_size);

    if (bytes && ptr == NULL) return ptr; // handle out of memory case

    if (bytes == 0) return ptr; // handle realloc to 0 size case

    set_footer(ptr, bytes, "realloc", SKIP_MEM_FRAME + 1);
  } else {
    return real_realloc(ptr, bytes);
  }
  return ptr;
#else
  return real_realloc(ptr, bytes);
#endif
}

