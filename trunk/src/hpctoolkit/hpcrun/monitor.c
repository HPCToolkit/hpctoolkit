 /* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Prepares and finalizes profiling for a process.  The library
//    intercepts the beginning execution point of the process,
//    determines the process' list of load modules (including DSOs)
//    and prepares PAPI_sprofil for profiling over each load module.
//    When the process exits, control will be transferred back to this
//    library where profile data is written for later processing. 
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
// Acknowledgements:
//    Phil Mucci's 'papiex' code provided guidance for handling threads.
//
//    The PAPI Initialization code was originally adapted from parts of The
//    Visual Profiler by Curtis L. Janssen (vmon.c).
//    
*****************************************************************************/

/************************** System Include Files ****************************/

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>   /* for 'PATH_MAX' */
#include <signal.h>
#include <inttypes.h>
#include <stdarg.h>   /* va_arg */
#include <errno.h>

#ifndef __USE_GNU
# define __USE_GNU /* must define on Linux to get RTLD_NEXT from <dlfcn.h> */
# define SELF_DEFINED__USE_GNU
#endif

#include <dlfcn.h>

/**************************** User Include Files ****************************/

#include "hpcpapi.h" /* <papi.h>, etc. */

#include "hpcrun.h"
#include "rtmap.h"
#include <lib/hpcfile/io.h>

/**************************** Forward Declarations **************************/

static void init_library();
static void fini_library();

static void init_process();
static void fini_process();

static hpcrun_profiles_desc_t* init_thread(int is_thread);

static void                    fini_thread(hpcrun_profiles_desc_t** profdesc, 
					   int is_thread);

static long hpcrun_gettid();

typedef uint32_t hpc_hist_bucket; /* a 4 byte histogram bucket */

static const uint64_t default_period = (1 << 15) - 1; /* (2^15 - 1) */

/**************************** Forward Declarations **************************/

/* libc intercepted routines */

#define PARAMS_START_MAIN (int (*main) (int, char **, char **),              \
			   int argc,                                         \
			   char *__unbounded *__unbounded ubp_av,            \
			   void (*init) (void),                              \
                           void (*fini) (void),                              \
			   void (*rtld_fini) (void),                         \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) PARAMS_START_MAIN;
static int  hpcrun_libc_start_main PARAMS_START_MAIN;

typedef void (*libc_start_main_fini_fptr_t) (void);
static void hpcrun_libc_start_main_fini(); 


#define PARAMS_EXECV  (const char *path, char *const argv[])
#define PARAMS_EXECVP (const char *file, char *const argv[])
#define PARAMS_EXECVE (const char *path, char *const argv[],                 \
                       char *const envp[])

typedef int (*execv_fptr_t)  PARAMS_EXECV;
typedef int (*execvp_fptr_t) PARAMS_EXECVP;
typedef int (*execve_fptr_t) PARAMS_EXECVE;

static int  hpcrun_execv  PARAMS_EXECV;
static int  hpcrun_execvp PARAMS_EXECVP;
static int  hpcrun_execve PARAMS_EXECVE;


typedef pid_t (*fork_fptr_t) (void);
static pid_t hpcrun_fork();


/* libpthread intercepted routines */

#define PARAMS_PTHREAD_CREATE (pthread_t* thread,                            \
			       const pthread_attr_t* attr,                   \
			       void *(*start_routine)(void*),                \
			       void* arg)

typedef struct {
  void* (*start_routine)(void*);    /* from pthread_create() */
  void* arg;                        /* from pthread_create() */
  hpcrun_profiles_desc_t* profdesc; /* profiling info */
} hpcrun_pthread_create_args_t;


typedef int (*pthread_create_fptr_t) PARAMS_PTHREAD_CREATE;
static int hpcrun_pthread_create PARAMS_PTHREAD_CREATE;
static void* hpcrun_pthread_create_start_routine(void* arg);
static void  hpcrun_pthread_cleanup_routine(void* arg);

typedef pthread_t (*pthread_self_fptr_t) (void);


/* intercepted signals */

static void hpcrun_sighandler(int sig);

/*************************** Variable Declarations **************************/

/* These variables are set when the library is initialized */

/* Intercepted libc and libpthread routines */
static libc_start_main_fptr_t      real_start_main;
static libc_start_main_fini_fptr_t real_start_main_fini;
static execv_fptr_t                real_execv;
static execvp_fptr_t               real_execvp;
static execve_fptr_t               real_execve;
static fork_fptr_t                 real_fork;
static pthread_create_fptr_t       real_pthread_create;
static pthread_self_fptr_t         real_pthread_self;

/* hpcrun options (this should be a tidy struct) */
static int   opt_debug = 0;
static int   opt_recursive = 0;
static hpc_threadprof_t opt_thread = HPCRUN_THREADPROF_NO;
static char* opt_eventlist = NULL;
static char  opt_outpath[PATH_MAX] = "";
static int   opt_flagscode = 0;

/*************************** Variable Declarations **************************/

/* These variables are set when libc_start_main is intercepted */

/* This info is constant throughout the process and can therefore be
   shared among multiple threads. */
static char*        profiled_cmd = NULL; /* profiled command */
static rtloadmap_t* rtloadmap = NULL;    /* run time load map */
static unsigned     numSysEvents  = 0;   /* estimate */
static unsigned     numPAPIEvents = 0;   /* estimate */

/* Profiling information for the first thread of execution in a
   process. N.B. The _shared_ profiling buffers live here when
   combining thread profiles. */
static hpcrun_profiles_desc_t* hpc_profdesc = NULL;

/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

static void init_option_debug();
static void init_options();
static void handle_any_dlerror();

/*
 *  Implicit interface
 */
void 
_init()
{
  init_library();
  /* process initialized with interception of libc_start_main */
}

void 
_fini()
{
  /* process finalized with libc_start_main */
  fini_library();
}


/*
 *  Explicit interface
 */
void 
hpcrun_init()
{
  init_library();
  init_process();
}

void 
hpcrun_fini()
{
  fini_process();
  fini_library();
}


/*
 *  Library initialization
 */
static void 
init_library()
{
  init_option_debug();
  
  if (opt_debug >= 1) { MSG(stderr, "*** init_library ***"); }
  
  init_options();

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
  handle_any_dlerror();
  if (!real_start_main) {
    real_start_main = 
      (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__BP___libc_start_main");
    handle_any_dlerror();
  }
  if (!real_start_main) {
    DIE("fatal error: Cannot intercept beginning of process execution and therefore cannot begin profiling.");
  }

  
  real_execv = (execv_fptr_t)dlsym(RTLD_NEXT, "execv");  
  handle_any_dlerror();

  real_execvp = (execvp_fptr_t)dlsym(RTLD_NEXT, "execvp");  
  handle_any_dlerror();
  
  real_execve = (execve_fptr_t)dlsym(RTLD_NEXT, "execve");
  handle_any_dlerror();
  
  
  real_fork = (fork_fptr_t)dlsym(RTLD_NEXT, "fork");
  handle_any_dlerror();
  
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
    handle_any_dlerror();
    if (!real_pthread_create) {
      DIE("fatal error: Cannot intercept POSIX thread creation and therefore cannot profile threads.");
    }
    
    real_pthread_self = dlsym(RTLD_NEXT, "pthread_self");
    handle_any_dlerror();
    if (!real_pthread_self) {
      DIE("fatal error: Cannot intercept POSIX thread id routine and therefore cannot profile threads.");
    }
  }
}


/*
 *  Library finalization
 */
static void 
fini_library()
{
  if (opt_debug >= 1) { MSG(stderr, "*** fini_library ***"); }
}


static void
init_option_debug()
{
  /* Debugging (get this first) : default is off */
  char *env_debug = getenv("HPCRUN_DEBUG");
  opt_debug = (env_debug ? atoi(env_debug) : 0);
}


static void
init_options()
{
  char *env_recursive, *env_thread, *env_eventlist, *env_outpath, *env_flags;
  
  if (opt_debug >= 1) { 
    MSG(stderr, "  LD_PRELOAD: %s", getenv("LD_PRELOAD")); 
  }
  
  /* Recursive profiling: default is on */
  env_recursive = getenv("HPCRUN_RECURSIVE");
  opt_recursive = (env_recursive ? atoi(env_recursive) : 1);
  if (!opt_recursive) {
    /* turn off profiling for any processes spawned by this one */
    unsetenv("LD_PRELOAD"); 
    /* FIXME: just extract HPCRUN_LIB */
  }

  if (opt_debug >= 1) { 
    MSG(stderr, "  recursive profiling: %d", opt_recursive); 
  }
  
  /* Threaded profiling: default is off */
  env_thread = getenv("HPCRUN_THREAD");
  opt_thread = (env_thread ? atoi(env_thread) : HPCRUN_THREADPROF_NO);
  
  /* Profiling event list: default PAPI_TOT_CYC:32767 (default_period) */
  opt_eventlist = "PAPI_TOT_CYC:32767"; 
  env_eventlist = getenv("HPCRUN_EVENT_LIST");
  if (env_eventlist) {
    opt_eventlist = env_eventlist;
  }

  if (opt_debug >= 1) { MSG(stderr, "  event list: %s", opt_eventlist); }
  
  /* Output path: default . */
  strncpy(opt_outpath, ".", PATH_MAX);
  env_outpath = getenv("HPCRUN_OUTPUT_PATH");
  if (env_outpath) {
    strncpy(opt_outpath, env_outpath, PATH_MAX);
  }
  
  /* Profiling flags: default PAPI_PROFIL_POSIX */
  {
    const hpcpapi_flagdesc_t *f = hpcpapi_flag_by_name("PAPI_PROFIL_POSIX");
    opt_flagscode = f->code;

    env_flags = getenv("HPCRUN_EVENT_FLAG");
    if (env_flags) {
      if ((f = hpcpapi_flag_by_name(env_flags)) == NULL) {
	DIE("fatal error: Invalid profiling flag '%s'.", env_flags);
      }
      opt_flagscode = f->code;
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
  profiled_cmd = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  real_start_main_fini = fini;
  
  /* initialize profiling for process */
  init_process();
  
  /* launch the process */
  if (opt_debug >= 1) {
    MSG(stderr, "*** launching intercepted app: %s ***", profiled_cmd);
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

static void parse_execl(const char*** argv, const char* const** envp,
			const char* arg, va_list arglist);


extern int 
execl(const char *path, const char *arg, ...)
{
  const char** argv = NULL;
  va_list arglist;

  va_start(arglist, arg);
  parse_execl(&argv, NULL, arg, arglist);
  va_end(arglist);
  
  return hpcrun_execv(path, (char* const*)argv);
}


extern int 
execlp(const char *file, const char *arg, ...)
{
  const char** argv = NULL;
  va_list arglist;
  
  va_start(arglist, arg);
  parse_execl(&argv, NULL, arg, arglist);
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
  parse_execl(&argv, &envp, arg, arglist);
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


static void 
parse_execl(const char*** argv, const char* const** envp,
	    const char* arg, va_list arglist)
{
  /* argv & envp are pointers to arrays of char* */
  /* va_start has already been called */

  const char* argp;
  int argvSz = 32, argc = 1;
  
  *argv = malloc((argvSz+1) * sizeof(const char*));
  if (!*argv) { DIE("fatal error: malloc() failed!"); }
  
  (*argv)[0] = arg;
  while ((argp = va_arg(arglist, const char*)) != NULL) { 
    if (argc > argvSz) {
      argvSz *= 2;
      *argv = realloc(*argv, (argvSz+1) * sizeof(const char*));
      if (!*argv) { DIE("fatal error: realloc() failed!"); }
    }
    (*argv)[argc] = argp;
    argc++;
  }
  (*argv)[argc] = NULL;
  
  if (envp != NULL) {
    *envp = va_arg(arglist, const char* const*);
  }

#if 0
  if (opt_debug >= 2) { 
    int i;
    for (i = 0; i < argc; ++i) {
      MSG(stderr, "  execl arg%d: %s", i, (*argv)[i]);
    }
    if (envp) {
      MSG(stderr, "  execl envp found");
    }
  }
#endif
  
  /* user calls va_end */
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
 * Intercepted signals
 ****************************************************************************/

/* We allow the user to kill profiling by intercepting the certain
   signals.  This can be very useful on long-running or misbehaving
   applications. */

static void 
hpcrun_sighandler(int sig)
{
  if (opt_debug >= 1) { MSG(stderr, "*** catching signal %d ***", sig); }
  
  signal(sig, SIG_DFL); /* return to default action */
    
  switch (sig) {
  case SIGINT: {
    break;
  }
  default: 
    ERRMSG("Warning: Handling unknown signal %d.", sig);
    break;
  }
  
  fini_process();
  fini_library();
}


/****************************************************************************/
/****************************************************************************/

/****************************************************************************
 * Initialize profiling 
 ****************************************************************************/

static void count_events(unsigned* sysEvents, unsigned* papiEvents);

static void 
init_profdesc(hpcrun_profiles_desc_t** profdesc, 
	      unsigned numSysEv, unsigned numPapiEv, 
	      rtloadmap_t* rtmap,
	      hpcrun_profiles_desc_t* sharedprofdesc);
static void 
init_sysprofdesc_buffer(hpcsys_profile_desc_vec_t* profdesc, 
			unsigned numEv, rtloadmap_t* rtmap,
			hpcsys_profile_desc_vec_t* sharedprofdesc);
static void 
init_papiprofdesc_buffer(hpcpapi_profile_desc_vec_t* profdesc, 
			 unsigned numEv, rtloadmap_t* rtmap,
			 hpcpapi_profile_desc_vec_t* sharedprofdesc);

static void init_profdesc_ofile(hpcrun_profiles_desc_t* profdesc, 
				int sharedprofdesc);

static void add_sysevent(hpcsys_profile_desc_vec_t* profdescs, 
			 rtloadmap_t* rtmap, int profidx, 
			 char* eventnm, uint64_t period);
static void start_sysprof(hpcsys_profile_desc_vec_t* profdescs);

static void init_papi_for_process();
static void add_papievent(hpcpapi_profile_desc_vec_t* profdescs, 
			  rtloadmap_t* rtmap, int profidx, 
			  char* eventnm, uint64_t period);
static void start_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);

static void init_sighandlers();


/*
 *  Prepare for profiling this process
 */
static void 
init_process()
{
  if (opt_debug >= 1) { MSG(stderr, "*** init_process ***"); }

  rtloadmap = hpcrun_get_rtloadmap(opt_debug);
  
  /* Initialize PAPI if necessary */
  count_events(&numSysEvents, &numPAPIEvents); /* no error checking */
  if (numPAPIEvents > 0) {
    init_papi_for_process();
  }
  
  hpc_profdesc = init_thread(0 /*is_thread*/);
}


/*
 *  Prepare profiling for this thread of execution.  N.B.: the caller
 *  must keep the returned data structure safe!  (The boolean argument
 *  'is_thread' refers to whether we are in an actual thread, i.e. not
 *  simply a the execution of a process.)
 */
static hpcrun_profiles_desc_t* 
init_thread(int is_thread)
{
  hpcrun_profiles_desc_t* profdesc = NULL, *sharedprofdesc = NULL;
  
  if (opt_debug >= 1) { MSG(stderr, "*** init_thread ***"); }
  
  /* Create profile info from event list and perform error checking. */
  if (is_thread && opt_thread == HPCRUN_THREADPROF_ALL) {
    sharedprofdesc = hpc_profdesc; /* share the histogram buffers */
  }
  init_profdesc(&profdesc, numSysEvents, numPAPIEvents, rtloadmap, 
		sharedprofdesc);
  
  /* Init signal handlers */
  init_sighandlers();

  /* Launch profilers */
  if (HPC_GET_SYSPROFS(profdesc)) {
    start_sysprof(HPC_GET_SYSPROFS(profdesc));
  }
  if (HPC_GET_PAPIPROFS(profdesc)) {
    start_papi_for_thread(HPC_GET_PAPIPROFS(profdesc));
  }

  return profdesc;
}


static void
count_events(unsigned* sysEvents, unsigned* papiEvents)
{
  char* tok, *tmp_eventlist;

  /* note: arguments must not be NULL */
  *sysEvents = 0;
  *papiEvents = 0;
  
  /* note: strtok() will destroy the string so we use strdup */
  tmp_eventlist = strdup(opt_eventlist);
  for (tok = strtok(tmp_eventlist, ";"); (tok != NULL);
       tok = strtok((char*)NULL, ";")) {
    /* There may be a ':' within 'tok'... */
    if ( (strncmp(tok, HPCRUN_EVENT_WALLCLK_STR, 
		  HPCRUN_EVENT_WALLCLK_STRLN) == 0) ||
	 (strncmp(tok, HPCRUN_EVENT_FWALLCLK_STR, 
		  HPCRUN_EVENT_FWALLCLK_STRLN) == 0) ) {
      (*sysEvents)++;
    }
    else {
      (*papiEvents)++;
    }
  }
  free(tmp_eventlist);
}


static void 
init_profdesc(hpcrun_profiles_desc_t** profdesc, 
	      unsigned numSysEv, unsigned numPapiEv, rtloadmap_t* rtmap,
	      hpcrun_profiles_desc_t* sharedprofdesc)
{
  /* PAPI should already be initialized if necessary */
  
  int xprofidx = -1, yprofidx = -1; /* nth prof event for x/y */
  const unsigned eventbufSZ = 128; /* really the last index, not the size */
  char eventbuf[eventbufSZ+1];
  char* tok, *tmp_eventlist;
  int rval, i;
      
  if (opt_debug >= 1) { 
    MSG(stderr, "Initializing profile descriptors");
    MSG(stderr, "  Found %d sys events and %d PAPI events", 
	numSysEv, numPapiEv);
  }
  
  /* 1a. Ensure we do not profile both system and PAPI events. */
  if (numSysEv > 0 && numPapiEv > 0) {
    DIE("Cannot profile both WALLCLK and PAPI events at the same time. (Both use SIGPROF.)");
  }
  
  /* 1b. Ensure no more than one wall clock event is profiled.  (Only
     one appropriate itimer (ITIMER_PROF) is provided per process.) */ 
  if (numSysEv > 1) {
    numSysEv = 1;
  }

  /* 1c. Ensure that we do not use system profile with threads */ 
  if (numSysEv > 0 && opt_thread) {
    DIE("Cannot profile WALLCLK on multithreaded process. (sprofil() limitation.)");
  }
  
  /* 1d. Ensure we have enough hardware counters if using PAPI.  Note:
     PAPI cannot profile when using multiplexing. */
  {
    int numHwCntrs = PAPI_num_hwctrs();
    if (numPapiEv > numHwCntrs) {
      ERRMSG("Warning: Too many events (%d) for hardware counters (%d).  Only using first %d events.", numPapiEv, numHwCntrs, numHwCntrs);
      numPapiEv = numHwCntrs;
    }
  }

  
  /* 2a. Initialize profdesc */
  *profdesc = (hpcrun_profiles_desc_t*)malloc(sizeof(hpcrun_profiles_desc_t));
  if (!(*profdesc)) { DIE("fatal error: malloc() failed!"); }
  memset(*profdesc, 0x00, sizeof(hpcrun_profiles_desc_t));
  
  /* 2b. Initialize system profdescs */
  if (numSysEv > 0) {
    unsigned int vecsz, sz = sizeof(hpcsys_profile_desc_vec_t);

    HPC_GETL_SYSPROFS(*profdesc) = (hpcsys_profile_desc_vec_t*)malloc(sz);
    if (!HPC_GET_SYSPROFS(*profdesc)) { 
      DIE("fatal error: malloc() failed!"); 
    }

    vecsz = sizeof(hpcsys_profile_desc_t) * numSysEv;    
    HPC_GET_SYSPROFS(*profdesc)->size = numSysEv;    
    
    HPC_GET_SYSPROFS(*profdesc)->vec = (hpcsys_profile_desc_t*)malloc(vecsz);
    if (!HPC_GET_SYSPROFS(*profdesc)->vec) { 
      DIE("fatal error: malloc() failed!"); 
    }
    memset(HPC_GET_SYSPROFS(*profdesc)->vec, 0x00, vecsz);
  }
  
  /* 2c. Initialize papi profdescs */
  if (numPapiEv > 0) {
    unsigned int vecsz, sz = sizeof(hpcpapi_profile_desc_vec_t);
    
    HPC_GETL_PAPIPROFS(*profdesc) = (hpcpapi_profile_desc_vec_t*)malloc(sz);
    if (!HPC_GET_PAPIPROFS(*profdesc)) { 
      DIE("fatal error: malloc() failed!"); 
    }

    vecsz = sizeof(hpcpapi_profile_desc_t) * numPapiEv;
    HPC_GET_PAPIPROFS(*profdesc)->size = numPapiEv;    
    
    HPC_GET_PAPIPROFS(*profdesc)->vec = (hpcpapi_profile_desc_t*)malloc(vecsz);
    if (!HPC_GET_PAPIPROFS(*profdesc)->vec) { 
      DIE("fatal error: malloc() failed!"); 
    }
    memset(HPC_GET_PAPIPROFS(*profdesc)->vec, 0x00, vecsz);

    HPC_GET_PAPIPROFS(*profdesc)->eset = PAPI_NULL;     
    if ((rval = PAPI_create_eventset(&HPC_GET_PAPIPROFS(*profdesc)->eset)) 
	!= PAPI_OK) {
      DIE("fatal error: PAPI error (%d): %s.", rval, PAPI_strerror(rval));
    }
  }

  
  /* 3. For each event:period pair, init corresponding profdescs
     entry.  Classification of events *must* be the same as count_events(). */
  tmp_eventlist = strdup(opt_eventlist);
  tok = strtok(tmp_eventlist, ";");
  for (i = 0; (tok != NULL); i++, tok = strtok((char*)NULL, ";")) {
    uint64_t period = 0;
    char* sep;
    unsigned evty = 0; /* 1 is system; 2 is papi */
    
    /* Extract event field from token */
    sep = strchr(tok, ':'); /* optional period delimiter */
    if (sep) {
      unsigned len = MIN(sep - tok, eventbufSZ);
      strncpy(eventbuf, tok, len);
      eventbuf[len] = '\0'; 
    }
    else {
      strncpy(eventbuf, tok, eventbufSZ);
      eventbuf[eventbufSZ] = '\0';
    }
    
    /* Determine the event type */
    if ( (strcmp(eventbuf, HPCRUN_EVENT_WALLCLK_STR) == 0) ||
	 (strcmp(eventbuf, HPCRUN_EVENT_FWALLCLK_STR) == 0) ) {
      evty = 1;
    } 
    else {
      evty = 2;
    }
    
    /* Extract period field from token */
    if (sep) {
      period = strtol(sep+1, (char **)NULL, 10);
    }
    else if (evty == 1) {
      period = 0;
    }
    else if (evty == 2) {
      period = default_period;
    }
    
    if (opt_debug >= 1) { 
      MSG(stderr, "  Event: '%s' (%d) '%"PRIu64"'", eventbuf, evty, period);
    }
    
    /* Add the event to the appropriate place */
    if (evty == 1) {
      xprofidx++;
      add_sysevent(HPC_GET_SYSPROFS(*profdesc), rtmap, xprofidx, 
		   eventbuf, period);
    } 
    else if (evty == 2) {
      yprofidx++;
      add_papievent(HPC_GET_PAPIPROFS(*profdesc), rtmap, yprofidx, 
		    eventbuf, period);
    } 
    else {
      DIE("fatal error: internal programming error - invalid event.");
    }
  }
  free(tmp_eventlist);

  /* N.B.: at this point x->sprofs an1d (*y)->sprofs remains uninitialized */

  /* 4a. For each sys profdescs entry, init sprofil()-specific info */
  if (HPC_GET_SYSPROFS(*profdesc)) {
    hpcsys_profile_desc_vec_t* sh = 
      (sharedprofdesc) ? HPC_GET_SYSPROFS(sharedprofdesc) : NULL;
    init_sysprofdesc_buffer(HPC_GET_SYSPROFS(*profdesc), numSysEv, rtmap, sh);
  }

  /* 4b. For each papi profdescs entry, init sprofil()-specific info */
  if (HPC_GET_PAPIPROFS(*profdesc)) {
    hpcpapi_profile_desc_vec_t* sh = 
      (sharedprofdesc) ? HPC_GET_PAPIPROFS(sharedprofdesc) : NULL;
    init_papiprofdesc_buffer(HPC_GET_PAPIPROFS(*profdesc), numPapiEv, rtmap,
			     sh);
  }

  /* 4c. Init file info if necessary.  Perform a filesystem test to
     make sure we will be able to write output data. */
  init_profdesc_ofile(*profdesc, (sharedprofdesc != NULL));
}


static void
init_sysprofdesc_buffer(hpcsys_profile_desc_vec_t* profdesc, 
			unsigned numEv, rtloadmap_t* rtmap,
			hpcsys_profile_desc_vec_t* sharedprofdesc)
{
  int i;

  for (i = 0; i < numEv; ++i) {
    int mapi;
    unsigned int sprofbufsz = sizeof(struct prof) * rtmap->count;
    hpcsys_profile_desc_t* prof = &profdesc->vec[i];
    
    if (sharedprofdesc) {
      prof->sprofs = sharedprofdesc->vec[i].sprofs;
      prof->numsprofs = sharedprofdesc->vec[i].numsprofs;
    }
    else {
      prof->sprofs = (struct prof*)malloc(sprofbufsz);
      if (!prof->sprofs) { DIE("fatal error: malloc() failed!"); }
      memset(prof->sprofs, 0x00, sprofbufsz);
      prof->numsprofs = rtmap->count;
    }
      
    if (opt_debug >= 4) { 
      MSG(stderr, "profile buffer details for %s:", prof->ename); 
      MSG(stderr, "  count = %d, sp=%"PRIu64" ef=%d",
	  prof->numsprofs, prof->period, prof->flags);
    }
    
    if (sharedprofdesc) {
      /* print msg */
    }
    else {
      for (mapi = 0; mapi < rtmap->count; ++mapi) {
	unsigned int bufsz;
	unsigned int ncntr;
	
	/* eliminate use of ceil() (link with libm) by adding 1 */
	ncntr = (rtmap->module[mapi].length / prof->bytesPerCodeBlk) + 1;
	bufsz = ncntr * prof->bytesPerCntr;
	
	/* buffer base and size */
	prof->sprofs[mapi].pr_base = (void*)malloc(bufsz);
	prof->sprofs[mapi].pr_size = bufsz;
	if (!prof->sprofs[mapi].pr_base) { 
	  DIE("fatal error: malloc() failed!"); 
	}
	memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);
	
	/* pc offset and scaling factor */
	prof->sprofs[mapi].pr_off = rtmap->module[mapi].offset;
	prof->sprofs[mapi].pr_scale = prof->scale;      
	
	if (opt_debug >= 4) {
	  /* 'pr_size'/'pr_off' are of type 'size_t' which is of pointer size */
	  MSG(stderr, "\tprofile[%d] base = %p size = %#"PRIxPTR" off = %#"PRIxPTR" scale = %#lx",
	      mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
	      prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
	}
      }
    }
  }
}


static void
init_papiprofdesc_buffer(hpcpapi_profile_desc_vec_t* profdesc, 
			 unsigned numEv, rtloadmap_t* rtmap,
			 hpcpapi_profile_desc_vec_t* sharedprofdesc)
{
  int i;

  for (i = 0; i < numEv; ++i) {
    int mapi;
    unsigned int sprofbufsz = sizeof(PAPI_sprofil_t) * rtmap->count;
    hpcpapi_profile_desc_t* prof = &profdesc->vec[i];
    
    if (sharedprofdesc) {
      prof->sprofs = sharedprofdesc->vec[i].sprofs;
      prof->numsprofs = sharedprofdesc->vec[i].numsprofs;      
    }
    else {
      prof->sprofs = (PAPI_sprofil_t*)malloc(sprofbufsz);
      if (!prof->sprofs) { DIE("fatal error: malloc() failed!"); }
      memset(prof->sprofs, 0x00, sprofbufsz);
      prof->numsprofs = rtmap->count;
      
    }
    
    if (opt_debug >= 4) { 
      MSG(stderr, "profile buffer details for %s:", prof->einfo.symbol); 
      MSG(stderr, "  count = %d, es=%#x ec=%#x sp=%"PRIu64" ef=%d",
	  prof->numsprofs, profdesc->eset, 
	  prof->ecode, prof->period, prof->flags);
    }
    
    if (sharedprofdesc) {
      /* print msg */
    }
    else {
      for (mapi = 0; mapi < rtmap->count; ++mapi) {
	unsigned int bufsz;
	unsigned int ncntr;
	
	/* eliminate use of ceil() (link with libm) by adding 1 */
	ncntr = (rtmap->module[mapi].length / prof->bytesPerCodeBlk) + 1;
	bufsz = ncntr * prof->bytesPerCntr;
	
	/* buffer base and size */
	prof->sprofs[mapi].pr_base = (void*)malloc(bufsz);
	prof->sprofs[mapi].pr_size = bufsz;
	if (!prof->sprofs[mapi].pr_base) {
	  DIE("fatal error: malloc() failed!");
	}
	memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);
	
	/* pc offset and scaling factor */
	prof->sprofs[mapi].pr_off = (caddr_t)rtmap->module[mapi].offset;
	prof->sprofs[mapi].pr_scale = prof->scale;
	
	if (opt_debug >= 4) {
	  MSG(stderr, 
	      "\tprofile[%d] base = %p size = %#x off = %p scale = %#x",
	      mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
	      prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
	}
      }
    }
  }  
}


static void 
init_profdesc_ofile(hpcrun_profiles_desc_t* profdesc, int sharedprofdesc)
{
  static unsigned int outfilenmLen = PATH_MAX; /* never redefined */
  static unsigned int hostnmLen = 128;         /* never redefined */
  char outfilenm[outfilenmLen];
  char hostnm[hostnmLen];
  char* cmd = profiled_cmd; 
  char* event = NULL, *evetc = "", *slash = NULL;
  unsigned numEvents = 0;
  FILE* fs;
  
  if (sharedprofdesc) {
    profdesc->ofile.fs = NULL;
    profdesc->ofile.fname = NULL;
    return;
  }


  /* Get components for constructing file name:
     <outpath>/<command>.<event1>.<hostname>.<pid>.<tid> */
  
  /* <command> */
  slash = rindex(cmd, '/');
  if (slash) {
    cmd = slash + 1; /* basename of cmd */
  }
  
  /* <event1> */
  if (HPC_GET_SYSPROFS(profdesc)) {
    numEvents += HPC_GET_SYSPROFS(profdesc)->size;
    event = HPC_GET_SYSPROFS(profdesc)->vec[0].ename;
  }
  if (!event && HPC_GET_PAPIPROFS(profdesc)) {
    numEvents += HPC_GET_PAPIPROFS(profdesc)->size;
    event = HPC_GET_PAPIPROFS(profdesc)->vec[0].einfo.symbol; /* first name */
  }
  if (numEvents > 1) {
    evetc = "-etc"; /* indicates unshown events */
  }
  
  /* <hostname> */
  gethostname(hostnm, hostnmLen);
  hostnm[hostnmLen-1] = '\0'; /* ensure NULL termination */

  /* Create file name */
  snprintf(outfilenm, outfilenmLen, "%s/%s.%s%s.%s.%d.%ld", 
	   opt_outpath, cmd, event, evetc, hostnm, getpid(), hpcrun_gettid());
  
  profdesc->ofile.fs = NULL;
  profdesc->ofile.fname = (char*)malloc(strlen(outfilenm)+1);
  if (!profdesc->ofile.fname) { DIE("fatal error: malloc() failed!"); }
  strcpy(profdesc->ofile.fname, outfilenm);

  /* Test whether we can write to this filesystem */
  fs = fopen(outfilenm, "w");
  if (fs == NULL) {
    DIE("fatal error: Filesystem test failed (cannot open file '%s').", 
	outfilenm);
  }
  fclose(fs);
  
  /* Note: it is possible for this filename to already exist
     (e.g. consider a process that exec's itself).  Claim our
     territory by leaving an empty file in the file system. */

  /* FIXME: what if this filename already exists? e.g., we exec ourself! */
}


static void
add_sysevent(hpcsys_profile_desc_vec_t* profdescs, rtloadmap_t* rtmap,
	     int profidx, char* eventnm, uint64_t period)
{
  /* Cf. the notes below on PAPI_sprofil().  As is presented below,

            scale = ( scaleval / 65536 ) = ( bh / b )

     In contrast to the below bh is not fixed at 2 but actually
     represents the size of histogram buckets (which leads to lost
     profiling resolution). 

     Some sample scale values, when bh is 4.

          scaleval            bytes_per_code_block (b)
          ----------------------------------------
          0x10000 (or 0xffff) 4  (size of many RISC instructions)
	  0x8000              8
	  0x4000              16 (size of Itanium instruction packet)
  */
  
  hpcsys_profile_desc_t* prof = NULL;
  
  if (profidx >= profdescs->size) {
    /* Assumes that the only system event is wallclock time */
    DIE("fatal error: Only one wallclock event may be profiled at a time.");
  }

  prof = &(profdescs->vec[profidx]);

  /* Find event */
  if (strcmp(eventnm, HPCRUN_EVENT_WALLCLK_STR) == 0) {
    prof->ename = HPCRUN_EVENT_WALLCLK_STR;
    prof->flags = 0;
    prof->period = 10; /* 10 millisecond */
  }
  else if (strcmp(eventnm, HPCRUN_EVENT_FWALLCLK_STR) == 0) {
    prof->ename = HPCRUN_EVENT_FWALLCLK_STR;
    prof->flags = PROF_FAST;
    prof->period = 10; /* should be 1 ms; cf. /usr/include/sys/profile.h */
  }
  else {
    DIE("fatal error: Invalid event: '%s'.", eventnm);
  }

  /* Profiling period (already set) */
  if (period != 0) {
    DIE("fatal error: Invalid period %"PRIu64" for event '%s'.", 
	period, eventnm);
  }
    
  /* Profiling flags */
  prof->flags |= PROF_UINT; /* hpc_hist_bucket */
  
  prof->bytesPerCntr = sizeof(hpc_hist_bucket); /* 4 */
  prof->bytesPerCodeBlk = 4;
  prof->scale = 0x10000;
  
  if ((prof->scale * prof->bytesPerCodeBlk) != (65536 * prof->bytesPerCntr)) {
    DIE("fatal error: internal programming error - invalid profiling scale.");
  }
}


static void 
start_sysprof(hpcsys_profile_desc_vec_t* profdescs)
{
  int ecode;

  /* Note: should only be one profdesc! */
  hpcsys_profile_desc_t* prof = &profdescs->vec[0];

  if (opt_debug >= 1) { MSG(stderr, "Calling sprofil(): %s", prof->ename); }
  
  ecode = sprofil(prof->sprofs, prof->numsprofs, NULL, prof->flags);
  if (ecode != 0) {
    DIE("fatal error: sprofil() error. %s.", strerror(errno));
  }
}


static void 
init_papi_for_process()
{  
  int rval;
  int papi_debug = 0;
  
  /* Initialize papi: hpc_init_papi_force() *must* be used for forks();
     it works for non-forks also. */
  if (hpc_init_papi_force(PAPI_library_init) != 0) { 
    exit(1); /* error already printed */
  }
  
  /* set PAPI debug */
  switch(opt_debug) {
  case 0: 
  case 1: 
  case 2:
    break;
  case 3:
    papi_debug = PAPI_VERB_ECONT;
    break;
  case 4:
    papi_debug = PAPI_VERB_ESTOP;
    break;
  }
  if (papi_debug != 0) {
    MSG(stderr, "setting PAPI debug option %d.", papi_debug);
    if ((rval = PAPI_set_debug(papi_debug)) != PAPI_OK) {
      DIE("fatal error: PAPI error (%d): %s.", rval, PAPI_strerror(rval));
    }
  }
  
  /* PAPI_set_domain(PAPI_DOM_ALL); */
  
  if (opt_thread) {
    if ((rval = PAPI_thread_init(real_pthread_self)) != PAPI_OK) {
      DIE("fatal error: PAPI error (%d): %s.", rval, PAPI_strerror(rval));
    }
  }
}


static void 
add_papievent(hpcpapi_profile_desc_vec_t* profdescs, rtloadmap_t* rtmap,
	      int profidx, char* eventnm, uint64_t period)
{ 
  /* Note on hpcpapi_profile_desc_t and PAPI_sprofil() scaling factor
     cf. man profil() or sprofil()
     
     The scale factor describes how the histogram buffer and each
     histogram counter correlates with the region to be profiled.  
     
     The region to be profiled can be thought of as being divided into
     n equally sized blocks, each b bytes long.  For historical
     reasons, we introduce a term, bh, representing the size in bytes
     of each histogram counter.  The value of the scale factor is the
     reciprocal of (b / bh):
           scale = reciprocal( b / bh )
     Note that now bh is *fixed* at 2.

     Since this scheme was devised when a memory word was 16-bits or
     2-bytes, the scale factor was represented as a 16-bit fixed-point
     fraction with the decimal point implied on the *left*.  Under
     this representation, the maximum value of the scale was 0xffff
     (essentially 1, 0x10000); this is *no longer* the case.

     Alternatively, and perhaps more conveniently, the scale can be
     thought of as a ratio between an unsigned integer scaleval and
     65536:
          scale = ( scaleval / 65536 ) = ( bh / b )

     Some sample scale values, given that bh is fixed at 2.

          scaleval            bytes_per_code_block (b)
          ----------------------------------------
          0x20000             1  (x86 insns may begin at any byte)
          0x10000 (or 0xffff) 2
	  0x8000              4  (size of many RISC instructions)
	  0x4000              8
	  0x2000              16 (size of Itanium instruction packet)

      
     Using this second notation, we can show the relation between the
     histogram counters and the region to profile.  The histogram
     counter that will be incremented for an interrupt at program
     counter pc is:
          histogram[ ((pc - load_offset)/bh) * (scaleval/65536) ]

     The number of counters in the histogram should equal the number
     of blocks in the region to be profiled.  */

  int pcode;
  hpcpapi_profile_desc_t* prof = NULL;
  
  if (!profdescs) {
    DIE("fatal error: internal programming error.");
  }

  if (profidx >= profdescs->size) {
    ERRMSG("Warning: Ignoring event '%s:%"PRIu64"'.", eventnm, period);
    return;
  }
    
  prof = &(profdescs->vec[profidx]);
  
  /* Find event info, ensuring it is available.  Note: it is
     necessary to do a query_event *and* get_event_info.  Sometimes
     the latter will return info on an event that does not exist. */
  if (PAPI_event_name_to_code(eventnm, &prof->ecode) != PAPI_OK) {
    DIE("fatal error: Event '%s' is not recognized.\n"
	"\tCheck the list of supported events with `hpcrun -L'.", eventnm);
  }
  if (PAPI_query_event(prof->ecode) != PAPI_OK) { 
    DIE("fatal error: PAPI_query_event for '%s' failed for unknown reason.", 
	eventnm);
  }
  if ((pcode = PAPI_get_event_info(prof->ecode, &prof->einfo)) != PAPI_OK) {
    DIE("fatal error: PAPI error (%d): %s.", pcode, PAPI_strerror(pcode));
  }
  
  /* NOTE: Although clumsy, this test has official sanction. */
  if ((prof->ecode & PAPI_PRESET_MASK) && (prof->einfo.count > 1) && 
      strcmp(prof->einfo.derived, "DERIVED_CMPD") != 0) {
    DIE("fatal error: '%s' is a PAPI derived event.\n"
	"\tSampling of derived events is not supported by PAPI.\n" 
	"\tUse `hpcrun -L' to find the component native events of '%s' that you can monitor separately.", eventnm, eventnm);
  }
  
  if ((pcode = PAPI_add_event(profdescs->eset, prof->ecode)) != PAPI_OK) {
    DIE("fatal error: (%d) Unable to add event '%s' to event set.\n"
	"\tPAPI error %s.", pcode, eventnm, PAPI_strerror(pcode));
  }
  
  /* Profiling period */
  if (period == 0) {
    DIE("fatal error: Invalid period %"PRIu64" for event '%s'.", 
	period, eventnm);
  }  
  prof->period = period;
    
  /* Profiling flags */
  prof->flags = opt_flagscode;
  prof->flags |= PAPI_PROFIL_BUCKET_32; /* hpc_hist_bucket */
    
  prof->bytesPerCntr = sizeof(hpc_hist_bucket); /* 4 */
  prof->bytesPerCodeBlk = 4;
  prof->scale = 0x8000;

  if ( (prof->scale * prof->bytesPerCodeBlk) != (65536 * 2) ) {
    DIE("fatal error: internal programming error - invalid profiling scale.");
  }
}


static void 
start_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs)
{
  int pcode;
  int i;

  if (!profdescs) { return; }
  
  /* Note: PAPI_sprofil() can profile only one event in an event set,
     though this function may be called on other events in the *same*
     event set to profile multiple events simultaneously.  The event
     set must be shared since PAPI will run only one event set at a
     time (PAPI_start()).  */

  /* 1. Prepare PAPI subsystem for profiling */  
  for (i = 0; i < profdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];

    if (opt_debug >= 1) { 
      MSG(stderr, "Calling PAPI_sprofil(): %s", prof->einfo.symbol);
    }
    
    pcode = PAPI_sprofil(prof->sprofs, prof->numsprofs, profdescs->eset, 
			 prof->ecode, prof->period, prof->flags);
    if (pcode != PAPI_OK) {
      DIE("fatal error: PAPI error (%d): %s.", pcode, PAPI_strerror(pcode));
    }
  }

  /* 2. Launch PAPI */
  if ((pcode = PAPI_start(profdescs->eset)) != PAPI_OK) {
    DIE("fatal error: PAPI error (%d): %s.", pcode, PAPI_strerror(pcode));
  }
}


static void init_sighandler(int sig);

static void 
init_sighandlers()
{
  init_sighandler(SIGINT);  /* Ctrl-C */
}


static void 
init_sighandler(int sig)
{
  if (signal(sig, SIG_IGN) != SIG_IGN) {
    signal(sig, hpcrun_sighandler);
  } 
  else {
    ERRMSG("Warning: Signal %d already has a handler.", sig);
  }
}


/****************************************************************************
 * Finalize profiling
 ****************************************************************************/

static void write_all_profiles(hpcrun_profiles_desc_t* profdesc, 
			       rtloadmap_t* rtmap);

static void stop_sysprof(hpcsys_profile_desc_vec_t* profdescs);

static void stop_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);
static void fini_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);
static void fini_papi_for_process();

static void fini_profdesc(hpcrun_profiles_desc_t** profdesc, 
			  int sharedprofdesc);


/*
 *  Finalize profiling for this process
 */
static void 
fini_process()
{
  if (opt_debug >= 1) { MSG(stderr, "*** fini_process ***"); }

  fini_thread(&hpc_profdesc, 0 /*is_thread*/);

  if (numPAPIEvents > 0) {
    fini_papi_for_process();
  }  
}


/*
 *  Finalize profiling for this thread and free profiling data.  See
 *  init_thread() for meaning of 'is_thread'.
 */
static void 
fini_thread(hpcrun_profiles_desc_t** profdesc, int is_thread)
{
  int sharedprofdesc = 0;
  
  if (opt_debug >= 1) { MSG(stderr, "*** fini_thread ***"); }

  /* Stop profiling */
  if (HPC_GET_PAPIPROFS(*profdesc)) {
    stop_papi_for_thread(HPC_GET_PAPIPROFS(*profdesc));
  }
  if (HPC_GET_SYSPROFS(*profdesc)) {
    stop_sysprof(HPC_GET_SYSPROFS(*profdesc));
  }

  /* Write data (if necessary) */
  write_all_profiles(*profdesc, rtloadmap);

  /* Finalize profiling subsystems and uninit descriptor */
  if (HPC_GET_PAPIPROFS(*profdesc)) {
    fini_papi_for_thread(HPC_GET_PAPIPROFS(*profdesc));
  }
  
  if (is_thread && opt_thread == HPCRUN_THREADPROF_ALL) {
    sharedprofdesc = 1; /* histogram buffers are shared */
  }
  fini_profdesc(profdesc, sharedprofdesc);
}


static void 
stop_sysprof(hpcsys_profile_desc_vec_t* profdescs)
{
  int ecode;

  /* Each call to sprofil will disable any profiling enabled by
     previous sprofil calls. */
  if ((ecode = sprofil(NULL, 0, NULL, 0)) != 0) {
#if 0
    DIE("fatal error: sprofil() error. %s.", strerror(errno));
#endif
  }
}


static void 
stop_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs)
{
  int pcode, i;
  long_long* values = NULL; // array the size of the eventset
  
  if ((pcode = PAPI_stop(profdescs->eset, values)) != PAPI_OK) {
#if 0
    DIE("fatal error: PAPI error (%d): %s.", pcode, PAPI_strerror(pcode));
#endif
  }

  /* Call PAPI_sprofil() with a 0 threshold to cleanup internal memory */
  for (i = 0; i < profdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];
    
    pcode = PAPI_sprofil(prof->sprofs, prof->numsprofs, profdescs->eset, 
			 prof->ecode, 0, prof->flags);
#if 0
    if (pcode != PAPI_OK) {
      DIE("fatal error: PAPI error (%d): %s.", pcode, PAPI_strerror(pcode));
    }
#endif
  }
}


static void 
fini_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs)
{
  /* No need to test for failures during PAPI calls -- we've already
     got the goods! */
  PAPI_cleanup_eventset(profdescs->eset);
  PAPI_destroy_eventset(&profdescs->eset);
  profdescs->eset = PAPI_NULL;
}


static void 
fini_papi_for_process()
{
  PAPI_shutdown();
}


static void 
fini_profdesc(hpcrun_profiles_desc_t** profdesc, int sharedprofdesc)
{
  int i, j;
  unsigned numSysEv = 0, numPapiEv = 0;
  
  if (!profdesc || !*profdesc) { return; }

  if (HPC_GET_SYSPROFS(*profdesc)) {
    numSysEv = HPC_GET_SYSPROFS(*profdesc)->size;
  }
  if (HPC_GET_PAPIPROFS(*profdesc)) {
    numPapiEv = HPC_GET_PAPIPROFS(*profdesc)->size;
  }

  /* 1a. Uninitialize system profdescs */
  for (i = 0; i < numSysEv; ++i) {
    hpcsys_profile_desc_t* prof = &HPC_GET_SYSPROFS(*profdesc)->vec[i];
    if (!sharedprofdesc) {
      for (j = 0; j < prof->numsprofs; ++j) {
	free(prof->sprofs[j].pr_base);
      }
      free(prof->sprofs);
    }
    prof->sprofs = NULL;
  }
  
  if (numSysEv > 0) {
    free(HPC_GET_SYSPROFS(*profdesc)->vec);
    free(HPC_GET_SYSPROFS(*profdesc));
  }

  /* 1b. Uninitialize papi profdescs */
  for (i = 0; i < numPapiEv; ++i) {
    hpcpapi_profile_desc_t* prof = &HPC_GET_PAPIPROFS(*profdesc)->vec[i];
    if (!sharedprofdesc) {
      for (j = 0; j < prof->numsprofs; ++j) {
	free(prof->sprofs[j].pr_base);
      }
      free(prof->sprofs);
    }
    prof->sprofs = NULL;
  }

  if (numPapiEv > 0) {
    free(HPC_GET_PAPIPROFS(*profdesc)->vec);
    free(HPC_GET_PAPIPROFS(*profdesc));
  }
  
  /* 1c. Uninitialize ofile */
  free((*profdesc)->ofile.fname);
  (*profdesc)->ofile.fname = NULL;
  
  /* 1d. Uninitialize profdesc */
  free(*profdesc);
  *profdesc = NULL;
}


/****************************************************************************
 * Write profile data
 ****************************************************************************/

static void write_module_profile(FILE* fp, rtloadmod_desc_t* mod,
				 hpcrun_profiles_desc_t* profdesc, 
				 int sprofidx);

static void write_event_hdr(FILE *fs, char* name, char* desc, 
			    uint64_t period);
static void write_sysevent_data(FILE *fs, hpcsys_profile_desc_t* prof,
				int sprofidx);
static void write_papievent_data(FILE *fp, hpcpapi_profile_desc_t* prof, 
			     int sprofidx);
static void write_event_data(FILE *fs, char* ename, hpc_hist_bucket* histo, 
			     uint64_t ncounters, unsigned int bytesPerCodeBlk);

static void write_string(FILE *fp, char *str);


/*
 *  Write profile data for this process.  See hpcrun.h for file format info.
 */
static void 
write_all_profiles(hpcrun_profiles_desc_t* profdesc, rtloadmap_t* rtmap)
{
  int i;
  FILE* fs;
  
  if (!profdesc->ofile.fname) {
    return;
  }

  fs = fopen(profdesc->ofile.fname, "w");
  if (fs == NULL) {
    DIE("fatal error: Could not open file '%s'.", profdesc->ofile.fname);
  }
  
  /* <header> */
  fwrite(HPCRUNFILE_MAGIC_STR, 1, HPCRUNFILE_MAGIC_STR_LEN, fs);
  fwrite(HPCRUNFILE_VERSION, 1, HPCRUNFILE_VERSION_LEN, fs);
  fputc(HPCRUNFILE_ENDIAN, fs);

  /* <loadmodule_list> */
  hpc_fwrite_le4(&(rtmap->count), fs);
  for (i = 0; i < rtmap->count; ++i) {
    write_module_profile(fs, &(rtmap->module[i]), profdesc, i);
  }
  
  fclose(fs);
}


static void 
write_module_profile(FILE* fs, rtloadmod_desc_t* mod,
		     hpcrun_profiles_desc_t* profdesc, int sprofidx)
{
  int i, numEv = 0;
  unsigned numSysEv = 0, numPapiEv = 0;
  
  if (opt_debug >= 2) { 
    MSG(stderr, "writing module %s (pid %d at offset %#"PRIx64")", 
	mod->name, getpid(), mod->offset); 
  }

  /* <loadmodule_name>, <loadmodule_loadoffset> */
  write_string(fs, mod->name);
  hpc_fwrite_le8(&(mod->offset), fs);

  /* <loadmodule_eventcount> */
  if (HPC_GET_SYSPROFS(profdesc)) { 
    numSysEv = HPC_GET_SYSPROFS(profdesc)->size;
    numEv += numSysEv; 
  }
  if (HPC_GET_PAPIPROFS(profdesc)) { 
    numPapiEv = HPC_GET_PAPIPROFS(profdesc)->size;
    numEv += numPapiEv; 
  }
  hpc_fwrite_le4(&numEv, fs);
  
  /* Event data */
  /*   <event_x_name> <event_x_description> <event_x_period> */
  /*   <event_x_data> */
  for (i = 0; i < numSysEv; ++i) {
    hpcsys_profile_desc_t* prof = &HPC_GET_SYSPROFS(profdesc)->vec[i];
    write_event_hdr(fs, prof->ename, prof->ename, prof->period);
    write_sysevent_data(fs, prof, sprofidx);
  }
  for (i = 0; i < numPapiEv; ++i) {
    hpcpapi_profile_desc_t* prof = &HPC_GET_PAPIPROFS(profdesc)->vec[i];
    write_event_hdr(fs, prof->einfo.symbol, prof->einfo.long_descr,
		    prof->period);
    write_papievent_data(fs, prof, sprofidx);
  }
}


static void 
write_event_hdr(FILE *fs, char* name, char* desc, uint64_t period)
{
  /* <event_x_name> <event_x_description> <event_x_period> */
  write_string(fs, name);
  write_string(fs, desc);
  hpc_fwrite_le8(&period, fs);
}


static void 
write_sysevent_data(FILE *fs, hpcsys_profile_desc_t* prof, int sprofidx)
{
  char* ename = prof->ename;
  struct prof* sprof = &(prof->sprofs[sprofidx]);
  hpc_hist_bucket* histo = (hpc_hist_bucket*)sprof->pr_base;
  uint64_t ncounters = (sprof->pr_size / prof->bytesPerCntr);
  
  write_event_data(fs, ename, histo, ncounters, prof->bytesPerCodeBlk);
}


static void 
write_papievent_data(FILE *fs, hpcpapi_profile_desc_t* prof, int sprofidx)
{
  char* ename = prof->einfo.symbol;
  PAPI_sprofil_t* sprof = &(prof->sprofs[sprofidx]);
  hpc_hist_bucket* histo = (hpc_hist_bucket*)sprof->pr_base;
  uint64_t ncounters = (sprof->pr_size / prof->bytesPerCntr);

  write_event_data(fs, ename, histo, ncounters, prof->bytesPerCodeBlk);
}


static void 
write_event_data(FILE *fs, char* ename, hpc_hist_bucket* histo, 
		 uint64_t ncounters, unsigned int bytesPerCodeBlk)
{
  uint64_t count = 0, offset = 0, i = 0, inz = 0;

  /* <histogram_non_zero_bucket_count> */
  count = 0;
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) { count++; inz = i; }
  }
  hpc_fwrite_le8(&count, fs);
  
  if (opt_debug >= 3) {
    MSG(stderr, "  profile for %s has %"PRIu64" of %"PRIu64" non-zero counters (last non-zero counter: %"PRIu64")", 
	ename, count, ncounters, inz);
  }
  
  /* <histogram_non_zero_bucket_x_value> 
     <histogram_non_zero_bucket_x_offset> */
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) {
      uint32_t cnt = histo[i];
      hpc_fwrite_le4(&cnt, fs);   /* count */

      offset = i * bytesPerCodeBlk;
      hpc_fwrite_le8(&offset, fs); /* offset (in bytes) from load addr */

      if (opt_debug >= 3) {
        MSG(stderr, "  (cnt,offset)=(%d,%"PRIx64")", cnt, offset);
      }
    }
  }
}


static void 
write_string(FILE *fs, char *str)
{
  /* <string_length> <string_without_terminator> */
  unsigned int len = strlen(str);
  hpc_fwrite_le4(&len, fs);
  fwrite(str, 1, len, fs);
}


/****************************************************************************/

/* hpcrun_gettid: return a thread id */
/* FIXME: return size_t or intptr_t */
static long
hpcrun_gettid()
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
handle_any_dlerror()
{
  /* Note: We assume dlsym() or something similar has just been called! */
  char *error;
  if ((error = dlerror()) != NULL) {
    DIE("fatal error: %s\n", error);
  }
}

