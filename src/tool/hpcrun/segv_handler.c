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

#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <signal.h>


//***************************************************************************
// libmonitor include files
//***************************************************************************

#include <monitor.h>


//***************************************************************************
// user include files 
//***************************************************************************

#include "main.h"
#include "thread_data.h"
#include "handling_sample.h"
#include "hpcrun_stats.h"

#include "memchk.h"

#include <messages/messages.h>
#include <utilities/arch/context-pc.h>


//***************************************************************************
// catch SIGSEGVs
//***************************************************************************

// FIXME: tallent: should this be together with hpcrun_drop_sample?

int
hpcrun_sigsegv_handler(int sig, siginfo_t* siginfo, void* context)
{
  if (hpcrun_is_handling_sample()) {
    hpcrun_stats_num_samples_segv_inc();

    thread_data_t *td = hpcrun_get_thread_data();

    // -----------------------------------------------------
    // print context
    // -----------------------------------------------------
    void* ctxt_pc = hpcrun_context_pc(context);
    if (ENABLED(UNW_SEGV_STOP)){
      EMSG("Unwind segv abort enabled ... Aborting!!, context pc = %p", ctxt_pc);
      monitor_real_abort();
    }

    EMSG("error: segv: context-pc=%p", ctxt_pc);
    // TODO: print more context details

    // -----------------------------------------------------
    // longjump, if possible
    // -----------------------------------------------------
    sigjmp_buf_t *it = &(td->bad_unwind);
    if (memchk((void *)it, '\0', sizeof(*it))) {
      EMSG("error: segv handler: invalid jmpbuf");
      // N.B. to handle this we need an 'outer' jump buffer that captures
      // the context right as we enter the sampling-trigger signal handler.
      monitor_real_abort();
    }

    hpcrun_bt_dump(td->btbuf_cur, "SEGV");

    (*hpcrun_get_real_siglongjmp())(it->jb, 9);
    return 0;
  }
  else {
    // pass segv to another handler
    TMSG(SEGV, "NON unwind segv encountered");
    return 1; // monitor_real_abort(); // TEST
  }
}


int
hpcrun_setup_segv()
{
  int ret = monitor_sigaction(SIGBUS, &hpcrun_sigsegv_handler, 0, NULL);
  if (ret != 0) {
    EMSG("Unable to install SIGBUS handler", __FILE__, __LINE__);
  }

  ret = monitor_sigaction(SIGSEGV, &hpcrun_sigsegv_handler, 0, NULL);
  if (ret != 0) {
    EMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }

  return ret;
}
