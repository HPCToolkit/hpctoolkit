// -*-Mode: C;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *


/* out-of-line defintions for library structures */
#ifndef CSPROF_STRUCTS_H
#define CSPROF_STRUCTS_H

#include <limits.h>             /* PATH_MAX */
#include <sys/types.h>          /* pid_t, e.g. */

#if defined(CSPROF_PTHREADS)
#include "xpthread.h"
#endif

#include "csprof_csdata.h"
#include "list.h"
#include "epoch.h"

#include <lush/lush.h>

#define CSPROF_PATH_SZ (PATH_MAX+1) /* path size */


/* represents options for the library */
typedef struct csprof_options_s {
  char lush_agent_paths[CSPROF_PATH_SZ]; /* paths for LUSH agents */

  char out_path[CSPROF_PATH_SZ]; /* path for output */
  char addr_file[CSPROF_PATH_SZ]; /* path for "bad address" file */
  unsigned long mem_sz;       /* initial private memory size, bytes */
  char *event;                /* name of the event */
  unsigned long sample_period;
  unsigned int max_metrics;

} csprof_options_t;

/* not currently used */
/* contents of the persistent state file for the profiler */
typedef struct csprof_pstate_s { 
    long int hostid;            /* host id of the machine FIXME */
    pid_t pid;                  /* the process's pid */
  // #ifdef CSPROF_THREADS // keep the structure, but don't use it in unthreaded case
    pthread_t thrid;            /* the particular thread */
  // #endif
    unsigned int ninit;         /* how many times the pid has been init'd */
} csprof_pstate_t;

typedef enum csprof_status_e {
  CSPROF_STATUS_UNINIT = 0, /* lib has not been initialized at all */
  CSPROF_STATUS_INIT,       /* lib has been initialized */
  CSPROF_STATUS_FINI        /* lib has been finalized (after init) */
} csprof_status_t;

/* profiling state of a single thread */
typedef struct csprof_state_s {
  /* information for recording function call returns; do not move this
     block, since `swizzle_return' must be the first member of the
     structure if we are doing backtracing */
  void *swizzle_return;
  void **swizzle_patch;

  /* last pc where we were signaled; useful for catching problems in
     threaded builds of the profiler (can be useful for debugging in
     non-threaded builds, too) */
  void *last_pc;

  csprof_frame_t *btbuf;      /* where we store backtraces */
  csprof_frame_t *bufend;     /* the end of the buffer */
  csprof_frame_t *bufstk;     /* the top frame in the current backtrace */
  void *treenode;             /* cached pointer into the tree */

  csprof_list_pool_t *pool;

  /* how many bogus samples we took */
  unsigned long trampoline_samples;
  /* various flags, such as whether an exception is being processed or
     whether we think there was a tail call since the last signal */
  unsigned int flags;

  // LUSH support
  lush_agent_pool_t* lush_agents;

  /* persistent state */
  csprof_pstate_t pstate;
#if CSPROF_NEED_PSTATE
  char pstate_fnm[CSPROF_PATH_SZ];
#endif

  /* call stack data, stored in private memory */
  csprof_csdata_t csdata;

  /* our notion of what the current epoch is */
  csprof_epoch_t *epoch;

  /* other profiling states which we have seen */
  struct csprof_state_s *next;

  /* support for alternate profilers whose needs we don't provide */
  void *extra_state;
} csprof_state_t;

#endif
