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
//    The PAPI Initialization code was originally adapted from parts of The
//    Visual Profiler by Curtis L. Janssen (vmon.c).
//    
*****************************************************************************/

/************************** System Include Files ****************************/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h> /* for 'PATH_MAX' */
#include <signal.h>
#include <inttypes.h>
#include <errno.h>

#ifndef __USE_GNU
# define __USE_GNU /* must define on Linux to get RTLD_NEXT from <dlfcn.h> */
# define SELF_DEFINED__USE_GNU
#endif

#include <dlfcn.h>

/**************************** User Include Files ****************************/

#include "hpcpapi.h" /* <papi.h>, etc. */

#include "hpcrun.h"
#include "map.h"
#include "io.h"

/**************************** Forward Declarations **************************/

/* Intercepted routines and signals */

#define START_MAIN_PARAMS (int (*main) (int, char **, char **), \
			   int argc, char *__unbounded *__unbounded ubp_av, \
			   void (*init) (void), void (*fini) (void), \
			   void (*rtld_fini) (void), \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) START_MAIN_PARAMS;
typedef void (*libc_start_main_fini_fptr_t) (void);

static int  hpcrun_libc_start_main START_MAIN_PARAMS;
static void hpcrun_libc_start_main_fini(); 


typedef pid_t (*fork_fptr_t) (void);
static pid_t hpcrun_fork();


/* catching signals */

static void hpcrun_sighandler(int sig);

/**************************** Forward Declarations **************************/

static void init_library();
static void fini_library();

static void init_process();
static void fini_process();

static void init_thread();
static void fini_thread();

typedef uint32_t hpc_hist_bucket; /* a 4 byte histogram bucket */

static const uint64_t default_period = (1 << 15) - 1; /* (2^15 - 1) */

/*************************** Variable Declarations **************************/

/* These variables are set when the library is initialized */

/* Intercepted libc routines */
static libc_start_main_fptr_t      real_start_main;
static libc_start_main_fini_fptr_t real_start_main_fini;
static fork_fptr_t real_fork;

/* hpcrun options (this should be a tidy struct) */
static int      opt_debug = 0;
static int      opt_recursive = 0;
static int      opt_thread = 0;
static char*    opt_eventlist = NULL;
static char     opt_outpath[PATH_MAX] = "";
static int      opt_flagscode = 0;

/*************************** Variable Declarations **************************/

/* These variables are set when libc_start_main is intercepted */

/* The profiled command and its run-time load map */
static char*        profiled_cmd;
static rtloadmap_t* rtloadmap;

/* System and PAPI based profiling information (one for each profile).
   Note that each segented-profile buffer should correspond to data in
   'rtloadmap'. */
static hpcrun_profile_desc_vec_t sys_profdescs;
static hpcpapi_profile_desc_vec_t papi_profdescs;

static unsigned numSysEvents = 0; /* estimate */
static unsigned numPAPIEvents= 0; /* estimate */

/* hpcrun output file */
static hpcrun_ofile_desc_t hpc_ofile;


/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

static void init_options();

/*
 *  Implicit interface
 */
void 
_init()
{
  init_library();
}

void 
_fini()
{
  fini_library();
}


/*
 *  Explicit interface
 */
void 
hpcrun_init()
{
  init_library();
}

void 
hpcrun_fini()
{
  fini_library();
}


/*
 *  Library initialization
 */
static void 
init_library()
{
  char *error;

  init_options();

  if (opt_debug >= 1) { 
    fprintf(stderr, "*** init_library ("HPCRUN_LIB", process %d) ***\n", 
	    getpid()); 
  }
  
  /* Grab pointers to functions that may be intercepted.

     Note: RTLD_NEXT is not documented in the dlsym man/info page
     but in <dlfcn.h>: 
  
     If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
     the run-time address of the symbol called NAME in the next shared
     object is returned.  The "next" relation is defined by the order
     the shared objects were loaded.  
  */

  /* from libc */
  real_start_main = (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main");
  if ((error = dlerror()) != NULL) {
    fputs(error, stderr);
    exit(1);
  }
  if (!real_start_main) {
    real_start_main = (libc_start_main_fptr_t)dlsym(RTLD_NEXT, 
					       "__BP___libc_start_main");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }
  if (!real_start_main) {
    DIE("fatal error: Cannot intercept beginning of process execution and therefore cannot begin profiling.");
  }
  
  real_fork = (fork_fptr_t)dlsym(RTLD_NEXT, "fork");
  if ((error = dlerror()) != NULL) {
    fputs(error, stderr);
    exit(1);
  }
  
#if 0 // THREAD
  /* from pthread, needed for PAPI */
  if (PROFILE_EACH_THREAD) {
    real_pthread_self = dlsym(RTLD_NEXT, "pthread_self");
    if (!real_pthread_self) {
      DIE("fatal error: Cannot profile thread.");
    }
  }
#endif
}


/*
 *  Library finalization
 */
static void 
fini_library()
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** fini_library ("HPCRUN_LIB", process %d) ***\n", 
	    getpid()); 
  }
}


static void
init_options()
{
  char *env_debug, *env_recursive, *env_thread, *env_eventlist, *env_outpath, 
    *env_flags;

  /* Debugging (get this first) : default is off */
  env_debug = getenv("HPCRUN_DEBUG");
  opt_debug = (env_debug ? atoi(env_debug) : 0);

  if (opt_debug >= 1) { fprintf(stderr, "** processing "HPCRUN_LIB" opts (process %d)\n", getpid()); }
  
  /* Recursive profiling: default is on */
  env_recursive = getenv("HPCRUN_RECURSIVE");
  opt_recursive = (env_recursive ? atoi(env_recursive) : 1);
  if (!opt_recursive) {
    /* turn off profiling for any processes spawned by this one */
    unsetenv("LD_PRELOAD"); 
    /* FIXME: just extract HPCRUN_LIB */
  }

  if (opt_debug >= 1) { 
    fprintf(stderr, "  LD_PRELOAD: %s (process %d)\n", getenv("LD_PRELOAD"), getpid()); 
    fprintf(stderr, "  recursive profiling: %d (process %d)\n", opt_recursive, getpid()); 
  }
  
  /* Threaded profiling: default is off */
  env_thread = getenv("HPCRUN_THREAD");
  opt_thread = (env_thread ? atoi(env_thread) : 0);

  /* Profiling event list: default PAPI_TOT_CYC:32767 (default_period) */
  opt_eventlist = "PAPI_TOT_CYC:32767"; 
  env_eventlist = getenv("HPCRUN_EVENT_LIST");
  if (env_eventlist) {
    opt_eventlist = env_eventlist;
  }

  if (opt_debug >= 1) { 
    fprintf(stderr, "  event list: %s (process %d)\n", opt_eventlist, getpid()); 
  }
  
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
int 
__libc_start_main START_MAIN_PARAMS
{
  hpcrun_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


int 
__BP___libc_start_main START_MAIN_PARAMS
{
  hpcrun_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


static int 
hpcrun_libc_start_main START_MAIN_PARAMS
{
  /* squirrel away for later use */
  profiled_cmd = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  real_start_main_fini = fini;
  
  /* initialize profiling for process */
  init_process();
  
  /* launch the process */
  if (opt_debug >= 1) {
    fprintf(stderr, "*** launching intercepted app: %s (process %d) ***\n", 
	    profiled_cmd, getpid());
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
 *  Intercept creation of new processes via fork(), vfork()
 */

pid_t 
fork()
{
  return hpcrun_fork();
}


pid_t 
vfork()
{
  return hpcrun_fork();
}


static pid_t 
hpcrun_fork()
{
  pid_t pid;

  if (opt_debug >= 1) { 
    fprintf(stderr, "** forking (process %d)\n", getpid()); 
  }
  
  pid = (*real_fork)();
  if (pid == 0) { /* child */
    if (opt_debug >= 1) { 
      fprintf(stderr, "** caught fork (process %d)\n", getpid()); 
    }
    if (hpc_init_papi_force() != 0) {
      fprintf(stderr, "** PAPI library initialization failed after fork (process %d)\n", getpid());
      exit(-1);
    }
    init_process();
  }
  
  return pid;
}


/*
 *  Intercept creation of new theads via pthread_create()
 */

#if 0
// THREAD
int pthread_create(pthread_t* thread, pthread_attr_t* attr, 
		    void * (*start_routine)(void *), void * arg)
{


}

static int 
hpcrun_pthread_create(...)
{

}


static int 
hpcrun_execute_pthread(...)
{
  // probe_thread_init_tramp
}

#endif 


/****************************************************************************
 * Intercepted signals
 ****************************************************************************/

/* We allow the user to kill profiling by intercepting the certain
   signals.  This can be very useful on long-running or misbehaving
   applications. */

static void 
hpcrun_sighandler(int sig)
{
  if (opt_debug >= 1) { fprintf(stderr, "*** catching signal %d (process %d) ***\n", sig, getpid()); }
  
  signal(sig, SIG_DFL); /* return to default action */
    
  switch (sig) {
  case SIGINT: {
    break;
  }
  default: 
    ERRMSG("Warning: Handling unknown signal %d.\n", sig);
    break;
  }
  
  hpcrun_fini();
  exit(0);
}


/****************************************************************************/
/****************************************************************************/

/****************************************************************************
 * Initialize profiling 
 ****************************************************************************/

static void count_events(unsigned* sysEvents, unsigned* papiEvents);

static void 
init_profdescs(hpcrun_profile_desc_vec_t* x, unsigned xpectedxSZ,
	       hpcpapi_profile_desc_vec_t* y, unsigned xpectedySZ,
	       rtloadmap_t* rtmap);

static void add_sysevent(hpcrun_profile_desc_vec_t* profdescs, 
			 rtloadmap_t* rtmap, int profidx, 
			 char* eventnm, uint64_t period);
static void start_sysprof(hpcrun_profile_desc_vec_t* profdescs);

static void init_papi_for_process();
static void add_papievent(hpcpapi_profile_desc_vec_t* profdescs, 
			  rtloadmap_t* rtmap, int profidx, 
			  char* eventnm, uint64_t period);
static void start_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);

static void init_output(hpcrun_ofile_desc_t* ofile,
			hpcrun_profile_desc_vec_t* xprofdescs,
			hpcpapi_profile_desc_vec_t* yprofdescs);

static void init_sighandlers();


/*
 *  Prepare for profiling this process
 */
static void 
init_process()
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** init_process ("HPCRUN_LIB", process %d) ***\n", 
	    getpid());
  }
  
  rtloadmap = hpcrun_code_lines_from_loadmap(opt_debug);
  
  /* Initialize PAPI if necessary */
  count_events(&numSysEvents, &numPAPIEvents); /* no error checking */
  if (numPAPIEvents > 0) {
    init_papi_for_process();
  }  
  
  init_thread();
}


/*
 *  Prepare profiling for this thread
 */
static void 
init_thread()
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** init_thread ("HPCRUN_LIB", process %d, tid %d) ***\n",
	    getpid(), 0);
  }

  /* Create profile info from event list.  Perform error checking. */
  init_profdescs(&sys_profdescs, numSysEvents, &papi_profdescs, numPAPIEvents, 
		 rtloadmap);

  // THREAD: PAPI_set_thr_specific

  /* init hpc_ofile and signal handlers */
  init_output(&hpc_ofile, &sys_profdescs, &papi_profdescs);

  init_sighandlers();
  
  if (sys_profdescs.size > 0) {
    start_sysprof(&sys_profdescs); // FIXME: threads?
  }
  if (papi_profdescs.size > 0) {
    start_papi_for_thread(&papi_profdescs);
  }
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
init_profdescs(hpcrun_profile_desc_vec_t* x, unsigned xpectedxSZ,
	       hpcpapi_profile_desc_vec_t* y, unsigned xpectedySZ,
	       rtloadmap_t* rtmap)
{
  /* PAPI should already be initialized if necessary */
  
  int xprofidx = -1, yprofidx = -1; /* nth prof event for x/y */
  const unsigned eventbufSZ = 128; /* really the last index, not the size */
  char eventbuf[eventbufSZ+1];
  char* tok, *tmp_eventlist;
  int rcode, i;

  if (opt_debug >= 1) { 
    fprintf(stderr, "Initializing profile descriptors\n");
    fprintf(stderr, "  Found %d sys events and %d PAPI events\n",
	    xpectedxSZ, xpectedySZ);
  }
  
  /* 1a. Ensure we do not profile both system and PAPI events. */
  if (xpectedxSZ > 0 && xpectedySZ > 0) {
    DIE("Cannot profile both WALLCLK and PAPI events at the same time. (Both use SIGPROF.)");
  }
  
  /* 1b. Ensure no more than one wall clock event is profiled.  (Only
     one appropriate itimer (ITIMER_PROF) is provided per process.) */ 
  if (xpectedxSZ > 1) {
    xpectedxSZ = 1;
  }
  
  /* 1c. Ensure we have enough hardware counters if using PAPI.  Note:
     PAPI cannot profile when using multiplexing. */
  {
    int numHwCntrs = PAPI_num_hwctrs();
    if (xpectedySZ > numHwCntrs) {
      ERRMSG("Warning: Too many events (%d) for hardware counters (%d).  Only using first %d events.", xpectedySZ, numHwCntrs, numHwCntrs);
      xpectedySZ = numHwCntrs;
    }
  }


  /* 2a. Initialize hpcrun profdescs */
  {
    unsigned int vecbufsz = sizeof(hpcrun_profile_desc_t) * xpectedxSZ;
    x->size = xpectedxSZ;
    x->vec = NULL;
    
    if (xpectedxSZ > 0) {
      x->vec = (hpcrun_profile_desc_t*)malloc(vecbufsz);
      if (!x->vec) {
	DIE("fatal error: malloc() failed!");
      }
      memset(x->vec, 0x00, vecbufsz);
    }
  }

  /* 2b. Initialize papi profdescs */
  {
    unsigned int vecbufsz = sizeof(hpcpapi_profile_desc_t) * xpectedySZ;
    y->size = xpectedySZ;
    y->vec = NULL;
    y->eset = PAPI_NULL; 
    
    if (xpectedySZ > 0) {
      y->vec = (hpcpapi_profile_desc_t*)malloc(vecbufsz);
      if (!y->vec) {
	DIE("fatal error: malloc() failed!");
      }
      memset(y->vec, 0x00, vecbufsz);
      
      if ((rcode = PAPI_create_eventset(&y->eset)) != PAPI_OK) {
	DIE("fatal error: (%d) PAPI error %s.", rcode, PAPI_strerror(rcode));
      }
      
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
      fprintf(stderr, "  Event: '%s' (%d) '%"PRIu64"' (process %d)\n",
	      eventbuf, evty, period, getpid());
    }
    
    /* Add the event to the appropriate place */
    if (evty == 1) {
      xprofidx++;
      add_sysevent(x, rtmap, xprofidx, eventbuf, period);
    } 
    else if (evty == 2) {
      yprofidx++;
      add_papievent(y, rtmap, yprofidx, eventbuf, period);
    } 
    else {
      DIE("fatal error: internal programming error - invalid event.");
    }
  }
  free(tmp_eventlist);

  /* N.B.: at this point x->sprofs an1d y->sprofs remains uninitialized */


  /* 4a. For each sys profdescs entry, init sprofil()-specific info */
  for (i = 0; i < x->size; ++i) {
    int mapi;
    unsigned int sprofbufsz = sizeof(struct prof) * rtmap->count;
    hpcrun_profile_desc_t* prof = &x->vec[i];
    
    prof->sprofs = (struct prof*)malloc(sprofbufsz);
    if (!prof->sprofs) {
      DIE("fatal error: malloc() failed!");
    }
    memset(prof->sprofs, 0x00, sprofbufsz);
    prof->numsprofs = rtmap->count;

    if (opt_debug >= 3) { 
      fprintf(stderr, "profile buffer details for %s (process %d):\n", 
	      prof->ename, getpid()); 
      fprintf(stderr, "  count = %d, sp=%"PRIu64" ef=%d\n",
	      prof->numsprofs, prof->period, prof->flags);
    }

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
      
      if (opt_debug >= 3) {
	fprintf(stderr, 
		"\tprofile[%d] base = %p size = %#x off = %#x scale = %#lx\n",
		mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
		prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
      }
    }
  }

  /* 4b. For each papi profdescs entry, init sprofil()-specific info */
  for (i = 0; i < y->size; ++i) {
    int mapi;
    unsigned int sprofbufsz = sizeof(PAPI_sprofil_t) * rtmap->count;
    hpcpapi_profile_desc_t* prof = &y->vec[i];
    
    prof->sprofs = (PAPI_sprofil_t*)malloc(sprofbufsz);
    if (!prof->sprofs) {
      DIE("fatal error: malloc() failed!");
    }
    memset(prof->sprofs, 0x00, sprofbufsz);
    prof->numsprofs = rtmap->count;

    if (opt_debug >= 3) { 
      fprintf(stderr, "profile buffer details for %s (process %d):\n", 
	      prof->einfo.symbol, getpid()); 
      fprintf(stderr, "  count = %d, es=%#x ec=%#x sp=%"PRIu64" ef=%d\n",
	      prof->numsprofs, y->eset, prof->ecode, prof->period, 
	      prof->flags);
    }

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
      
      if (opt_debug >= 3) {
	fprintf(stderr, 
		"\tprofile[%d] base = %p size = %#x off = %p scale = %#x\n",
		mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
		prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
      }
    }
  }
}


static void
add_sysevent(hpcrun_profile_desc_vec_t* profdescs, rtloadmap_t* rtmap,
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
  
  hpcrun_profile_desc_t* prof = NULL;
  
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
start_sysprof(hpcrun_profile_desc_vec_t* profdescs)
{
  int ecode;

  /* Note: should only be one profdesc! */
  hpcrun_profile_desc_t* prof = &profdescs->vec[0];

  if (opt_debug >= 1) { 
    fprintf(stderr, "Calling sprofil(): %s (process %d)\n", 
	    prof->ename, getpid());
  }
  
  ecode = sprofil(prof->sprofs, prof->numsprofs, NULL, prof->flags);
  if (ecode != 0) {
    DIE("fatal error: sprofil() error. %s.", strerror(errno));
  }
}


static void 
init_papi_for_process()
{  
  int pcode;
  int papi_debug = 0;

  /* Initialize papi */
  if (hpc_init_papi() != 0) {
    exit(-1);
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
    fprintf(stderr, "setting PAPI debug option %d. (process %d)\n", papi_debug, getpid());
    if ((pcode = PAPI_set_debug(papi_debug)) != PAPI_OK) {
      DIE("fatal error: (%d) PAPI error %s.", pcode, PAPI_strerror(pcode));
    }
  }
  
  //PAPI_set_domain(PAPI_DOM_ALL); // FIXME

#if 0 // THREAD
  // delay this to init_process and after papi_init
  if (PROFILING_PAPI_EVENTS && PROFILE_EACH_THREAD) {
    retval = PAPI_thread_init(thread_id_handle);
    if (retval != PAPI_OK)
      error("PAPI_thread_init",retval);
  }
#endif
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
	"\tCheck the list of supported events with 'hpcrun -L'.", eventnm);
  }
  if (PAPI_query_event(prof->ecode) != PAPI_OK) { 
    DIE("fatal error: PAPI_query_event for '%s' failed for unknown reason.", 
	eventnm);
  }
  if ((pcode = PAPI_get_event_info(prof->ecode, &prof->einfo)) != PAPI_OK) {
    DIE("fatal error: (%d) PAPI error %s.", pcode, PAPI_strerror(pcode));
  }
  
  /* NOTE: Although clumsy, this test has official sanction. */
  if ((prof->ecode & PAPI_PRESET_MASK) && (prof->einfo.count > 1) && 
      strcmp(prof->einfo.derived, "DERIVED_CMPD") != 0) {
    DIE("fatal error: '%s' is a PAPI derived event.\n"
	"\tSampling of derived events is not supported by PAPI.\n" 
	"\tUse 'hpcrun -L' to find the component native events of '%s' that you can monitor separately.", eventnm, eventnm);
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
  
  /* Note: PAPI_sprofil() can profile only one event in an event set,
     though this function may be called on other events in the *same*
     event set to profile multiple events simultaneously.  The event
     set must be shared since PAPI will run only one event set at a
     time (PAPI_start()).  */

  /* 1. Prepare PAPI subsystem for profiling */  
  for (i = 0; i < profdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];

    if (opt_debug >= 1) { 
      fprintf(stderr, "Calling PAPI_sprofil(): %s (process %d)\n", prof->einfo.symbol, getpid());
    }
    
    pcode = PAPI_sprofil(prof->sprofs, prof->numsprofs, profdescs->eset, 
			 prof->ecode, prof->period, prof->flags);
    if (pcode != PAPI_OK) {
      DIE("fatal error: (%d) PAPI error %s.", pcode, PAPI_strerror(pcode));
    }
  }

  /* 2. Launch PAPI */
  if ((pcode = PAPI_start(profdescs->eset)) != PAPI_OK) {
    DIE("fatal error: (%d) PAPI error %s.", pcode, PAPI_strerror(pcode));
  }
}


static void 
init_output(hpcrun_ofile_desc_t* ofile, hpcrun_profile_desc_vec_t* xprofdescs,
	    hpcpapi_profile_desc_vec_t* yprofdescs)
{
  static unsigned int outfilenmLen = PATH_MAX; /* never redefined */
  static unsigned int hostnmLen = 128;         /* never redefined */
  char outfilenm[outfilenmLen];
  char hostnm[hostnmLen];
  char* cmd = profiled_cmd; 
  char* event = NULL, *evetc = "", *slash = NULL;
  
  // THREAD: add <tid> tag
  
  /* Get components for constructing file name:
     <outpath>/<command>.<event1>.<hostname>.<pid> */
  
  /* <command> */
  slash = rindex(cmd, '/');
  if (slash) {
    cmd = slash + 1; /* basename of cmd */
  }
  
  /* <event1> */
  if (xprofdescs->size > 0) {
    event = xprofdescs->vec[0].ename;
  }
  if (!event && yprofdescs->size > 0) {
    event = yprofdescs->vec[0].einfo.symbol; /* first event name */
  }
  if ((xprofdescs->size + yprofdescs->size) > 1) {
    evetc = "-etc"; /* indicates unshown events */
  }
  
  /* <hostname> */
  gethostname(hostnm, hostnmLen);
  hostnm[hostnmLen-1] = '\0'; /* ensure NULL termination */

  
  /* Create file name */
  snprintf(outfilenm, outfilenmLen, "%s/%s.%s%s.%s.%d", 
	   opt_outpath, cmd, event, evetc, hostnm, getpid());
  
  ofile->fs = fopen(outfilenm, "w");
  if (ofile->fs == NULL) {
    DIE("fatal error: Failed to open output file '%s'.", outfilenm);
  }
  
  ofile->fname = NULL; // FIXME: skip setting ofile->fname for now
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
    ERRMSG("Warning: Signal %d already has a handler.\n", sig);
  }
}


/****************************************************************************
 * Finalize profiling
 ****************************************************************************/

static void write_all_profiles(hpcrun_ofile_desc_t* ofile, rtloadmap_t* rtmap,
			       hpcrun_profile_desc_vec_t* xprofdescs,
			       hpcpapi_profile_desc_vec_t* yprofdescs);

static void stop_sysprof(hpcrun_profile_desc_vec_t* profdescs);
static void fini_sysprof(hpcrun_profile_desc_vec_t* profdescs);

static void stop_papi(hpcpapi_profile_desc_vec_t* profdescs);
static void fini_papi(hpcpapi_profile_desc_vec_t* profdescs);


/*
 *  Finalize profiling for this thread
 */
static void 
fini_thread()
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** fini_thread ("HPCRUN_LIB", process %d, tid %d) ***\n",
	    getpid(), 0);
  }
  
  // THREAD: PAPI_get_thr_specific

  if (papi_profdescs.size > 0) {
    stop_papi(&papi_profdescs);
  }
  if (sys_profdescs.size > 0) {
    stop_sysprof(&sys_profdescs); // FIXME: for threads?
  }
  
  write_all_profiles(&hpc_ofile, rtloadmap, &sys_profdescs, &papi_profdescs);
  
  if (sys_profdescs.size > 0) {
    fini_sysprof(&sys_profdescs); /* uninit sys_profdescs */
  }
  if (papi_profdescs.size > 0) {
    fini_papi(&papi_profdescs);   /* uninit papi_profdescs */
  }
}


/*
 *  Finalize profiling for this process
 */
static void 
fini_process()
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** fini_process ("HPCRUN_LIB", process %d) ***\n", 
	    getpid());
  }

  fini_thread();
}


static void 
stop_sysprof(hpcrun_profile_desc_vec_t* profdescs)
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
fini_sysprof(hpcrun_profile_desc_vec_t* profdescs)
{
}


static void 
stop_papi(hpcpapi_profile_desc_vec_t* profdescs)
{
  int pcode;
  long_long* values = NULL; // array the size of the eventset
  
  if ((pcode = PAPI_stop(profdescs->eset, values)) != PAPI_OK) {
#if 0
    DIE("fatal error: (%d) PAPI error %s.", pcode, PAPI_strerror(pcode));
#endif
  }
}


static void 
fini_papi(hpcpapi_profile_desc_vec_t* profdescs)
{
  int i, j;

  /* No need to test for failures during PAPI calls -- we've already
     got the goods! */
  
  for (i = 0; i < profdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];
    
    for (j = 0; j < prof->numsprofs; ++j) {
      free(prof->sprofs[j].pr_base);
      prof->sprofs[j].pr_base = 0;
    }
    free(prof->sprofs);
    prof->sprofs = 0;
  }
  
  PAPI_cleanup_eventset(profdescs->eset);
  PAPI_destroy_eventset(&profdescs->eset);
  profdescs->eset = PAPI_NULL;
  
  free(profdescs->vec);
  profdescs->vec = 0;
  profdescs->size = 0;
  
  PAPI_shutdown();
}


/****************************************************************************
 * Write profile data
 ****************************************************************************/

static void write_module_profile(FILE* fp, rtloadmod_desc_t* mod,
				 hpcrun_profile_desc_vec_t* xprofdescs,
				 hpcpapi_profile_desc_vec_t* yprofdescs, 
				 int sprofidx);

static void write_event_hdr(FILE *fs, char* name, char* desc, 
			    uint64_t period);
static void write_sysevent_data(FILE *fs, hpcrun_profile_desc_t* prof,
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
write_all_profiles(hpcrun_ofile_desc_t* ofile, rtloadmap_t* rtmap,
		   hpcrun_profile_desc_vec_t* xprofdescs,
		   hpcpapi_profile_desc_vec_t* yprofdescs)
{
  int i;
  FILE* fs = ofile->fs;
    
  /* <header> */
  fwrite(HPCRUNFILE_MAGIC_STR, 1, HPCRUNFILE_MAGIC_STR_LEN, fs);
  fwrite(HPCRUNFILE_VERSION, 1, HPCRUNFILE_VERSION_LEN, fs);
  fputc(HPCRUNFILE_ENDIAN, fs);
  
  /* <loadmodule_list> */
#if 0  
  hpcpapi_profile_desc_t* prof = &yprofdescs->vec[i];
#endif

  hpc_fwrite_le4(&(rtmap->count), fs);
  for (i = 0; i < rtmap->count; ++i) {
    write_module_profile(fs, &(rtmap->module[i]), xprofdescs, yprofdescs, i);
  }
  
  fclose(fs);
  ofile->fs = NULL;
}


static void 
write_module_profile(FILE* fs, rtloadmod_desc_t* mod,
		     hpcrun_profile_desc_vec_t* xprofdescs,
		     hpcpapi_profile_desc_vec_t* yprofdescs, int sprofidx)
{
  int i, evcount;
  
  if (opt_debug >= 2) { fprintf(stderr, "writing module %s (process %d at offset %#"PRIx64")\n", mod->name, getpid(), mod->offset); }

  /* <loadmodule_name>, <loadmodule_loadoffset> */
  write_string(fs, mod->name);
  hpc_fwrite_le8(&(mod->offset), fs);

  /* <loadmodule_eventcount> */
  evcount = xprofdescs->size + yprofdescs->size;
  hpc_fwrite_le4(&evcount, fs);
  
  /* Event data */
  /*   <event_x_name> <event_x_description> <event_x_period> */
  /*   <event_x_data> */
  for (i = 0; i < xprofdescs->size; ++i) {
    hpcrun_profile_desc_t* prof = &xprofdescs->vec[i];
    write_event_hdr(fs, prof->ename, prof->ename, prof->period);
    write_sysevent_data(fs, prof, sprofidx);
  }
  for (i = 0; i < yprofdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &yprofdescs->vec[i];
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
write_sysevent_data(FILE *fs, hpcrun_profile_desc_t* prof, int sprofidx)
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
  
  if (opt_debug >= 2) {
    fprintf(stderr, "  profile for %s has %"PRIu64" of %"PRIu64" non-zero counters (process %d)", 
	    ename, count, ncounters, getpid());
    fprintf(stderr, " (last non-zero counter: %"PRIu64")\n", inz);
  }
  
  /* <histogram_non_zero_bucket_x_value> 
     <histogram_non_zero_bucket_x_offset> */
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) {
      uint32_t cnt = histo[i];
      hpc_fwrite_le4(&cnt, fs);   /* count */

      offset = i * bytesPerCodeBlk;
      hpc_fwrite_le8(&offset, fs); /* offset (in bytes) from load addr */

      if (opt_debug >= 2) {
        fprintf(stderr, "  (cnt,offset)=(%d,%"PRIx64")\n", cnt, offset);
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

