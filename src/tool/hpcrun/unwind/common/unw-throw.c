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

// Purpose:
//   Common code for abandoning an unwind via a longjump to the (thread-local) jmpbuf

//*************************************************************************
//   System Includes
//*************************************************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <setjmp.h>


//*************************************************************************
//   Local Includes
//*************************************************************************
#include <messages/messages.h>
#include <unwind/common/backtrace.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/main.h>

#include "unw-throw.h"

//*************************************************************************
//   Interface functions
//*************************************************************************

//****************************************************************************
//   Local data 
//****************************************************************************

static int DEBUG_NO_LONGJMP = 0;

//
// Actually drop a sample, as opposed to recording a partial unwind
//
void
hpcrun_unw_drop(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  td->btbuf_cur = td->btbuf_beg; // flush any collected backtrace frames

  sigjmp_buf_t *it = &(td->bad_unwind);
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}

void
hpcrun_unw_throw(void)
{
  if (DEBUG_NO_LONGJMP) return;

  if (hpcrun_below_pmsg_threshold()) {
    hpcrun_bt_dump(TD_GET(btbuf_cur), "PARTIAL");
  }

  hpcrun_up_pmsg_count();

  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}
