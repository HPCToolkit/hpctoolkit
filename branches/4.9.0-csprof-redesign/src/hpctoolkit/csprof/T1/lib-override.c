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
