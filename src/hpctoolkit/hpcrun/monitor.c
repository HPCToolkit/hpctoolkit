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

/*************************** Variable Declarations **************************/

/* These variables are set when the library is initialized */

/* Intercepted libc routines */
static libc_start_main_fptr_t      start_main;
static libc_start_main_fini_fptr_t start_main_fini;

/* hpcrun options (this should be a tidy struct) */
static int      opt_debug;
static int      opt_recursive;
static char*    opt_eventname;
static uint64_t opt_sampleperiod;
static char     opt_outpath[PATH_MAX];
static int      opt_flagscode;

/*************************** Variable Declarations **************************/

/* These variables are set when libc_start_main is intercepted */

/* The profiled command and its run time load map */
static char*        profiled_cmd;
static rtloadmap_t* rtloadmap;

/* PAPI profiling information (list) */
static hpcpapi_profile_desc_vec_t papi_profdescs;
   static hpcpapi_profile_desc_t papi_profxxx; /* temporary */

/* hpcrun output files (list) */
static hpcrun_ofile_desc_vec_t hpc_ofiles;
   static hpcrun_ofile_desc_t ofile; /* temporary */

/****************************************************************************/

/*
 *  Library initialization
 */
void 
_init(void)
{
  char *error;

  init_options();

  if (opt_debug >= 1) { fprintf(stderr, "** initializing libhpcrun.so **\n"); }

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
    fputs("hpcrun: cannot intercept beginning of process execution and therefore cannot begin profiling\n", stderr);
    exit(1);
  }
}

/*
 *  Library finalization
 */
void 
_fini(void)
{
  if (opt_debug >= 1) { fprintf(stderr, "** finalizing libhpcrun.so **\n"); }
}


static void
init_options(void)
{
  char *env_debug, *env_recursive, *env_event, *env_period, *env_outpath, 
    *env_flags;
  int ret;

  /* Debugging: default is off */
  env_debug = getenv("HPCRUN_DEBUG");
  opt_debug = (env_debug ? atoi(env_debug) : 0);

  if (opt_debug >= 1) { fprintf(stderr, "* processing options\n"); }
  
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
  
  /* Profiling event: default PAPI_TOT_CYC */
  opt_eventname = "PAPI_TOT_CYC";
  env_event = getenv("HPCRUN_EVENT_NAME");
  if (env_event) {
    opt_eventname = env_event;
  }
  
  /* Profiling period: default 2^15-1 */
  opt_sampleperiod = (1 << 15) - 1;
  env_period = getenv("HPCRUN_SAMPLE_PERIOD");
  if (env_period) {
    opt_sampleperiod = atoi(env_period);
  }
  if (opt_sampleperiod == 0) {
    fprintf(stderr, "hpcrun: invalid period '%llu'. Aborting.\n", 
	    opt_sampleperiod);
    exit(-1);
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
	fprintf(stderr, "hpcrun: invalid profiling flag '%s'. Aborting.\n",
		env_flags);
	exit(-1);
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
  
  /* initialize papi profiling */
  init_hpcrun();
  
  /* launch the process */
  if (opt_debug >= 1) {
    fprintf(stderr, "** launching application: %s\n", profiled_cmd);
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

static void init_papi(hpcpapi_profile_desc_vec_t* profdescs, rtloadmap_t *lm);
static void start_papi(hpcpapi_profile_desc_vec_t* profdescs);
static void init_output(hpcpapi_profile_desc_vec_t* profdescs);

/*
 *  Prepare for PAPI profiling
 */
static void 
init_hpcrun(void)
{
  if (opt_debug >= 1) { 
    fprintf(stderr, "* initializing hpcrun monitoring\n");
  }
  
  rtloadmap = hpcrun_code_lines_from_loadmap(opt_debug);
  
  init_papi(&papi_profdescs, rtloadmap); /* init papi_profdescs */
  init_output(&papi_profdescs);
  start_papi(&papi_profdescs);
}


static void 
init_papi(hpcpapi_profile_desc_vec_t* profdescs, rtloadmap_t *lm)
{  
  int pcode;
  const char* evstr;
  
  /* Initiailize PAPI library */
  if (hpc_init_papi() != 0) {
    exit(-1);
  }
  
  /* Profiling event */
  papi_profxxx.ecode = 0;
  if ((pcode = PAPI_event_name_to_code(opt_eventname, &papi_profxxx.ecode))
      != PAPI_OK) {
    fprintf(stderr, "hpcrun: invalid PAPI event '%s'. Aborting.\n",
	    opt_eventname);
    exit(-1);
  }
  if ((pcode = PAPI_query_event(papi_profxxx.ecode)) != PAPI_OK) { 
    /* should not strictly be necessary */
    fprintf(stderr, 
	    "hpcrun: PAPI event '%s' not supported (code=%d). Aborting.\n",
	    evstr, pcode);
    exit(-1);
  }
  if ((pcode = PAPI_get_event_info(papi_profxxx.ecode, &papi_profxxx.einfo)) 
      != PAPI_OK) {
    fprintf(stderr, "hpcrun: PAPI error %d: '%s'. Aborting.\n",
	    pcode, PAPI_strerror(pcode));
    exit(-1);
  }
  
  papi_profxxx.eset = PAPI_NULL;
  if (PAPI_create_eventset(&papi_profxxx.eset) != PAPI_OK) {
    fprintf(stderr, "hpcrun: failed to create PAPI event set. Aborting.\n");
    exit(-1);
  }
  
  if (PAPI_add_event(papi_profxxx.eset, papi_profxxx.ecode) != PAPI_OK) {
    fprintf(stderr, "hpcrun: failed to add event '%s'. Aborting.\n", evstr);
    exit(-1);
  }
  
  /* Profiling period */
  papi_profxxx.period = opt_sampleperiod;

  /* Set profiling scaling factor */

  /*  cf. man profil()

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
      of blocks in the region to be profiled.
      
  */
  
  /* Profiling flags */
  papi_profxxx.flags = opt_flagscode;
  papi_profxxx.flags |= PAPI_PROFIL_BUCKET_32;
  
  //papi_profxxx.bytesPerCntr = sizeof(unsigned short); /* 2, PAPI v. 2 */
  papi_profxxx.bytesPerCntr = sizeof(unsigned int);   /* 4 */
  papi_profxxx.bytesPerCodeBlk = 4;
  papi_profxxx.scale = 0x10000; // 0x8000 * (papi_profxxx.bytesPerCntr >> 1);
  
  if ( (papi_profxxx.scale * papi_profxxx.bytesPerCodeBlk) != (65536 * papi_profxxx.bytesPerCntr) ) {
    fprintf(stderr, "hpcrun: Programming error!\n");
  }
}


static void 
start_papi(hpcpapi_profile_desc_vec_t* profdescs)
{
  rtloadmap_t *lm = rtloadmap; /* FIXME */
  unsigned int bufsz;
  int i;

  /* 1. Prepare profiling buffers and structures */
  papi_profxxx.sprofs = 
    (PAPI_sprofil_t *) malloc(sizeof(PAPI_sprofil_t) * lm->count);

  if (opt_debug >= 3) { fprintf(stderr, "profile buffer details:\n"); }

  for (i = 0; i < lm->count; ++i) {
    /* eliminate use of libm and ceil() by adding 1 */
    unsigned int ncntr = (lm->module[i].length / papi_profxxx.bytesPerCodeBlk) + 1;
    bufsz = ncntr * papi_profxxx.bytesPerCntr;
    
    /* buffer base */
    papi_profxxx.sprofs[i].pr_base = (unsigned short *)malloc(bufsz);
    /* buffer size */
    papi_profxxx.sprofs[i].pr_size = bufsz;
    /* pc offset */
    papi_profxxx.sprofs[i].pr_off = (caddr_t) (unsigned long) lm->module[i].offset;
    /* pc scaling */
    papi_profxxx.sprofs[i].pr_scale = papi_profxxx.scale;
    
    /* zero out the profile buffer */
    memset(papi_profxxx.sprofs[i].pr_base, 0x00, bufsz);

    if (opt_debug >= 3) {
      fprintf(stderr, 
	      "\tprofile[%d] base = %p size = %u off = %lx scale = %u\n",
	      i, papi_profxxx.sprofs[i].pr_base, papi_profxxx.sprofs[i].pr_size, 
	      papi_profxxx.sprofs[i].pr_off, papi_profxxx.sprofs[i].pr_scale);
    }
  }
  
  if (opt_debug >= 3) {
    fprintf(stderr, "count = %d, es=%x ec=%x sp=%llu ef=%d\n",
	    lm->count, papi_profxxx.eset, papi_profxxx.ecode, 
	    papi_profxxx.period, papi_profxxx.flags);
  }

  /* 2. Prepare PAPI subsystem for profiling */
  {
    int r = PAPI_sprofil(papi_profxxx.sprofs, lm->count, papi_profxxx.eset, 
			 papi_profxxx.ecode, papi_profxxx.period, 
			 papi_profxxx.flags);
    if (r != PAPI_OK) {
      fprintf(stderr,  "hpcrun: PAPI_sprofil failed, code %d. Aborting.\n", 
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
	fprintf(stderr,"hpcrun: failed to set debug level. Aborting.\n");
	exit(-1);
      }
    }
  }

  /* 4. Launch PAPI */
  if (PAPI_start(papi_profxxx.eset) != PAPI_OK) {
    fprintf(stderr,"hpcrun: failed to start event set. Aborting.\n");
    exit(-1);
  }
}


static void 
init_output(hpcpapi_profile_desc_vec_t* profdescs)
{
  static unsigned int outfilenmLen = PATH_MAX+32;
  int pcode;
  char outfilenm[outfilenmLen];
  char* executable = profiled_cmd; 
  char* eventstr;
  unsigned int len;
  char hostnam[128]; 
  
  eventstr = opt_eventname; /* FIXME: turn into papi_eventname */
  
  gethostname(hostnam, 128);
  
  {
    char* slash = rindex(executable, '/');
    if (slash) {
      executable = slash+1; /* basename of executable */
    }
  }
  
  len = strlen(opt_outpath) + strlen(executable) + strlen(eventstr) + 32;
  if (len >= outfilenmLen) {
    fprintf(stderr,"hpcrun: output file pathname too long! Aborting.\n");
    exit(-1);
  }

  sprintf(outfilenm, "%s/%s.%s.%s.%d", opt_outpath, executable, eventstr, 
	hostnam, getpid());
  
  ofile.fs = fopen(outfilenm, "w");

  if (ofile.fs == NULL) {
    fprintf(stderr,"hpcrun: failed to open output file '%s'. Aborting.\n",
	    outfilenm);
    exit(-1);
  }
}


/****************************************************************************
 * Finalize PAPI profiling
 ****************************************************************************/

static void write_all_profiles(FILE* fp, rtloadmap_t* lm, 
			       hpcpapi_profile_desc_vec_t* profdescs);

static void 
fini_papi(hpcpapi_profile_desc_vec_t* profdescs);

/*
 *  Finalize PAPI profiling
 */
static void 
fini_hpcrun(void)
{
  long long ct = 0;
  
  if (opt_debug >= 1) { fprintf(stderr, "fini_hpcrun:\n"); }
  PAPI_stop(papi_profxxx.eset, &ct);
  
  write_all_profiles(ofile.fs, rtloadmap, &papi_profdescs);
  
  fini_papi(&papi_profdescs);
}

static void 
fini_papi(hpcpapi_profile_desc_vec_t* profdescs)
{
  PAPI_cleanup_eventset(papi_profxxx.eset);
  PAPI_destroy_eventset(&papi_profxxx.eset);
  papi_profxxx.eset = PAPI_NULL;
  
  PAPI_shutdown();
}


/****************************************************************************
 * Write profile data
 ****************************************************************************/

static void write_module_profile(FILE *fp, rtloadmod_desc_t *m, 
				  PAPI_sprofil_t *p);
static void write_module_data(FILE *fp, PAPI_sprofil_t *p);
static void write_string(FILE *fp, char *str);


/*
 *  Write profile data for this process
 */
static void 
write_all_profiles(FILE* fp, rtloadmap_t* lm, 
		   hpcpapi_profile_desc_vec_t* profdescs)
{
  /* Header information */
  fwrite(HPCRUNFILE_MAGIC_STR, 1, HPCRUNFILE_MAGIC_STR_LEN, fp);
  fwrite(HPCRUNFILE_VERSION, 1, HPCRUNFILE_VERSION_LEN, fp);
  fputc(HPCRUNFILE_ENDIAN, fp);

  /* Load modules */
  {
    unsigned int i;
    hpc_fwrite_le4(&(lm->count), fp);
    for (i = 0; i < lm->count; ++i) {
      write_module_profile(fp, &(lm->module[i]), &(papi_profxxx.sprofs[i]));
    }
  }

  fclose(fp);
}

static void 
write_module_profile(FILE *fp, rtloadmod_desc_t *m, PAPI_sprofil_t *p)
{
  /* Load module name and load offset */
  if (opt_debug >= 1) { fprintf(stderr, "writing module %s\n", m->name); }
  write_string(fp, m->name);
  hpc_fwrite_le8(&(m->offset), fp);
  
  /* Profiling event name, description and period */
  write_string(fp, papi_profxxx.einfo.symbol);     /* event name */
  write_string(fp, papi_profxxx.einfo.long_descr); /* event description */
  hpc_fwrite_le8(&papi_profxxx.period, fp);
  
  /* Profiling data */
  write_module_data(fp, p);
}

static void 
write_module_data(FILE *fp, PAPI_sprofil_t *p)
{
  unsigned int count = 0, offset = 0, i = 0, inz = 0;
  unsigned int ncounters = (p->pr_size / papi_profxxx.bytesPerCntr);
  unsigned int *lpr_base = p->pr_base;
  
  /* Number of profiling entries */
  count = 0;
  for (i = 0; i < ncounters; ++i) {
    if (lpr_base[i] != 0) { count++; inz = i; }
  }
  hpc_fwrite_le4(&count, fp);
  
  if (opt_debug >= 1) {
    fprintf(stderr, "  profile has %d of %d non-zero counters", count,
	    ncounters);
    fprintf(stderr, " (last non-zero counter: %d)\n", inz);
  }
  
  /* Profiling entries */
  for (i = 0; i < ncounters; ++i) {
    if (lpr_base[i] != 0) {
      uint32_t cnt = lpr_base[i];
      hpc_fwrite_le4(&cnt, fp); /* count */
      offset = i * papi_profxxx.bytesPerCodeBlk;
      hpc_fwrite_le4(&offset, fp); /* offset (in bytes) from load addr */
      if (opt_debug >= 1) {
        fprintf(stderr, "  (cnt,offset)=(%d,%x)\n",cnt,offset);
      }
    }
  }
  
  /* cleanup */
  if (p->pr_base) {
    free(p->pr_base);
    p->pr_base = 0;
  }
}

static void 
write_string(FILE *fp, char *str)
{
  /* Note: there is no null terminator on 'str' */
  unsigned int len = strlen(str);
  hpc_fwrite_le4(&len, fp);
  fwrite(str, 1, len, fp);
}

/****************************************************************************/

