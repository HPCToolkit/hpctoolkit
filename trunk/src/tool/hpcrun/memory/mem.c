// -*-Mode: C++;-*-
// $Id$
//
// The new memory allocator.  We mmap() a large, single region (6 Meg)
// per thread and dole out pieces via csprof_malloc().  Pieces are
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

//
// no redefinition of csprof_malloc and friends inside mem.c
//

#define _IN_MEM_C 1

#  include "csprof-malloc.h"

#undef _IN_MEM_C

#include "env.h"
#include "monitor.h"
#include "newmem.h"
#include "sample_event.h"
#include "thread_data.h"

#include <messages/messages.h>

#define DEFAULT_MEMSIZE   (6 * 1024 * 1024)
#define BGP_MEMSIZE      (10 * 1024 * 1024)
#define MIN_LOW_MEMSIZE  (80 * 1024)
#define DEFAULT_PAGESIZE  4096

static size_t memsize = DEFAULT_MEMSIZE;
static size_t low_memsize = MIN_LOW_MEMSIZE;
static size_t pagesize = DEFAULT_PAGESIZE;
static int allow_extra_mmap = 1;

static long num_segments = 0;
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

#ifdef HOST_SYSTEM_IBM_BLUEGENE
  memsize = BGP_MEMSIZE;
  allow_extra_mmap = 0;
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
  }

  NMSG(MALLOC, "%s: size = %ld, fd = %d, addr = %p",
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

// Allocate space and init a thread's memstore.
void
hpcrun_make_memstore(hpcrun_meminfo_t *mi)
{
  hpcrun_mem_init();

  mi->mi_start = hpcrun_mmap_anon(memsize);
  if (mi->mi_start == NULL) {
    EMSG("%s: mmap failed, shutting down", __func__);
    monitor_real_abort();
  }

  mi->mi_size = memsize;
  mi->mi_low = mi->mi_start;
  mi->mi_high = mi->mi_start + memsize;
  num_segments++;

  NMSG(MALLOC, "new memstore: [%p, %p)", mi->mi_start, mi->mi_high);
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
csprof_malloc(size_t size)
{
  // Special case
  if (size == 0) {
    return NULL;
  }

  hpcrun_meminfo_t *mi = &TD_GET(memstore);
  void *addr;

  // Non-recoverable out of memory.
  if (mi->mi_high < mi->mi_start + 2*low_memsize) {
    if (allow_extra_mmap) {
      hpcrun_make_memstore(mi);
    } else {
      if (! out_of_mem_mesg) {
	EMSG("%s: out of memory, turning off sampling", __func__);
	out_of_mem_mesg = 1;
      }
      csprof_disable_sampling();
      TMSG(MALLOC, "%s: size = %ld, failure: out of memory",
	   __func__, size);
      num_failures++;
      return NULL;
    }
  }
  size = round_up(size);
  addr = mi->mi_high - size;

  // Recoverable out of memory.
  if (addr <= mi->mi_low) {
    TD_GET(mem_low) = 1;
    TMSG(MALLOC, "%s: size = %ld, failure: temporarily out of memory",
	 __func__, size);
    num_failures++;
    return NULL;
  }

  // Low on memory.
  if (addr < mi->mi_low + low_memsize) {
    TD_GET(mem_low) = 1;
    TMSG(MALLOC, "%s: low on memory, setting epoch flush flag", __func__);
  }

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
csprof_malloc_freeable(size_t size)
{
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
}

void
hpcrun_memory_summary(void)
{
  double meg = 1024.0 * 1024.0;

  AMSG("MEMORY: segment size: %.1f meg, num segments: %ld, "
       "total allocation: %.1f meg, reclaims: %ld",
       memsize/meg, num_segments,
       (num_segments * memsize)/meg, num_reclaims);

  AMSG("MEMORY: total freeable: %.1f meg, total non-freeable: %.1f meg, "
       "malloc failures: %ld",
       total_freeable/meg, total_non_freeable/meg, num_failures);
}
