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
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define __USE_XOPEN_EXTENDED // for gethostid()
#include <unistd.h>

//*************************** User Include Files ****************************

#include "OSUtil.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

uint
OSUtil_pid()
{
  pid_t pid = getpid();
  return (uint)pid;
}


const char*
OSUtil_jobid()
{
  char* jid = NULL;

  // Cobalt
  jid = getenv("COBALT_JOBID");
  if (jid) {
    return jid;
  }

  // PBS
  jid = getenv("PBS_JOBID");
  if (jid) {
    return jid;
  }

  // SLURM
  jid = getenv("SLURM_JOB_ID");
  if (jid) {
    return jid;
  }

  // Sun Grid Engine
  jid = getenv("JOB_ID");
  if (jid) {
    return jid;
  }

  return jid;
}


// On early RH/CentOS 6.x systems, the statically linked gethostid()
// throws an FPE (floating point exception) and does not return.  So,
// wrap the function with sigsetjmp().  We cache the value, so the
// performance hit is negligible.

#define OSUtil_hostid_NULL (-1)
#define OSUtil_hostid_fail (0x0bad1d);

static sigjmp_buf hostid_jbuf;
static int hostid_jbuf_active;

static void
hostid_fpe_handler(int sig)
{
  if (hostid_jbuf_active) {
    siglongjmp(hostid_jbuf, 1);
  }

  // can't get here
  abort();
}

long
OSUtil_hostid()
{
  static long hostid = OSUtil_hostid_NULL;
  struct sigaction act, old_act;

  if (hostid == OSUtil_hostid_NULL) {
    memset(&act, 0, sizeof(act));
    memset(&old_act, 0, sizeof(old_act));
    act.sa_handler = hostid_fpe_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    hostid_jbuf_active = 0;
    sigaction(SIGFPE, &act, &old_act);

    if (sigsetjmp(hostid_jbuf, 1) == 0) {
      // normal return
      hostid_jbuf_active = 1;

      // gethostid returns long, but only 32 bits
      hostid = (uint32_t) gethostid();
    }
    else {
      // error return from signal handler
      hostid = OSUtil_hostid_fail;
    }
    hostid_jbuf_active = 0;

    // reset the handler so we don't get called again
    sigaction(SIGFPE, &old_act, NULL);
  }

  return hostid;
}
