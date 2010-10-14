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

void
hpcrun_unw_throw(void)
{
  if (DEBUG_NO_LONGJMP) return;

  if (hpcrun_below_pmsg_threshold()) {
    hpcrun_bt_dump(TD_GET(btbuf_cur), "DROP");
  }

  hpcrun_up_pmsg_count();

  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}
