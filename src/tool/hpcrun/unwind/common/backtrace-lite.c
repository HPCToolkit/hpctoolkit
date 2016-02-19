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
// system include files
//***************************************************************************

#include <stdbool.h>
#include <assert.h>

#include <sys/types.h>
#include <ucontext.h>


//***************************************************************************
// local include files
//***************************************************************************

#include "unwind.h"

#include "backtrace.h"
#include "epoch.h"
#include "monitor.h"
#include "sample_event.h"

#include <messages/messages.h>

//***************************************************************************
// 
//***************************************************************************


#if (HPC_UNW_LITE)
static int 
hpcrun_backtrace_lite(void** buffer, int size, ucontext_t* context)
{
  // special trivial case: size == 0 (N.B.: permit size < 0)
  if (size <= 0) {
    return 0;
  }

  // INVARIANT: 'size' > 0; 'buffer' is non-empty; 'context' is non-NULL
  if ( !(size > 0 && buffer && context) ) {
    return -1; // error
  }

  hpcrun_unw_init();

  hpcrun_unw_cursor_t cursor;
  hpcrun_unw_init_cursor(&cursor, context);

  int my_size = 0;
  while (my_size < size) {
    int ret;

    unw_word_t ip = 0;
    ret = hpcrun_unw_get_ip_reg(&cursor, &ip);
    if (ret < 0) { /* ignore error */ }

    buffer[my_size] = ip; // my_size < size
    my_size++;

    ret = hpcrun_unw_step(&cursor);
    if (ret <= 0) {
      // N.B. (ret < 0) indicates an unwind error, which we ignore
      break;
    }
  }
  
  return my_size;
}
#endif


#if (HPC_UNW_LITE)
static int
test_backtrace_lite(ucontext_t* context)
{
  const int bufsz = 100;

  void* buffer[bufsz];
  int sz = hpcrun_backtrace_lite(buffer, bufsz, context);

  for (int i = 0; i < sz; ++i) {
    TMSG(UNW, "backtrace_lite: pc=%p", buffer[i]);
  }

  return sz;
}
#endif


//***************************************************************************
// 
//***************************************************************************
