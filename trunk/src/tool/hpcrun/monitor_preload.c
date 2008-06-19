/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Implements monitoring that is launched with a preloaded library,
//    something that only works for dynamically linked applications.
//    The library intercepts the beginning execution point of the
//    process and then starts monitoring. When the process exits,
//    control will be transferred back to this library where profiling
//    is finalized.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

/************************** System Include Files ****************************/

#include <stdlib.h>
#include <stdarg.h>   /* va_arg */
#include <string.h>   /* memset */
#include <errno.h>
#include <pthread.h>

#ifndef __USE_GNU
# define __USE_GNU /* must define on Linux to get RTLD_NEXT from <dlfcn.h> */
# define SELF_DEFINED__USE_GNU
#endif

#include <dlfcn.h>

#undef __USE_GNU

/**************************** User Include Files ****************************/

#include "monitor.h"
#include "hpcrun.h"


/**************************** Forward Declarations **************************/

/****************************************************************************/


void 
monitor_init_library()
{
  init_library();
  /* process initialized with interception of libc_start_main */
}


void 
monitor_fini_library()
{
  /* process finalized with libc_start_main */
  fini_library();
}


void 
monitor_init_process(const char* name, int* argc, char** argv, 
		     const unsigned pid)
{
  hpcrun_cmd = name;  /* command is also in /proc/pid/cmdline */
  if (opt_debug >= 1) {
    fprintf(stderr, "Init process callback from monitor received\n");
  }
  init_process();
}


void
monitor_fini_process()
{
  if (opt_debug >= 1) {
    fprintf(stderr, "Fini process callback from monitor received\n");
  }
  fini_process();
}


void 
monitor_init_thread_support(void)
{
  int rval;
  if ((rval = PAPI_thread_init(pthread_self)) != PAPI_OK) {
    DIEx("error: PAPI error (%d): %s.", rval, PAPI_strerror(rval));
  }
}


void* 
monitor_init_thread(unsigned tid)
{
  if (opt_debug >= 1) {
    fprintf(stderr, "init_thread(TID=0x%lx) callback from monitor received\n", tid);
  }
  return((void *)init_thread(1));
}


void 
monitor_fini_thread(void* data)
{
  if (opt_debug >= 1) {
    fprintf(stderr, "fini_thread(TID=0x%lx) callback from monitor received\n", (long)pthread_self());
  }
  fini_thread((hpcrun_profiles_desc_t **)(&data), 1 /*is_thread*/);
}


void 
monitor_dlopen(char* lib)
{
  if (opt_debug >= 1) {
    fprintf(stderr, "dlopen(%s) callback from monitor received\n", lib); 
  }
  /* update profile tables */
  //handle_dlopen();
}

/****************************************************************************/
