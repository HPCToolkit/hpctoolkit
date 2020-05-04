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
// Copyright ((c)) 2002-2020, Rice University
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

//------------------------------------------------------------------------------
// File: poll.c 
//  
// Purpose: 
//   wrapper for libc poll. if poll returns a -1 because it was interrupted, 
//   assume the interrupt was from asynchronous sampling caused by hpcrun and 
//   restart. 
//------------------------------------------------------------------------------


//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE
#include <poll.h>
#include <dlfcn.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef int (*poll_fn)(struct pollfd *fds, nfds_t nfds, int timeout);



//******************************************************************************
// local data
//******************************************************************************

static poll_fn real_poll;



//******************************************************************************
// local operations
//******************************************************************************

static void 
find_poll
(
  void
)
{
  real_poll = dlsym(RTLD_NEXT, "poll");
  assert(real_poll);
}



//******************************************************************************
// interface operations
//******************************************************************************

int 
poll
(
  struct pollfd *fds, 
  nfds_t nfds, 
  int timeout
)
{
 static pthread_once_t initialized;
  pthread_once(&initialized, find_poll);

  int retval;
  int incoming_errno = errno; // save incoming errno

  for(;;) {
    // call the libc poll operation
    retval = real_poll(fds, nfds, timeout);

    if (retval == -1) {
      if (errno == EINTR) {
        // restart on EINTR
        errno = incoming_errno; // restore incoming errno
        continue;
      }
    }

    // return otherwise
    break;
  }

  return retval;
}
