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
// Copyright ((c)) 2002-2019, Rice University
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


//***************************************************************************
// libmonitor include files
//***************************************************************************

#include <monitor.h>


//***************************************************************************
// user include files 
//***************************************************************************

#include "term_handler.h"
#include <messages/messages.h>
#include <utilities/arch/context-pc.h>


//***************************************************************************
// catch SIGTERM
//***************************************************************************

static int
hpcrun_term_handler(int sig, siginfo_t* siginfo, void* context)
{
   void* pc = hpcrun_context_pc(context);
   EEMSG("hpcrun_term_handler: aborting execution on SIGTERM,"
	 " context pc = %p\n", pc);
   monitor_real_abort();
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
