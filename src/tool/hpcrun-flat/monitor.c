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
// Copyright ((c)) 2002-2016, Rice University
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

/****************************************************************************
//
// File: 
//    $HeadURL$
//
// Purpose:
//    A library of routines for preparing and finalizing monitoring
//    for a process.  The library can determine a process' list of
//    load modules (including DSOs), prepare PAPI_sprofil for
//    profiling over each load module, finalize profiling and write
//    the data for later processing.
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
#include <stdio.h>
#include <stdarg.h>   /* va_arg */
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <limits.h>   /* for 'PATH_MAX' */
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

/**************************** User Include Files ****************************/

#include <include/hpctoolkit-config.h>
#include <include/uint.h>
#include <include/min-max.h>

#include "monitor.h"

#ifdef HAVE_MONITOR  
// FIXME: use libmonitor completely and include it
extern unsigned long monitor_gettid();
#endif

#include "hpcrun.h"
#include "hpcpapi.h" /* <papi.h>, etc. */
#include "rtmap.h"

#include <lib/prof-lean/hpcio.h>

/**************************** Forward Declarations **************************/

/* FIXME: this should be part of the data file */
typedef uint32_t hpc_hist_bucket; /* a 4 byte histogram bucket */

static const uint64_t default_period = 999999; /* not (2^15 - 1) */

/**************************** Forward Declarations **************************/

/* intercepted signals */
static void hpcrun_sighandler(int sig);

/*************************** Variable Declarations **************************/

/* This info is constant throughout the process and can therefore be
   shared among multiple threads. */

/* hpcrun options: set when the library is initialized */
int   opt_debug = 0;
int   opt_recursive = 0;
hpc_threadprof_t opt_thread = HPCRUN_THREADPROF_EACH;
char* opt_eventlist = NULL;
char  opt_outpath[PATH_MAX] = "";
char  opt_prefix[PATH_MAX] = "";
char  opt_file[PATH_MAX] = "";
int   opt_flagscode = 0;

/* monitored command: set when library or process is initialized  */
const char* hpcrun_cmd = NULL;

/* monitoring variables: set when the process is initialized */
static rtloadmap_t* rtloadmap = NULL;    /* run time load map */
static uint     numSysEvents  = 0;   /* estimate */
static uint     numPAPIEvents = 0;   /* estimate */

/* Profiling information for the first thread of execution in a
   process. N.B. The _shared_ profiling buffers live here when
   combining thread profiles. */
static hpcrun_profiles_desc_t* hpc_profdesc = NULL;

/* PAPI-specific variables */
static int domain = 0;

#define OPEN_OUTPUTFILE_AT_BEG 1

/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

static void init_option_debug();
static void init_options();

/*
 *  Library initialization
 */
extern void 
init_library()
{
  init_option_debug();
  
  if (opt_debug >= 1) { MSG0(stderr, "*** init_library ***"); }
  
  init_options();

#ifndef HAVE_MONITOR  
  init_library_SPECIALIZED();
#endif
}


/*
 *  Library finalization. Since this routine can be called more than
 *  once per process, ensure that it is idempotent.
 */
extern void 
fini_library()
{
  static int is_finalized = 0;
  if (is_finalized) {
    if (opt_debug >= 1) { MSG0(stderr, "*** fini_library (skip) ***"); }
    return;
  }
  
  is_finalized = 1;
  if (opt_debug >= 1) { MSG0(stderr, "*** fini_library ***"); }
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
  int is_opt_file, is_opt_prefix, is_opt_dir;
  is_opt_file = is_opt_prefix = is_opt_dir = 0; /* off be default */

  /* Handle HPCRUN_OPTIONS */
  char *opts = getenv("HPCRUN_OPTIONS");
  if (opts && (strlen(opts)>0)) {
    char* tmp = strtok(opts,", ");
    if (tmp) {
      do {
        if (strcmp(tmp,"USER") == 0)
	  domain |= PAPI_DOM_USER;
        else if (strcmp(tmp,"KERNEL") == 0)
          domain |= PAPI_DOM_KERNEL;
        else if (strcmp(tmp,"OTHER") == 0)
          domain |= PAPI_DOM_OTHER;
        else if (strcmp(tmp,"SUPERVISOR") == 0)
          domain |= PAPI_DOM_SUPERVISOR;
	else if (strcmp(tmp, "FILE") == 0)
	  is_opt_file = 1;
	else if (strcmp(tmp, "PREFIX") == 0)
	  is_opt_prefix = 1;
	else if (strcmp(tmp, "DIR") == 0)
	  is_opt_dir = 1;
      } while ((tmp = strtok(NULL,",")) != NULL);
    } 
  }
  
  if (opt_debug >= 1) { 
    MSGx(stderr, "  LD_PRELOAD: %s", getenv("LD_PRELOAD")); 
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
    MSGx(stderr, "  recursive profiling: %d", opt_recursive); 
  }
  
  /* Threaded profiling: default is off */
  env_thread = getenv("HPCRUN_THREAD");
  if (env_thread) {
    opt_thread = (hpc_threadprof_t)atoi(env_thread);
  }
  
  /* Profiling event list: default PAPI_TOT_CYC:999999 (default_period) */
  opt_eventlist = "PAPI_TOT_CYC:999999"; 
  env_eventlist = getenv("HPCRUN_EVENT_LIST");
  if (env_eventlist) {
    opt_eventlist = env_eventlist;
  }

  if (opt_debug >= 1) { MSGx(stderr, "  event list: %s", opt_eventlist); }
  
  /* Output path: default . */
  env_outpath = getenv("HPCRUN_OUTPUT");
  if (is_opt_dir && env_outpath) {
    strncpy(opt_outpath, env_outpath, PATH_MAX);
    if(mkdir(opt_outpath, 0755))
      if (errno != EEXIST) {
        DIEx("error: mkdir(%s) failed. %s\n", opt_outpath, strerror(errno));
      }
  }
  else
    strcpy(opt_outpath, ".");

  /* user-supplied prefix */
  if (is_opt_prefix && env_outpath)
    strncpy(opt_prefix, env_outpath, PATH_MAX);

  /* user-supplied file name */
  if (is_opt_file && env_outpath)
    strncpy(opt_file, env_outpath, PATH_MAX);
  
  /* Profiling flags: default PAPI_PROFIL_POSIX */
  {
    const hpcpapi_flagdesc_t *f = hpcpapi_flag_by_name("PAPI_PROFIL_POSIX");
    opt_flagscode = f->code;

    env_flags = getenv("HPCRUN_EVENT_FLAG");
    if (env_flags) {
      char *ptr = NULL, *token = NULL;
      token = strtok_r(env_flags,",:",&ptr);
      while (token) {
	if ((f = hpcpapi_flag_by_name(token)) == NULL) {
	  DIEx("error: Invalid profiling flag '%s'.", token);
	}
	opt_flagscode |= f->code;
	if (opt_debug >=1) {
	  MSGx(stderr, "  flag: %s, 0x%x, 0x%x", token, f->code, opt_flagscode);
	}
	token = strtok_r(NULL,",:",&ptr);
      }
    }
  }
}


/****************************************************************************
 * Intercepted routines 
 ****************************************************************************/

/* none for now */

/****************************************************************************
 * Intercepted signals
 ****************************************************************************/

/* We allow the user to kill profiling by intercepting the certain
   signals.  This can be very useful on long-running or misbehaving
   applications. */

static void 
hpcrun_sighandler(int sig)
{
  if (opt_debug >= 1) { MSGx(stderr, "*** catching signal %d ***", sig); }
  
  signal(sig, SIG_DFL); /* return to default action */
    
  switch (sig) {
  case SIGINT:
  case SIGABRT: {
    break;
  }
  default: 
    MSGx(stderr, "warning: Handling unknown signal %d.", sig);
    break;
  }
  
  fini_process();
  fini_library();
  
  signal(SIGABRT, SIG_DFL); /* return SIGABRT to default before abort()! */
  abort();
}


/****************************************************************************/
/****************************************************************************/

/****************************************************************************
 * Initialize profiling 
 ****************************************************************************/

static void 
count_events(uint* sysEvents, uint* papiEvents);

static void 
init_profdesc(hpcrun_profiles_desc_t** profdesc, 
	      uint numSysEv, uint numPapiEv, 
	      rtloadmap_t* rtmap,
	      hpcrun_profiles_desc_t* sharedprofdesc);

static void 
init_sysprofdesc_buffer(hpcsys_profile_desc_vec_t* profdesc, 
			uint numEv, rtloadmap_t* rtmap,
			hpcsys_profile_desc_vec_t* sharedprofdesc);
static void 
append_sysprofdesc_buffer(hpcsys_profile_desc_vec_t* profdesc, 
			uint numEv, rtloadmap_t* rtmap,
			hpcsys_profile_desc_vec_t* sharedprofdesc);
static void 
init_papiprofdesc_buffer(hpcpapi_profile_desc_vec_t* profdesc, 
			 uint numEv, rtloadmap_t* rtmap,
			 hpcpapi_profile_desc_vec_t* sharedprofdesc);
static void 
append_papiprofdesc_buffer(hpcpapi_profile_desc_vec_t* profdesc, 
			   uint numEv, rtloadmap_t* rtmap,
			   hpcpapi_profile_desc_vec_t* sharedprofdesc);

static void 
init_profdesc_ofile(hpcrun_profiles_desc_t* profdesc, 
		    int sharedprofdesc);

static void
notify_ofile(hpcrun_profiles_desc_t* profdesc, 
	     hpcrun_profiles_desc_t* sharedprofdesc);

static void 
add_sysevent(hpcsys_profile_desc_vec_t* profdescs, 
	     rtloadmap_t* rtmap, int profidx, 
	     char* eventnm, uint64_t period);

static void 
start_sysprof(hpcsys_profile_desc_vec_t* profdescs);

static void 
init_papi_for_process();

static void 
add_papievent(hpcpapi_profile_desc_vec_t* profdescs, 
	      rtloadmap_t* rtmap, int profidx, 
	      char* eventnm, uint64_t period);

static void 
start_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);

static void 
init_sighandlers();


/* Stop profiling */
static void 
stop_sysprof(hpcsys_profile_desc_vec_t* profdescs);

static void 
stop_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);



volatile int DEBUGGER_WAIT = 1;

/*
 *  Prepare for profiling this process
 */
extern void 
init_process()
{
  if (getenv("HPCRUN_WAIT")){
    while(DEBUGGER_WAIT);
  }

  if (opt_debug >= 1) { MSG0(stderr, "*** init_process ***"); }

  rtloadmap = hpcrun_get_rtloadmap(opt_debug);

  /* Initialize PAPI if necessary */
  count_events(&numSysEvents, &numPAPIEvents); /* no error checking */
  if (numPAPIEvents > 0) {
    init_papi_for_process();
  }

  hpc_profdesc = init_thread(0 /*is_thread*/);
}

/*
 * Called after the loading of a module using dlopen() to update the
 * profiling tables
 *
 */
extern void
handle_dlopen()
{
  if (hpc_profdesc == NULL) {
    DIE0("dlopen before process initialization!");
  }

  /* Stop profiling */
  stop_papi_for_thread(HPC_GET_PAPIPROFS(hpc_profdesc));
  stop_sysprof(HPC_GET_SYSPROFS(hpc_profdesc));
	
  /* Clear the rtloadmap */
  free(rtloadmap->module);

  /* Get the new module(s) from /proc/pid/maps */
  rtloadmap = hpcrun_get_rtloadmap(opt_debug);
  
  /* Determine if profile data needs to be shared across threads */
  hpcrun_profiles_desc_t* sharedprofdesc = NULL;
  if (opt_thread == HPCRUN_THREADPROF_ALL)
    sharedprofdesc = hpc_profdesc;
  
  /* For each sys profdescs entry, append sprofil()-specific info */
  if (HPC_GET_SYSPROFS(hpc_profdesc)) {
    hpcsys_profile_desc_vec_t* sh = 
      (sharedprofdesc) ? HPC_GET_SYSPROFS(sharedprofdesc) : NULL;
    append_sysprofdesc_buffer(HPC_GET_SYSPROFS(hpc_profdesc), numSysEvents, 
			      rtloadmap, sh);
  }

  /* For each papi profdescs entry, append sprofil()-specific info */
  if (HPC_GET_PAPIPROFS(hpc_profdesc)) {
    hpcpapi_profile_desc_vec_t* sh = 
      (sharedprofdesc) ? HPC_GET_PAPIPROFS(sharedprofdesc) : NULL;
    append_papiprofdesc_buffer(HPC_GET_PAPIPROFS(hpc_profdesc), numPAPIEvents,
			       rtloadmap, sh);
  }

  /* Restart profiling */
  if (HPC_GET_SYSPROFS(hpc_profdesc)) {
    start_sysprof(HPC_GET_SYSPROFS(hpc_profdesc));
  }
  if (HPC_GET_PAPIPROFS(hpc_profdesc)) {
    start_papi_for_thread(HPC_GET_PAPIPROFS(hpc_profdesc));
  }
  if (opt_debug >= 1) { MSG0(stderr, "*** dlopen handling complete ***"); }
}


/*
 *  Prepare profiling for this thread of execution.  N.B.: the caller
 *  must keep the returned data structure safe!  (The boolean argument
 *  'is_thread' refers to whether we are in an actual thread, i.e. not
 *  simply a the execution of a process.)
 */
extern hpcrun_profiles_desc_t* 
init_thread(int is_thread)
{
  hpcrun_profiles_desc_t* profdesc = NULL, *sharedprofdesc = NULL;
  
  if (opt_debug >= 1) { MSG0(stderr, "*** init_thread ***"); }
  
  /* Create profile info from event list and perform error checking. */
  if (is_thread && opt_thread == HPCRUN_THREADPROF_ALL) {
    sharedprofdesc = hpc_profdesc; /* share the histogram buffers */
  }
  init_profdesc(&profdesc, numSysEvents, numPAPIEvents, rtloadmap, 
		sharedprofdesc);
  
#if (OPEN_OUTPUTFILE_AT_BEG)
  /* Init file info if necessary. */
  init_profdesc_ofile(profdesc, (sharedprofdesc != NULL));
  notify_ofile(profdesc, sharedprofdesc);
#endif

  /* Init signal handlers */
  init_sighandlers();

  /* Launch profilers */
  if (HPC_GET_SYSPROFS(profdesc)) {
    start_sysprof(HPC_GET_SYSPROFS(profdesc));
  }
  if (HPC_GET_PAPIPROFS(profdesc)) {
    if (opt_debug >= 3) {
      dump_hpcpapi_profile_desc_vec(HPC_GET_PAPIPROFS(profdesc));
    }
    start_papi_for_thread(HPC_GET_PAPIPROFS(profdesc));
  }
  
  return profdesc;
}


static void
count_events(uint* sysEvents, uint* papiEvents)
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
	      uint numSysEv, uint numPapiEv, rtloadmap_t* rtmap,
	      hpcrun_profiles_desc_t* sharedprofdesc)
{
  /* PAPI should already be initialized if necessary */
  
  int xprofidx = -1, yprofidx = -1; /* nth prof event for x/y */
  const uint eventbufSZ = 128; /* really the last index, not the size */
  char eventbuf[eventbufSZ+1];
  char* tok, *tmp_eventlist;
  int rval, i;
      
  if (opt_debug >= 1) { 
    MSG0(stderr, "Initializing profile descriptors");
    MSGx(stderr, "  Found %d sys events and %d PAPI events", 
	 numSysEv, numPapiEv);
  }
  
  /* 1a. Ensure we do not profile both system and PAPI events. */
  if (numSysEv > 0 && numPapiEv > 0) {
    DIE0("Cannot profile both WALLCLK and PAPI events at the same time. (Both use SIGPROF.)");
  }
  
  /* 1b. Ensure no more than one wall clock event is profiled.  (Only
     one appropriate itimer (ITIMER_PROF) is provided per process.) */ 
  if (numSysEv > 1) {
    numSysEv = 1;
  }

  /* 1c. Ensure that we do not use system profile with threads */ 
  if (numSysEv > 0 && opt_thread) {
    DIE0("Cannot profile WALLCLK on multithreaded process. (sprofil() limitation.)");
  }
  
  /* 1d. Ensure we have enough hardware counters if using PAPI.  Note:
     PAPI cannot profile when using multiplexing. */
  {
    int numHwCntrs = PAPI_num_hwctrs();
    if (numPapiEv > numHwCntrs) {
      MSGx(stderr, "warning: Too many events (%d) for hardware counters (%d).  Only using first %d events.", numPapiEv, numHwCntrs, numHwCntrs);
      numPapiEv = numHwCntrs;
    }
  }

  
  /* 2a. Initialize profdesc */
  *profdesc = (hpcrun_profiles_desc_t*)malloc(sizeof(hpcrun_profiles_desc_t));
  if (!(*profdesc)) { DIE0("error: malloc() failed!"); }
  memset(*profdesc, 0x00, sizeof(hpcrun_profiles_desc_t));
  
  /* 2b. Initialize system profdescs */
  if (numSysEv > 0) {
    uint vecsz, sz = sizeof(hpcsys_profile_desc_vec_t);

    HPC_GETL_SYSPROFS(*profdesc) = (hpcsys_profile_desc_vec_t*)malloc(sz);
    if (!HPC_GET_SYSPROFS(*profdesc)) { 
      DIE0("error: malloc() failed!"); 
    }

    vecsz = sizeof(hpcsys_profile_desc_t) * numSysEv;    
    HPC_GET_SYSPROFS(*profdesc)->size = numSysEv;    
    
    HPC_GET_SYSPROFS(*profdesc)->vec = (hpcsys_profile_desc_t*)malloc(vecsz);
    if (!HPC_GET_SYSPROFS(*profdesc)->vec) { 
      DIE0("error: malloc() failed!"); 
    }
    memset(HPC_GET_SYSPROFS(*profdesc)->vec, 0x00, vecsz);
  }
  
  /* 2c. Initialize papi profdescs */
  if (numPapiEv > 0) {
    uint vecsz, sz = sizeof(hpcpapi_profile_desc_vec_t);
    
    HPC_GETL_PAPIPROFS(*profdesc) = (hpcpapi_profile_desc_vec_t*)malloc(sz);
    if (!HPC_GET_PAPIPROFS(*profdesc)) { 
      DIE0("error: malloc() failed!"); 
    }

    vecsz = sizeof(hpcpapi_profile_desc_t) * numPapiEv;
    HPC_GET_PAPIPROFS(*profdesc)->size = numPapiEv;    
    
    HPC_GET_PAPIPROFS(*profdesc)->vec = (hpcpapi_profile_desc_t*)malloc(vecsz);
    if (!HPC_GET_PAPIPROFS(*profdesc)->vec) { 
      DIE0("error: malloc() failed!"); 
    }
    memset(HPC_GET_PAPIPROFS(*profdesc)->vec, 0x00, vecsz);

    HPC_GET_PAPIPROFS(*profdesc)->eset = PAPI_NULL;     
    rval = PAPI_create_eventset(&HPC_GET_PAPIPROFS(*profdesc)->eset);
    if (rval != PAPI_OK) {
      DIEx("error: PAPI_create_eventset (%d): %s.", rval, PAPI_strerror(rval));
    }
  }

  
  /* 3. For each event:period pair, init corresponding profdescs
     entry.  Classification of events *must* be the same as count_events(). */
  tmp_eventlist = strdup(opt_eventlist);
  tok = strtok(tmp_eventlist, ";");
  for (i = 0; (tok != NULL); i++, tok = strtok((char*)NULL, ";")) {
    uint64_t period = 0;
    char* dlm;
    uint evty = 0; /* 1 is system; 2 is papi */
    
    // Extract event field from token. 
    //   'dlm' points to the optional period delimiter (a colon), if
    //   available; search from the end of the string in case the event
    //   name itself has colon.
    dlm = strrchr(tok, ':');
    if (dlm) {
      if (isdigit(dlm[1])) { // assume this is the period
	uint len = MIN(dlm - tok, eventbufSZ);
	strncpy(eventbuf, tok, len);
	eventbuf[len] = '\0';
      }
      else {
	dlm = NULL; // it's not the period; fall through
      }
    }
    if (!dlm) { // the fall through, not the 'else'!
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
    
    // Extract period field from token
    if (dlm) {
      period = strtol(dlm+1, (char **)NULL, 10);
    }
    else if (evty == 1) {
      period = 0;
    }
    else if (evty == 2) {
      period = default_period;
    }
    
    if (opt_debug >= 1) { 
      MSGx(stderr, "  Event: '%s' (%d) '%"PRIu64"'", eventbuf, evty, period);
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
      DIE0("error: internal programming error - invalid event.");
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
}


static void
init_sysprofdesc_buffer(hpcsys_profile_desc_vec_t* profdesc, 
			uint numEv, rtloadmap_t* rtmap,
			hpcsys_profile_desc_vec_t* sharedprofdesc)
{
  int i;

  for (i = 0; i < numEv; ++i) {
    int mapi;
    uint sprofbufsz = sizeof(struct prof) * rtmap->count;
    hpcsys_profile_desc_t* prof = &profdesc->vec[i];
    
    if (sharedprofdesc) {
      prof->sprofs = sharedprofdesc->vec[i].sprofs;
      prof->numsprofs = sharedprofdesc->vec[i].numsprofs;
    }
    else {
      prof->sprofs = (struct prof*)malloc(sprofbufsz);
      if (!prof->sprofs) { DIE0("error: malloc() failed!"); }
      memset(prof->sprofs, 0x00, sprofbufsz);
      prof->numsprofs = rtmap->count;
    }
      
    if (opt_debug >= 4) { 
      MSGx(stderr, "profile buffer details for %s:", prof->ename); 
      MSGx(stderr, "  count = %d, sp=%"PRIu64" ef=%d",
	   prof->numsprofs, prof->period, prof->flags);
    }
    
    if (sharedprofdesc) {
      /* print msg */
    }
    else {
      for (mapi = 0; mapi < rtmap->count; ++mapi) {
	uint bufsz;
	uint ncntr;
	
	/* eliminate use of ceil() (link with libm) by adding 1 */
	ncntr = (rtmap->module[mapi].length / prof->bytesPerCodeBlk) + 1;
	bufsz = ncntr * prof->bytesPerCntr;
	
	/* buffer base and size */
	prof->sprofs[mapi].pr_base = (void*)malloc(bufsz);
	prof->sprofs[mapi].pr_size = bufsz;
	if (!prof->sprofs[mapi].pr_base) { 
	  DIE0("error: malloc() failed!"); 
	}
	memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);
	
	/* pc offset and scaling factor */
	prof->sprofs[mapi].pr_off = rtmap->module[mapi].offset;
	prof->sprofs[mapi].pr_scale = prof->scale;      
	
	if (opt_debug >= 4) {
	  /* 'pr_size'/'pr_off' are of type 'size_t' which is of pointer size */
	  MSGx(stderr, "\tprofile[%d] base = %p size = %#"PRIxPTR" off = %#"PRIxPTR" scale = %#lx",
	      mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
	      prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
	}
      }
    }
  }
}

static void
append_sysprofdesc_buffer(hpcsys_profile_desc_vec_t* profdesc, 
			  uint numEv, rtloadmap_t* rtmap,
			  hpcsys_profile_desc_vec_t* sharedprofdesc)
{
  int i;
  
  for (i = 0; i < numEv; ++i) {
    int mapi;
    uint sprofbufsz = sizeof(struct prof) * rtmap->count;
    hpcsys_profile_desc_t* prof = &profdesc->vec[i];
    uint oldcount = prof->numsprofs;
    
    if (!sharedprofdesc) {
      prof->sprofs = (struct prof*)realloc(prof->sprofs, sprofbufsz);
      if (!prof->sprofs) { DIE0("error: realloc() failed!"); }
      memset(&(prof->sprofs[oldcount]), 0x00, sprofbufsz-(oldcount*sizeof(PAPI_sprofil_t)));
      prof->numsprofs = rtmap->count;
    }
    
    if (opt_debug >= 4) { 
      MSGx(stderr, "profile buffer details for %s:", prof->ename); 
      MSGx(stderr, "  count = %d, sp=%"PRIu64" ef=%d",
	   prof->numsprofs, prof->period, prof->flags);
    }
    
    if (sharedprofdesc) {
      /* print msg */
    }
    else {
      for (mapi = oldcount; mapi < rtmap->count; ++mapi) {
	uint bufsz;
	uint ncntr;
	
	/* eliminate use of ceil() (link with libm) by adding 1 */
	ncntr = (rtmap->module[mapi].length / prof->bytesPerCodeBlk) + 1;
	bufsz = ncntr * prof->bytesPerCntr;
	
	/* buffer base and size */
	prof->sprofs[mapi].pr_base = (void*)malloc(bufsz);
	prof->sprofs[mapi].pr_size = bufsz;
	if (!prof->sprofs[mapi].pr_base) { 
	  DIE0("error: malloc() failed!"); 
	}
	memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);
	
	/* pc offset and scaling factor */
	prof->sprofs[mapi].pr_off = rtmap->module[mapi].offset;
	prof->sprofs[mapi].pr_scale = prof->scale;      
	
	if (opt_debug >= 4) {
	  /* 'pr_size'/'pr_off' are of type 'size_t' which is of pointer size */
	  MSGx(stderr, "\tprofile[%d] base = %p size = %#"PRIxPTR" off = %#"PRIxPTR" scale = %#lx",
	       mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
	       prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
	}
      }
    }
  }
}

static void
init_papiprofdesc_buffer(hpcpapi_profile_desc_vec_t* profdesc, 
			 uint numEv, rtloadmap_t* rtmap,
			 hpcpapi_profile_desc_vec_t* sharedprofdesc)
{
  int i;

  for (i = 0; i < numEv; ++i) {
    int mapi;
    uint sprofbufsz = sizeof(PAPI_sprofil_t) * rtmap->count;
    hpcpapi_profile_desc_t* prof = &profdesc->vec[i];
    
    if (sharedprofdesc) {
      prof->sprofs = sharedprofdesc->vec[i].sprofs;
      prof->numsprofs = sharedprofdesc->vec[i].numsprofs;      
    }
    else {
      prof->sprofs = (PAPI_sprofil_t*)malloc(sprofbufsz);
      if (!prof->sprofs) { DIE0("error: malloc() failed!"); }
      memset(prof->sprofs, 0x00, sprofbufsz);
      prof->numsprofs = rtmap->count;
      
    }
    
    if (opt_debug >= 4) { 
      MSGx(stderr, "profile buffer details for %s:", prof->einfo.symbol); 
      MSGx(stderr, "  count = %d, es=%#x ec=%#x sp=%"PRIu64" ef=%d",
	   prof->numsprofs, profdesc->eset, 
	   prof->ecode, prof->period, prof->flags);
    }
    
    if (sharedprofdesc) {
      /* print msg */
    }
    else {
      for (mapi = 0; mapi < rtmap->count; ++mapi) {
	uint bufsz;
	uint ncntr;
	
	/* eliminate use of ceil() (link with libm) by adding 1 */
	ncntr = (rtmap->module[mapi].length / prof->bytesPerCodeBlk) + 1;
	bufsz = ncntr * prof->bytesPerCntr;
	
	/* buffer base and size */
	prof->sprofs[mapi].pr_base = (void*)malloc(bufsz);
	prof->sprofs[mapi].pr_size = bufsz;
	if (!prof->sprofs[mapi].pr_base) {
	  DIE0("error: malloc() failed!");
	}
	memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);
	
	/* pc offset and scaling factor (note: 'caddr_t' is a 'void*') */
	prof->sprofs[mapi].pr_off = 
	  (caddr_t)(uintptr_t)rtmap->module[mapi].offset;
	prof->sprofs[mapi].pr_scale = prof->scale;
	
	if (opt_debug >= 4) {
	  MSGx(stderr, 
	       "\tprofile[%d] base = %p size = %#x off = %p scale = %#x",
	       mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
	       prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
	}
      }
    }
  }  
}

static void
append_papiprofdesc_buffer(hpcpapi_profile_desc_vec_t* profdesc, 
			   uint numEv, rtloadmap_t* rtmap,
			   hpcpapi_profile_desc_vec_t* sharedprofdesc)
{
  int i;

  for (i = 0; i < numEv; ++i) {
    int mapi;
    uint sprofbufsz = sizeof(PAPI_sprofil_t) * rtmap->count;
    hpcpapi_profile_desc_t* prof = &profdesc->vec[i];
    uint oldcount = prof->numsprofs;
		
    if (!sharedprofdesc) {
      prof->sprofs = (PAPI_sprofil_t*)realloc(prof->sprofs, sprofbufsz);
      if (!prof->sprofs) { DIE0("error: realloc() failed!"); }
      memset(&(prof->sprofs[oldcount]), 0x00, sprofbufsz-(oldcount*sizeof(PAPI_sprofil_t)));
      prof->numsprofs = rtmap->count;
    }
    
    if (opt_debug >= 4) { 
      MSGx(stderr, "profile buffer details for %s:", prof->einfo.symbol); 
      MSGx(stderr, "  count = %d, es=%#x ec=%#x sp=%"PRIu64" ef=%d",
	   prof->numsprofs, profdesc->eset, 
	   prof->ecode, prof->period, prof->flags);
    }
    
    if (sharedprofdesc) {
      /* print msg */
    }
    else {
      for (mapi = oldcount; mapi < rtmap->count; ++mapi) {
	uint bufsz;
	uint ncntr;
	
	/* eliminate use of ceil() (link with libm) by adding 1 */
	ncntr = (rtmap->module[mapi].length / prof->bytesPerCodeBlk) + 1;
	bufsz = ncntr * prof->bytesPerCntr;
	
	/* buffer base and size */
	prof->sprofs[mapi].pr_base = (void*)malloc(bufsz);
	prof->sprofs[mapi].pr_size = bufsz;
	if (!prof->sprofs[mapi].pr_base) {
	  DIE0("error: malloc() failed!");
	}
	memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);
	
	/* pc offset and scaling factor (note: 'caddr_t' is a 'void*') */
	prof->sprofs[mapi].pr_off = 
	  (caddr_t)(uintptr_t)rtmap->module[mapi].offset;
	prof->sprofs[mapi].pr_scale = prof->scale;
	
	if (opt_debug >= 4) {
	  MSGx(stderr, 
	       "\tprofile[%d] base = %p size = %#x off = %p scale = %#x",
	       mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
	       prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
	}
      }
    }
  }
  
  //dump_hpcpapi_profile_desc_vec(HPC_GET_PAPIPROFS(profdesc));
}


// This function checks the existence of path,
// if it doesn't exist, 0 is returned, else, the
// next available generation is returned
static int get_next_gen(const char *path) {
  struct stat buf;
  char tmp_path[PATH_MAX];
  strcpy(tmp_path, path);
  int rc = stat(tmp_path, &buf);
  if ((rc<0) && (errno==ENOENT)) return 0;
  int inst = 0;
  do {
    inst++;
    strcpy(tmp_path, path);
    sprintf(tmp_path, "%s.%d", path, inst);
    rc = stat(tmp_path, &buf);
  } while (!((rc<0) && (errno==ENOENT))); 
  return inst;
}


static void 
init_profdesc_ofile(hpcrun_profiles_desc_t* profdesc, int sharedprofdesc)
{
  /* Perform a filesystem test to make sure we will be able to write
     output data. */

  static uint outfilenmLen = PATH_MAX; /* never redefined */
  static uint hostnmLen = 128;         /* never redefined */
  char outfilenm[outfilenmLen];
  char hostnm[hostnmLen];
  const char* cmd = hpcrun_cmd; 
  char* slash = NULL;
  //uint numEvents = 0;
  //char* event = NULL;
  FILE* fs;
  
  if (sharedprofdesc) {
    profdesc->ofile.fs = NULL;
    profdesc->ofile.fname = NULL;
    return;
  }


  /* Get components for constructing file name:
     <outpath>/<command>.hpcrun.<hostname>.<pid>.<tid> */
  
  /* <command> */
  slash = rindex(cmd, '/');
  if (slash) {
    cmd = slash + 1; /* basename of cmd */
  }
  
#if 0
  /* <event1> */
  if (HPC_GET_SYSPROFS(profdesc)) {
    numEvents += HPC_GET_SYSPROFS(profdesc)->size;
    event = HPC_GET_SYSPROFS(profdesc)->vec[0].ename;
  }
  if (!event && HPC_GET_PAPIPROFS(profdesc)) {
    numEvents += HPC_GET_PAPIPROFS(profdesc)->size;
    event = HPC_GET_PAPIPROFS(profdesc)->vec[0].einfo.symbol; /* first name */
  }
#endif
  
  /* <hostname> */
  gethostname(hostnm, hostnmLen);
  hostnm[hostnmLen-1] = '\0'; /* ensure NULL termination */

  /* Create file name */
  snprintf(outfilenm, outfilenmLen, "%s/%s%s."HPCRUN_NAME".%s.%d.0x%lx",
	   opt_outpath, opt_prefix, cmd, hostnm, getpid(), hpcrun_gettid());

  /* Use a generation suffix if file exists */
  if (strlen(opt_file) != 0) {
    /* If user has supplied an output filename use that overrides the
       setting above */
    int inst = get_next_gen(opt_file);
    if (inst) {
      snprintf(outfilenm, outfilenmLen, "%s.%d", opt_file, inst);
    } 
    else {
      strncpy(outfilenm, opt_file, outfilenmLen);
    }
  } 
  else {
    int inst = get_next_gen(outfilenm);
    if (inst) {
      snprintf(opt_file, outfilenmLen, "%s.%d", outfilenm, inst);
      strncpy(outfilenm, opt_file, outfilenmLen);
    }
  }
  

  profdesc->ofile.fs = NULL;
  profdesc->ofile.fname = (char*)malloc(strlen(outfilenm)+1);
  if (!profdesc->ofile.fname) { DIE0("error: malloc() failed!"); }
  strcpy(profdesc->ofile.fname, outfilenm);

  /* Test whether we can write to this filesystem */
  do {
    if (opt_debug >= 1 && (errno == ENFILE || errno == EMFILE)) {
      MSG0(stderr, "* waiting for file descriptors to test filesystem!");
    }
    errno = 0;
    fs = fopen(outfilenm, "w");
  }
  while (errno == ENFILE || errno == EMFILE /* too many open files */);

  if (fs == NULL) {
    DIEx("error: Filesystem test failed (cannot open file '%s'): %s", 
	 outfilenm, strerror(errno));
  }
  fclose(fs);
}


static void
notify_ofile(hpcrun_profiles_desc_t* profdesc, 
	     hpcrun_profiles_desc_t* sharedprofdesc)
{
  /* Let user know about output file */
  const char* out_fname = profdesc->ofile.fname;
  const char* out_sfx = "";
  if (!out_fname) {
    out_fname = sharedprofdesc->ofile.fname;
    out_sfx = " (SHARED)";
  }
  MSGx(stderr, "Using output file %s%s\n", out_fname, out_sfx);
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
    DIE0("error: Only one wallclock event may be profiled at a time.");
  }

  prof = &(profdescs->vec[profidx]);

  /* Find event */
  if (strcmp(eventnm, HPCRUN_EVENT_WALLCLK_STR) == 0) {
    prof->ename = HPCRUN_EVENT_WALLCLK_STR;
    prof->flags = 0;
    prof->period = 1; /* 1 millisecond (not 10!); discovered empirically */
  }
  else if (strcmp(eventnm, HPCRUN_EVENT_FWALLCLK_STR) == 0) {
    prof->ename = HPCRUN_EVENT_FWALLCLK_STR;
    prof->flags = PROF_FAST;
    prof->period = 1; /* should be 1 ms; cf. /usr/include/sys/profile.h */
  }
  else {
    DIEx("error: Invalid event: '%s'.", eventnm);
  }

  /* Profiling period (already set) */
  if (period != 0) {
    DIEx("error: Invalid period %"PRIu64" for event '%s'.", 
	 period, eventnm);
  }
    
  /* Profiling flags */
  prof->flags |= PROF_UINT; /* hpc_hist_bucket */
  
  prof->bytesPerCntr = sizeof(hpc_hist_bucket); /* 4 */
  prof->bytesPerCodeBlk = 4;
  prof->scale = 0x10000;
  
  if ((prof->scale * prof->bytesPerCodeBlk) != (65536 * prof->bytesPerCntr)) {
    DIE0("error: internal programming error - invalid profiling scale.");
  }
}


static void 
start_sysprof(hpcsys_profile_desc_vec_t* profdescs)
{
  int ecode;

  /* Note: should only be one profdesc! */
  hpcsys_profile_desc_t* prof = &profdescs->vec[0];

  if (opt_debug >= 1) { MSGx(stderr, "Calling sprofil(): %s", prof->ename); }
  
  ecode = sprofil(prof->sprofs, prof->numsprofs, NULL, prof->flags);
  if (ecode != 0) {
    DIEx("error: sprofil() error. %s.", strerror(errno));
  }
}


static void 
init_papi_for_process()
{  
  int rval;
  
  /* Initialize papi: hpc_init_papi_force() *must* be used for forks();
     it works for non-forks also. */
  if (hpc_init_papi_force(PAPI_library_init) != 0) { 
    exit(1); /* error already printed */
  }
  
  /* set PAPI debug */
  if (opt_debug >= 1) {
    MSG0(stderr, "setting PAPI debug!");
    rval = PAPI_set_debug(PAPI_VERB_ECONT);
    if (rval != PAPI_OK) {
      DIEx("error: PAPI_set_debug (%d): %s.", rval, PAPI_strerror(rval));
    }
  }
  
  /* PAPI_set_domain(PAPI_DOM_ALL); */
  if (domain == 0) {
    domain = PAPI_DOM_USER;
  }
  if (opt_debug >= 1) {
    MSGx(stderr, "Setting PAPI domain to: %d", domain);
  }
  rval = PAPI_set_domain(domain);
  if (rval != PAPI_OK) {
    DIEx("error: PAPI_set_domain (%d): %s.", rval, PAPI_strerror(rval));
  }


#ifndef HAVE_MONITOR  
  init_papi_for_process_SPECIALIZED();
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

  int rval;
  hpcpapi_profile_desc_t* prof = NULL;
  
  if (!profdescs) {
    DIE0("error: internal programming error.");
  }

  if (profidx >= profdescs->size) {
    MSGx(stderr, "warning: Ignoring event '%s:%"PRIu64"'.", eventnm, period);
    return;
  }
    
  prof = &(profdescs->vec[profidx]);
  
  /* Find event info, ensuring it is available.  Note: it is
     necessary to do a query_event *and* get_event_info.  Sometimes
     the latter will return info on an event that does not exist. */
  rval = PAPI_event_name_to_code(eventnm, &prof->ecode);
  if (rval != PAPI_OK) {
    DIEx("error: Event '%s' is not recognized.\n"
	 "\tCheck the list of supported events with `"HPCRUN_NAME" -L'.",
	 eventnm);
  }
  rval = PAPI_query_event(prof->ecode);
  if (rval != PAPI_OK) {
    DIEx("error: Event '%s' is not supported on this platform.\n"
	 "\tCheck the list of supported events with `"HPCRUN_NAME" -L'.",
	 eventnm);
  }
  rval = PAPI_get_event_info(prof->ecode, &prof->einfo);
  if (rval != PAPI_OK) {
    DIEx("error: PAPI_get_event_info (%d): %s.", rval, PAPI_strerror(rval));
  }
  
  /* NOTE: Although clumsy, this test has official sanction. */
  if ((prof->ecode & PAPI_PRESET_MASK) && (prof->einfo.count > 1) && 
      strcmp(prof->einfo.derived, "DERIVED_CMPD") != 0) {
    DIEx("error: '%s' is a PAPI derived event.\n"
	 "\tSampling of derived events is not supported by PAPI.\n" 
	 "\tUse `"HPCRUN_NAME" -L' to find the component native events of '%s' that you can monitor separately.", eventnm, eventnm);
  }
  
  rval = PAPI_add_event(profdescs->eset, prof->ecode);
  if (rval != PAPI_OK) {
    DIEx("error: (%d) Unable to add event '%s' to event set.\n"
	 "\tPAPI_add_event %s.", rval, eventnm, PAPI_strerror(rval));
  }
  
  /* Profiling period */
  if (period == 0) {
    DIEx("error: Invalid period %"PRIu64" for event '%s'.", 
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
    DIE0("error: internal programming error - invalid profiling scale.");
  }
}


static void 
start_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs)
{
  int rval;
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
      MSGx(stderr, "Calling PAPI_sprofil(): %s", prof->einfo.symbol);
    }
    
    rval = PAPI_sprofil(prof->sprofs, prof->numsprofs, profdescs->eset, 
			prof->ecode, prof->period, prof->flags);
    if (rval != PAPI_OK) {
      DIEx("error: PAPI_sprofil (%d): %s.", rval, PAPI_strerror(rval));
    }
  }

  /* 2. Launch PAPI */
  rval = PAPI_start(profdescs->eset);
  if (rval != PAPI_OK) {
    DIEx("error: PAPI_start (%d): %s.", rval, PAPI_strerror(rval));
  }
}


static void init_sighandler(int sig);

static void 
init_sighandlers()
{
  init_sighandler(SIGINT);   /* Ctrl-C */
  init_sighandler(SIGABRT);  /* abort() */
}


static void 
init_sighandler(int sig)
{
  if (signal(sig, SIG_IGN) != SIG_IGN) {
    signal(sig, hpcrun_sighandler);
  } 
  else {
    MSGx(stderr, "warning: Signal %d already has a handler.", sig);
  }
}


/****************************************************************************
 * Finalize profiling
 ****************************************************************************/

static void write_all_profiles(hpcrun_profiles_desc_t* profdesc, 
			       rtloadmap_t* rtmap);

static void fini_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs);
static void fini_papi_for_process();

static void fini_profdesc(hpcrun_profiles_desc_t** profdesc, 
			  int sharedprofdesc);


/*
 *  Finalize profiling for this process.  Since this routine can be
 *  called more than once per process, ensure that it is idempotent.
 */
extern void 
fini_process()
{
  static int is_finalized = 0;

  if (is_finalized) {
    if (opt_debug >= 1) { MSG0(stderr, "*** fini_process (skip) ***"); }
    return;
  }
  
  is_finalized = 1;
  if (opt_debug >= 1) { MSG0(stderr, "*** fini_process ***"); }

  fini_thread(&hpc_profdesc, 0 /*is_thread*/);

  if (numPAPIEvents > 0) {
    fini_papi_for_process();
  }  
}


/*
 *  Finalize profiling for this thread and free profiling data.  See
 *  init_thread() for meaning of 'is_thread'.
 */
extern void 
fini_thread(hpcrun_profiles_desc_t** profdesc, int is_thread)
{
  int sharedprofdesc = 0;
  
  if (opt_debug >= 1) { MSG0(stderr, "*** fini_thread ***"); }

  /* Stop profiling */
  if (HPC_GET_PAPIPROFS(*profdesc)) {
    stop_papi_for_thread(HPC_GET_PAPIPROFS(*profdesc));
    if (opt_debug >= 3) {
      dump_hpcpapi_profile_desc_vec(HPC_GET_PAPIPROFS(*profdesc));
    }
  }
  if (HPC_GET_SYSPROFS(*profdesc)) {
    stop_sysprof(HPC_GET_SYSPROFS(*profdesc));
  }

  
  if (is_thread && opt_thread == HPCRUN_THREADPROF_ALL) {
    sharedprofdesc = 1; /* histogram buffers are shared */
  }

  /* Write data (if necessary) */
#if (!OPEN_OUTPUTFILE_AT_BEG)
  MSG0(stderr, "*** TST ***\n");
  init_profdesc_ofile(*profdesc, sharedprofdesc);
  notify_ofile(*profdesc, hpc_profdesc);
#endif
  write_all_profiles(*profdesc, rtloadmap);

  /* Finalize profiling subsystems and uninit descriptor */
  if (HPC_GET_PAPIPROFS(*profdesc)) {
    fini_papi_for_thread(HPC_GET_PAPIPROFS(*profdesc));
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
    DIEx("error: sprofil() error. %s.", strerror(errno));
#endif
  }
}


static void 
stop_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs)
{
  int rval, i;
  long_long* values = NULL; // array the size of the eventset

  rval = PAPI_stop(profdescs->eset, values);
  if (rval != PAPI_OK) {
    //DIEx("error: PAPI_stop (%d): %s.", rval, PAPI_strerror(rval));
  }

  /* Call PAPI_sprofil() with a 0 threshold to cleanup internal memory */
  for (i = 0; i < profdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];
    
    rval = PAPI_sprofil(prof->sprofs, prof->numsprofs, profdescs->eset, 
			prof->ecode, 0, prof->flags);
    if (rval != PAPI_OK) {
      //DIEx("error: PAPI_sprofil (%d): %s.", rval, PAPI_strerror(rval));
    }
  }
}


static void 
fini_papi_for_thread(hpcpapi_profile_desc_vec_t* profdescs)
{
  int rval;
  /* Error need not be fatal -- we've already got the goods! */
  rval = PAPI_cleanup_eventset(profdescs->eset);
  if (rval != PAPI_OK) {
    MSGx(stderr, "warning: PAPI_cleanup_eventset (%d): %s.", rval, PAPI_strerror(rval));
  }

  rval = PAPI_destroy_eventset(&profdescs->eset);
  if (rval != PAPI_OK) {
    MSGx(stderr, "warning: PAPI_destroy_eventset (%d): %s.", rval, PAPI_strerror(rval));
  }
  profdescs->eset = PAPI_NULL;

  /* Call PAPI_stop, PAPI_cleanup_eventset and PAPI_destroy_eventset
     before PAPI_unregister_thread */
  rval = PAPI_unregister_thread();
  if (rval != PAPI_OK) {
    MSGx(stderr, "warning: PAPI_unregister_thread (%d): %s.", rval, PAPI_strerror(rval));
  }
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
  uint numSysEv = 0, numPapiEv = 0;
  
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
			     uint64_t ncounters, uint bytesPerCodeBlk);

static void write_string(FILE *fp, char *str);


/*
 *  Write profile data for this process.  See hpcrun.h for file format info.
 */
static void 
write_all_profiles(hpcrun_profiles_desc_t* profdesc, rtloadmap_t* rtmap)
{
  int i;
  FILE* fs;
  
  if (opt_debug >= 1) { MSG0(stderr, "*** write_all_profiles: begin ***"); }

  if (!profdesc->ofile.fname) {
    return;
  }

  /* open the data file (wait if too many files are already open) */
  do {
    if (opt_debug >= 1 && (errno == ENFILE || errno == EMFILE)) {
      MSG0(stderr, "* waiting for file descriptors to write data!");
    }
    errno = 0;
    fs = fopen(profdesc->ofile.fname, "w");
  }
  while (errno == ENFILE || errno == EMFILE /* too many open files */);

  if (fs == NULL) {
    DIEx("error: Could not open file '%s': %s", profdesc->ofile.fname, 
	 strerror(errno));
  }
  
  /* <header> */
  fwrite(HPCRUNFLAT_FMT_Magic, 1, HPCRUNFLAT_FMT_MagicLen, fs);
  fwrite(HPCRUNFLAT_Version, 1, HPCRUNFLAT_VersionLen, fs);
  fputc(HPCRUNFLAT_FMT_Endian, fs);

  if (opt_debug >= 1) { MSGx(stderr, "rtmap count: %d", rtmap->count); }

  /* <loadmodule_list> */
  hpcio_le4_fwrite(&(rtmap->count), fs);
  for (i = 0; i < rtmap->count; ++i) {
    write_module_profile(fs, &(rtmap->module[i]), profdesc, i);
  }
  
  fclose(fs);

  if (opt_debug >= 1) { MSG0(stderr, "*** write_all_profiles: end ***"); }
}


static void 
write_module_profile(FILE* fs, rtloadmod_desc_t* mod,
		     hpcrun_profiles_desc_t* profdesc, int sprofidx)
{
  int i;
  uint numEv = 0;
  uint numSysEv = 0, numPapiEv = 0;
  
  if (opt_debug >= 2) { 
    MSGx(stderr, "writing module %s (at offset %#"PRIx64")", 
	 mod->name, mod->offset); 
  }

  /* <loadmodule_name>, <loadmodule_loadoffset> */
  write_string(fs, mod->name);
  hpcio_le8_fwrite(&(mod->offset), fs);

  /* <loadmodule_eventcount> */
  if (HPC_GET_SYSPROFS(profdesc)) { 
    numSysEv = HPC_GET_SYSPROFS(profdesc)->size;
    numEv += numSysEv; 
  }
  if (HPC_GET_PAPIPROFS(profdesc)) { 
    numPapiEv = HPC_GET_PAPIPROFS(profdesc)->size;
    numEv += numPapiEv; 
  }
  hpcio_le4_fwrite(&numEv, fs);
  
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
  hpcio_le8_fwrite(&period, fs);
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

  if (opt_debug >= 4) { 
    MSGx(stderr, "  writing %p[%d] = %p for %s with buf (%p):", 
	 prof, sprofidx, sprof, ename, histo); 
  }

  write_event_data(fs, ename, histo, ncounters, prof->bytesPerCodeBlk);
}


static void 
write_event_data(FILE *fs, char* ename, hpc_hist_bucket* histo, 
		 uint64_t ncounters, uint bytesPerCodeBlk)
{
  uint64_t count = 0, offset = 0, i = 0, inz = 0;

  /* <histogram_non_zero_bucket_count> */
  count = 0;
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) { count++; inz = i; }
  }
  hpcio_le8_fwrite(&count, fs);
  
  if (opt_debug >= 3) {
    MSGx(stderr, "  buffer (%p) for %s has %"PRIu64" of %"PRIu64" non-zero counters (last non-zero counter: %"PRIu64")", 
	 histo, ename, count, ncounters, inz);
  }
  
  /* <histogram_non_zero_bucket_x_value> 
     <histogram_non_zero_bucket_x_offset> */
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) {
      uint32_t cnt = histo[i];
      hpcio_le4_fwrite(&cnt, fs);   /* count */

      offset = i * bytesPerCodeBlk;
      hpcio_le8_fwrite(&offset, fs); /* offset (in bytes) from load addr */

      if (opt_debug >= 3) {
        MSGx(stderr, "  (cnt,offset)=(%d,%"PRIx64")", cnt, offset);
      }
    }
  }
}


static void 
write_string(FILE *fs, char *str)
{
  /* <string_length> <string_without_terminator> */
  uint len = strlen(str);
  hpcio_le4_fwrite(&len, fs);
  fwrite(str, 1, len, fs);
}


/****************************************************************************/

/* hpcrun_gettid: return a thread id */
/* FIXME: return size_t or intptr_t */
extern long
hpcrun_gettid()
{
#ifdef HAVE_MONITOR
  return (long)(monitor_gettid());
#else
  return hpcrun_gettid_SPECIALIZED();
#endif
}

extern void 
hpcrun_parse_execl(const char*** argv, const char* const** envp,
		   const char* arg, va_list arglist)
{
  /* argv & envp are pointers to arrays of char* */
  /* va_start has already been called */

  const char* argp;
  int argvSz = 32, argc = 1;
  
  *argv = malloc((argvSz+1) * sizeof(const char*));
  if (!*argv) { DIE0("error: malloc() failed!"); }
  
  (*argv)[0] = arg;
  while ((argp = va_arg(arglist, const char*)) != NULL) { 
    if (argc > argvSz) {
      argvSz *= 2;
      *argv = realloc(*argv, (argvSz+1) * sizeof(const char*));
      if (!*argv) { DIE0("error: realloc() failed!"); }
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
      MSGx(stderr, "  execl arg%d: %s", i, (*argv)[i]);
    }
    if (envp) {
      MSG0(stderr, "  execl envp found");
    }
  }
#endif
  
  /* user calls va_end */
}
