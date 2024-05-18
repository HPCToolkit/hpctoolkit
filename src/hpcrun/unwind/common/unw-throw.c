// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

// Purpose:
//   Common code for abandoning an unwind via a longjump to the (thread-local) jmpbuf

//*************************************************************************
//   System Includes
//*************************************************************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <setjmp.h>


//*************************************************************************
//   Local Includes
//*************************************************************************
#include "../../messages/messages.h"
#include "backtrace.h"
#include "../../thread_data.h"
#include "../../main.h"

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
  siglongjmp(it->jb, 9);
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
  siglongjmp(it->jb, 9);
}
