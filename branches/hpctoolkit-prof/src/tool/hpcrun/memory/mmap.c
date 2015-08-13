// interface to unix (anonymous) mmap operations

//------------------------------------------------------------------
// System includes
//------------------------------------------------------------------
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//------------------------------------------------------------------
// Local includes
//------------------------------------------------------------------
#include "mmap.h"
#include <messages/messages.h>

//------------------------------------------------------------------
// Internal constants
//------------------------------------------------------------------

#define DEFAULT_PAGESIZE 4096;
static size_t pagesize = DEFAULT_PAGESIZE;

//------------------------------------------------------------------
// Internal functions
//------------------------------------------------------------------
static inline size_t
hpcrun_align_pagesize(size_t size)
{
  return ((size + pagesize - 1)/pagesize) * pagesize;
}

//------------------------------------------------------------------
// Interface functions
//------------------------------------------------------------------

//
// Returns: address of mmap-ed region, else NULL on failure.
//
// Note: we leak fds in the rare case of a process that creates many
// threads running on a system that doesn't allow MAP_ANON.
//
void*
hpcrun_mmap_anon(size_t size)
{
  int prot, flags, fd;
  char* str;
  void* addr;

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

#ifdef FIXME
#endif
  //
  // *** MUST use NMSG,
  //     as this function is called before thread_id is established
  //
  TMSG(MMAP, "%s: size = %ld, fd = %d, addr = %p",
       __func__, size, fd, addr);
  return addr;
}

// Look up pagesize if this is possible
//
void
hpcrun_mmap_init(void)
{
  static bool init_done = false;
  long ans;

  if (init_done)
    return;

#ifdef _SC_PAGESIZE
  if ((ans = sysconf(_SC_PAGESIZE)) > 0) {
    TMSG(MMAP, "sysconf gives pagesize = %ld", ans);
    pagesize = ans;
  }
#endif
  TMSG(MMAP, "pagesize = %ld", pagesize);
  init_done = true;
}
