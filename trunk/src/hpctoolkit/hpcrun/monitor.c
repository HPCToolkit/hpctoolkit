/* -*-Mode: C;-*- */
/* $Id$ */

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

#include <papi.h>

/**************************** User Include Files ****************************/

#include "papirun.h"
#include "map.h"
#include "events.h"
#include "flags.h"
#include "io.h"

/****************************** Type Declarations ***************************/

/* start main */

#define START_MAIN_PARAMS (int (*main) (int, char **, char **), \
			   int argc, char *__unbounded *__unbounded ubp_av, \
			   void (*init) (void), void (*fini) (void), \
			   void (*rtld_fini) (void), \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) START_MAIN_PARAMS;

static int pr_libc_start_main START_MAIN_PARAMS;

static libc_start_main_fptr_t sm;

static void (*real_fini) (void);

/**************************** Forward Declarations **************************/

static void pr_fini(void); 

static void init_options(void);

static void init_papirun(void);
static void done_papirun(void);

static void write_all_profiles(void);

/*************************** Variable Declarations **************************/

/* the profiled command */
static char *command_name;

/* papirun options (this should be a tidy struct) */
static int      opt_debug;
static int      opt_recursive;
static int      opt_eventcode;
static uint64_t opt_sampleperiod;
static char     opt_outpath[PATH_MAX];
static int      opt_flagscode;
static int      opt_dump_events; /* no, short, long dump (0,1,2) */

/* used for PAPI stuff (this should be a tidy struct) */
static int             papi_eventcode;
static int             papi_eventset;
static uint64_t        papi_sampleperiod;
static int             papi_flagscode;
static PAPI_sprofil_t *papi_profiles;

static unsigned int papi_bytesPerCodeBlk;
static unsigned int papi_bytesPerCntr = sizeof(unsigned short); /* 2 */
static unsigned int papi_scale;

/* papirun */
static loadmodules_t *loadmodules;
static FILE *proffile;

/****************************************************************************/

/*
 *  Library initialization
 */
void 
_init(void)
{
  char *error;

  init_options();

  if (opt_debug >= 1) { fprintf(stderr, "** initializing papirun.so **\n"); }

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
    sm = (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__BP___libc_start_main"); 
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }
  if (!sm) {
    fputs("papirun: cannot intercept beginning of process execution and therefore cannot begin profiling\n", stderr);
    exit(1);
  }
}

/*
 *  Library finalization
 */
void 
_fini(void)
{
  if (opt_debug >= 1) { fprintf(stderr, "** finalizing papirun.so **\n"); }
}


static void
init_options(void)
{
  char *env_debug, *env_recursive, *env_event, *env_period, *env_outpath, 
    *env_flags, *env_dump_events;

  /* Debugging: default is off */
  env_debug = getenv("PAPIRUN_DEBUG");
  opt_debug = (env_debug ? atoi(env_debug) : 0);

  if (opt_debug >= 1) { fprintf(stderr, "* processing options\n"); }
  
  /* Recursive profiling: default is on */
  env_recursive = getenv("PAPIRUN_RECURSIVE");
  opt_recursive = (env_recursive ? atoi(env_recursive) : 1);
  if (!opt_recursive) {
    /* turn off profiling for any processes spawned by this one */
    unsetenv("LD_PRELOAD");
  }

  if (opt_debug >= 1) { 
    fprintf(stderr, "  recursive profiling: %d\n", opt_recursive); 
  }
  
  /* Profiling event: default PAPI_TOT_CYC */
  {
    const papi_event_t* e = papirun_event_by_name("PAPI_TOT_CYC");
    opt_eventcode = e->code;
    
    env_event = getenv("PAPIRUN_EVENT_NAME");
    if (env_event) {
      if ((e = papirun_event_by_name(env_event)) == NULL) {
	fprintf(stderr, "papirun: invalid PAPI event '%s'. Aborting.\n",
		env_event);
	exit(-1);
      }
      opt_eventcode = e->code;
    }
  }
  
  /* Profiling period: default 2^15-1 */
  opt_sampleperiod = (1 << 15) - 1;
  env_period = getenv("PAPIRUN_SAMPLE_PERIOD");
  if (env_period) {
    opt_sampleperiod = atoi(env_period);
  }
  if (opt_sampleperiod == 0) {
    fprintf(stderr, "papirun: invalid period '%llu'. Aborting.\n", 
	    opt_sampleperiod);
    exit(-1);
  }

  /* Output path: default . */
  strncpy(opt_outpath, ".", PATH_MAX);
  env_outpath = getenv("PAPIRUN_OUTPUT_PATH");
  if (env_outpath) {
    strncpy(opt_outpath, env_outpath, PATH_MAX);
  }

  /* Profiling flags: default PAPI_PROFIL_POSIX */
  {
    const papi_flagdesc_t *f = papirun_flag_by_name("PAPI_PROFIL_POSIX");
    opt_flagscode = f->code;

    env_flags = getenv("PAPIRUN_EVENT_FLAG");
    if (env_flags) {
      if ((f = papirun_flag_by_name(env_flags)) == NULL) {
	fprintf(stderr, "papirun: invalid profiling flag '%s'. Aborting.\n",
		env_flags);
	exit(-1);
      }
      opt_flagscode = f->code;
    }
  }

  /* Dump events: default 0 */
  env_dump_events = getenv("PAPIRUN_DUMP_EVENTS");
  opt_dump_events = (env_dump_events ? atoi(env_dump_events) : 0);
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
  pr_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}

int 
__BP___libc_start_main START_MAIN_PARAMS
{
  pr_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}

static int 
pr_libc_start_main START_MAIN_PARAMS
{
  /* squirrel away for later use */
  command_name = ubp_av[0];  /* command is also in /proc/pid/cmdline */
  real_fini = fini;
  
  /* initialize papi profiling */
  init_papirun();
  
  /* launch the process */
  if (opt_debug >= 1) {
    fprintf(stderr, "** launching application: %s\n", command_name);
  }
  (*sm)(main, argc, ubp_av, init, pr_fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


/* papirun fini */
static void 
pr_fini(void)
{
  if (real_fini) (*real_fini)();
  done_papirun();
  exit(0);
}


/****************************************************************************
 * Initialize PAPI profiling
 ****************************************************************************/

static void init_papi(void);
static void start_papi(void);
static void init_output(void);

static void papirun_dump_valid_papi_events(int dump);

/*
 *  Prepare for PAPI profiling
 */
static void 
init_papirun(void)
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "* initializing papirun monitoring\n");
  }

  init_papi();

  /* List all PAPI events */
  if (opt_dump_events > 0) {
    papirun_dump_valid_papi_events(opt_dump_events);
    exit(0);
  }

  init_output();
  
  start_papi();
}


static void 
init_papi(void)
{  
  int pcode;
  const char* evstr;

  /* Initiailize PAPI library */
  int papi_version; 
  papi_version = PAPI_library_init(PAPI_VER_CURRENT);
  if (papi_version != PAPI_VER_CURRENT) {
    fprintf(stderr, "papirun: PAPI library initialization failure - expected version %d, dynamic library was version %d. Aborting.\n", PAPI_VER_CURRENT, papi_version);
    exit(-1);
  }

  /* Profiling event */
  papi_eventcode = opt_eventcode;
  evstr = papirun_event_by_code(papi_eventcode)->name;

  if ((pcode = PAPI_query_event(papi_eventcode)) != PAPI_OK) { 
    fprintf(stderr, 
	    "papirun: PAPI event '%s' not supported (code=%d). Aborting.\n",
	    evstr, pcode);
    exit(-1);
  }
  
  papi_eventset = PAPI_NULL;
  if (PAPI_create_eventset(&papi_eventset) != PAPI_OK) {
    fprintf(stderr, "papirun: failed to create PAPI event set. Aborting.\n");
    exit(-1);
  }

  if (PAPI_add_event(&papi_eventset, papi_eventcode) != PAPI_OK) {
    fprintf(stderr, "papirun: failed to add event '%s'. Aborting.\n", evstr);
    exit(-1);
  }
  
  /* Profiling period */
  papi_sampleperiod = opt_sampleperiod;

  /* Profiling flags */
  papi_flagscode = opt_flagscode;

  /* Set profiling scaling factor */

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
      this representation, the maximum value of scale is 0xffff
      (essentially 1). (0x10000 can also be used.)

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
}

static void 
start_papi(void)
{
  loadmodules_t *lm = papirun_code_lines_from_loadmap(opt_debug);
  unsigned int bufsz;
  int i;

  /* 1. Prepare profiling buffers and structures */
  loadmodules = lm;
  papi_profiles = 
    (PAPI_sprofil_t *) malloc(sizeof(PAPI_sprofil_t) * lm->count);

  if (opt_debug >= 3) { fprintf(stderr, "profile buffer details:\n"); }

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

    if (opt_debug >= 3) {
      fprintf(stderr, 
	      "\tprofile[%d] base = %p size = %u off = %lx scale = %u\n",
	      i, papi_profiles[i].pr_base, papi_profiles[i].pr_size, 
	      papi_profiles[i].pr_off, papi_profiles[i].pr_scale);
    }
  }
  
  if (opt_debug >= 3) {
    fprintf(stderr, "count = %d, es=%x ec=%x sp=%llu ef=%d\n",
	    lm->count, papi_eventset, papi_eventcode, 
	    papi_sampleperiod, papi_flagscode);
  }

  /* 2. Prepare PAPI subsystem for profiling */
  {
    int r = PAPI_sprofil(papi_profiles, lm->count, papi_eventset, 
			 papi_eventcode, papi_sampleperiod, 
			 papi_flagscode);
    if (r != PAPI_OK) {
      fprintf(stderr,  "papirun: PAPI_sprofil failed, code %d. Aborting.\n", 
	      r);
      exit(-1);
    }
  }

  /* 3. Set PAPI debug */
  {
    int papi_opt_debug;
    switch(opt_debug) {
    case 0: case 1: case 2:
      break;
    case 3:
      papi_opt_debug = PAPI_VERB_ECONT;
      break;
    case 4:
      papi_opt_debug = PAPI_VERB_ESTOP;
      break;
    }
    if (opt_debug >= 3) {
      fprintf(stderr, "setting debug option %d.\n", papi_opt_debug);
      if (PAPI_set_debug(papi_opt_debug) != PAPI_OK) {
	fprintf(stderr,"papirun: failed to set debug level. Aborting.\n");
	exit(-1);
      }
    }
  }

  /* 4. Launch PAPI */
  if (PAPI_start(papi_eventset) != PAPI_OK) {
    fprintf(stderr,"papirun: failed to start event set. Aborting.\n");
    exit(-1);
  }
}

static void 
init_output(void)
{
  static unsigned int outfilenmLen = PATH_MAX+32;
  char outfilenm[outfilenmLen];
  char* executable = command_name; 
  const char* eventstr = papirun_event_by_code(papi_eventcode)->name;
  unsigned int len;
  char hostnam[128]; 

  gethostname(hostnam, 128);

  {
    char* slash = rindex(executable, '/');
    if (slash) {
      executable = slash+1; /* basename of executable */
    }
  }
  
  len = strlen(opt_outpath) + strlen(executable) + strlen(eventstr) + 32;
  if (len >= outfilenmLen) {
    fprintf(stderr,"papirun: output file pathname too long! Aborting.\n");
    exit(-1);
  }

  sprintf(outfilenm, "%s/%s.%s.%s.%d", opt_outpath, executable, eventstr, 
	hostnam, getpid());
  
  proffile = fopen(outfilenm, "w");

  if (proffile == NULL) {
    fprintf(stderr,"papirun: failed to open output file '%s'. Aborting.\n",
	    outfilenm);
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
done_papirun(void)
{
  long long ct = 0;

  if (opt_debug >= 1) { fprintf(stderr, "done_papirun:\n"); }
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
write_all_profiles(void)
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
  /* Load module name and load offset */
  write_module_name(fp, m->name);
  hpc_fwrite_le8(&(m->offset), fp);

  /* Profiling event and period */
  hpc_fwrite_le4((uint32_t*)&papi_eventcode, fp);
  hpc_fwrite_le8(&papi_sampleperiod, fp);
  
  /* Profiling data */
  write_module_data(fp, p);
}

static void 
write_module_name(FILE *fp, char *name)
{
  /* Note: there is no null terminator on the name string */
  unsigned int namelen = strlen(name);
  hpc_fwrite_le4(&namelen, fp);
  fwrite(name, 1, namelen, fp);
  
  if (opt_debug >= 1) { fprintf(stderr, "writing module %s\n", name); }
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
  
  if (opt_debug >= 1) {
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

/****************************************************************************
 * Dump papi events
 ****************************************************************************/

static void 
papirun_dump_valid_papi_events(int dump)
{
  /* PAPI_library_init must have been called */
  
  papi_event_t *e = papirun_event_table;
  fprintf(stderr, "papirun events for this architecture:\n");

  for (; e->name != NULL; e++) {
    int pcode;
    if ((pcode = PAPI_query_event(e->code)) == PAPI_OK) { 
      if (dump == 1) {
	papirun_write_wrapped_event_list(stderr, e);
      } else if (dump == 2) {
	papirun_write_event(stderr, e);
      }
    }
  }
  fprintf(stderr,"\n");
}

