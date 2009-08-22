// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <signal.h>

#include "monitor.h"

#include "thread_data.h"
#include "handling_sample.h"

#include "memchk.h"

#include <messages/messages.h>

// FIXME: tallent: should this be together with csprof_drop_sample?

/* catch SIGSEGVs */

int segv_count = 0;
extern int csprof_sample;

int
hpcrun_sigsegv_handler(int sig, siginfo_t* siginfo, void* context)
{
  if (csprof_is_handling_sample()) {
    segv_count++;

    thread_data_t *td = csprof_get_thread_data();

    // -----------------------------------------------------
    // print context
    // -----------------------------------------------------
    csprof_state_t* state = td->state;
    void* unw_pc = (state) ? state->unwind_pc : NULL;
    void* ctxt_pc = context_pc(context);
    PMSG_LIMIT(EMSG("error: segv: context-pc=%p, unwind-pc=%p", ctxt_pc, unw_pc));
    // TODO: print more context details

    // -----------------------------------------------------
    // longjump
    // -----------------------------------------------------
    sigjmp_buf_t *it = &(td->bad_unwind);
    if (memchk((void *)it, '\0', sizeof(*it))) {
      EMSG("error: segv handler: invalid jmpbuf");
      return 0; // monitor_real_abort();
    }
    
    siglongjmp(it->jb,9);
  }
  else {
    // pass segv to another handler
    return 1;
  }
}


int
setup_segv(void)
{
  int ret = monitor_sigaction(SIGSEGV, &hpcrun_sigsegv_handler, 0, NULL);

  if(ret != 0) {
    EMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
  return ret;
}
