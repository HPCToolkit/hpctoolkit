// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/datacentric-overrides.c $
// $Id: datacentric-overrides.c 3575 2011-08-02 23:52:25Z xl10 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
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
#include <errno.h>
#include <stdint.h>
#include <string.h>
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

#include <sample-sources/datacentric.h>
#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <utilities/arch/inline-asm-gctxt.h>
#include <utilities/arch/mcontext.h>


/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef struct datainfo_s {
  long magic;
  void *start;
  void *end;
  struct datainfo_s *left;
  struct datainfo_s *right;
}datainfo_t;

datainfo_t datainfo_NULL = {.magic = 0};

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

#define DATACENTRIC_USE_HYBRID_LAYOUT 1

#define DATACENTRIC_MAGIC 0x68706374
#define DATACENTRIC_DEFAULT_PAGESIZE  4096

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



/******************************************************************************
 * private data
 *****************************************************************************/

static int datacentric_enabled = 0; // default is off
static int datacentric_init = 0;    // default is uninitialized

static struct datainfo_s *datacentric_tree_root = NULL;
static spinlock_t datacentric_memtree_lock = SPINLOCK_UNLOCKED;

static int datainfo_size = sizeof(struct datainfo_s);
static long datacentric_pagesize = DATACENTRIC_DEFAULT_PAGESIZE;

enum {
  DATACENTRIC_LOC_HEAD = 1,
  DATACENTRIC_LOC_FOOT,
  DATACENTRIC_LOC_NONE
};

static char *loc_name[4] = {
  NULL, "header", "footer", "none"
};



/******************************************************************************
 * splay operations
 *****************************************************************************/

static struct datainfo_s *
splay(struct datainfo_s *root, void *key)
{
  INTERVAL_SPLAY_TREE(datainfo_s, root, key, start, end, left, right);
  return root;
}

static void
splay_insert(struct datainfo_s *node)
{
  struct datainfo_s *t;
 
  if(node->start >= node->end) {
    TMSG(MEMLEAK, "In splay insert: start is greater or equal than end");
    return;
  }

  spinlock_lock(&datacentric_memtree_lock);
  if(datacentric_tree_root == NULL) {
    node->left = NULL;
    node->right = NULL;
    datacentric_tree_root = node;
    spinlock_unlock(&datacentric_memtree_lock);
    return;
  }

  node->left = node->right = NULL;
  
  if(datacentric_tree_root != NULL) {
    datacentric_tree_root = splay(datacentric_tree_root, node->start);
    /* insert left of the root */
    if(node->end <= datacentric_tree_root->start) {
      node->left = datacentric_tree_root->left;
      node->right = datacentric_tree_root;
      datacentric_tree_root->left = NULL;
      datacentric_tree_root = node;
      spinlock_unlock(&datacentric_memtree_lock);
      return;
    }
    /* insert right of the root */
    t = splay(datacentric_tree_root->right, datacentric_tree_root->start);
    if((datacentric_tree_root->end <= node->start) &&
       (t == NULL || node->end <= t->start)) {
      node->left = datacentric_tree_root;
      node->right = t;
      datacentric_tree_root->right = NULL;
      datacentric_tree_root = node;
      spinlock_unlock(&datacentric_memtree_lock);
      return;
    }

    datacentric_tree_root->right = t;
    spinlock_unlock(&datacentric_memtree_lock);
    return;
  }
}

static struct datainfo_s *
splay_delete(void *start, void *end)
{
  struct datainfo_s *ltree, *rtree, *t, *del;

  spinlock_lock(&datacentric_memtree_lock);
  /* empty tree */
  if(datacentric_tree_root == NULL) {
    spinlock_unlock(&datacentric_memtree_lock);
    TMSG(MEMLEAK, "datacentric splay tree empty: unable to delete %p", start);
    return NULL;
  }

  t = splay(datacentric_tree_root, start);
  if(t->end <= start) {
    ltree = t;
    t = t->right;
    ltree->right = NULL;
  } else {
    ltree = t->left;
    t->left = NULL;
  }

  t = splay(t, end);
  if(t == NULL) {
    del = NULL;
    rtree = NULL;
  } else if(end <= t->start) {
    del = t->left;
    rtree = t;
    t->left = NULL;
  } else {
    del = t;
    rtree = t->right;
    t->right = NULL;
  }

  /* combine the left and right pieces to make the new tree */
  if(ltree == NULL) {
    datacentric_tree_root = rtree;
  } else if(rtree == NULL) {
    datacentric_tree_root = ltree;
  } else {
    datacentric_tree_root = splay(ltree, end);
    datacentric_tree_root->right = rtree;
  }
  
  spinlock_unlock(&datacentric_memtree_lock);

  return del;
}

struct datainfo_s *
splay_lookup(void *p)
{
  datacentric_tree_root = splay(datacentric_tree_root, p);
  if(datacentric_tree_root != NULL &&
     datacentric_tree_root->start <= p &&
     p < datacentric_tree_root->end)
    return datacentric_tree_root;
  return NULL;
}

/******************************************************************************
 * private operations
 *****************************************************************************/


static void
datacentric_initialize(void)
{
  if (datacentric_init)
    return;

#ifdef _SC_PAGESIZE
  datacentric_pagesize = sysconf(_SC_PAGESIZE);
#else
  datacentric_pagesize = DATACENTRIC_DEFAULT_PAGESIZE;
#endif

  // unconditionally enable datacentric analysis for now
  datacentric_enabled = 1;
  datacentric_init = 1;

  TMSG(MEMLEAK, "init");
}


// Returns: 1 if p1 and p2 are on the same physical page.
//
static inline int
datacentric_same_page(void *p1, void *p2)
{
  uintptr_t n1 = (uintptr_t) p1;
  uintptr_t n2 = (uintptr_t) p2;

  return (n1 / datacentric_pagesize) == (n2 / datacentric_pagesize);
}


// Choose the location of the datainfo struct at malloc().  Use a
// header if the system and application pointers are on the same page
// (so free can look for a header without risking a segfault).  Note:
// aligned mallocs never get headers.
//
// sys_ptr = pointer returned by sysem malloc
// bytes = size of application's region
// align = non-zero if an aligned malloc
//
// Returns: enum constant for header/footer
// appl_ptr = value given to the application
// info_ptr = location of the datainfo struct
//
static int
datacentric_get_malloc_loc(void *sys_ptr, size_t bytes, size_t align,
		           void **appl_ptr, datainfo_t **info_ptr)
{
#if DATACENTRIC_USE_HYBRID_LAYOUT
  if (datacentric_same_page(sys_ptr, sys_ptr + datainfo_size) && align == 0) {
    *appl_ptr = sys_ptr + datainfo_size;
    *info_ptr = sys_ptr;
    return DATACENTRIC_LOC_HEAD;
  }
#endif

  // footer
  *appl_ptr = sys_ptr;
  *info_ptr = sys_ptr + bytes;
  return DATACENTRIC_LOC_FOOT;
}


// Find the location of the datainfo struct at free().
//
// Returns: enum constant for header/footer/none,
// and system and datainfo pointers.
//
static int
datacentric_get_free_loc(void *appl_ptr, void **sys_ptr, datainfo_t **info_ptr)
{
  static int num_errors = 0;

#if DATACENTRIC_USE_HYBRID_LAYOUT
  // try header first
  *info_ptr = (datainfo_t *) (appl_ptr - datainfo_size);
  if (datacentric_same_page(*info_ptr, appl_ptr)
      && (*info_ptr)->magic == DATACENTRIC_MAGIC
      && (*info_ptr)->start == appl_ptr) {
    *sys_ptr = *info_ptr;
    return DATACENTRIC_LOC_HEAD;
  }
#endif

  // always try footer
  *sys_ptr = appl_ptr;
  /* we should give an interval for deleting. We give [appl_ptr, appl_ptr+1)*/
  *info_ptr = splay_delete(appl_ptr, appl_ptr+1);
  if (*info_ptr == NULL) {
    return DATACENTRIC_LOC_NONE;
  }
  if ((*info_ptr)->magic == DATACENTRIC_MAGIC
      && (*info_ptr)->start == appl_ptr) {
    return DATACENTRIC_LOC_FOOT;
  }

  // must be memory corruption somewhere.  maybe a bug in our code,
  // but more likely someone else has stomped on our data.
  num_errors++;
  if (num_errors < 100) {
    AMSG("MEMLEAK: Warning: memory corruption in datainfo node: %p "
	 "sys: %p appl: %p magic: 0x%lx start: %p end: %p",
	 *info_ptr, *sys_ptr, appl_ptr, (*info_ptr)->magic,
	 (*info_ptr)->start, (*info_ptr)->end);
  }
  *info_ptr = NULL;
  return DATACENTRIC_LOC_NONE;
}


// Fill in the datainfo struct, add to splay tree
// (if footer) and print TMSG.
//

static void
datacentric_add_datainfo(const char *name, void *sys_ptr, void *appl_ptr,
		         datainfo_t *info_ptr, size_t bytes, ucontext_t *uc,
		         int loc)
{
  char *loc_str;

  if (info_ptr == NULL) {
    TMSG(MEMLEAK, "Warning: %s: bytes: %ld sys: %p appl: %p info: %p "
	 "(NULL datainfo pointer, this should not happen)",
	 name, bytes, sys_ptr, appl_ptr, info_ptr);
    return;
  }

  info_ptr->magic = DATACENTRIC_MAGIC;
  info_ptr->start = appl_ptr;
  info_ptr->end = appl_ptr + bytes;
  info_ptr->left = NULL;
  info_ptr->right = NULL;

  if (hpcrun_datacentric_active()) {
    hpcrun_async_block();
    hpcrun_sample_callpath(uc, hpcrun_datacentric_alloc_id(), bytes, 0, 1);
    hpcrun_async_unblock();
    loc_str = loc_name[loc];
  } else {
    loc_str = "inactive";
  }
  if (loc == DATACENTRIC_LOC_FOOT) {
    splay_insert(info_ptr);
  }

  TMSG(MEMLEAK, "%s: bytes: %ld sys: %p appl: %p info: %p(%s)",
       name, bytes, sys_ptr, appl_ptr, info_ptr, loc_str);
}


// Unified function for all of the mallocs, aligned and unaligned.
// Do the malloc, add the datainfo struct and print TMSG.
//
// bytes = size of application region
// align = size of alignment, or 0 for unaligned
// clear = 1 if want region memset to 0 (for calloc)
// ret = return value from posix_memalign()
//
static void *
datacentric_malloc_helper(const char *name, size_t bytes, size_t align,
		          int clear, ucontext_t *uc, int *ret)
{
  void *sys_ptr, *appl_ptr;
  datainfo_t *info_ptr;
  int active, loc;
  size_t size;

  TMSG(MEMLEAK, "%s: bytes: %ld", name, bytes);

  // do the real malloc, aligned or not
  active = datacentric_enabled && hpcrun_datacentric_active();
  size = bytes + (active ? datainfo_size : 0);
  if (align != 0) {

    // there is no __libc_posix_memalign(), so we use __libc_memalign()
    // instead, or else use dlsym().
#if 0
    errno = real_posix_memalign(&sys_ptr, align, size);
    if (errno != 0) {
      sys_ptr = NULL;
    }
    if (ret != NULL) {
      *ret = errno;
    }
#else
    sys_ptr = real_memalign(align, size);
    if (ret != NULL) {
      *ret = (sys_ptr == NULL) ? errno : 0;
    }
#endif

  } else {
    sys_ptr = real_malloc(size);
  }
  if (clear && sys_ptr != NULL) {
    memset(sys_ptr, 0, size);
  }

  // inactive or failed malloc
  if (! active) {
    TMSG(MEMLEAK, "%s: bytes: %ld, sys: %p (inactive)",
	 name, bytes, sys_ptr);
    return sys_ptr;
  }
  if (sys_ptr == NULL) {
    TMSG(MEMLEAK, "%s: bytes: %ld, sys: %p (failed)",
	 name, bytes, sys_ptr);
    return sys_ptr;
  }

  loc = datacentric_get_malloc_loc(sys_ptr, bytes, align, &appl_ptr, &info_ptr);
  datacentric_add_datainfo(name, sys_ptr, appl_ptr, info_ptr, bytes, uc, loc);

  return appl_ptr;
}


// Reclaim the data in the datainfo struct,
// invalidate the struct and print TMSG.
//
// sys_ptr = the system malloc pointer
// appl_ptr = the application malloc pointer
// info_ptr = pointer to our datainfo struct
// loc = enum constant for header/footer/none
//
static void
datacentric_free_helper(const char *name, void *sys_ptr, void *appl_ptr,
	  	        datainfo_t *info_ptr, int loc)
{
  char *loc_str;

  if (info_ptr == NULL) {
    TMSG(MEMLEAK, "%s: sys: %p appl: %p (no malloc)", name, sys_ptr, appl_ptr);
    return;
  }

  if (hpcrun_datacentric_active()) {
//    hpcrun_free_inc(info_ptr->context, info_ptr->bytes);
    loc_str = loc_name[loc];
  } else {
    loc_str = "inactive";
  }
  info_ptr->magic = 0;

  TMSG(MEMLEAK, "%s: bytes: %ld sys: %p appl: %p info: %p(%s)",
       name, info_ptr->end - info_ptr->start, sys_ptr, appl_ptr, info_ptr,
       loc_str);
}


/******************************************************************************
 * interface operations
 *****************************************************************************/


int
MONITOR_EXT_WRAP_NAME(posix_memalign)(void **memptr, size_t alignment,
                                      size_t bytes)
{
  ucontext_t uc;
  int ret;

  datacentric_initialize();
#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(posix_memalign, uc);
#endif // USE_SYS_GCTXT
  *memptr = datacentric_malloc_helper("posix_memalign", bytes, alignment, 0, &uc, &ret);
  return ret;
}


void *
MONITOR_EXT_WRAP_NAME(memalign)(size_t boundary, size_t bytes)
{
  ucontext_t uc;

  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(memalign, uc);
#endif // USE_SYS_GCTXT

  return datacentric_malloc_helper("memalign", bytes, boundary, 0, &uc, NULL);
}


void *
MONITOR_EXT_WRAP_NAME(valloc)(size_t bytes)
{
  ucontext_t uc;

  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(valloc, uc);
#endif // USE_SYS_GCTXT

  return datacentric_malloc_helper("valloc", bytes, datacentric_pagesize, 0, &uc, NULL);
}


extern void LMALLOC;

void *
MONITOR_EXT_WRAP_NAME(malloc)(size_t bytes)
{
  ucontext_t uc;
  //  mcontext_t* mc = GET_MCONTEXT(&uc);

  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(malloc, uc);
#endif // USE_SYS_GCTXT
  
  return datacentric_malloc_helper("malloc", bytes, 0, 0, &uc, NULL);
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
  ucontext_t uc;

  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(calloc, uc);
#endif // USE_SYS_GCTXT

  return datacentric_malloc_helper("calloc", nmemb * bytes, 0, 1, &uc, NULL);
}


// For free() and realloc(), we must always look for a header, even if
// the metric is inactive and we don't record it in the CCT (unless
// datacentric is entirely disabled).  If the region has a header, then
// the system ptr is not the application ptr, and we must find the
// sytem ptr or else free() will crash.
//
void
MONITOR_EXT_WRAP_NAME(free)(void *ptr)
{
  datainfo_t *info_ptr;
  void *sys_ptr;
  int loc;

  datacentric_initialize();
  TMSG(MEMLEAK, "free: ptr: %p", ptr);

  if (! datacentric_enabled) {
    real_free(ptr);
    TMSG(MEMLEAK, "free: ptr: %p (inactive)", ptr);
    return;
  }
  if (ptr == NULL) {
    return;
  }

  loc = datacentric_get_free_loc(ptr, &sys_ptr, &info_ptr);
  datacentric_free_helper("free", sys_ptr, ptr, info_ptr, loc);

  real_free(sys_ptr);
}


void *
MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t bytes)
{
  ucontext_t uc;
  datainfo_t *info_ptr;
  void *ptr2, *appl_ptr, *sys_ptr;
  int loc, loc2;

  datacentric_initialize();
  TMSG(MEMLEAK, "realloc: ptr: %p bytes: %ld", ptr, bytes);

  if (! datacentric_enabled) {
    return real_realloc(ptr, bytes);
  }

  // realloc(NULL, bytes) means malloc(bytes)

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(realloc, uc);
#endif // USE_SYS_GCTXT

  if (ptr == NULL) {
    return datacentric_malloc_helper("realloc/malloc", bytes, 0, 0, &uc, NULL);
  }

  // treat realloc as a free of the old bytes and delete the node in the splay tree
  loc = datacentric_get_free_loc(ptr, &sys_ptr, &info_ptr);
  datacentric_free_helper("realloc/free", sys_ptr, ptr, info_ptr, loc);

  // realloc(ptr, 0) means free(ptr)
  if (bytes == 0) {
    real_free(sys_ptr);
    return NULL;
  }

  // if inactive, then do real_realloc() and return.
  // but if there used to be a header, then must slide user data.
  if (! hpcrun_datacentric_active()) {
    if (loc == DATACENTRIC_LOC_HEAD) {
      // slide left
      memmove(sys_ptr, ptr, bytes);
    }
    appl_ptr = real_realloc(sys_ptr, bytes);
    TMSG(MEMLEAK, "realloc: bytes: %ld ptr: %p (inactive)", bytes, appl_ptr);
    return appl_ptr;
  }

  // realloc and add datacentric info to new location.  treat this as a
  // malloc of the new size.  note: if realloc moves the data and
  // switches header/footer, then need to slide the user data.
  size_t size = bytes + datainfo_size;
  ptr2 = real_realloc(sys_ptr, size);
  loc2 = datacentric_get_malloc_loc(ptr2, bytes, 0, &appl_ptr, &info_ptr);
  if (loc == DATACENTRIC_LOC_HEAD && loc2 != DATACENTRIC_LOC_HEAD) {
    // slide left
    memmove(ptr2, ptr2 + datainfo_size, bytes);
  }
  else if (loc != DATACENTRIC_LOC_HEAD && loc2 == DATACENTRIC_LOC_HEAD) {
    // slide right
    memmove(ptr2 + datainfo_size, ptr, bytes);
  }
  datacentric_add_datainfo("realloc/malloc", ptr2, appl_ptr, info_ptr, bytes, &uc, loc2);

  return appl_ptr;
}
