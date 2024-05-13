// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   OS Utilities
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, John Mellor-Crummey, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <sys/types.h> // getpid()

#include <stdio.h> // snprintf
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define __USE_XOPEN_EXTENDED // for gethostid()
#include <unistd.h>

//*************************** User Include Files ****************************

#include "OSUtil.h"
#include "linux_info.h"
#include <stdatomic.h>

//*************************** macros **************************

#define KERNEL_NAME_FORMAT    "%s." HOSTID_FORMAT

//*************************** Forward Declarations **************************

//***************************************************************************
//
//***************************************************************************

struct envvar_rank_entry {
  const char* local;
  const char* global;
};


static const char* const envvars_jobid[] = {
  "LSB_JOBID",  // LSF
  "COBALT_JOBID",  // Cobalt
  "PBS_JOBID",  // PBS
  "SLURM_JOB_ID",  // SLURM
  "JOB_ID",  // Sun Grid Engine
  "FLUX_JOB_ID",  // Flux
  NULL
};


static const struct envvar_rank_entry envvars_rank[] = {
  {"PMI_LOCAL_RANK", "PMI_RANK"},  // PMI layer
  {"OMPI_COMM_WORLD_LOCAL_RANK", "OMPI_COMM_WORLD_RANK"},  // OpenMPI
  {"MPI_LOCALRANKID", NULL},  // MPICH
  {"SLURM_LOCALID", "SLURM_PROCID"},  // SLURM
  {"JSM_NAMESPACE_LOCAL_RANK", "JSM_NAMESPACE_RANK"},  // LSF
  {"PALS_LOCAL_RANKID", "PALS_RANKID"},  // PBS Pro
  {"FLUX_TASK_LOCAL_ID", "FLUX_TASK_RANK"},  // Flux
  {NULL, NULL}
};


unsigned int
OSUtil_pid()
{
  pid_t pid = getpid();
  return (unsigned int)pid;
}


const char*
OSUtil_jobid()
{
  for (const char* const* envvar = envvars_jobid; *envvar != NULL; ++envvar) {
    const char* jid = getenv(*envvar);
    if (jid != NULL)
      return jid;
  }
  return NULL;
}


const char*
OSUtil_local_rank()
{
  for (const struct envvar_rank_entry* envvar = envvars_rank;
       envvar->local != NULL || envvar->global != NULL; ++envvar) {
    if (envvar->local != NULL) {
      const char* rid = getenv(envvar->local);
      if (rid != NULL)
        return rid;
    }
  }
  return NULL;
}


static long long
parse_uint(const char* str)
{
  if(str == NULL || str[0] == '\0')
    return -1;

  char* end = NULL;
  long long result = strtoll(str, &end, 10);
  return *end == '\0' ? result : -1;
}


long long
OSUtil_rank()
{
  for (const struct envvar_rank_entry* envvar = envvars_rank;
       envvar->local != NULL || envvar->global != NULL; ++envvar) {
    if (envvar->global != NULL) {
      const char* rid = getenv(envvar->global);
      if (rid != NULL)
        return parse_uint(rid);
    }
  }
  return -1;
}

uint32_t
OSUtil_hostid()
{
  // NOTE: ATOMIC_VAR_INIT is going away in C2x (was depreciated in C17).
  // Default (zero) initialization is explicitly valid, so use that.
  static atomic_uint_fast32_t cache;

  uint_fast32_t result = atomic_load(&cache);
  if (result == 0) {
    // On many 64-bit systems `long == int64_t`, so the value out of gethostid()
    // may be sign-extended from the 32-bit hostid. This cast smashes out the
    // higher-order bits and makes it unsigned so it won't be sign-extended later.
    uint_fast32_t myresult = (uint32_t) gethostid();

    // We don't want to call gethostid() again in case it gives a different
    // result (happens sometimes on systems with bad network setups). So if it
    // gave 0, remap to something not 0. Like 1.
    if (myresult == 0) myresult = UINT32_C(1);

    // Only the cache value needs to be synchronized, relaxed mem order is fine.
    if(atomic_compare_exchange_strong(&cache, &result, myresult)) {
      result = myresult;
    }
  }
  // At this point, result is the final answer.
  return result;
}

int
OSUtil_setCustomKernelName(char *buffer, size_t max_chars)
{
  int n = snprintf(buffer, max_chars, KERNEL_NAME_FORMAT,
           LINUX_KERNEL_NAME_REAL, OSUtil_hostid());

  return n;
}


int
OSUtil_setCustomKernelNameWrap(char *buffer, size_t max_chars)
{
  int n = snprintf(buffer, max_chars, KERNEL_SYMBOLS_DIRECTORY "/" KERNEL_NAME_FORMAT,
           LINUX_KERNEL_NAME_REAL, OSUtil_hostid());

  return n;
}
