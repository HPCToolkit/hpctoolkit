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
// File: select.c 
//  
// Purpose: 
//   wrapper for libc select. if select returns a -1 because it was
//   interrupted, assume the interrupt was from asynchronous sampling
//   caused by hpcrun and restart. 
//------------------------------------------------------------------------------


//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/select.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef int (*select_fn)
(
 int nfds,
 fd_set *read_fds,
 fd_set *write_fds,
 fd_set *except_fds, 
 struct timeval *timeout
);



//******************************************************************************
// local data
//******************************************************************************

static select_fn real_select;



//******************************************************************************
// local operations
//******************************************************************************

static void 
find_select
(
  void
)
{
  real_select = dlsym(RTLD_NEXT, "select");
  assert(real_select);
}



//******************************************************************************
// interface operations
//******************************************************************************

int 
select
(
  int nfds, 
  fd_set *read_fds, 
  fd_set *write_fds, 
  fd_set *except_fds, 
  struct timeval *timeout
)
{
  static pthread_once_t initialized;
  pthread_once(&initialized, find_select);

  int retval;

  int incoming_errno = errno; // save incoming errno

  for(;;) {
    retval = real_select(nfds, read_fds, write_fds, except_fds, timeout);
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
