/* $Id$ */
/* -*-C-*- */

/****************************************************************************
//
// File: 
//    preload.c
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
//    The PAPI Initialization code was adapted from parts of The
//    Visual Profiler by Curtis L. Janssen (vmon.c).
//    
//***************************************************************************/

/************************** System Include Files ****************************/

#include <sys/param.h> /* MAXPATHLEN */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#ifndef __USE_GNU
# define __USE_GNU /* must define on Linux to get RTLD_NEXT from <dlfcn.h> */
# define SELF_DEFINED__USE_GNU
#endif

#include <dlfcn.h>

/* #include <bp-sym.h> */
#include <papi.h>

/**************************** User Include Files ****************************/

#include "papirun.h"
#include "map.h"
#include "events.h"
#include "flags.h"
#include "io.h"

/****************************** Type Declarations ***************************/

/* start main */

typedef void (*libc_start_main_fptr_t) 
     (int (*main) (int, char **, char **),
      int argc, char *__unbounded *__unbounded ubp_av,
      void (*init) (void), void (*fini) (void),
      void (*rtld_fini) (void), void *__unbounded stack_end);

static libc_start_main_fptr_t sm;

static void (*real_fini) (void);

/**************************** Forward Declarations **************************/

static void pr_fini(); 

static int  init_global_options();

static void init_papirun();
static void done_papirun();

static void write_all_profiles();

/*************************** Variable Declarations **************************/

/* the profiled command */
static char *command_name;

/* used for PAPI stuff */
static int papi_eventset; 
static int papi_eventcode; 
static int papi_sampleperiod; 
static int papi_eventflags;
static PAPI_sprofil_t *papi_profiles;

static unsigned int papi_bytesPerCodeBlk;
static unsigned int papi_bytesPerCntr = sizeof(unsigned short); /* 2 */
static unsigned int papi_scale;

/* papirun */ 
static int debug_level = 0;

static loadmodules_t *loadmodules;
static FILE *proffile;


/****************************************************************************/

/*
 *  Library initialization
 */
void _init()
{
  char *error;

  init_global_options();

  if (debug_level >= 1) { fprintf(stderr, "** initializing papirun.so **\n"); }

  /* Grab poiners to functions that will be intercepted.

     Note: RTLD_NEXT is not documented in the dlsym man/info page
     but in <dlfcn.h>: 
  
     If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
     the run-time address of the symbol called NAME in the next shared
     object is returned.  The "next" relation is defined by the order
     the shared objects were loaded.  
  */

  /* from libc */
  sm = (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main"); 
  if ((error = dlerror()) != NULL) {
    fputs(error, stderr);
    exit(1);
  }
  if (!sm) {
    fputs("papirun: cannot intercept beginning of process execution and therefore cannot begin profiling\n", stderr);
    exit(1);
  }
}

/*
 *  Library finalization
 */
void _fini()
{
  if (debug_level >= 1) { fprintf(stderr, "** finalizing papirun.so **\n"); }
}


static int 
init_global_options()
{
  char *debug = getenv("PAPIRUN_DEBUG");
  debug_level = (debug ? atoi(debug) : 0);
}

/****************************************************************************/

/*
 *  Intercept the start routine: this is from glibc
 *    glibc-x/sysdeps/generic/libc-start.c
 */
int __libc_start_main (int (*main) (int, char **, char **),
			    int argc, char *__unbounded *__unbounded ubp_av,
			    void (*init) (void), void (*fini) (void),
			    void (*rtld_fini) (void), 
			    void *__unbounded stack_end)
{
  /* squirrel away for later use */
  command_name = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  real_fini = fini;
  
  /* initialize papi profiling */
  init_papirun();
  
  /* launch the process */
  if (debug_level >= 1) {
    fprintf(stderr, "** launching application: %s\n", command_name);
  }
  (*sm)(main, argc, ubp_av, init, pr_fini, rtld_fini, stack_end);
}

/* papirun fini */
static void pr_fini()
{
  if (real_fini) (*real_fini)();
  done_papirun();
  exit(0);
}

/****************************************************************************
 * Initialize PAPI profiling
 ****************************************************************************/

static void init_papi_options();
static void init_papi();
static void start_papi();
static void init_output();


/*
 *  Prepare for PAPI profiling
 */
static void 
init_papirun()
{
  if (debug_level >= 1) { 
    fprintf(stderr, "* initializing papirun monitoring\n");
  }

  init_papi();
  init_papi_options();
  start_papi();
}


static void 
init_papi()
{  
  /* Initiailize PAPI library */
  int papi_version; 
  papi_version = PAPI_library_init(PAPI_VER_CURRENT);
  if (papi_version != PAPI_VER_CURRENT) {
    fprintf(stderr, "papirun: PAPI library initialization failure - expected version %d, dynamic library was version %d. Aborting.\n", PAPI_VER_CURRENT, papi_version);
    exit(-1);
  }
}

static void 
init_papi_options()
{
  static int default_sampleperiod = (2 << 12) -1;

  char *papirun_listall   = getenv("PAPIRUN_DUMP_EVENTS");
  char *papirun_event     = getenv("PAPIRUN_EVENT_NAME");
  char *papirun_period    = getenv("PAPIRUN_SAMPLE_PERIOD");
  char *papirun_recursive = getenv("PAPIRUN_RECURSIVE");
  char *papirun_flags     = getenv("PAPIRUN_EVENT_FLAG");

  if (debug_level >= 1) { fprintf(stderr, "init_papi_options: begin\n"); }

  /* List all PAPI events */
  if (papirun_listall) {
    papirun_dump_valid_papi_events();
    exit(0);
  }

  /* Profiling flags */
  if (papirun_flags == NULL) {
    fprintf(stderr, "papirun: no profiling flags specified. Aborting\n");
    exit(-1);
  }

  {
    const papi_flagdesc_t *e;
    if ((e = papirun_find_flag_by_name(papirun_flags)) == NULL) {
      fprintf(stderr, 
	      "papirun: \"%s\" is not a legal profiling flag. Aborting\n");
      exit(-1);
    }
    papi_eventflags = e->flag;
  }

  /* Recursive profiling */
  if (papirun_recursive == NULL) {
    /* turn off profiling for any processes spawned by this one */
    unsetenv("LD_PRELOAD");
  } 

  /* Profiling event */
  if (papirun_event == NULL) { 
    fprintf(stderr, "papirun: no event specified. Aborting\n");
    exit(-1);
  }

  {
    const papi_event_t *e;
    if ((e = papirun_papi_event_by_name(papirun_event)) == NULL) {
      fprintf(stderr, "papirun: \"%s\" is an invalid PAPI event. Aborting\n", 
	      papirun_event);
      exit(-1);
    }
    papi_eventcode = e->code;
  }

  {
    int pcode;
    if ((pcode = PAPI_query_event(papi_eventcode)) != PAPI_OK) { 
      fprintf(stderr, 
	      "papirun: PAPI event \"%s\" not supported. pcode=%d Aborting.\n", 
	      papirun_event, pcode);
      exit(-1);
    }
  }

  papi_eventset = PAPI_NULL;
  if (PAPI_create_eventset(&papi_eventset) != PAPI_OK) {
    fprintf(stderr, "papirun: failed to create PAPI event set. Aborting.\n");
    exit(-1);
  }

  if (PAPI_add_event(&papi_eventset, papi_eventcode) != PAPI_OK) {
    fprintf(stderr, "papirun: failed to add event \"%s\". Aborting.\n",
	    papirun_event);
    exit(-1);
  }

  /* Profiling period */
  if (papirun_period != NULL) 
    papi_sampleperiod = atoi(papirun_period);
  else {
    papi_sampleperiod = default_sampleperiod;
    fprintf(stderr, 
	    "papirun: no sample period specified, using default of %d.\n", 
	    default_sampleperiod);
  }

  if (debug_level >= 1) { fprintf(stderr,"init_papi_options: done\n"); }
}

static void 
start_papi()
{
  loadmodules_t *lm = papirun_code_lines_from_loadmap(debug_level);
  unsigned int bufsz;
  int i;

  /* 1. Set profiling scaling factor */

  /*  cf. man profil()

      The scale factor describes how much smaller the histogram buffer
      is than the region to be profiled.  It also describes how each
      histogram counter correlates with the region to be profiled.

      The region to be profiled can be thought of being divided into n
      equally sized blocks, each b bytes long.  If each histogram
      counter is bh bytes long, the value of the scale factor is the
      reciprocal of (b / bh).  Since each histogram counter is 2 bytes
      (unsigned short),
           scale = reciprocal( b / 2 )

      The representation of the scale factor is a 16-bit fixed-point
      fraction with the decimal point implied on the *left*.  Under
      this representation, the maximum value of scale is 1 (0xffff).

      FIXME: for PAPI_profil can we have scales greater than 1?

      Alternatively, and perhaps more conveniently, the scale can be
      thought of as a ratio between an unsigned integer and 65536:
          scale = ( scaleval / 65536 ) = ( 2 / b )

      Some sample scale values: 

          scaleval            bytes_per_code_block
          ----------------------------------------
          0xffff (or 0x10000) 2
          0x8000              4  (size of many RISC instructions)
	  0x4000              8
	  0x2000              16 (size of Itanium instruction packet)
      
      Using this second notation, we can show the relation between the
      histogram counters and the region to profile.  The histogram
      counter that will be incremented for an interrupt at program
      counter pc is:
          histogram[ ((pc - load_offset)/2) * (scaleval/65536) ]

      The number of counters in the histogram should equal the number
      of blocks in the region to be profiled.
      
  */
  papi_bytesPerCodeBlk = 4;
  papi_scale = 0x8000;
  
  if ( (papi_scale * papi_bytesPerCodeBlk) != (65536 * papi_bytesPerCntr) ) {
    fprintf(stderr, "papirun: Programming error!\n");
  }

  /* 2. Prepare buffers/structures for profiling */
  loadmodules = lm;
  papi_profiles = 
    (PAPI_sprofil_t *) malloc(sizeof(PAPI_sprofil_t) * lm->count);

  if (debug_level >= 3) { fprintf(stderr, "profile buffer details:\n"); }

  for (i = 0; i < lm->count; ++i) {
    /* eliminate use of libm and ceil() by adding 1 */
    unsigned int ncntr = (lm->module[i].length / papi_bytesPerCodeBlk) + 1;
    bufsz = ncntr * papi_bytesPerCntr;
    
    /* buffer base */
    papi_profiles[i].pr_base = (unsigned short *)malloc(bufsz);
    /* buffer size */
    papi_profiles[i].pr_size = bufsz;
    /* pc offset */
    papi_profiles[i].pr_off = lm->module[i].offset;
    /* pc scaling */
    papi_profiles[i].pr_scale = papi_scale;
    
    /* zero out the profile buffer */
    memset(papi_profiles[i].pr_base, 0x00, bufsz);

    if (debug_level >= 3) {
      fprintf(stderr, 
	      "\tprofile[%d] base = %lx size = %d off = %lx scale = %d\n",
	      i, papi_profiles[i].pr_base, papi_profiles[i].pr_size, 
	      papi_profiles[i].pr_off, papi_profiles[i].pr_scale);
    }
  }

  /* 3. Prepare output files */
  init_output(lm, papi_eventcode, papi_sampleperiod);

  if (debug_level >= 3) {
    fprintf(stderr, "count = %d, es=%x ec=%x sp=%d ef=%d\n",
	    lm->count, papi_eventset, papi_eventcode, 
	    papi_sampleperiod, papi_eventflags);
  }

  /* 4. Launch PAPI */
  {
    int r = PAPI_sprofil(papi_profiles, lm->count, papi_eventset, 
			 papi_eventcode, papi_sampleperiod, 
			 papi_eventflags);
    if (r != PAPI_OK) {
      fprintf(stderr, "papirun: sprofil start failed with failure code %d. Aborting.\n");
      exit(-1);
    }
  }

  {
    int papi_debug_level;
    switch(debug_level) {
    case 0: case 1: case 2:
      break;
    case 3:
      papi_debug_level = PAPI_VERB_ECONT;
      break;
    case 4:
      papi_debug_level = PAPI_VERB_ESTOP;
      break;
    }
    if (debug_level >= 3) {
      fprintf(stderr, "setting debug option %d.\n", papi_debug_level);
      if (PAPI_set_debug(papi_debug_level) != PAPI_OK) {
	fprintf(stderr,"papirun: failed to set debug level. Aborting.\n");
	exit(-1);
      }
    }
  }

  if (PAPI_start(papi_eventset) != PAPI_OK) {
    fprintf(stderr,"papirun: failed to start event set. Aborting.\n");
    exit(-1);
  }
}

static void 
init_output()
{
  char filename[MAXPATHLEN+32];
  char *executable = command_name; 
  char *slash = rindex(executable,'/');

  if (slash) {
    executable = slash+1; /* basename of executable */
  }
  sprintf(filename, "%s.%s.%d", executable, 
	  papirun_papi_event_by_type(papi_eventcode)->name, 
	  getpid()); 
  
  proffile = fopen(filename, "w");

  if (proffile == NULL) {
    fprintf(stderr,"papirun: failed to open output file. Aborting.\n");
    exit(-1);
  }
}


/****************************************************************************
 * Finalize PAPI profiling
 ****************************************************************************/

/*
 *  Finalize PAPI profiling
 */
static void 
done_papirun()
{
  long long ct = 0;

  if (debug_level >= 1) { fprintf(stderr, "done_papirun:\n"); }
  PAPI_stop(papi_eventset, &ct);
  PAPI_rem_event(&papi_eventset, papi_eventcode);
  PAPI_destroy_eventset(&papi_eventset);

  write_all_profiles();

  papi_eventset = PAPI_NULL;
  PAPI_shutdown();
}

/****************************************************************************
 * Write profile data
 ****************************************************************************/

static void write_module_profile(FILE *fp, loadmodule_t *m, 
				  PAPI_sprofil_t *p);
static void write_module_name(FILE *fp, char *name);
static void write_module_data(FILE *fp, PAPI_sprofil_t *p);


/*
 *  Write profile data for this process
 */
static void 
write_all_profiles()
{
  FILE *fp = proffile;

  /* Header information */
  fwrite(PAPIRUN_MAGIC_STR, 1, PAPIRUN_MAGIC_STR_LEN, fp);
  fwrite(PAPIRUN_VERSION, 1, PAPIRUN_VERSION_LEN, fp);
  fputc(PAPIRUN_ENDIAN, fp);

  /* Load modules */
  {
    unsigned int i;
    loadmodules_t *lm = loadmodules;
    hpc_fwrite_le4(&(lm->count), fp);
    for (i = 0; i < lm->count; ++i) {
      write_module_profile(fp, &(lm->module[i]), &(papi_profiles[i]));
    }
  }

  fclose(fp);
}

static void 
write_module_profile(FILE *fp, loadmodule_t *m, PAPI_sprofil_t *p)
{
  unsigned long long offset;

  /* Load module name and load offset */
  write_module_name(fp, m->name);
  offset = m->offset;
  hpc_fwrite_le8(&(offset), fp);

  /* Profiling event and period */
  hpc_fwrite_le4(&papi_eventcode, fp);
  hpc_fwrite_le4(&papi_sampleperiod, fp);

  /* Profiling data */
  write_module_data(fp, p);
}

static void 
write_module_name(FILE *fp, char *name)
{
  /* Note: there is no null terminator on the name string */
  int n;
  unsigned int namelen = strlen(name);
  hpc_fwrite_le4(&namelen, fp);
  fwrite(name, 1, namelen, fp);
  
  if (debug_level >= 1) { fprintf(stderr, "writing module %s\n", name); }
}

static void 
write_module_data(FILE *fp, PAPI_sprofil_t *p)
{
  unsigned int count = 0, offset = 0, i = 0, inz = 0;
  unsigned int ncounters = (p->pr_size / papi_bytesPerCntr);

  /* Number of profiling entries */
  count = 0;
  for (i = 0; i < ncounters; ++i) {
    if (p->pr_base[i] != 0) { count++; inz = i; }
  }
  hpc_fwrite_le4(&count, fp);
  
  if (debug_level >= 1) {
    fprintf(stderr, "  profile has %d of %d non-zero counters", count,
	    ncounters);
    fprintf(stderr, " (last non-zero counter: %d)\n", inz);
  }
  
  /* Profiling entries */
  for (i = 0; i < ncounters; ++i) {
    if (p->pr_base[i] != 0) {
      hpc_fwrite_le2(&(p->pr_base[i]), fp); /* count */
      offset = i * papi_bytesPerCodeBlk;
      hpc_fwrite_le4(&offset, fp); /* offset (in bytes) from load addr */
    }
  }
  
  /* cleanup */
  if (p->pr_base) {
    free(p->pr_base);
    p->pr_base = 0;
  }
}

__libc_fork ()
{
 fprintf (stderr, "In __libc_fork\n");
 exit (0);
}

__fork ()
{
 fprintf (stderr, "In __fork\n");
 exit (0);
}

pthread_create ()
{
 fprintf (stderr, "In pthread_create\n");
 exit (0);
}

__clone ()
{
 fprintf (stderr, "In __clone\n");
 exit (0);
}

__clone2 ()
{
 fprintf (stderr, "In __clone2\n");
 exit (0);
}
