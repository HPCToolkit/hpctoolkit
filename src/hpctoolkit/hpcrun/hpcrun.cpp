/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Launch the PAPI profiler by setting up a preloaded library that
//    will intercept an application's execution and start the
//    profiler.  This file processes hpcrun arguments and passes them
//    to the profiling library through environment variables.
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
#include <unistd.h> /* for getopt(); cf. stdlib.h */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include <sys/types.h> /* for wait() */
#include <sys/wait.h>  /* for wait() */

/**************************** User Include Files ****************************/

#include "hpcpapi.h" /* <papi.h>, etc. */

#include "hpcrun.h"

/**************************** Forward Declarations **************************/

#define LD_PRELOAD "LD_PRELOAD"
#define HPCRUN_HOME "HPCRUN_HOME"

typedef enum {
  LIST_NONE  = 0, /* none */
  LIST_SHORT,     /* 'short' */
  LIST_LONG       /* 'long' */
} event_list_t;

#define eventlistBufSZ PATH_MAX

/* Options that will be passed to profiling library */
static const char* opt_recursive = "1";
static char        opt_eventlist[eventlistBufSZ] = "";
static const char* opt_out_path = NULL;
static const char* opt_flag = NULL;
static const char* opt_debug = NULL;
static char**      opt_command_argv = NULL; /* command to profile */

/* Options that will be consumed here */
static int myopt_list_events = LIST_NONE;
static int myopt_debug = 0;

static void
args_parse(int argc, char* argv[]);

/**************************** Forward Declarations **************************/

static void
init_papi();

static int
launch_and_profile(char* argv[]);

static void 
list_available_events(event_list_t listType);

/****************************************************************************/

/****************************************************************************
 * 
 ****************************************************************************/

int
main(int argc, char* argv[])
{
  int retval; 
  
  args_parse(argc, argv);
  
  // Special options that short circuit profiling
  if (myopt_list_events) {
    list_available_events(myopt_list_events);
    return 0;
  }
  
  // Launch and profile
  if ((retval = launch_and_profile(opt_command_argv)) != 0) {
    return retval; // error already printed
  }
  
  // FIXME: free memory in opt_command_argv
  return 0;
}


static void
init_papi() 
{
  /* Initiailize PAPI library */
  if (hpc_init_papi() != 0) {
    exit(-1);
  }
}


/****************************************************************************
 * Parse options
 ****************************************************************************/

static const char* args_command;

static const char* args_version_info =
"version " HPCRUN_VERSION;
/* #include <include/HPCToolkitVersionInfo.h> */

static const char* args_usage_summary1 =
"[-r] [-e <event>[:<period>]...] [-o <outpath>] [-f <flag>] -- <command> [arguments]\n";

static const char* args_usage_summary2 =
"[-l | -L] [-V] [-h]\n";

static const char* args_usage_details =
"hpcrun profiles the execution of an arbitrary command using the PAPI\n"
"profiling interface. During execution of <command>, the specified PAPI or\n"
"native events will be monitored.  Given multiple events it can create\n"
"multiple profile histograms during one run.\n"
"\n"
"Each <period> instances of the specified <event>, a counter associated with\n"
"the instruction at the current program counter location will be\n"
"incremented. When <command> terminates normally, a profile -- a histogram\n"
"of counts for instructions in each load module) will be written to a file\n"
"with name <command>.<event1>.<hostname>.<pid>.\n"
"\n"
"The special option '--' can be used to stop hpcrun option parsing.  This is\n"
"especially useful when <command> takes arguments of its own.\n"
"\n"
"General Options:\n"
"  -l           List PAPI and native events available on this architecture\n"
"  -L           Similar to above but with more information.\n"
"  -V           Print version information.\n"
"  -h           Print this help.\n"
"\n"
"Profiling Options: Defaults are shown in curly brackets {}.\n"
"  -r  By default all processes spawned by <command> will be profiled, each\n"
"      receiving its own output file. Use this option to turn off recursive\n"
"      profiling; only <command> will be profiled.\n"
"  -e <event>[:<period>]                                {PAPI_TOT_CYC:32767}\n"
"      An event to profile and its corresponding sample period.  <event>\n"
"      may be either a PAPI or native processor event\n"
"  -o <outpath>                                                          {.}\n"
"      Directory for output data\n"
"  -f <flag>                                             {PAPI_POSIX_PROFIL}\n"
"      Profile style flag\n"
"\n"
"NOTES:\n"
"* Because hpcrun uses LD_PRELOAD to initiate profiling it cannot be used\n"
"  to profile setuid commands; LD_PRELOAD is not permitted for them.\n"
"* Bug: For non-recursive profiling, LD_PRELOAD is currently unsetenv'd.\n"
"  Child processes that otherwise depend LD_PRELOAD will likely die.\n"
"\n";

static void 
args_print_version(FILE* fs);

static void 
args_print_usage(FILE* fs);

static void 
args_print_error(FILE* fs, const char* msg);


static void
args_parse(int argc, char* argv[])
{
  int cmdExists = 0;
  args_command = argv[0]; 
  
  // -------------------------------------------------------
  // Parse the command line
  // -------------------------------------------------------
  // Note: option list follows usage message
  extern char *optarg;
  extern int optind;
  int c;
  while ((c = getopt(argc, (char**)argv, "hVd:re:o:f:lL")) != EOF) {
    switch (c) {
      
    // Special options that are immediately evaluated
    case 'h': {
      args_print_usage(stderr);
      exit(0); 
      break; 
    }
    case 'V': { 
      args_print_version(stderr);
      exit(0);
      break; 
    }
    case 'd': { // debug
      opt_debug = optarg;
      myopt_debug = atoi(optarg);
      break; 
    }
    
    // Other options
    case 'r': {
      opt_recursive = "0";
      break; 
    }
    case 'e': {
      if ( (strlen(opt_eventlist) + strlen(optarg) + 1) >= eventlistBufSZ-1) {
	args_print_error(stderr, "Internal error: event list too long!");
	exit(1);
      }
      strcat(opt_eventlist, optarg);
      strcat(opt_eventlist, ";");
      break; 
    }
    case 'o': {
      opt_out_path = optarg;
      break; 
    }
    case 'f': {
      opt_flag = optarg;
      break; 
    }
    case 'l': {
      myopt_list_events = 1;
      break; 
    }
    case 'L': { 
      myopt_list_events = 2;
      break; 
    }
    
    // Error
    case ':':
    case '?': {
      args_print_error(stderr, "Error parsing arguments.");
      exit(1);
      break; 
    }
    }
  }
  
  // -------------------------------------------------------
  // Sift through results, checking for semantic errors
  // -------------------------------------------------------
  
  // Find command if it exists
  cmdExists = (argv[optind] != NULL);
  
  // If command does not exist, one of the list options must be present
  if (!cmdExists && !myopt_list_events) {
    args_print_error(stderr, "Missing <command> to profile.");
    exit(1);
  }
  
  if (cmdExists) {
    int i;
    int sz = argc - optind + 1;
    opt_command_argv = malloc(sz * sizeof(char *));
    for (i = optind; i < argc; ++i) {
      opt_command_argv[i - optind] = strdup(argv[i]);
    }
    opt_command_argv[i - optind] = NULL;
  }
}


static void 
args_print_version(FILE* fs)
{
  fprintf(fs, "%s: %s\n", args_command, args_version_info);
}


static void 
args_print_usage(FILE* fs)
{
  fprintf(fs, "Usage:\n  %s %s  %s %s\n%s", 
	  args_command, args_usage_summary1,
	  args_command, args_usage_summary2,
	  args_usage_details);
} 


static void 
args_print_error(FILE* fs, const char* msg)
{
  fprintf(fs, "%s: %s\n", args_command, msg);
  fprintf(fs, "Try `%s -h' for more information.\n", args_command);
}


/****************************************************************************
 * Profile
 ****************************************************************************/

static int
check_and_prepare_env_for_profiling();

static int
launch_and_profile(char* argv[])
{
  pid_t pid;
  int status;
  int retval;
  
  if (check_and_prepare_env_for_profiling() != 0) {
    return 1;
  }
  
  // Fork and exec the command to profile
  if ((pid = fork()) == 0) {
    // Child process
    const char* cmd = opt_command_argv[0];
    if (execvp(cmd, opt_command_argv) == -1) {
      fprintf(stderr, "hpcrun: Error exec'ing %s: %s\n", cmd, strerror(errno));
      return 1;
    }
    // never reached
  }
  
  // Parent process
  wait(&status);
  return WEXITSTATUS(status);
}


static int
check_and_prepare_env_for_profiling()
{
  // -------------------------------------------------------
  // Sanity check environment
  // -------------------------------------------------------
  char *hpcrunhome;
  hpcrunhome = getenv(HPCRUN_HOME);
  if (!hpcrunhome) {
    fprintf(stderr, "hpcrun: Please set " HPCRUN_HOME " before using\n");
    return 1;
  }
  
  // -------------------------------------------------------
  // Prepare environment: LD_PRELOAD
  // -------------------------------------------------------
  static char newpreloadval[PATH_MAX] = "";
  char *preloadval;
  
  sprintf(newpreloadval, " %s/" HPCRUN_LIB, hpcrunhome);
  
  preloadval = getenv(LD_PRELOAD);
  if (preloadval) {
    sprintf(newpreloadval + strlen(newpreloadval), " %s", preloadval);
  }
  setenv(LD_PRELOAD, newpreloadval, 1);  
  
  // -------------------------------------------------------
  // Prepare environment: Profiler options
  // -------------------------------------------------------
  if (opt_recursive) {
    setenv("HPCRUN_RECURSIVE", opt_recursive, 1);
  }
  if (opt_eventlist[0] != '\0') {
    setenv("HPCRUN_EVENT_LIST", opt_eventlist, 1);
  }
  if (opt_out_path) {
    setenv("HPCRUN_OUTPUT_PATH", opt_out_path, 1);
  }
  if (opt_flag) {
    setenv("HPCRUN_EVENT_FLAG", opt_flag, 1);
  }
  if (opt_debug) {
    setenv("HPCRUN_DEBUG", opt_debug, 1);
  }
  
  return 0;
}


/****************************************************************************
 * List profiling events
 ****************************************************************************/

/*
 *  List available PAPI and native events.  (Based on PAPI's
 *  src/ctests/avail.c) 
 */
static void 
list_available_events(event_list_t listType)
{
  if (listType == LIST_NONE) {
    return;
  }
  
  // -------------------------------------------------------
  // Ensure PAPI is initialized
  // -------------------------------------------------------
  init_papi();

  int i, count;
  int retval;
  int isIntel, isP4;
  PAPI_event_info_t info;
  const PAPI_hw_info_t* hwinfo = NULL;
  static const char* separator_major = "\n=========================================================================\n\n";
  static const char* separator_minor = "-------------------------------------------------------------------------\n";

  static const char* HdrPAPIShort = "Name\t\tDescription\n";
  static const char* FmtPAPIShort = "%s\t%s\n";
  static const char* HdrPAPILong = 
    "Name\t\tDerived\tDescription (Implementation Note)\n";
  static const char* FmtPAPILong = "%s\t%s\t%s (%s)\n";

  static const char* HdrNtvShort = "Name\t\t\t\tDescription\n";
  static const char* FmtNtvShort = "%-31s %s\n";
  static const char* HdrNtvLong =  "Name\t\t\t\tDescription\n";
  static const char* FmtNtvLong =  "%-31s %s\n";
  static const char* FmtNtvGrp    = "* %-29s %s\n";
  static const char* FmtNtvGrpMbr = "  %-29s %s\n";

  // -------------------------------------------------------
  // Hardware information
  // -------------------------------------------------------
  if ((hwinfo = PAPI_get_hardware_info()) == NULL) {
    fprintf(stderr,"papirun: failed to set debug level. Aborting.\n");
    exit(-1);
  }
  printf("*** Hardware information ***\n");
  printf(separator_minor);
  printf("Vendor string and code  : %s (%d)\n", hwinfo->vendor_string,
	 hwinfo->vendor);
  printf("Model string and code   : %s (%d)\n", 
	 hwinfo->model_string, hwinfo->model);
  printf("CPU Revision            : %f\n", hwinfo->revision);
  printf("CPU Megahertz           : %f\n", hwinfo->mhz);
  printf("CPU's in this Node      : %d\n", hwinfo->ncpu);
  printf("Nodes in this System    : %d\n", hwinfo->nnodes);
  printf("Total CPU's             : %d\n", hwinfo->totalcpus);
  printf("Number Hardware Counters: %d\n", 
	 PAPI_get_opt(PAPI_MAX_HWCTRS, NULL));
  printf("Max Multiplex Counters  : %d\n", PAPI_MPX_DEF_DEG);
  printf(separator_major);

  // -------------------------------------------------------
  // PAPI events
  // -------------------------------------------------------
  printf("*** Available PAPI preset events ***\n");
  printf(separator_minor);
  if (listType == LIST_SHORT) {
    printf(HdrPAPIShort);
  }
  else if (listType == LIST_LONG) {
    printf(HdrPAPILong);
  }
  printf(separator_minor);
  i = 0 | PAPI_PRESET_MASK;
  count = 0;
  do {
    if (PAPI_get_event_info(i, &info) == PAPI_OK) {
      if (listType == LIST_SHORT) {
	printf(FmtPAPIShort, info.symbol, info.long_descr);
      } 
      else if (listType == LIST_LONG) {
	printf(FmtPAPILong, 
	       info.symbol, (info.count > 1 ? "Yes" : "No"),
	       info.long_descr, (info.vendor_name ? info.vendor_name : ""));
      }
      count++;
    }
  } while (PAPI_enum_event(&i, PAPI_PRESET_ENUM_AVAIL) == PAPI_OK);
  printf("Total PAPI events reported: %d\n", count);
  printf(separator_major);  

  // -------------------------------------------------------
  // Native events
  // -------------------------------------------------------

  /* PAPI does not always correctly return a vendor id */
  isIntel = (hwinfo->vendor == PAPI_VENDOR_INTEL || 
	     (hwinfo->vendor_string && 
	      strcmp(hwinfo->vendor_string, "GenuineIntel") == 0));
  isP4 = isIntel && (hwinfo->model == 11 || hwinfo->model == 12
		     || hwinfo->model == 16);
  
  printf("*** Available native events ***\n");
  if (isP4) { printf("Note: Pentium 4 listing is by event groups; group names (*) are not events.\n"); }
  printf(separator_minor);
  if (listType == LIST_SHORT) { 
    printf(HdrNtvShort); 
  }
  else if (listType == LIST_LONG) { 
    printf(HdrNtvLong); 
  }
  printf(separator_minor);
  i = 0 | PAPI_NATIVE_MASK;
  count = 0;
  
  if (isP4) {
    /* Pentium IV special case */
    do {
      int j = i;
      unsigned l = 0;
      if (PAPI_get_event_info(i, &info) == PAPI_OK) {
	printf(FmtNtvGrp, info.symbol, info.long_descr);
	l = strlen(info.long_descr);
      }
      while (PAPI_enum_event(&j, PAPI_PENT4_ENUM_BITS) == PAPI_OK) {
	if (PAPI_get_event_info(j, &info) == PAPI_OK) {
	  printf(FmtNtvGrpMbr, info.symbol, info.long_descr + l + 1);
	  count++;
	}
      }
    } while (PAPI_enum_event(&i, PAPI_PENT4_ENUM_GROUPS) == PAPI_OK);
  }
  else {
    do {
      if (PAPI_get_event_info(i, &info) == PAPI_OK) {
	if (listType == LIST_SHORT || listType == LIST_LONG) {
	  const char* desc = info.long_descr;
	  if (strncmp(info.symbol, desc, strlen(info.symbol)) == 0) {
	    desc += strlen(info.symbol);
	  }
	  printf(FmtNtvShort, info.symbol, desc);
	} 
	count++;
      }
    } while (PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK);
  }
  
  printf("Total native events reported: %d\n", count);
  printf(separator_major);
}
