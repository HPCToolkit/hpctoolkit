/* -*-Mode: C++;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Implements monitoring for any application (including statically
//    linked), launched by linking this library into one's
//    application.  The library starts and stops monitoring.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#define USE_STATIC_CONSTRUCTOR 1

/************************** System Include Files ****************************/

#include <stdlib.h>
#include <errno.h>
/* #include <pthread.h> */

#include <dlfcn.h>

/**************************** User Include Files ****************************/

#include "monitor.h"
#include "hpcrun.h"
#include "rtmap.h"

/**************************** Forward Declarations **************************/

/* 
   For profiling of statically linked apps:
  
     - User must relink application, placing libhpcrun-static.o after
       all user code libraries/objects but before either libpthread or
       libc.

     - We can launch the application with hpcrun.  This will set the
       environment (and also set the unnecessary LD_* vars, but who
       cares?)
     - We will have to change hpcrun to be linked with papi.  It will
       not work to be dynamically link on a statically linked platform.

     - We can initialize profiling with a C++ static constructor
     - We should also be able to 'intercept' __libc_start_main

     - Similarly, we should be able to intercept thread creation,
       forks, etc:
       - use special bogus names for the definition of the routines we
         want to intercept, but use the real names when calling the
         intercepted routines
       - link in the real routines: pre-link our lib against libc or
         libpthreads
       - hide the real routines
       - rename our entry points to the real names *so they are found
         on the link line)	 
*/


/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

#if USE_STATIC_CONSTRUCTOR

/*
 *  C++ Static constructor/deconstructor: (FIXME)
 */
class Launcher {
public:
  Launcher()  { 
    /* initialize */ 
    init_library();
    init_process();
  }
  ~Launcher() { 
    /* finalize */ 
    fini_process();
    fini_library(); 
  }
};

static Launcher mylauncher;

#endif


/*
 *  Library initialization
 */
extern void 
init_library_SPECIALIZED()
{
#if USE_STATIC_CONSTRUCTOR
  hpcrun_cmd = hpcrun_get_cmd(opt_debug);

  if (opt_debug >= 1) {
    MSG(stderr, "*** launching intercepted app: %s ***", hpcrun_cmd);
  }
#endif
}


/****************************************************************************
 * Intercepted routines 
 ****************************************************************************/

#if !USE_STATIC_CONSTRUCTOR

// FIXME: do not need real_libc_start_main as extern

// __libc_start_main __BP___libc_start_main 
// execv execvp execve
// pthread_create pthread_self

/*
 *  Intercept the start routine: this is from glibc and can be one of
 *  two different names depending on how the macro BP_SYM is defined.
 *    glibc-x/sysdeps/generic/libc-start.c
 */
extern int 
HPCRUNSTINTERCEPT__libc_start_main PARAMS_START_MAIN
{
  hpcrun_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


extern int 
HPCRUNSTINTERCEPT__BP___libc_start_main PARAMS_START_MAIN
{
  hpcrun_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


static int 
hpcrun_libc_start_main PARAMS_START_MAIN
{
  MSG(stderr, "*** static: libc_start_main ***");
  setenv("HPCRUN_DEBUG", "1", 1);
      
  /* squirrel away for later use */
  hpcrun_cmd = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  real_start_main_fini = fini;

  MSG(stderr, "*** static: A ***");
  
  /* initialize profiling for process */
  init_library();
  init_process();

  MSG(stderr, "*** static: C ***");
  
  /* launch the process */
  if (opt_debug >= 1) {
    MSG(stderr, "*** launching intercepted app: %s ***", hpcrun_cmd);
  }

  __libc_start_main(main, argc, ubp_av, init, hpcrun_libc_start_main_fini, 
		    rtld_fini, stack_end);
  return 0; /* never reached */
}


static void 
hpcrun_libc_start_main_fini()
{
  if (real_start_main_fini) {
    (*real_start_main_fini)();
  }
  fini_process();
  fini_library();
  exit(0);
}

#endif

/****************************************************************************
 * Initialize profiling 
 ****************************************************************************/

extern void 
init_papi_for_process_SPECIALIZED()
{
  if (opt_thread) {
    MSG(stderr, "FIXME: init_papi_for_process_SPECIALIZED.");
  }
}

/****************************************************************************/

extern long
hpcrun_gettid_SPECIALIZED()
{
  return 0; // FIXME
}

/****************************************************************************/
