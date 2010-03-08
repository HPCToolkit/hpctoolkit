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
// Copyright ((c)) 2002-2010, Rice University 
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

#include <signal.h>
#include <sys/signal.h>         /* sigaction() */
#include <sys/time.h>           /* setitimer() */

#include "msg.h"

static struct itimerval itimer;

static 

static void set_timer(void){
  setitimer(ITIMER_PROF, &itimer,NULL);
}

static void itimer_signal_handler(int sig, siginfo_t *siginfo, void *context){
  MSG("Got itimer signal!!");
  set_timer();
}

void init_signal_handler(void){
  struct sigaction sa;
  int ret;

  MSG("Got to init signal handler");

  sa.sa_sigaction = itimer_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  ret = sigaction(SIGPROF, &sa, 0);
  return ret;
}

void hpc_init_(void){
  // extern void csprof_init_internal(void);

  //  csprof_init_internal();
  init_timer_val();
  setup_timer_signal();
  set_timer();
}
