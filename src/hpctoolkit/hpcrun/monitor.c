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
#include <inttypes.h>

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

/* libc_start_main related */

#define START_MAIN_PARAMS (int (*main) (int, char **, char **), \
			   int argc, char *__unbounded *__unbounded ubp_av, \
			   void (*init) (void), void (*fini) (void), \
			   void (*rtld_fini) (void), \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) START_MAIN_PARAMS;
typedef void (*libc_start_main_fini_fptr_t) (void);

static int  hpcr_libc_start_main START_MAIN_PARAMS;
static void hpcr_fini(void); 

/**************************** Forward Declarations **************************/

static void init_options(void);

static void init_hpcrun(void);
static void fini_hpcrun();

typedef uint32_t hpc_hist_bucket; /* a 4 byte histogram bucket */

static const uint64_t default_period = (1 << 15) - 1; /* (2^15 - 1) */

/*************************** Variable Declarations **************************/

/* These variables are set when the library is initialized */

/* Intercepted libc routines */
static libc_start_main_fptr_t      start_main;
static libc_start_main_fini_fptr_t start_main_fini;

/* hpcrun options (this should be a tidy struct) */
static int      opt_debug = 0;
static int      opt_recursive = 0;
static char*    opt_eventlist = NULL;
static char     opt_outpath[PATH_MAX] = "";
static int      opt_flagscode = 0;

/*************************** Variable Declarations **************************/

/* These variables are set when libc_start_main is intercepted */

/* The profiled command and its run-time load map */
static char*        profiled_cmd;
static rtloadmap_t* rtloadmap;

/* PAPI profiling information (one for each profiled event) */
static hpcpapi_profile_desc_vec_t papi_profdescs;

/* hpcrun output file */
static hpcrun_ofile_desc_t hpc_ofile;

/****************************************************************************/

/*
 *  Library initialization
 */
void 
_init(void)
{
  char *error;

  init_options();

  if (opt_debug >= 1) { 
    fprintf(stderr, "*** initializing "HPCRUN_LIB" ***\n"); 
  }
  
  /* Grab pointers to functions that will be intercepted.

     Note: RTLD_NEXT is not documented in the dlsym man/info page
     but in <dlfcn.h>: 
  
     If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
     the run-time address of the symbol called NAME in the next shared
     object is returned.  The "next" relation is defined by the order
     the shared objects were loaded.  
  */

  /* from libc */
  start_main = (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main");
  if ((error = dlerror()) != NULL) {
    fputs(error, stderr);
    exit(1);
  }
  if (!start_main) {
    start_main = (libc_start_main_fptr_t)dlsym(RTLD_NEXT, 
					       "__BP___libc_start_main");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }
  if (!start_main) {
    DIE("Cannot intercept beginning of process execution and therefore cannot begin profiling.");
  }
}

/*
 *  Library finalization
 */
void 
_fini(void)
{
  if (opt_debug >= 1) { fprintf(stderr, "*** finalizing "HPCRUN_LIB" ***\n"); }
}


static void
init_options(void)
{
  char *env_debug, *env_recursive, *env_eventlist, *env_eventlistSZ, 
    *env_outpath, *env_flags;
  int ret;

  /* Debugging (get this first) : default is off */
  env_debug = getenv("HPCRUN_DEBUG");
  opt_debug = (env_debug ? atoi(env_debug) : 0);

  if (opt_debug >= 1) { fprintf(stderr, "** processing "HPCRUN_LIB" opts\n"); }
  
  /* Recursive profiling: default is on */
  env_recursive = getenv("HPCRUN_RECURSIVE");
  opt_recursive = (env_recursive ? atoi(env_recursive) : 1);
  if (!opt_recursive) {
    /* turn off profiling for any processes spawned by this one */
    unsetenv("LD_PRELOAD"); 
    /* FIXME: just extract HPCRUN_LIB */
  }

  if (opt_debug >= 1) { 
    fprintf(stderr, "  recursive profiling: %d\n", opt_recursive); 
  }
  
  /* Profiling event list: default PAPI_TOT_CYC:32767 (default_period) */
  opt_eventlist = "PAPI_TOT_CYC:32767"; 
  env_eventlist = getenv("HPCRUN_EVENT_LIST");
  if (env_eventlist) {
    opt_eventlist = env_eventlist;
  }

  if (opt_debug >= 1) { 
    fprintf(stderr, "  event list: %s\n", env_eventlist); 
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
	DIE("Invalid profiling flag '%s'. Aborting.", env_flags);
      }
      opt_flagscode = f->code;
    }
  }
}


/****************************************************************************/

/*
 *  Intercept the start routine: this is from glibc and can be one of
 *  two different names depending on how the macro BP_SYM is defined.
 *    glibc-x/sysdeps/generic/libc-start.c
 */
int 
__libc_start_main START_MAIN_PARAMS
{
  hpcr_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


int 
__BP___libc_start_main START_MAIN_PARAMS
{
  hpcr_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


static int 
hpcr_libc_start_main START_MAIN_PARAMS
{
  /* squirrel away for later use */
  profiled_cmd = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  start_main_fini = fini;
  
  /* initialize profiling */
  init_hpcrun();
  
  /* launch the process */
  if (opt_debug >= 1) {
    fprintf(stderr, "*** launching intercepted app: %s ***\n", profiled_cmd);
  }
  (*start_main)(main, argc, ubp_av, init, hpcr_fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


/* hpcrun fini */
static void 
hpcr_fini(void)
{
  if (start_main_fini) {
    (*start_main_fini)();
  }
  fini_hpcrun();
  exit(0);
}


/****************************************************************************
 * Initialize PAPI profiling
 ****************************************************************************/

static void init_papi(hpcpapi_profile_desc_vec_t* profdescs, 
		      rtloadmap_t* rtmap);
static void start_papi(hpcpapi_profile_desc_vec_t* profdescs);
static void init_output(hpcrun_ofile_desc_t* ofile, 
			hpcpapi_profile_desc_vec_t* profdescs);

/*
 *  Prepare for PAPI profiling
 */
static void 
init_hpcrun(void)
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** initializing "HPCRUN_LIB" monitoring ***\n");
  }
  
  rtloadmap = hpcrun_code_lines_from_loadmap(opt_debug);
  
  init_papi(&papi_profdescs, rtloadmap);    /* init papi_profdescs */
  init_output(&hpc_ofile, &papi_profdescs); /* init hpc_ofile */
  start_papi(&papi_profdescs);
}


static void 
init_papi(hpcpapi_profile_desc_vec_t* profdescs, rtloadmap_t* rtmap)
{  
  int pcode, i;
  int numHwCntrs = 0;
  unsigned profdescsSZ = 0; 
  const unsigned eventbufSZ = 128; /* really the last index, not the size */
  char eventbuf[eventbufSZ+1];
  char* tok, *tmp_eventlist;

  /* Note on hpcpapi_profile_desc_t and PAPI_sprofil() scaling factor
     cf. man profil()
     
     The scale factor describes how much smaller the histogram buffer
     is than the region to be profiled.  It also describes how each
     histogram counter correlates with the region to be profiled.
     
     The region to be profiled can be thought of being divided into n
     equally sized blocks, each b bytes long.  If each histogram
     counter is bh bytes long, the value of the scale factor is the
     reciprocal of (b / bh).  
           scale = reciprocal( b / bh )

     The representation of the scale factor is a 16-bit fixed-point
     fraction with the decimal point implied on the *left*.  Under
     this representation, the maximum value of scale is 0xffff
     (essentially 1). (0x10000 can also be used.)

     Alternatively, and perhaps more conveniently, the scale can be
     thought of as a ratio between an unsigned integer scaleval and
     65536:
          scale = ( scaleval / 65536 ) = ( bh / b )

     Some sample scale values when bh is 4 (unsigned int):

          scaleval            bytes_per_code_block (b)
          ----------------------------------------
          0xffff (or 0x10000) 4  (size of many RISC instructions)
	  0x8000              8
	  0x4000              16 (size of Itanium instruction packet)

      
     Using this second notation, we can show the relation between the
     histogram counters and the region to profile.  The histogram
     counter that will be incremented for an interrupt at program
     counter pc is:
          histogram[ ((pc - load_offset)/bh) * (scaleval/65536) ]

     The number of counters in the histogram should equal the number
     of blocks in the region to be profiled.  */

  
  /* 1. Initialize PAPI library */
  if (hpc_init_papi() != 0) {
    exit(-1);
  }
  
  /* 2. Initialize 'profdescs' */
  
  /* 2a. Find number of 'profdescs' (strtok() will destroy the string) */
  tmp_eventlist = strdup(opt_eventlist);
  for (tok = strtok(tmp_eventlist, ";"); (tok != NULL); 
       tok = strtok((char*)NULL, ";")) {
    profdescsSZ++;
  }
  free(tmp_eventlist);
  
  /* 2b. Initialize 'profdescs' */
  {
    unsigned int vecbufsz = sizeof(hpcpapi_profile_desc_t) * profdescsSZ;
    profdescs->size = profdescsSZ;
    profdescs->vec = (hpcpapi_profile_desc_t*)malloc(vecbufsz);
    if (!profdescs->vec) {
      DIE("malloc() failed! Aborting.");
    }
    memset(profdescs->vec, 0x00, vecbufsz);
  }

  profdescs->eset = PAPI_NULL; 
  if ((pcode = PAPI_create_eventset(&profdescs->eset)) != PAPI_OK) {
    DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
  }

  /* 2c. For each event:period pair, init corresponding 'profdescs' entry */
  tmp_eventlist = strdup(opt_eventlist);
  tok = strtok(tmp_eventlist, ";");
  for (i = 0; (tok != NULL); i++, tok = strtok((char*)NULL, ";")) {
    hpcpapi_profile_desc_t* prof = &(profdescs->vec[i]);
    uint64_t period = default_period;
    char* sep;
    
    /* Extract fields from token */
    sep = strchr(tok, ':'); /* optional period delimiter */
    if (sep) {
      unsigned len = MIN(sep - tok, eventbufSZ);
      strncpy(eventbuf, tok, len);
      period = strtol(sep+1, (char **)NULL, 10);
      eventbuf[len] = '\0'; 
    }
    else {
      strncpy(eventbuf, tok, eventbufSZ);
      eventbuf[eventbufSZ] = '\0';
    }
    
    if (opt_debug >= 1) { 
      fprintf(stderr, "Finding event: '%s' '%llu'\n", eventbuf, period);
    }

    /* Find event info, ensuring it is available.  Note: it is
       necessary to do a query_event *and* get_event_info.  Sometimes
       the latter will return info on an event that does not exist. */
    if (PAPI_event_name_to_code(eventbuf, &prof->ecode) != PAPI_OK) {
      DIE("Invalid event '%s'. Aborting.", eventbuf);
    }
    if (PAPI_query_event(prof->ecode) != PAPI_OK) { 
      DIE("Event '%s' not supported. Aborting.", eventbuf);
    }
    if ((pcode = PAPI_get_event_info(prof->ecode, &prof->einfo)) != PAPI_OK) {
      DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
    }

    if ((pcode = PAPI_add_event(profdescs->eset, prof->ecode)) != PAPI_OK) {
      DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
    }
  
    /* Profiling period */
    if (period == 0) {
      DIE("Invalid period %llu for event '%s'. Aborting.", period, eventbuf);
    }  
    prof->period = period;
    
    /* Profiling flags */
    prof->flags = opt_flagscode;
    prof->flags |= PAPI_PROFIL_BUCKET_32; /* hpc_hist_bucket */
    
    prof->bytesPerCntr = sizeof(hpc_hist_bucket); /* 4 */
    prof->bytesPerCodeBlk = 4;
    prof->scale = 0x10000; // 0x8000 * (prof->bytesPerCntr >> 1);
    
    if ( (prof->scale * prof->bytesPerCodeBlk) != 
	 (65536 * prof->bytesPerCntr) ) {
      DIE("Programming error: invalid profiling scale.");
    }
    
    /* N.B.: prof->sprofs remains uninitialized */
  }
  free(tmp_eventlist);
  
  /* 2d. Ensure we have enough hardware counters */
  // The order in which we do this depends on event set stuff. FIXME
  numHwCntrs = PAPI_num_hwctrs();
  
  
  /* 3. For each 'profdescs' entry, init sprofil()-specific info */
  for (i = 0; i < profdescs->size; ++i) {
    int mapi;
    unsigned int sprofbufsz = sizeof(PAPI_sprofil_t) * rtmap->count;
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];
    
    prof->sprofs = (PAPI_sprofil_t*)malloc(sprofbufsz);
    if (!prof->sprofs) {
      DIE("malloc() failed! Aborting.");
    }
    memset(prof->sprofs, 0x00, sprofbufsz);
    prof->numsprofs = rtmap->count;

    if (opt_debug >= 3) { 
      fprintf(stderr, "profile buffer details for %s:\n", prof->einfo.symbol); 
      fprintf(stderr, "  count = %d, es=%#x ec=%#x sp=%llu ef=%d\n",
	      prof->numsprofs, profdescs->eset, prof->ecode, prof->period, 
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
	DIE("malloc() failed! Aborting.");
      }
      memset(prof->sprofs[mapi].pr_base, 0x00, bufsz);

      /* pc offset and scaling factor */
      prof->sprofs[mapi].pr_off = (caddr_t)rtmap->module[mapi].offset;
      prof->sprofs[mapi].pr_scale = prof->scale;
      
      if (opt_debug >= 3) {
	fprintf(stderr, 
		"\tprofile[%d] base = %p size = %#x off = %#lx scale = %#x\n",
		mapi, prof->sprofs[mapi].pr_base, prof->sprofs[mapi].pr_size, 
		prof->sprofs[mapi].pr_off, prof->sprofs[mapi].pr_scale);
      }
    }
  }

  
  /* 4. Set PAPI debug */
  {
    int papi_debug = 0;
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
      fprintf(stderr, "setting PAPI debug option %d.\n", papi_debug);
      if ((pcode = PAPI_set_debug(papi_debug)) != PAPI_OK) {
	DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
      }
    }
  }
}


static void 
start_papi(hpcpapi_profile_desc_vec_t* profdescs)
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
      fprintf(stderr, "Calling PAPI_sprofil(): %s\n", prof->einfo.symbol);
    }
    
    pcode = PAPI_sprofil(prof->sprofs, prof->numsprofs, profdescs->eset, 
			 prof->ecode, prof->period, prof->flags);
    if (pcode != PAPI_OK) {
      DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
    }
  }

  /* 2. Launch PAPI */
  if ((pcode = PAPI_start(profdescs->eset)) != PAPI_OK) {
    DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
  }
}


static void 
init_output(hpcrun_ofile_desc_t* ofile, hpcpapi_profile_desc_vec_t* profdescs)
{
  static unsigned int outfilenmLen = PATH_MAX;
  static unsigned int hostnameLen = 128;
  char outfilenm[outfilenmLen];
  char hostnam[hostnameLen];
  char* cmd = profiled_cmd; 
  char* slash;
  int i;

  /* Get components for constructing file name */
  slash = rindex(cmd, '/');
  if (slash) {
    cmd = slash + 1; /* basename of cmd */
  }
  
  gethostname(hostnam, hostnameLen);
  hostnam[hostnameLen-1] = '\0'; /* ensure NULL termination */
  
  char* event = profdescs->vec[0].einfo.symbol; /* first event name */
  
  /* Create file name */
  snprintf(outfilenm, outfilenmLen, "%s/%s.%s.%s.%d", 
	   opt_outpath, cmd, event, hostnam, getpid());
  
  ofile->fs = fopen(outfilenm, "w");
  if (ofile->fs == NULL) {
    DIE("Failed to open output file '%s'. Aborting.", outfilenm);
  }
  
  ofile->fname = NULL; // FIXME: skip setting ofile->fname for now
}


/****************************************************************************
 * Finalize PAPI profiling
 ****************************************************************************/

static void write_all_profiles(hpcrun_ofile_desc_t* ofile, rtloadmap_t* rtmap,
			       hpcpapi_profile_desc_vec_t* profdescs);

static void 
stop_papi(hpcpapi_profile_desc_vec_t* profdescs);

static void 
fini_papi(hpcpapi_profile_desc_vec_t* profdescs);


/*
 *  Finalize PAPI profiling
 */
static void 
fini_hpcrun(void)
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "*** finalizing "HPCRUN_LIB" monitoring ***\n");
  }
  
  stop_papi(&papi_profdescs);
  write_all_profiles(&hpc_ofile, rtloadmap, &papi_profdescs);
  fini_papi(&papi_profdescs); /* uninit 'papi_profdescs' */
}


static void 
stop_papi(hpcpapi_profile_desc_vec_t* profdescs)
{
  int pcode;
  int i;
  long_long* values = NULL; // array the size of the eventset
  
  if ((pcode = PAPI_stop(profdescs->eset, values)) != PAPI_OK) {
    DIE("PAPI error: (%d) %s. Aborting.", pcode, PAPI_strerror(pcode));
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
				 hpcpapi_profile_desc_vec_t* profdescs, 
				 int sprofidx);
static void write_event_data(FILE *fp, hpcpapi_profile_desc_t* prof, 
			     int sprofidx);
static void write_string(FILE *fp, char *str);


/*
 *  Write profile data for this process.  See hpcrun.h for file format info.
 */
static void 
write_all_profiles(hpcrun_ofile_desc_t* ofile, rtloadmap_t* rtmap,
		   hpcpapi_profile_desc_vec_t* profdescs)
{
  int i;
  FILE* fs = ofile->fs;
    
  /* <header> */
  fwrite(HPCRUNFILE_MAGIC_STR, 1, HPCRUNFILE_MAGIC_STR_LEN, fs);
  fwrite(HPCRUNFILE_VERSION, 1, HPCRUNFILE_VERSION_LEN, fs);
  fputc(HPCRUNFILE_ENDIAN, fs);
  
  /* <loadmodule_list> */
  hpcpapi_profile_desc_t* prof = &profdescs->vec[i];

  hpc_fwrite_le4(&(rtmap->count), fs);
  for (i = 0; i < rtmap->count; ++i) {
    write_module_profile(fs, &(rtmap->module[i]), profdescs, i);
  }
  
  fclose(fs);
  ofile->fs = NULL;
}


static void 
write_module_profile(FILE* fs, rtloadmod_desc_t* mod,
		     hpcpapi_profile_desc_vec_t* profdescs, int sprofidx)
{
  int i;
  
  if (opt_debug >= 2) { fprintf(stderr, "writing module %s\n", mod->name); }

  /* <loadmodule_name>, <loadmodule_loadoffset> */
  write_string(fs, mod->name);
  hpc_fwrite_le8(&(mod->offset), fs);

  /* <loadmodule_eventcount> */
  hpc_fwrite_le4(&(profdescs->size), fs);

  /* Event data */
  for (i = 0; i < profdescs->size; ++i) {
    hpcpapi_profile_desc_t* prof = &profdescs->vec[i];

    /* <event_x_name> <event_x_description> <event_x_period> */
    write_string(fs, prof->einfo.symbol);     /* name */
    write_string(fs, prof->einfo.long_descr); /* description */
    hpc_fwrite_le8(&prof->period, fs);
    
    /* <event_x_data> */
    write_event_data(fs, prof, sprofidx);
  }
}


static void 
write_event_data(FILE *fs, hpcpapi_profile_desc_t* prof, int sprofidx)
{
  unsigned int count = 0, offset = 0, i = 0, inz = 0;
  PAPI_sprofil_t* sprof = &(prof->sprofs[sprofidx]);
  hpc_hist_bucket* histo = (hpc_hist_bucket*)sprof->pr_base;
  unsigned int ncounters = (sprof->pr_size / prof->bytesPerCntr);
  
  /* <histogram_non_zero_bucket_count> */
  count = 0;
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) { count++; inz = i; }
  }
  hpc_fwrite_le4(&count, fs);
  
  if (opt_debug >= 2) {
    fprintf(stderr, "  profile for %s has %d of %d non-zero counters", 
	    prof->einfo.symbol, count, ncounters);
    fprintf(stderr, " (last non-zero counter: %d)\n", inz);
  }
  
  /* <histogram_non_zero_bucket_x_value> 
     <histogram_non_zero_bucket_x_offset> */
  for (i = 0; i < ncounters; ++i) {
    if (histo[i] != 0) {
      uint32_t cnt = histo[i];
      hpc_fwrite_le4(&cnt, fs);   /* count */

      offset = i * prof->bytesPerCodeBlk;
      hpc_fwrite_le4(&offset, fs); /* offset (in bytes) from load addr */

      if (opt_debug >= 2) {
        fprintf(stderr, "  (cnt,offset)=(%d,%#x)\n",cnt,offset);
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

