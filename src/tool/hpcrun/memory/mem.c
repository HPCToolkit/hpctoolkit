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

//
// The new memory allocator.  We mmap() a large, single region (4 Meg)
// per thread and dole out pieces via hpcrun_malloc().  Pieces are
// either freeable (CCT nodes) or not freeable (everything else).
// When memory gets low, we write out an epoch and reclaim the CCT
// nodes.
//

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// no redefinition of hpcrun_malloc and friends inside mem.c
#define _IN_MEM_C 1
#  include "hpcrun-malloc.h"
#undef _IN_MEM_C

#include "env.h"
#include "newmem.h"
#include "sample_event.h"
#include "thread_data.h"

#include <messages/messages.h>

#define DEFAULT_MEMSIZE   (4 * 1024 * 1024)
#define MIN_LOW_MEMSIZE  (80 * 1024)
#define DEFAULT_PAGESIZE  4096

static size_t memsize = DEFAULT_MEMSIZE;
static size_t low_memsize = MIN_LOW_MEMSIZE;
static size_t pagesize = DEFAULT_PAGESIZE;
static int allow_extra_mmap = 1;

static long num_segments = 0;
static long total_allocation = 0;
static long num_reclaims = 0;
static long num_failures = 0;
static long total_freeable = 0;
static long total_non_freeable = 0;

static int out_of_mem_mesg = 0;

//------------------------------------------------------------------
// Internal functions
//------------------------------------------------------------------

static inline size_t
round_up(size_t size)
{
  return (size + 7) & ~7L;
}

static inline size_t
hpcrun_align_pagesize(size_t size)
{
  return ((size + pagesize - 1)/pagesize) * pagesize;
}

// Look up environ variables and pagesize.
static void
hpcrun_mem_init(void)
{
  static int init_done = 0;
  char *str;
  long ans;

  if (init_done)
    return;

#ifdef _SC_PAGESIZE
  if ((ans = sysconf(_SC_PAGESIZE)) > 0) {
    pagesize = ans;
  }
#endif

  str = getenv(HPCRUN_MEMSIZE);
  if (str != NULL && sscanf(str, "%ld", &ans) == 1) {
    memsize = hpcrun_align_pagesize(ans);
  }

  str = getenv(HPCRUN_LOW_MEMSIZE);
  if (str != NULL && sscanf(str, "%ld", &ans) == 1) {
    low_memsize = ans;
  } else {
    low_memsize = memsize/40;
    if (low_memsize < MIN_LOW_MEMSIZE)
      low_memsize = MIN_LOW_MEMSIZE;
  }

  TMSG(MALLOC, "%s: pagesize = %ld, memsize = %ld, "
       "low memsize = %ld, extra mmap = %d",
       __func__, pagesize, memsize, low_memsize, allow_extra_mmap);
  init_done = 1;
}

//
// Returns: address of mmap-ed region, else NULL on failure.
//
// Note: we leak fds in the rare case of a process that creates many
// threads running on a system that doesn't allow MAP_ANON.
//
static void *
hpcrun_mmap_anon(size_t size)
{
  int prot, flags, fd;
  char *str;
  void *addr;

  size = hpcrun_align_pagesize(size);
  prot = PROT_READ | PROT_WRITE;
  fd = -1;

#if defined(MAP_ANON)
  flags = MAP_PRIVATE | MAP_ANON;
#elif defined(MAP_ANONYMOUS)
  flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
  flags = MAP_PRIVATE;
  fd = open("/dev/zero", O_RDWR);
  if (fd < 0) {
    str = strerror(errno);
    EMSG("%s: open /dev/null failed: %s", __func__, str);
    return NULL;
  }
#endif

  addr = mmap(NULL, size, prot, flags, fd, 0);
  if (addr == MAP_FAILED) {
    str = strerror(errno);
    EMSG("%s: mmap failed: %s", __func__, str);
    addr = NULL;
  } else {
    num_segments++;
    total_allocation += size;
  }

  TMSG(MALLOC, "%s: size = %ld, fd = %d, addr = %p",
       __func__, size, fd, addr);
  return addr;
}

//------------------------------------------------------------------
// External functions
//------------------------------------------------------------------

//
// memstore layout:
//
//   +------------------+------------+----------------+
//   |  freeable (cct)  |            |  non-freeable  |
//   +------------------+------------+----------------+
//   mi_start           mi_low       mi_high
//

// After fork(), the parent's memstores are still allocated in the
// child, so don't reset num_segments.
// FIXME: it's not clear which stats should be reset here.
void
hpcrun_memory_reinit(void)
{
  num_reclaims = 0;
  num_failures = 0;
  total_freeable = 0;
  total_non_freeable = 0;
  out_of_mem_mesg = 0;
}

// Allocate space and init a thread's memstore.
// If failure, shutdown sampling and leave old memstore in place.
void
hpcrun_make_memstore(hpcrun_meminfo_t *mi, int is_child)
{
  void *addr;

  hpcrun_mem_init();

  // If in the child after fork(), then continue to use the parent's
  // memstore if it looks ok, else mmap a new one.  Note: we can't
  // reset the memstore to empty unless we delete everything that was
  // created via hpcrun_malloc() (cct, ui_tree, ...).
  if (is_child && mi->mi_start != NULL
      && mi->mi_start <= mi->mi_low && mi->mi_low <= mi->mi_high
      && mi->mi_high <= mi->mi_start + mi->mi_size) {
    return;
  }

  addr = hpcrun_mmap_anon(memsize);
  if (addr == NULL) {
    if (! out_of_mem_mesg) {
      EMSG("%s: out of memory, shutting down sampling", __func__);
      out_of_mem_mesg = 1;
    }
    hpcrun_disable_sampling();
    return;
  }

  mi->mi_start = addr;
  mi->mi_size = memsize;
  mi->mi_low = mi->mi_start;
  mi->mi_high = mi->mi_start + memsize;

  TMSG(MALLOC, "new memstore: [%p, %p)", mi->mi_start, mi->mi_high);
}

// Reclaim the freeable CCT memory at the low end.
void
hpcrun_reclaim_freeable_mem(void)
{
  hpcrun_meminfo_t *mi = &TD_GET(memstore);

  mi->mi_low = mi->mi_start;
  TD_GET(mem_low) = 0;
  num_reclaims++;
  TMSG(MALLOC, "%s: %d", __func__, num_reclaims);
}

//
// Returns: address of non-freeable region at the high end,
// else NULL on failure.
//
void *
hpcrun_malloc(size_t size)
{
  hpcrun_meminfo_t *mi;
  void *addr;

  // Lush wants to ask for 0 bytes and get back NULL.
  if (size == 0) {
    return NULL;
  }

  mi = &TD_GET(memstore);
  size = round_up(size);

  // For a large request that doesn't fit within the existing
  // memstore, mmap a separate region for it.
  if (size > memsize/5 && allow_extra_mmap
      && (mi->mi_start == NULL || size > mi->mi_high - mi->mi_low)) {
    addr = hpcrun_mmap_anon(size);
    if (addr == NULL) {
      if (! out_of_mem_mesg) {
	EMSG("%s: out of memory, shutting down sampling", __func__);
	out_of_mem_mesg = 1;
      }
      hpcrun_disable_sampling();
      num_failures++;
      return NULL;
    }
    TMSG(MALLOC, "%s: size = %ld, addr = %p", __func__, size, addr);
    total_non_freeable += size;
    return addr;
  }

  // See if we need to allocate a new memstore.
  if (mi->mi_start == NULL
      || mi->mi_high - mi->mi_low < low_memsize
      || mi->mi_high - mi->mi_low < size) {
    if (allow_extra_mmap) {
      hpcrun_make_memstore(mi, 0);
    } else {
      if (! out_of_mem_mesg) {
	EMSG("%s: out of memory, shutting down sampling", __func__);
	out_of_mem_mesg = 1;
      }
      hpcrun_disable_sampling();
    }
  }

  // There is no memstore, for some reason.
  if (mi->mi_start == NULL) {
    TMSG(MALLOC, "%s: size = %ld, failure: no memstore",
	 __func__, size);
    num_failures++;
    return NULL;
  }

  // Not enough space in existing memstore.
  addr = mi->mi_high - size;
  if (addr <= mi->mi_low) {
    TMSG(MALLOC, "%s: size = %ld, failure: out of memory",
	 __func__, size);
    num_failures++;
    return NULL;
  }

  // Success
  mi->mi_high = addr;
  total_non_freeable += size;
  TMSG(MALLOC, "%s: size = %ld, addr = %p", __func__, size, addr);
  return addr;
}

//
// Returns: address of freeable region at the high end,
// else NULL on failure.
//
void *
hpcrun_malloc_freeable(size_t size)
{
  return hpcrun_malloc(size);

  // For now, don't bother with freeable memory.
#if 0
  hpcrun_meminfo_t *mi = &TD_GET(memstore);
  void *addr, *ans;

  size = round_up(size);
  addr = mi->mi_low + size;

  // Recoverable out of memory.
  if (addr >= mi->mi_high) {
    TD_GET(mem_low) = 1;
    TMSG(MALLOC, "%s: size = %ld, failure: temporary out of memory",
	 __func__, size);
    num_failures++;
    return NULL;
  }

  // Low on memory.
  if (addr + low_memsize > mi->mi_high) {
    TD_GET(mem_low) = 1;
    TMSG(MALLOC, "%s: low on memory, setting epoch flush flag", __func__);
  }

  ans = mi->mi_low;
  mi->mi_low = addr;
  total_freeable += size;
  TMSG(MALLOC, "%s: size = %ld, addr = %p", __func__, size, addr);
  return ans;
#endif
}

void
hpcrun_memory_summary(void)
{
  double meg = 1024.0 * 1024.0;

  AMSG("MEMORY: segment size: %.1f meg, num segments: %ld, "
       "total allocation: %.1f meg, reclaims: %ld",
       memsize/meg, num_segments, total_allocation/meg, num_reclaims);

  AMSG("MEMORY: total freeable: %.1f meg, total non-freeable: %.1f meg, "
       "malloc failures: %ld",
       total_freeable/meg, total_non_freeable/meg, num_failures);
}
