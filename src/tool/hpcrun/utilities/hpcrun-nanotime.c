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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <assert.h>
#include <errno.h>
#include <time.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "hpcrun-nanotime.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define NS_PER_SEC 1000000000


//*****************************************************************************
//*****************************************************************************
// interface operations
//*****************************************************************************

uint64_t
hpcrun_nanotime()
{
  struct timespec now;

  int res = clock_gettime(CLOCK_REALTIME, &now);

  assert(res == 0);

  uint64_t now_sec = now.tv_sec;
  uint64_t now_ns = now_sec * NS_PER_SEC + now.tv_nsec;

  return now_ns;
}


int32_t
hpcrun_nanosleep
(
  uint32_t nsec
)
{
  struct timespec time_wait = {.tv_sec=0, .tv_nsec=nsec};
  struct timespec time_rem = {.tv_sec=0, .tv_nsec=0};
  int32_t ret;

  for(;;){
    ret = nanosleep(&time_wait, &time_rem);
    if (! (ret < 0 && errno == EINTR)){
      // normal non-signal return
      break;
    }
  time_wait = time_rem;
  }

  return ret;
}
