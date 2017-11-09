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
// Copyright ((c)) 2002-2017, Rice University
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

#include <messages/messages.h>
#include <safe-sampling.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>

#include <sample-sources/datacentric/datacentric.h>

#include "data_tree.h"

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

typedef void *memalign_fcn(size_t, size_t);
typedef void *valloc_fcn(size_t);
typedef void *malloc_fcn(size_t);
typedef void  free_fcn(void *);
typedef void *realloc_fcn(void *, size_t);
typedef void *calloc_fcn(size_t nmemb, size_t size);


/******************************************************************************
 * macros
 *****************************************************************************/

#define DATACENTRIC_DEFAULT_PAGESIZE  4096

#define HPCRUN_DATACENTRIC_PROB  "HPCRUN_DATACENTRIC_PROB"
#define DEFAULT_PROB  0.1

#ifdef HPCRUN_STATIC_LINK

#define real_memalign   __real_memalign
#define real_valloc   __real_valloc
#define real_malloc   __real_malloc
#define real_free     __real_free
#define real_realloc  __real_realloc
#define real_calloc   __real_calloc

#else

#define real_memalign   __libc_memalign
#define real_valloc   __libc_valloc
#define real_malloc   __libc_malloc
#define real_free     __libc_free
#define real_realloc  __libc_realloc
#define real_calloc   __libc_calloc

#endif

extern memalign_fcn       real_memalign;
extern valloc_fcn         real_valloc;
extern malloc_fcn         real_malloc;
extern free_fcn           real_free;
extern realloc_fcn        real_realloc;
extern calloc_fcn         real_calloc;



/******************************************************************************
 * private data
 *****************************************************************************/

static int leak_detection_enabled = 0; // default is off
static int use_datacentric_prob = 0;
static float datacentric_prob = 0.0;

static long datacentric_pagesize = DATACENTRIC_DEFAULT_PAGESIZE;


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
  static int datacentric_overrides_init = 0;    // default is uninitialized

  if (datacentric_overrides_init)
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
  datacentric_overrides_init = 1;

  TMSG(DATACENTRIC, "init");
}


// Fill in the leakinfo struct, add metric to CCT, add to splay tree
// (if footer) and print TMSG.
//
static void
datacentric_add_info(const char *name, void *appl_ptr,
                     size_t bytes, ucontext_t *uc)
{
  datainfo_t *info_ptr = real_malloc(sizeof(datainfo_t));

  info_ptr->bytes     = bytes;
  info_ptr->memblock  = appl_ptr;
  info_ptr->rmemblock = info_ptr->memblock + info_ptr->bytes;
  info_ptr->left      = NULL;
  info_ptr->right     = NULL;
  info_ptr->context   = NULL;

  if (hpcrun_datacentric_active()) {
    sampling_info_t sampling_info;
    memset(&sampling_info, 0, sizeof(sampling_info_t));

    sampling_info.flags = SAMPLING_IN_MALLOC;

    sample_val_t smpl = hpcrun_sample_callpath(uc, hpcrun_datacentric_alloc_id(),
                                               (hpcrun_metricVal_t) {.i=bytes},
                                               0, 1, &sampling_info);
    info_ptr->context = smpl.sample_node;
  }
  splay_insert(info_ptr);
}

// Unified function for all of the mallocs, aligned and unaligned.
// Do the malloc, add the leakinfo struct and print TMSG.
//
// bytes = size of application region
// ret = if allocation is active or not
//
static int
datacentric_alloc_record(void *appl_ptr, const char *name, size_t bytes, ucontext_t *uc)
{
  char *message = "allocation recorded";
  int active    = 1;

  // inside dlopen, that would lead to deadlock.

  if (! (leak_detection_enabled && hpcrun_datacentric_active())) {
    active = 0;
  } else if (TD_GET(inside_dlfcn)) {
    active = 0;
    message = "unable to monitor: inside dlfcn";
  } else if (use_datacentric_prob && (random()/(float)RAND_MAX > datacentric_prob)) {
    active = 0;
    message = "not sampled";
  } else if (appl_ptr == NULL) {
    active = 0;
    message = "null pointer";
  } else if (bytes < MIN_BYTES) {
    active = 0;
    message = "too small";
  }

  if (active) {
    datacentric_add_info(name, appl_ptr, bytes, uc);
  }
  TMSG(DATACENTRIC, "%s: bytes: %ld, ptr: %p, msg: %s", name, bytes, appl_ptr, message);

  return active;
}




/******************************************************************************
 * interface operations
 *****************************************************************************/



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

  ptr = real_valloc(bytes);

  datacentric_alloc_record(ptr, "valloc", bytes, &uc);

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

  ptr = real_malloc(bytes);

  datacentric_alloc_record(ptr, "malloc", bytes, &uc);

  hpcrun_safe_exit();
  return ptr;
}


void *
MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t bytes)
{
  ucontext_t uc;
  void *ptr;

  if (! hpcrun_safe_enter()) {
    ptr = real_calloc(nmemb, bytes);
    return ptr;
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  ptr = real_calloc(nmemb, bytes);

  datacentric_alloc_record(ptr, "calloc", nmemb * bytes, &uc);

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
  // look for header, even if came from inside our code.
  int safe = hpcrun_safe_enter();

  datacentric_initialize();

  if (! leak_detection_enabled) {
    real_free(ptr);
    goto finish;
  }
  if (ptr == NULL) {
    goto finish;
  }

  if (hpcrun_datacentric_active()) {

    datainfo_t *info_ptr = splay_delete(ptr);
    if (info_ptr != NULL) {
      hpcrun_datacentric_free_inc(info_ptr->context, info_ptr->bytes);
      real_free(info_ptr);

      TMSG(DATACENTRIC, "free: bytes: %ld sys: %p   info: %p cct: %p ",
                        info_ptr->bytes, ptr, info_ptr, info_ptr->context);
    }
  }

  real_free(ptr);

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

  if (! hpcrun_safe_enter()) {
    ptr = real_realloc(ptr, bytes);
    return ptr;
  }
  datacentric_initialize();

#ifdef USE_SYS_GCTXT
  getcontext(&uc);
#else // ! USE_SYS_GCTXT
  INLINE_ASM_GCTXT(uc);
#endif // USE_SYS_GCTXT

  ptr = real_realloc(ptr, bytes);

  datacentric_alloc_record(ptr, "realloc", bytes, &uc);

  hpcrun_safe_exit();
  return ptr;
}
