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

#include <dlfcn.h>
#include <stdio.h>

#include "sighandlers.h"
#include "timers.h"

/* problem ?? */

#ifndef RTLD_NEXT
#define RTLD_NEXT -1
#endif

#include "msg.h"

#define PARAMS_START_MAIN (int (*main) (int, char **, char **),              \
			   int argc,                                         \
			   char *__unbounded *__unbounded ubp_av,            \
			   void (*init) (void),                              \
                           void (*fini) (void),                              \
			   void (*rtld_fini) (void),                         \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) PARAMS_START_MAIN;

static libc_start_main_fptr_t real_start_main;

void get_real_addrs(void){
  MSG("getting real start_main address\n");
  real_start_main = 
    (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main");
  if (!real_start_main) {
    real_start_main = 
      (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__BP___libc_start_main");
  }
  if (!real_start_main) {
    MSG("!! Failed to grab libc_start_main !!\n");
  }
}

void _init(void){
  get_real_addrs();
}

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  extern void unwind_n(int n);

  install_sampling_handler();
  install_segv_handler();
  MSG("calling limited unwind\n");
  unwind_n(3);
  setup_timer();
  (*the_main)(n,argv,env);
}

extern int my_libc_start_main PARAMS_START_MAIN {
  int retv;

  MSG("Sucessfully hooked libc_start_main");
  the_main = main;
  retv = (*real_start_main)(faux_main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  S: return retv;
}

extern int __libc_start_main PARAMS_START_MAIN {
  my_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}
