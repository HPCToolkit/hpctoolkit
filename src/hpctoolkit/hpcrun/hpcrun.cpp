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

/* Options that will be passed to profiling library */
static const char* opt_recursive = "1";
static const char* opt_event = NULL;
static const char* opt_period = NULL;
static const char* opt_out_path = NULL;
static const char* opt_flag = NULL;
static const char* opt_debug = NULL;
static char** opt_command_argv = NULL; /* command to profile */

/* Options that will be consumed here */
static int myopt_list_events = 0; /* 0, 1, 2: none, short, long */
static int myopt_debug = 0;

static void
args_parse(int argc, char* argv[]);

/**************************** Forward Declarations **************************/

static void
init_papi();

static int
launch_and_profile(char* argv[]);

static void 
list_available_events(int listType);

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
"[-r] [-e <event>...] [-p <period>] [-o <outpath>] [-f <flag>] -- <command> [args_to_command...]\n";

static const char* args_usage_summary2 =
"[-l | -L] [-V] [-h]\n";

static const char* args_usage_details =
"hpcrun profiles the execution of an arbitrary command using the PAPI\n"
"profiling interface. During execution of <command>, the specified PAPI or\n"
"native events will be monitored.\n"
"\n"
"Each <period> instances of the specified event, a counter associated with\n"
"the instruction at the current program counter location will be\n"
"incremented. When <command> terminates normally, a profile (a histogram of\n"
"counts for instructions in each load module) will be written to a file with\n"
"name <command>.<event>.<pid>.\n"
"\n"
"The special option '--' can be used to stop hpcrun option parsing.  This is\n"
"especially useful when <command> takes arguments of its own.\n"
"\n"
"General Options:\n"
"  -l           List PAPI and native events available on this architecture\n"
"  -L           <currently the same as the above>\n"
"  -V           Print version information.\n"
"  -h           Print this help.\n"
"\n"
"Profiling Options: Defaults are shown in square brackets [].\n"
"  -r           By default all processes spawned by <command> will be\n"
"               profiled, each receiving its own output file. Use this\n"
"               option to turn off recursive profiling; only <command> will\n"
"               be profiled.\n"
"  -e <event>   PAPI or native event to sample           [PAPI_TOT_CYCLES]\n"
"  -p <period>  Sample period                            [2^15 - 1]\n"
"  -o <outpath> Directory for output data                [.]\n"
"  -f <flag>    Profile style flag                       [PAPI_POSIX_PROFIL]\n"
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
  while ((c = getopt(argc, (char**)argv, "hVd:re:p:o:f:lL")) != EOF) {
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
      opt_event = optarg;
      break; 
    }
    case 'p': {
      opt_period = optarg;
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
  if (opt_event) {
    setenv("HPCRUN_EVENT_NAME", opt_event, 1);
  }
  if (opt_period) {
    setenv("HPCRUN_SAMPLE_PERIOD", opt_period, 1);
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
list_available_events(int listType)
{
  /* Ensure PAPI is initialized */
  init_papi();

  int i, count;
  int retval;
  PAPI_event_info_t info;
  const PAPI_hw_info_t* hwinfo = NULL;
  static const char* separator_major = "\n=========================================================================\n\n";
  static const char* separator_minor = "-------------------------------------------------------------------------\n";

#if (!defined(PAPI_ENUM_ALL))
  int PAPI_ENUM_ALL = 0; /* supposed to be defined according to man page... */
#endif

  /* Hardware information */
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

  /* PAPI events */
  printf("*** Available PAPI preset events ***\n");
  printf(separator_minor);
  printf("Name\t\tDerived\tDescription (Implementation Note)\n");
  printf(separator_minor);
  i = 0 | PRESET_MASK;
  count = 0;
  do {
    if (PAPI_get_event_info(i, &info) == PAPI_OK) {
      printf("%s\t%s\t%s (%s)\n",
	     info.symbol,
	     (info.count > 1 ? "Yes" : "No"),
	     info.long_descr, 
	     (info.vendor_name ? info.vendor_name : ""));
      count++;
    }
  } while (PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK);
  printf("Total PAPI events reported: %d\n", count);
  printf(separator_major);  
  
  /* Native events */
  printf("*** Available native events ***\n");  
  printf(separator_minor);
  printf("Name\t\t\t\tDescription\n");
  printf(separator_minor);
  i = 0 | NATIVE_MASK;
  count = 0;
  do {      
    if (PAPI_get_event_info(i, &info) == PAPI_OK) {
      const char* desc = info.long_descr;
      if (strncmp(info.symbol, desc, strlen(info.symbol)) == 0) {
	desc += strlen(info.symbol);
      }
      printf("%-30s %s\n", info.symbol, desc);
      count++;
    }
  } while (PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK);
  printf("Total native events reported: %d\n", count);
  printf(separator_major);
}
