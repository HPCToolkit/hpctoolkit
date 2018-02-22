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
// Copyright ((c)) 2002-2018, Rice University
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

// DATACENTRIC overrides the malloc family of functions and provides two
// metrics: number of bytes allocated and number of bytes freed per
// dynamic context.  Subtracting these two values is a way to find
// memory leaks.
//
// Override functions:
// posix_memalign, memalign, valloc
// malloc, calloc, free, realloc

/******************************************************************************
 * standard include files
 *****************************************************************************/

#define __USE_XOPEN_EXTENDED
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>

/* definition for posix memalign */
#undef _XOPEN_SOURCE         // avoid complaint about redefinition
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
#include <safe-sampling.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include "datacentric.h"

// FIXME: the inline getcontext macro is broken on 32-bit x86, so
// revert to the getcontext syscall for now.
#if defined(__i386__)
#ifndef USE_SYS_GCTXT
#define USE_SYS_GCTXT
#endif
#else  // ! __i386__
#include <utilities/arch/inline-asm-gctxt.h>
#include <utilities/arch/mcontext.h>
#endif

#define MIN_BYTES 8192

/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef struct leakinfo_s {
  long magic;
  cct_node_t *context;
  size_t bytes;
  void *memblock;
  void *rmemblock;
  struct leakinfo_s *left;
  struct leakinfo_s *right;
} leakinfo_t;

leakinfo_t leakinfo_NULL = { .magic = 0, .context = NULL, .bytes = 0 };

typedef void *memalign_fcn(size_t, size_t);
typedef void *valloc_fcn(size_t);
typedef void *malloc_fcn(size_t);
typedef void  free_fcn(void *);
typedef void *realloc_fcn(void *, size_t);



/******************************************************************************
 * macros
 *****************************************************************************/

#define DATACENTRIC_USE_HYBRID_LAYOUT 0

#define DATACENTRIC_MAGIC 0x68706374
#define DATACENTRIC_DEFAULT_PAGESIZE  4096

#define HPCRUN_DATACENTRIC_PROB  "HPCRUN_DATACENTRIC_PROB"
#define DEFAULT_PROB  0.1

#ifdef HPCRUN_STATIC_LINK
#define real_memalign   __real_memalign
#define real_valloc   __real_valloc
#define real_malloc   __real_malloc
#define real_free     __real_free
#define real_realloc  __real_realloc
#else
#define real_memalign   __libc_memalign
#define real_valloc   __libc_valloc
#define real_malloc   __libc_malloc
#define real_free     __libc_free
#define real_realloc  __libc_realloc
#endif

extern memalign_fcn       real_memalign;
extern valloc_fcn         real_valloc;
extern malloc_fcn         real_malloc;
extern free_fcn           real_free;
extern realloc_fcn        real_realloc;



/******************************************************************************
 * private data
 *****************************************************************************/

static int leak_detection_enabled = 0; // default is off
static int leak_detection_init = 0;    // default is uninitialized
static int use_datacentric_prob = 0;
static float datacentric_prob = 0.0;

static struct leakinfo_s *datacentric_tree_root = NULL;
static spinlock_t memtree_lock = SPINLOCK_UNLOCKED;

static int leakinfo_size = sizeof(struct leakinfo_s);
static long datacentric_pagesize = DATACENTRIC_DEFAULT_PAGESIZE;

enum {
  DATACENTRIC_LOC_HEAD = 1,
  DATACENTRIC_LOC_FOOT,
  DATACENTRIC_LOC_NONE
};

static char *loc_name[4] = {
  NULL, "header", "footer", "none"
};

//static int byte_threshold = MIN_BYTES;

/******************************************************************************
 * splay operations
 *****************************************************************************/


static struct leakinfo_s *
splay(struct leakinfo_s *root, void *key)
{
  REGULAR_SPLAY_TREE(leakinfo_s, root, key, memblock, left, right);
  return root;
}

static struct leakinfo_s *
interval_splay(struct leakinfo_s *root, void *key)
{
  INTERVAL_SPLAY_TREE(leakinfo_s, root, key, memblock, rmemblock, left, right);
  return root;
}

static void
splay_insert(struct leakinfo_s *node)
{
  void *memblock = node->memblock;

  node->left = node->right = NULL;

  spinlock_lock(&memtree_lock);  
  if (datacentric_tree_root != NULL) {
    datacentric_tree_root = splay(datacentric_tree_root, memblock);

    if (memblock < datacentric_tree_root->memblock) {
      node->left = datacentric_tree_root->left;
      node->right = datacentric_tree_root;
      datacentric_tree_root->left = NULL;
    } else if (memblock > datacentric_tree_root->memblock) {
      node->left = datacentric_tree_root;
      node->right = datacentric_tree_root->right;
      datacentric_tree_root->right = NULL;
    } else {
      TMSG(DATACENTRIC, "datacentric splay tree: unable to insert %p (already present)", 
	   node->memblock);
      assert(0);
    }
  }
  datacentric_tree_root = node;
  spinlock_unlock(&memtree_lock);  
}


static struct leakinfo_s *
splay_delete(void *memblock)
{
  struct leakinfo_s *result = NULL;

  spinlock_lock(&memtree_lock);  
  if (datacentric_tree_root == NULL) {
    spinlock_unlock(&memtree_lock);  
    TMSG(DATACENTRIC, "datacentric splay tree empty: unable to delete %p", memblock);
    return NULL;
  }

  datacentric_tree_root = splay(datacentric_tree_root, memblock);

  if (memblock != datacentric_tree_root->memblock) {
    spinlock_unlock(&memtree_lock);  
    TMSG(DATACENTRIC, "datacentric splay tree: %p not in tree", memblock);
    return NULL;
  }

  result = datacentric_tree_root;

  if (datacentric_tree_root->left == NULL) {
    datacentric_tree_root = datacentric_tree_root->right;
    spinlock_unlock(&memtree_lock);  
    return result;
  }

  datacentric_tree_root->left = splay(datacentric_tree_root->left, memblock);
  datacentric_tree_root->left->right = datacentric_tree_root->right;
  datacentric_tree_root =  datacentric_tree_root->left;
  spinlock_unlock(&memtree_lock);  
  return result;
}

/* interface for data-centric analysis */
cct_node_t *
splay_lookup(void *key, void **start, void **end)
{
  if(!datacentric_tree_root || !key) {
    return NULL;
  }

  struct leakinfo_s *info;
  spinlock_lock(&memtree_lock);  
  datacentric_tree_root = interval_splay(datacentric_tree_root, key);
  info = datacentric_tree_root;
  if(!info) {
    spinlock_unlock(&memtree_lock);  
    return NULL;
  }
  if((info->memblock <= key) && (info->rmemblock > key)) {
    *start = info->memblock;
    *end = info->rmemblock;
    spinlock_unlock(&memtree_lock);  
    return info->context;
  }
  spinlock_unlock(&memtree_lock);  
  return NULL;
}

/******************************************************************************
 * private operations
 *****************************************************************************/

// Accept 0.ddd as floating point or x/y as fraction.
static float
string_to_prob(char *str)
{
  int x, y;
  float ans;

  if (strchr(str, '/') != NULL) {
    if (sscanf(str, "%d/%d", &x, &y) == 2 && y > 0) {
      ans = (float)x / (float)y;
    } else {
      ans = DEFAULT_PROB;
    }
  } else {
    if (sscanf(str, "%f", &ans) < 1) {
      ans = DEFAULT_PROB;
    }
  }

  return ans;
}


static void
datacentric_initialize(void)
{
  struct timeval tv;
  char *prob_str;
  unsigned int seed;
  int fd;

  if (leak_detection_init)
    return;

#ifdef _SC_PAGESIZE
  datacentric_pagesize = sysconf(_SC_PAGESIZE);
#else
  datacentric_pagesize = DATACENTRIC_DEFAULT_PAGESIZE;
#endif

  // If we are sampling the mallocs, then read the probability and
  // seed the random number generator.
  prob_str = getenv(HPCRUN_DATACENTRIC_PROB);
  if (prob_str != NULL) {
    use_datacentric_prob = 1;
    datacentric_prob = string_to_prob(prob_str);
    TMSG(DATACENTRIC, "sampling mallocs with prob = %f", datacentric_prob);

    seed = 0;
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
      read(fd, &seed, sizeof(seed));
      close(fd);
    }
    gettimeofday(&tv, NULL);
    seed += (getpid() << 16) + (tv.tv_usec << 4);
    srandom(seed);
  }

  // unconditionally enable leak detection for now
  leak_detection_enabled = 1;
  leak_detection_init = 1;

  TMSG(DATACENTRIC, "init");
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


// Choose the location of the leakinfo struct at malloc().  Use a
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
// info_ptr = location of the leakinfo struct
//
static int
datacentric_get_malloc_loc(void *sys_ptr, size_t bytes, size_t align,
		       void **appl_ptr, leakinfo_t **info_ptr)
{
#if DATACENTRIC_USE_HYBRID_LAYOUT
  if (datacentric_same_page(sys_ptr, sys_ptr + leakinfo_size) && align == 0) {
    *appl_ptr = sys_ptr + leakinfo_size;
    *info_ptr = sys_ptr;
    return DATACENTRIC_LOC_HEAD;
  }
#endif

  // footer
  *appl_ptr = sys_ptr;
  *info_ptr = sys_ptr + bytes;
  return DATACENTRIC_LOC_FOOT;
}


// Find the location of the leakinfo struct at free().
//
// Returns: enum constant for header/footer/none,
// and system and leakinfo pointers.
//
static int
datacentric_get_free_loc(void *appl_ptr, void **sys_ptr, leakinfo_t **info_ptr)
{
  static int num_errors = 0;

#if DATACENTRIC_USE_HYBRID_LAYOUT
  // try header first
  *info_ptr = (leakinfo_t *) (appl_ptr - leakinfo_size);
  if (datacentric_same_page(*info_ptr, appl_ptr)
      && (*info_ptr)->magic == DATACENTRIC_MAGIC
      && (*info_ptr)->memblock == appl_ptr) {
    *sys_ptr = *info_ptr;
    return DATACENTRIC_LOC_HEAD;
  }
#endif

  // always try footer
  *sys_ptr = appl_ptr;
  *info_ptr = splay_delete(appl_ptr);
  if (*info_ptr == NULL) {
    return DATACENTRIC_LOC_NONE;
  }
  if ((*info_ptr)->magic == DATACENTRIC_MAGIC
      && (*info_ptr)->memblock == appl_ptr) {
    return DATACENTRIC_LOC_FOOT;
  }

  // must be memory corruption somewhere.  maybe a bug in our code,
  // but more likely someone else has stomped on our data.
  num_errors++;
  if (num_errors < 100) {
    AMSG("DATACENTRIC: Warning: memory corruption in leakinfo node: %p "
	 "sys: %p appl: %p magic: 0x%lx context: %p bytes: %ld memblock: %p",
	 *info_ptr, *sys_ptr, appl_ptr, (*info_ptr)->magic, (*info_ptr)->context,
	 (*info_ptr)->bytes, (*info_ptr)->memblock);
  }
  *info_ptr = NULL;
  return DATACENTRIC_LOC_NONE;
}


// Fill in the leakinfo struct, add metric to CCT, add to splay tree
// (if footer) and print TMSG.
//
static void
datacentric_add_leakinfo(const char *name, void *sys_ptr, void *appl_ptr,
		     leakinfo_t *info_ptr, size_t bytes, ucontext_t *uc,
		     int loc)
{
  char *loc_str;

  if (info_ptr == NULL) {
    TMSG(DATACENTRIC, "Warning: %s: bytes: %ld sys: %p appl: %p info: %p "
	 "(NULL leakinfo pointer, this should not happen)",
	 name, bytes, sys_ptr, appl_ptr, info_ptr);
    return;
  }

  info_ptr->magic = DATACENTRIC_MAGIC;
  info_ptr->bytes = bytes;
  info_ptr->memblock = appl_ptr;
  info_ptr->rmemblock = info_ptr->memblock + info_ptr->bytes;
  info_ptr->left = NULL;
  info_ptr->right = NULL;
  if (hpcrun_datacentric_active()) {
    TD_GET(mem_data.in_malloc) = 1;
    sample_val_t smpl =
      hpcrun_sample_callpath(uc, hpcrun_datacentric_alloc_id(), 
        (hpcrun_metricVal_t) {.i=bytes}, 
        0, 1, NULL);
    TD_GET(mem_data.in_malloc) = 0;
    info_ptr->context = smpl.sample_node;
    
    loc_str = loc_name[loc];
  } else {
    info_ptr->context = NULL;
    loc_str = "inactive";
  }
  if (loc == DATACENTRIC_LOC_FOOT) {
    splay_insert(info_ptr);
  }

  TMSG(DATACENTRIC, "%s: bytes: %ld sys: %p appl: %p info: %p cct: %p (%s)",
       name, bytes, sys_ptr, appl_ptr, info_ptr, info_ptr->context, loc_str);
}


// Unified function for all of the mallocs, aligned and unaligned.
// Do the malloc, add the leakinfo struct and print TMSG.
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
  leakinfo_t *info_ptr;
  char *inactive_mesg = "inactive";
  int active, loc;
  size_t size;

  TMSG(DATACENTRIC, "%s: bytes: %ld", name, bytes);

  // do the real malloc, aligned or not.  note: we can't track malloc
  // inside dlopen, that would lead to deadlock.
  active = 1;
  if (! (leak_detection_enabled && hpcrun_datacentric_active())) {
    active = 0;
  } else if (TD_GET(inside_dlfcn)) {
    active = 0;
    inactive_mesg = "unable to monitor: inside dlfcn";
  } else if (use_datacentric_prob && (random()/(float)RAND_MAX > datacentric_prob)) {
    active = 0;
    inactive_mesg = "not sampled";
  }
  size = bytes + (active ? leakinfo_size : 0);
  if (align != 0) {
    // there is no __libc_posix_memalign(), so we use __libc_memalign()
    // instead, or else use dlsym().
    sys_ptr = real_memalign(align, size);
    if (ret != NULL) {
      *ret = (sys_ptr == NULL) ? errno : 0;
    }
  } else {
    sys_ptr = real_malloc(size);
  }
  if (clear && sys_ptr != NULL) {
    memset(sys_ptr, 0, size);
  }

  // inactive or failed malloc
  if (! active) {
    TMSG(DATACENTRIC, "%s: bytes: %ld, sys: %p (%s)",
	 name, bytes, sys_ptr, inactive_mesg);
    return sys_ptr;
  }
  if (sys_ptr == NULL) {
    TMSG(DATACENTRIC, "%s: bytes: %ld, sys: %p (failed)",
	 name, bytes, sys_ptr);
    return sys_ptr;
  }
  if (bytes <= MIN_BYTES) return sys_ptr;

  loc = datacentric_get_malloc_loc(sys_ptr, bytes, align, &appl_ptr, &info_ptr);
  datacentric_add_leakinfo(name, sys_ptr, appl_ptr, info_ptr, bytes, uc, loc);

  // support for first touch analysis

  // Change the permission for the newly allocated memory ranges (rounded to 
  // the page boundary). Will segfault at the first touch of the memory range.
  int pagesize = getpagesize();
  // round to the upper-bound of the next page
  void *p = (void *)(((uint64_t)(uintptr_t) sys_ptr + pagesize-1) & ~(pagesize-1));
  // monitor all pages of the allocation (expect the page of splay section)
  uint64_t msize = (uint64_t)(uintptr_t) info_ptr-(uint64_t)(uintptr_t) p - pagesize;
  if (msize <= 0 ) msize = pagesize - 1;
  mprotect (p, msize, PROT_NONE);
  // change the splay node section to be read/write.
  // make sure pages with the splay section have correct permissions
  p = (void *)((uint64_t)(uintptr_t) info_ptr & ~(pagesize - 1));
  mprotect (p, (uint64_t)(uintptr_t) info_ptr + leakinfo_size - (uint64_t)(uintptr_t) p, PROT_READ | PROT_WRITE);
  return appl_ptr;
}


// Reclaim the data in the leakinfo struct, add metric to CCT,
// invalidate the struct and print TMSG.
//
// sys_ptr = the system malloc pointer
// appl_ptr = the application malloc pointer
// info_ptr = pointer to our leakinfo struct
// loc = enum constant for header/footer/none
//
static void
datacentric_free_helper(const char *name, void *sys_ptr, void *appl_ptr,
		    leakinfo_t *info_ptr, int loc)
{
  char *loc_str;

  if (info_ptr == NULL) {
    TMSG(DATACENTRIC, "%s: sys: %p appl: %p (no malloc)", name, sys_ptr, appl_ptr);
    return;
  }

  if (info_ptr->context != NULL && hpcrun_datacentric_active()) {
    hpcrun_datacentric_free_inc(info_ptr->context, info_ptr->bytes);
    loc_str = loc_name[loc];
  } else {
    loc_str = "inactive";
  }
  info_ptr->magic = 0;

  TMSG(DATACENTRIC, "%s: bytes: %ld sys: %p appl: %p info: %p cct: %p (%s)",
       name, info_ptr->bytes, sys_ptr, appl_ptr, info_ptr,
       info_ptr->context, loc_str);
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

// The datacentric overrides pose extra challenges for safe sampling.
// When we enter a datacentric override, if we're coming from inside our
// own code, we can't just automatically call the real version of the
// function and return.  The problem is that sometimes we put headers
// in front of the malloc'd region and thus the application and the
// system don't use the same pointer for the beginning of the region.
//
// For malloc, memalign, etc, it's ok to return the real version
// without a header.  The free code checks for the case of no header
// or footer.  And actually, there are always a few system mallocs
// that happen before we get initialized that go untracked.
//
// But for free, we can't just call the real free if the region might
// have a header.  In that case, we'd be freeing the wrong pointer and
// the memory system would crash.
//
// Now, we could call the real free if we were 100% certain that the
// malloc also came from our code and thus had no header.  But if
// there's anything that slips through the cracks, then the program
// crashes.  OTOH, running our code looking for a header might hit
// deadlock if the debug MSGS are on.  Choose your poison, but
// probably this case never happens.
//
// The moral is: be careful not to use malloc or free in our code.
// Use mmap and hpcrun_malloc instead.

int
MONITOR_EXT_WRAP_NAME(posix_memalign)(void **memptr, size_t alignment,
                                      size_t bytes)
{
  ucontext_t uc;
  int ret;

  if (! hpcrun_safe_enter()) {
    *memptr = real_memalign(alignment, bytes);
    return (*memptr == NULL) ? errno : 0;
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  *memptr = datacentric_malloc_helper("posix_memalign", bytes, alignment, 0, &uc, &ret);
  hpcrun_safe_exit();
  return ret;
}


void *
MONITOR_EXT_WRAP_NAME(memalign)(size_t boundary, size_t bytes)
{
  ucontext_t uc;
  void *ptr;

  if (! hpcrun_safe_enter()) {
    return real_memalign(boundary, bytes);
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  ptr = datacentric_malloc_helper("memalign", bytes, boundary, 0, &uc, NULL);
  hpcrun_safe_exit();
  return ptr;
}


void *
MONITOR_EXT_WRAP_NAME(valloc)(size_t bytes)
{
  ucontext_t uc;
  void *ptr;

  if (! hpcrun_safe_enter()) {
    return real_valloc(bytes);
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  ptr = datacentric_malloc_helper("valloc", bytes, datacentric_pagesize, 0, &uc, NULL);
  hpcrun_safe_exit();
  return ptr;
}


void *
MONITOR_EXT_WRAP_NAME(malloc)(size_t bytes)
{
  ucontext_t uc;
  void *ptr;

  if (! hpcrun_safe_enter()) {
    return real_malloc(bytes);
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  ptr = datacentric_malloc_helper("malloc", bytes, 0, 0, &uc, NULL);
  hpcrun_safe_exit();
  return ptr;
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
  ucontext_t uc;
  void *ptr;

  if (! hpcrun_safe_enter()) {
    ptr = real_malloc(nmemb * bytes);
    if (ptr != NULL) {
      memset(ptr, 0, nmemb * bytes);
    }
    return ptr;
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  ptr = datacentric_malloc_helper("calloc", nmemb * bytes, 0, 1, &uc, NULL);
  hpcrun_safe_exit();
  return ptr;
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
  leakinfo_t *info_ptr;
  void *sys_ptr;
  int loc;

  // look for header, even if came from inside our code.
  int safe = hpcrun_safe_enter();

  datacentric_initialize();
  TMSG(DATACENTRIC, "free: ptr: %p", ptr);

  if (! leak_detection_enabled) {
    real_free(ptr);
    TMSG(DATACENTRIC, "free: ptr: %p (inactive)", ptr);
    goto finish;
  }
  if (ptr == NULL) {
    goto finish;
  }

  loc = datacentric_get_free_loc(ptr, &sys_ptr, &info_ptr);
  datacentric_free_helper("free", sys_ptr, ptr, info_ptr, loc);
  real_free(sys_ptr);

finish:
  if (safe) {
    hpcrun_safe_exit();
  }
  return;
}


void *
MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t bytes)
{
  ucontext_t uc;
  leakinfo_t *info_ptr;
  void *ptr2, *appl_ptr, *sys_ptr;
  char *inactive_mesg = "inactive";
  int loc, loc2, active;

  // look for header, even if came from inside our code.
  int safe = hpcrun_safe_enter();

  datacentric_initialize();
  TMSG(DATACENTRIC, "realloc: ptr: %p bytes: %ld", ptr, bytes);

  if (! leak_detection_enabled) {
    appl_ptr = real_realloc(ptr, bytes);
    goto finish;
  }

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  // realloc(NULL, bytes) means malloc(bytes)
  if (ptr == NULL) {
    appl_ptr = datacentric_malloc_helper("realloc/malloc", bytes, 0, 0, &uc, NULL);
    goto finish;
  }

  // for datacentric metric, treat realloc as a free of the old bytes
  loc = datacentric_get_free_loc(ptr, &sys_ptr, &info_ptr);
  datacentric_free_helper("realloc/free", sys_ptr, ptr, info_ptr, loc);

  // realloc(ptr, 0) means free(ptr)
  if (bytes == 0) {
    real_free(sys_ptr);
    appl_ptr = NULL;
    goto finish;
  }

  // if inactive, then do real_realloc() and return.
  // but if there used to be a header, then must slide user data.
  // again, can't track malloc inside dlopen.
  active = 1;
  if (! (leak_detection_enabled && hpcrun_datacentric_active())) {
    active = 0;
  } else if (TD_GET(inside_dlfcn)) {
    active = 0;
    inactive_mesg = "unable to monitor: inside dlfcn";
  } else if (use_datacentric_prob && (random()/(float)RAND_MAX > datacentric_prob)) {
    active = 0;
    inactive_mesg = "not sampled";
  }
  if (! active) {
    if (loc == DATACENTRIC_LOC_HEAD) {
      // slide left
      memmove(sys_ptr, ptr, bytes);
    }
    appl_ptr = real_realloc(sys_ptr, bytes);
    TMSG(DATACENTRIC, "realloc: bytes: %ld ptr: %p (%s)",
	 bytes, appl_ptr, inactive_mesg);
    goto finish;
  }

  // realloc and add leak info to new location.  treat this as a
  // malloc of the new size.  note: if realloc moves the data and
  // switches header/footer, then need to slide the user data.
  size_t size = bytes + leakinfo_size;
  ptr2 = real_realloc(sys_ptr, size);
  loc2 = datacentric_get_malloc_loc(ptr2, bytes, 0, &appl_ptr, &info_ptr);
  if (loc == DATACENTRIC_LOC_HEAD && loc2 != DATACENTRIC_LOC_HEAD) {
    // slide left
    memmove(ptr2, ptr2 + leakinfo_size, bytes);
  }
  else if (loc != DATACENTRIC_LOC_HEAD && loc2 == DATACENTRIC_LOC_HEAD) {
    // slide right
    memmove(ptr2 + leakinfo_size, ptr, bytes);
  }
  datacentric_add_leakinfo("realloc/malloc", ptr2, appl_ptr, info_ptr, bytes, &uc, loc2);

finish:
  if (safe) {
    hpcrun_safe_exit();
  }
  return appl_ptr;
}
