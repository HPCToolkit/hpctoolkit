// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"

#include "../main.h"
#include "../start-stop.h"
#include "../libmonitor/monitor.h"
#include "../ompt/ompt-interface.h"
#include "../monitor-exts/openmp.h"
#include "../sample-sources/io-over.h"
#include "../sample-sources/memleak-overrides.h"
#include "../sample-sources/ga-overrides.h"
#include "../sample-sources/pthread-blame-overrides.h"

#ifdef USE_ROCM
#include "../gpu/api/amd/rocprofiler-api.h"
#endif

#ifdef USE_LEVEL0
#include "../gpu/api/intel/level0/level0-api.h"
#endif

#ifdef ENABLE_OPENCL
#include "../gpu/api/opencl/opencl-api-wrappers.h"
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
  if (strcmp(name, "libc_start_main") == 0)
    return foilbase_libc_start_main;

  // Process control
  if (strcmp(name, "fork") == 0)
    return foilbase_fork;
  if (strcmp(name, "vfork") == 0)
    return foilbase_vfork;
  if (strcmp(name, "execv") == 0)
    return foilbase_execv;
  if (strcmp(name, "execvp") == 0)
    return foilbase_execvp;
  if (strcmp(name, "execve") == 0)
    return foilbase_execve;
  if (strcmp(name, "system") == 0)
    return foilbase_system;
  if (strcmp(name, "exit") == 0)
    return foilbase_exit;
  if (strcmp(name, "_exit") == 0)
    return foilbase__exit;
  if (strcmp(name, "_Exit") == 0)
    return foilbase__Exit;

  // Signals
  if (strcmp(name, "sigaction") == 0)
    return foilbase_sigaction;
  if (strcmp(name, "signal") == 0)
    return foilbase_signal;
  if (strcmp(name, "sigprocmask") == 0)
    return foilbase_sigprocmask;
  if (strcmp(name, "sigwait") == 0)
    return foilbase_sigwait;
  if (strcmp(name, "sigwaitinfo") == 0)
    return foilbase_sigwaitinfo;
  if (strcmp(name, "sigtimedwait") == 0)
    return foilbase_sigtimedwait;

  // Threads
  if (strcmp(name, "pthread_create") == 0)
    return foilbase_pthread_create;
  if (strcmp(name, "pthread_exit") == 0)
    return foilbase_pthread_exit;
  if (strcmp(name, "pthread_sigmask") == 0)
    return foilbase_pthread_sigmask;

  // MPI
  if (strcmp(name, "MPI_Comm_rank") == 0)
    return foilbase_MPI_Comm_rank;
  if (strcmp(name, "mpi_comm_rank") == 0)
    return foilbase_mpi_comm_rank;
  if (strcmp(name, "mpi_comm_rank_") == 0)
    return foilbase_mpi_comm_rank_;
  if (strcmp(name, "mpi_comm_rank__") == 0)
    return foilbase_mpi_comm_rank__;
  if (strcmp(name, "MPI_Finalize") == 0)
    return foilbase_MPI_Finalize;
  if (strcmp(name, "mpi_finalize") == 0)
    return foilbase_mpi_finalize;
  if (strcmp(name, "mpi_finalize_") == 0)
    return foilbase_mpi_finalize_;
  if (strcmp(name, "mpi_finalize__") == 0)
    return foilbase_mpi_finalize__;
  if (strcmp(name, "MPI_Init") == 0)
    return foilbase_MPI_Init;
  if (strcmp(name, "mpi_init") == 0)
    return foilbase_mpi_init;
  if (strcmp(name, "mpi_init_") == 0)
    return foilbase_mpi_init_;
  if (strcmp(name, "mpi_init__") == 0)
    return foilbase_mpi_init__;
  if (strcmp(name, "MPI_Init_thread") == 0)
    return foilbase_MPI_Init_thread;
  if (strcmp(name, "mpi_init_thread") == 0)
    return foilbase_mpi_init_thread;
  if (strcmp(name, "mpi_init_thread_") == 0)
    return foilbase_mpi_init_thread_;
  if (strcmp(name, "mpi_init_thread__") == 0)
    return foilbase_mpi_init_thread__;
  if (strcmp(name, "PMPI_Init") == 0)
    return foilbase_PMPI_Init;
  if (strcmp(name, "pmpi_init") == 0)
    return foilbase_pmpi_init;
  if (strcmp(name, "pmpi_init_") == 0)
    return foilbase_pmpi_init_;
  if (strcmp(name, "pmpi_init__") == 0)
    return foilbase_pmpi_init__;
  if (strcmp(name, "PMPI_Init_thread") == 0)
    return foilbase_PMPI_Init_thread;
  if (strcmp(name, "pmpi_init_thread") == 0)
    return foilbase_pmpi_init_thread;
  if (strcmp(name, "pmpi_init_thread_") == 0)
    return foilbase_pmpi_init_thread_;
  if (strcmp(name, "pmpi_init_thread__") == 0)
    return foilbase_pmpi_init_thread__;
  if (strcmp(name, "PMPI_Finalize") == 0)
    return foilbase_PMPI_Finalize;
  if (strcmp(name, "pmpi_finalize") == 0)
    return foilbase_pmpi_finalize;
  if (strcmp(name, "pmpi_finalize_") == 0)
    return foilbase_pmpi_finalize_;
  if (strcmp(name, "pmpi_finalize__") == 0)
    return foilbase_pmpi_finalize__;
  if (strcmp(name, "PMPI_Comm_rank") == 0)
    return foilbase_PMPI_Comm_rank;
  if (strcmp(name, "pmpi_comm_rank") == 0)
    return foilbase_pmpi_comm_rank;
  if (strcmp(name, "pmpi_comm_rank_") == 0)
    return foilbase_pmpi_comm_rank_;
  if (strcmp(name, "pmpi_comm_rank__") == 0)
    return foilbase_pmpi_comm_rank__;

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
