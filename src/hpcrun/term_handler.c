// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
// system include files
//***************************************************************************

#define _GNU_SOURCE

#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>


//***************************************************************************
// libmonitor include files
//***************************************************************************

#include "libmonitor/monitor.h"


//***************************************************************************
// user include files
//***************************************************************************

#include "term_handler.h"
#include "messages/messages.h"
#include "utilities/arch/context-pc.h"


//***************************************************************************
// catch SIGTERM
//***************************************************************************

static int
hpcrun_term_handler(int sig, siginfo_t* siginfo, void* context)
{
   EEMSG("hpcrun_term_handler: aborting execution on SIGTERM");
   hpcrun_terminate();
   return 0;
}


int
hpcrun_setup_term()
{
  int ret = monitor_sigaction(SIGTERM, &hpcrun_term_handler, 0, NULL);
  if (ret != 0) {
    EMSG("hpcrun_setup_term: unable to install SIGTERM handler",
         __FILE__, __LINE__);
  }

  return ret;
}
