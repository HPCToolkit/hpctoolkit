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

static void hpcrun_handle_any_dlerror();

/**************************** Forward Declarations **************************/

/* libc intercepted routines */
static int  hpcrun_libc_start_main PARAMS_START_MAIN;
static void hpcrun_libc_start_main_fini(); 

static int  hpcrun_execv  PARAMS_EXECV;
static int  hpcrun_execvp PARAMS_EXECVP;
static int  hpcrun_execve PARAMS_EXECVE;

static pid_t hpcrun_fork();

/* libpthread intercepted routines */
static int hpcrun_pthread_create PARAMS_PTHREAD_CREATE;
static void* hpcrun_pthread_create_start_routine(void* arg);
static void  hpcrun_pthread_cleanup_routine(void* arg);

/*************************** Variable Declarations **************************/

/* Intercepted libc and libpthread routines: set when the library is
   initialized */
static libc_start_main_fptr_t      real_start_main;
static libc_start_main_fini_fptr_t real_start_main_fini;
static execv_fptr_t                real_execv;
static execvp_fptr_t               real_execvp;
static execve_fptr_t               real_execve;
static fork_fptr_t                 real_fork;
static pthread_create_fptr_t       real_pthread_create;
static pthread_self_fptr_t         real_pthread_self;

/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

/*
 *  Library constructor/deconstructor
 */
extern void 
_init()
{
  init_library();
  /* process initialized with interception of libc_start_main */
}

extern void
_fini()
{
  /* process finalized with libc_start_main */
  fini_library();
}


/*
 *  Library initialization
 */
extern void 
init_library_SPECIALIZED()
{
  /* Grab pointers to functions that may be intercepted.

     Note: RTLD_NEXT is not documented in the dlsym man/info page
     but in <dlfcn.h>: 
  
     If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
     the run-time address of the symbol called NAME in the next shared
     object is returned.  The "next" relation is defined by the order
     the shared objects were loaded.  
  */

  /* ----------------------------------------------------- 
   * libc interceptions
   * ----------------------------------------------------- */
  real_start_main = 
    (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main");
  hpcrun_handle_any_dlerror();
  if (!real_start_main) {
    real_start_main = 
      (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__BP___libc_start_main");
    hpcrun_handle_any_dlerror();
  }
  if (!real_start_main) {
    DIE("fatal error: Cannot intercept beginning of process execution and therefore cannot begin profiling.");
  }

  
  real_execv = (execv_fptr_t)dlsym(RTLD_NEXT, "execv");  
  hpcrun_handle_any_dlerror();

  real_execvp = (execvp_fptr_t)dlsym(RTLD_NEXT, "execvp");  
  hpcrun_handle_any_dlerror();
  
  real_execve = (execve_fptr_t)dlsym(RTLD_NEXT, "execve");
  hpcrun_handle_any_dlerror();
  
  
  real_fork = (fork_fptr_t)dlsym(RTLD_NEXT, "fork");
  hpcrun_handle_any_dlerror();
  
  /* ----------------------------------------------------- 
   * libpthread interceptions
   * ----------------------------------------------------- */
  
  /* Note: PAPI only supports profiling on a per-thread basis */
  if (opt_thread != HPCRUN_THREADPROF_NO) {
    /* FIXME: I suppose it would be possible for someone to dlopen
       libpthread which means it would not be visible now. Perhaps we
       should do a dlsym on libpthread instead. */
    real_pthread_create = 
      (pthread_create_fptr_t)dlsym(RTLD_NEXT, "pthread_create");
    hpcrun_handle_any_dlerror();
    if (!real_pthread_create) {
      DIE("fatal error: Cannot intercept POSIX thread creation and therefore cannot profile threads.");
    }
    
    real_pthread_self = dlsym(RTLD_NEXT, "pthread_self");
    hpcrun_handle_any_dlerror();
    if (!real_pthread_self) {
      DIE("fatal error: Cannot intercept POSIX thread id routine and therefore cannot profile threads.");
    }
  }
}




/****************************************************************************
 * Intercepted routines 
 ****************************************************************************/

/*
 *  Intercept the start routine: this is from glibc and can be one of
 *  two different names depending on how the macro BP_SYM is defined.
 *    glibc-x/sysdeps/generic/libc-start.c
 */
extern int 
__libc_start_main PARAMS_START_MAIN
{
  hpcrun_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


extern int 
__BP___libc_start_main PARAMS_START_MAIN
{
  hpcrun_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


static int 
hpcrun_libc_start_main PARAMS_START_MAIN
{
  /* squirrel away for later use */
  hpcrun_cmd = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  real_start_main_fini = fini;
  
  /* initialize profiling for process */
  init_process();
  
  /* launch the process */
  if (opt_debug >= 1) {
    MSG(stderr, "*** launching intercepted app: %s ***", hpcrun_cmd);
  }
  (*real_start_main)(main, argc, ubp_av, init, hpcrun_libc_start_main_fini, 
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
  exit(0);
}



/*
 *  Intercept creation of exec() family of routines because profiling
 *  *must* be off when the exec takes place or it interferes with
 *  ld.so (and the latter will not preload our monitoring library).
 *  Moreover, this allows us to safely write any profile data to disk.
 *
 *  execl, execlp, execle / execv, execvp, execve  
 *
 *  Note: We cannot simply intercept libc'c __execve (which implements
 *  all other routines on Linux) because it is a libc-local symbol.
 *    glibc-x/sysdeps/generic/execve.c
 * 
 *  Note: Stupid C has no way of passing along the input vararg list
 *  intact.
 */

extern int 
execl(const char *path, const char *arg, ...)
{
  const char** argv = NULL;
  va_list arglist;

  va_start(arglist, arg);
  hpcrun_parse_execl(&argv, NULL, arg, arglist);
  va_end(arglist);
  
  return hpcrun_execv(path, (char* const*)argv);
}


extern int 
execlp(const char *file, const char *arg, ...)
{
  const char** argv = NULL;
  va_list arglist;
  
  va_start(arglist, arg);
  hpcrun_parse_execl(&argv, NULL, arg, arglist);
  va_end(arglist);
  
  return hpcrun_execvp(file, (char* const*)argv);
}


extern int 
execle(const char *path, const char *arg, ...)
{
  const char** argv = NULL;
  const char* const* envp = NULL;
  va_list arglist;

  va_start(arglist, arg);
  hpcrun_parse_execl(&argv, &envp, arg, arglist);
  va_end(arglist);
  
  return hpcrun_execve(path, (char* const*)argv, (char* const*)envp);
}


extern int 
execv PARAMS_EXECV
{
  return hpcrun_execv(path, argv);
}


extern int 
execvp PARAMS_EXECVP
{
  return hpcrun_execvp(file, argv);
}


extern int 
execve PARAMS_EXECVE
{
  return hpcrun_execve(path, argv, envp);
}


static int
hpcrun_execv PARAMS_EXECV
{
  if (opt_debug >= 1) { MSG(stderr, "==> execv-ing <=="); }
  fini_process();
  return (*real_execv)(path, argv);
}


static int
hpcrun_execvp PARAMS_EXECVP
{
  if (opt_debug >= 1) { MSG(stderr, "==> execvp-ing <=="); }
  fini_process();
  return (*real_execvp)(file, argv);
}


static int
hpcrun_execve PARAMS_EXECVE
{
  if (opt_debug >= 1) { MSG(stderr, "==> execve-ing <=="); }
  fini_process();
  return (*real_execve)(path, argv, envp);
}



/*
 *  Intercept creation of new processes via fork(), vfork()
 */

extern pid_t 
fork()
{
  return hpcrun_fork();
}


extern pid_t 
vfork()
{
  return hpcrun_fork();
}


static pid_t 
hpcrun_fork()
{
  pid_t pid;

  if (opt_debug >= 1) { MSG(stderr, "==> forking <=="); }
  
  pid = (*real_fork)();
  if (pid == 0) { 
    /* Initialize profiling for child process */
    if (opt_debug >= 1) { MSG(stderr, "==> caught fork <=="); }
    init_process();
  } 
  /* Nothing to do for parent process */
  
  return pid;
}


/*
 *  Intercept creation of new theads via pthread_create()
 */

extern int 
pthread_create PARAMS_PTHREAD_CREATE
{
  return hpcrun_pthread_create(thread, attr, start_routine, arg);
}


static int 
hpcrun_pthread_create PARAMS_PTHREAD_CREATE
{
  hpcrun_pthread_create_args_t* hpcargs;
  int rval, sz;

  if (opt_debug >= 1) { MSG(stderr, "==> creating thread <=="); }
  
  if (!real_pthread_create) {
    DIE("fatal error: Cannot intercept POSIX thread creation.  Please use the -t option to profile threaded applications.");
  }
  
  /* squirrel away original arguments */
  sz = sizeof(hpcrun_pthread_create_args_t);
  hpcargs = (hpcrun_pthread_create_args_t*)malloc(sz);
  if (!hpcargs) { DIE("fatal error: malloc() failed!"); }
  memset(hpcargs, 0x0, sizeof(hpcrun_pthread_create_args_t));
  hpcargs->start_routine = start_routine;
  hpcargs->arg = arg;
  
  /* create the thread using our own start routine */
  rval = real_pthread_create(thread, attr, hpcrun_pthread_create_start_routine,
			     (void*)hpcargs);
  return rval;
}


static void*
hpcrun_pthread_create_start_routine(void* arg)
{
  hpcrun_pthread_create_args_t* hpcargs = (hpcrun_pthread_create_args_t*)arg;
  void* rval;
  
  if (opt_debug >= 1) { MSG(stderr, "==> caught thread <=="); }
  
  pthread_cleanup_push(hpcrun_pthread_cleanup_routine, hpcargs);
  hpcargs->profdesc = init_thread(1 /*is_thread*/);
  rval = (hpcargs->start_routine)(hpcargs->arg);
  pthread_cleanup_pop(1);
  return rval;
}


static void  
hpcrun_pthread_cleanup_routine(void* arg)
{
  hpcrun_pthread_create_args_t* hpcargs = (hpcrun_pthread_create_args_t*)arg;
  hpcrun_profiles_desc_t* profdesc = hpcargs->profdesc;
  
  free(hpcargs); /* from hpcrun_pthread_create() */
  fini_thread(&profdesc, 1 /*is_thread*/);
}

/****************************************************************************
 * Initialize profiling 
 ****************************************************************************/

extern void 
init_papi_for_process_SPECIALIZED()
{
  int rval;

  if (opt_thread) {
    if ((rval = PAPI_thread_init(real_pthread_self)) != PAPI_OK) {
      DIE("fatal error: PAPI error (%d): %s.", rval, PAPI_strerror(rval));
    }
  }
}

/****************************************************************************/

extern long
hpcrun_gettid_SPECIALIZED()
{
  /* We only support POSIX threads right now */
  if (real_pthread_self) {
    return (long)real_pthread_self();
  }
  else {
    return 0;
  }
}

static void
hpcrun_handle_any_dlerror()
{
  /* Note: We assume dlsym() or something similar has just been called! */
  char *error;
  if ((error = dlerror()) != NULL) {
    DIE("fatal error: %s\n", error);
  }
}

/****************************************************************************/
