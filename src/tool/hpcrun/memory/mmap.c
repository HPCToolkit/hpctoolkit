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
// Copyright ((c)) 2002-2022, Rice University
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

// interface to unix (anonymous) mmap operations

//------------------------------------------------------------------
// System includes
//------------------------------------------------------------------
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
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
static inline size_t hpcrun_align_pagesize(size_t size) {
  return ((size + pagesize - 1) / pagesize) * pagesize;
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
void* hpcrun_mmap_anon(size_t size) {
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
  TMSG(MMAP, "%s: size = %ld, fd = %d, addr = %p", __func__, size, fd, addr);
  return addr;
}

// Look up pagesize if this is possible
//
void hpcrun_mmap_init(void) {
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
