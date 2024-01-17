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
// Copyright ((c)) 2002-2023, Rice University
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
#include "../../include/linux_info.h"
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
  int n = snprintf(buffer, max_chars, "<" KERNEL_NAME_FORMAT ">",
           LINUX_KERNEL_NAME_REAL, OSUtil_hostid());

  return n;
}
