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
// Copyright ((c)) 2002-2024, Rice University
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

#include "foil.h"

#include "hpctoolkit-config.h"

#include "../main.h"
#include "../start-stop.h"
#include "../ompt/ompt-interface.h"
#include "../monitor-exts/openmp.h"
#include "../sample-sources/io-over.h"
#include "../sample-sources/memleak-overrides.h"
#include "../sample-sources/ga-overrides.h"
#include "../sample-sources/pthread-blame-overrides.h"

#ifdef USE_ROCM
#include "../gpu/amd/rocprofiler-api.h"
#endif

#ifdef USE_LEVEL0
#include "../gpu/intel/level0/level0-api.h"
#endif

#ifdef ENABLE_OPENCL
#include "../gpu/opencl/opencl-api-wrappers.h"
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FOILBASE(name, fn) \
  if (strcmp(name, #fn) == 0) return foilbase_ ## fn

HPCRUN_EXPOSED void* hpcrun_foil_base_lookup(const char* name) {
  // TODO: Replace the slow lookup process below with a Gprof hash table.

  // Core always-on foils
  if (strcmp(name, "monitor_at_main") == 0)
    return foilbase_monitor_at_main;
  if (strcmp(name, "monitor_begin_process_exit") == 0)
    return foilbase_monitor_begin_process_exit;
  if (strcmp(name, "monitor_fini_process") == 0)
    return foilbase_monitor_fini_process;
  if (strcmp(name, "monitor_fini_thread") == 0)
    return foilbase_monitor_fini_thread;
  if (strcmp(name, "monitor_init_mpi") == 0)
    return foilbase_monitor_init_mpi;
  if (strcmp(name, "monitor_init_process") == 0)
    return foilbase_monitor_init_process;
  if (strcmp(name, "monitor_init_thread") == 0)
    return foilbase_monitor_init_thread;
  if (strcmp(name, "monitor_mpi_pre_init") == 0)
    return foilbase_monitor_mpi_pre_init;
  if (strcmp(name, "monitor_post_fork") == 0)
    return foilbase_monitor_post_fork;
  if (strcmp(name, "monitor_pre_fork") == 0)
    return foilbase_monitor_pre_fork;
  if (strcmp(name, "monitor_reset_stacksize") == 0)
    return foilbase_monitor_reset_stacksize;
  if (strcmp(name, "monitor_start_main_init") == 0)
    return foilbase_monitor_start_main_init;
  if (strcmp(name, "monitor_thread_post_create") == 0)
    return foilbase_monitor_thread_post_create;
  if (strcmp(name, "monitor_thread_pre_create") == 0)
    return foilbase_monitor_thread_pre_create;
  if (strcmp(name, "hpctoolkit_sampling_is_active") == 0)
    return foilbase_hpctoolkit_sampling_is_active;
  if (strcmp(name, "hpctoolkit_sampling_start") == 0)
    return foilbase_hpctoolkit_sampling_start;
  if (strcmp(name, "hpctoolkit_sampling_stop") == 0)
    return foilbase_hpctoolkit_sampling_stop;
  if (strcmp(name, "ompt_start_tool") == 0)
    return foilbase_ompt_start_tool;
  if (strcmp(name, "_mp_init") == 0)
    return foilbase__mp_init;

  // IO
  if (strcmp(name, "read") == 0)
    return foilbase_read;
  if (strcmp(name, "write") == 0)
    return foilbase_write;
  if (strcmp(name, "fread") == 0)
    return foilbase_fread;
  if (strcmp(name, "fwrite") == 0)
    return foilbase_fwrite;

  // MEMLEAK
  if (strcmp(name, "posix_memalign") == 0)
    return foilbase_posix_memalign;
  if (strcmp(name, "memalign") == 0)
    return foilbase_memalign;
  if (strcmp(name, "valloc") == 0)
    return foilbase_valloc;
  if (strcmp(name, "malloc") == 0)
    return foilbase_malloc;
  if (strcmp(name, "calloc") == 0)
    return foilbase_calloc;
  if (strcmp(name, "free") == 0)
    return foilbase_free;
  if (strcmp(name, "realloc") == 0)
    return foilbase_realloc;

  // GA
  if (strcmp(name, "pnga_create") == 0)
    return foilbase_pnga_create;
  if (strcmp(name, "pnga_create_handle") == 0)
    return foilbase_pnga_create_handle;
  if (strcmp(name, "pnga_get") == 0)
    return foilbase_pnga_get;
  if (strcmp(name, "pnga_put") == 0)
    return foilbase_pnga_put;
  if (strcmp(name, "pnga_acc") == 0)
    return foilbase_pnga_acc;
  if (strcmp(name, "pnga_nbget") == 0)
    return foilbase_pnga_nbget;
  if (strcmp(name, "pnga_nbput") == 0)
    return foilbase_pnga_nbput;
  if (strcmp(name, "pnga_nbacc") == 0)
    return foilbase_pnga_nbacc;
  if (strcmp(name, "pnga_nbwait") == 0)
    return foilbase_pnga_nbwait;
  if (strcmp(name, "pnga_brdcst") == 0)
    return foilbase_pnga_brdcst;
  if (strcmp(name, "pnga_gop") == 0)
    return foilbase_pnga_gop;
  if (strcmp(name, "pnga_sync") == 0)
    return foilbase_pnga_sync;

  // PTHREAD
  if (strcmp(name, "pthread_cond_timedwait") == 0)
    return foilbase_pthread_cond_timedwait;
  if (strcmp(name, "pthread_cond_wait") == 0)
    return foilbase_pthread_cond_wait;
  if (strcmp(name, "pthread_cond_broadcast") == 0)
    return foilbase_pthread_cond_broadcast;
  if (strcmp(name, "pthread_cond_signal") == 0)
    return foilbase_pthread_cond_signal;
  if (strcmp(name, "pthread_mutex_lock") == 0)
    return foilbase_pthread_mutex_lock;
  if (strcmp(name, "pthread_mutex_unlock") == 0)
    return foilbase_pthread_mutex_unlock;
  if (strcmp(name, "pthread_mutex_timedlock") == 0)
    return foilbase_pthread_mutex_timedlock;
  if (strcmp(name, "pthread_spin_lock") == 0)
    return foilbase_pthread_spin_lock;
  if (strcmp(name, "pthread_spin_unlock") == 0)
    return foilbase_pthread_spin_unlock;
  if (strcmp(name, "sched_yield") == 0)
    return foilbase_sched_yield;
  if (strcmp(name, "sem_wait") == 0)
    return foilbase_sem_wait;
  if (strcmp(name, "sem_post") == 0)
    return foilbase_sem_post;
  if (strcmp(name, "sem_timedwait") == 0)
    return foilbase_sem_timedwait;

#ifdef USE_ROCM
  // ROCm
  if (strcmp(name, "OnLoadToolProp") == 0)
    return foilbase_OnLoadToolProp;
  if (strcmp(name, "OnUnloadTool") == 0)
    return foilbase_OnUnloadTool;
#endif

#ifdef USE_LEVEL0
  // Level Zero
  FOILBASE(name, zeInit);
  FOILBASE(name, zeCommandListAppendLaunchKernel);
  FOILBASE(name, zeCommandListAppendMemoryCopy);
  FOILBASE(name, zeCommandListCreate);
  FOILBASE(name, zeCommandListCreateImmediate);
  FOILBASE(name, zeCommandListDestroy);
  FOILBASE(name, zeCommandListReset);
  FOILBASE(name, zeCommandQueueExecuteCommandLists);
  FOILBASE(name, zeEventPoolCreate);
  FOILBASE(name, zeEventDestroy);
  FOILBASE(name, zeEventHostReset);
  FOILBASE(name, zeModuleCreate);
  FOILBASE(name, zeModuleDestroy);
  FOILBASE(name, zeKernelCreate);
  FOILBASE(name, zeKernelDestroy);
  FOILBASE(name, zeFenceDestroy);
  FOILBASE(name, zeFenceReset);
  FOILBASE(name, zeCommandQueueSynchronize);
#endif

#ifdef ENABLE_OPENCL
  if (strcmp(name, "clBuildProgram") == 0)
    return foilbase_clBuildProgram;
  if (strcmp(name, "clCreateContext") == 0)
    return foilbase_clCreateContext;
  if (strcmp(name, "clCreateCommandQueue") == 0)
    return foilbase_clCreateCommandQueue;
  if (strcmp(name, "clCreateCommandQueueWithProperties") == 0)
    return foilbase_clCreateCommandQueueWithProperties;
  if (strcmp(name, "clEnqueueNDRangeKernel") == 0)
    return foilbase_clEnqueueNDRangeKernel;
  if (strcmp(name, "clEnqueueTask") == 0)
    return foilbase_clEnqueueTask;
  if (strcmp(name, "clEnqueueReadBuffer") == 0)
    return foilbase_clEnqueueReadBuffer;
  if (strcmp(name, "clEnqueueWriteBuffer") == 0)
    return foilbase_clEnqueueWriteBuffer;
  if (strcmp(name, "clEnqueueMapBuffer") == 0)
    return foilbase_clEnqueueMapBuffer;
  if (strcmp(name, "clCreateBuffer") == 0)
    return foilbase_clCreateBuffer;
  if (strcmp(name, "clSetKernelArg") == 0)
    return foilbase_clSetKernelArg;
  if (strcmp(name, "clReleaseMemObject") == 0)
    return foilbase_clReleaseMemObject;
  if (strcmp(name, "clReleaseKernel") == 0)
    return foilbase_clReleaseKernel;
  if (strcmp(name, "clWaitForEvents") == 0)
    return foilbase_clWaitForEvents;
  if (strcmp(name, "clReleaseCommandQueue") == 0)
    return foilbase_clReleaseCommandQueue;
  if (strcmp(name, "clFinish") == 0)
    return foilbase_clFinish;
#endif

  // if (strcmp(name, "") == 0)
  //   return foilbase_;

  assert(false && "Failed attempt to look up an invalid base function");
  abort();
}
