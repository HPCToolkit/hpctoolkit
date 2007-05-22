// -*-Mode: C++;-*-
// $Id$

//***************************************************************************
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
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <unistd.h> /* for getopt(); cf. stdlib.h */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include <sys/types.h> /* for wait() */
#include <sys/wait.h>  /* for wait() */

//*************************** User Include Files ****************************

#include "hpcpapi.h" /* <papi.h>, etc. */
#include "dlpapi.h"
#include "hpcrun.h"
#include <lib/support/findinstall.h>

//*************************** Forward Declarations **************************

enum event_list_t {
  LIST_NONE  = 0, /* none */
  LIST_SHORT,     /* 'short' */
  LIST_LONG       /* 'long' */
};

#define eventlistBufSZ PATH_MAX

/* Options that will be passed to profiling library */
static const char* opt_recursive = "1";
static const char* opt_thread = "0";
static char        opt_eventlist[eventlistBufSZ] = "";
static const char* opt_out_path = NULL;
static const char* opt_flag = NULL;
static const char* opt_debug = NULL;
static char**      opt_command_argv = NULL; /* command to profile */

/* Options that will be consumed here */
static event_list_t myopt_list_events = LIST_NONE;
static int myopt_debug = 0;

static void
args_parse(int argc, char* argv[]);

//*************************** Forward Declarations **************************

#define LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#define LD_PRELOAD      "LD_PRELOAD"

static void
init_papi();

static int
prepare_ld_lib_path_for_papi();


static int
launch_and_profile(const char* installpath, char* argv[]);

static int
list_available_events(char* argv[], event_list_t listType);


static int
prepend_to_ld_lib_path(const char* str);

//***************************************************************************
//
//***************************************************************************

int
main(int argc, char* argv[])
{
  int retval; 
  char* installpath;
  
  args_parse(argc, argv);
  
  // Check for special options that short circuit profiling
  if (myopt_list_events) {
    // List events
    return list_available_events(argv, myopt_list_events);
  }
  else {
    // Launch and profile
    if ((installpath = findinstall(argv[0], "hpcrun")) == NULL) {
      fprintf(stderr, "%s: Cannot locate installation path\n", argv[0]);
    }
    if ((retval = launch_and_profile(installpath, opt_command_argv)) != 0) {
      return retval; // error already printed
    }
  }
  
  // never reached except on error: No need to free memory in opt_command_argv
  return 0;
}


static void
init_papi() 
{
  /* Initialize PAPI library */
  if (hpc_init_papi(dl_PAPI_is_initialized, dl_PAPI_library_init) != 0) {
    exit(-1); /* message already printed */
  }
}


static int
prepare_ld_lib_path_for_papi()
{
  /* LD_LIBRARY_PATH */
  return prepend_to_ld_lib_path(HPC_PAPI_LIBPATH);
}


//***************************************************************************
// Parse options
//***************************************************************************

static const char* args_command;

static const char* args_version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* args_usage_summary1 =
"[profiling-options] -- <command> [arguments]\n";

static const char* args_usage_summary2 =
"[-l | -L] [-P] [-V] [-h]\n";

static const char* args_usage_details =
"hpcrun profiles the execution of an arbitrary command using statistical\n"
"sampling. During execution of <command>, the specified events will be\n"
"monitored.  Events may be specified using either PAPI platform independent\n"
"names or the native event names for the processor.  hpcrun can sample.\n"
"multiple events during a single execution.\n"
"\n"
"For each event 'e' and its period 'p', for every 'p' instances of 'e' a\n"
"counter associated with the instruction at the current program counter\n"
"location will be incremented.  When <command> terminates normally, a\n"
"profile -- a histogram of counts for instructions in each load module --\n"
"will be written to a file with the name\n"
"<command>.<event1>.<hostname>.<pid>.<tid>. If multiple events are\n"
"specified, data for all events will be recorded in this single file even\n"
"though the file naming convention only uses <event1> in the name of the\n"
"output file.\n"
"\n"
"The special option '--' can be used to stop hpcrun option parsing.  This is\n"
"especially useful when <command> takes arguments of its own.\n"
"\n"
"hpcrun allows the user to abort a process *and* write the partial profiling\n"
"data to disk by sending the Interrupt signal (INT or Ctrl-C).  This can be\n"
"extremely useful on long-running or misbehaving applications.\n"
"\n"
"General Options:\n"
"  -l           List events available on this architecture \n"
"               (system, PAPI, native)\n"
"  -L           Similar to above but with more information.\n"
"  -P           Print the PAPI installation path.\n"
"  -V           Print version information.\n"
"  -h           Print this help.\n"
"\n"
"Profiling Options: Defaults are shown in curly brackets {}.\n"
"  -r  By default all processes spawned by <command> will be profiled, each\n"
"      receiving its own output file. Use this option to turn off recursive\n"
"      profiling; only <command> will be profiled.\n"

"  -t [<mode>]                                                        {each}\n"
"      Profile threads. (Without this flag, profiling of a multithreaded\n"
"      process will silently die.) There are two modes:\n"
"        each: Create separate profiles for each thread.\n"
"        all:  Create one combined profile of all threads.\n"
"      NOTE 1: The WALLCLK event cannot be used in a multithreaded process.\n"
"      NOTE 2: Only POSIX threads are supported.\n"
"  -e <event>[:<period>]                               {PAPI_TOT_CYC:999999}\n"
"      An event to profile and its corresponding sample period.  <event>\n"
"      may be either a PAPI or native processor event\n"
"      NOTE 1: It is recommended that you always specify the sampling period\n"
"              for each profiling event.\n"
"      NOTE 2: You may use the special event WALLCLK to profile with wall\n"
"              clock time.  It may be used only *once* and cannot be used\n"
"              with another event.  It is an error to specify a period.\n"
"      NOTE 3: Multiple events may be selected for profiling during an\n" 
"              execution by using  multiple '-e' arguments. \n"
"      NOTE 4: The maximum number of events that can be monitored during a\n"
"              single execution depends on the processor. Not all combinations\n"
"              of events may be monitored in the same execution; allowable\n"
"              combinations depend on the processor. check your \n"
"              processor documentation for the details of both issues.\n"
"  -o <outpath>                                                          {.}\n"
"      Directory for output data\n"
"  -f <flag>                                             {PAPI_POSIX_PROFIL}\n"
"      Profile style flag\n"
"\n"
"NOTES:\n"
"* Because hpcrun uses LD_PRELOAD to initiate profiling, it cannot be used\n"
"  to profile setuid commands.\n"
"* For the same reason, it cannot profile statically linked applications.\n"
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
args_print_papi(FILE* fs);


static void
args_parse(int argc, char* argv[])
{
  int cmdExists = 0;
  int c;

  args_command = argv[0];
  
  // -------------------------------------------------------
  // Parse the command line
  // -------------------------------------------------------
  // Note: option list follows usage message

  // extern char *optarg; -- provided by unistd.h
  // extern int optind; 

  // NOTE: This getopt string uses some GNU extensions (::).  
  // FIXME: CONVERT to use HPCToolkit's option parser.
  while ((c = getopt(argc, (char**)argv, "hVd:Prt::e:o:f:lL")) != EOF) {
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
    case 'P': { 
      args_print_papi(stderr);
      exit(0);
      break; 
    }
    
    // Other options
    case 'r': {
      opt_recursive = "0";
      break; 
    }
    case 't': {
      if (optarg) {
	if (strcmp(optarg, "each") == 0) {
	  opt_thread = HPCRUN_THREADPROF_EACH_STR;
	}
	else if (strcmp(optarg, "all") == 0) {
	  opt_thread = HPCRUN_THREADPROF_ALL_STR;
	}
	else {
	  args_print_error(stderr, "Invalid option to -t!");
	  exit(1);
	}
      }
      else {
	opt_thread = HPCRUN_THREADPROF_EACH_STR;
      }
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
      myopt_list_events = LIST_SHORT;
      break; 
    }
    case 'L': { 
      myopt_list_events = LIST_LONG;
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
    opt_command_argv = (char**)malloc(sz * sizeof(char *));
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


static void 
args_print_papi(FILE* fs)
{
  fprintf(fs, "%s: Using PAPI installed at '"HPC_PAPI_LIBSO"'\n",
	  args_command);
}

//***************************************************************************
// Profile
//***************************************************************************

static int
check_and_prepare_env_for_profiling(const char* installpath);


static int
launch_and_profile(const char* installpath, char* argv[])
{
  pid_t pid;
  int status;
  
  if (check_and_prepare_env_for_profiling(installpath) != 0) {
    return 1;
  }

  if (myopt_debug >= 1) {
    fprintf(stderr, "hpcrun (pid %d) ==> %s\n", getpid(), opt_command_argv[0]);
  }
  
  // Fork and exec the command to profile
  if ((pid = fork()) == 0) {
    // Child process
    const char* cmd = opt_command_argv[0];
    if (execvp(cmd, opt_command_argv) == -1) {
      fprintf(stderr, "%s: Error exec'ing '%s': %s\n", 
	      args_command, cmd, strerror(errno));
      return 1;
    }
    // never reached
  }
  
  // Parent process
  wait(&status);
  return WEXITSTATUS(status);
}


static int
check_and_prepare_env_for_profiling(const char* installpath)
{  
#define HPCRUN_PATH "/lib/hpctoolkit/"
  char newval[PATH_MAX] = "";
  char *oldval;
  int sz;

  // -------------------------------------------------------
  // Prepare environment
  // -------------------------------------------------------
  
  /* LD_LIBRARY_PATH */
  prepare_ld_lib_path_for_papi();

  /* LD_PRELOAD */
  snprintf(newval, PATH_MAX, "%s" HPCRUN_PATH HPCRUN_LIB, installpath);
#ifdef HAVE_MONITOR
  const char* INSTALLED_MONITOR = HPC_MONITOR_LIBSO_INSTALLED;
  int ofst = strlen(installpath) + strlen(HPCRUN_PATH HPCRUN_LIB);
  if (INSTALLED_MONITOR[0] == '/') {
    snprintf(newval + ofst, PATH_MAX - ofst, " " HPC_MONITOR_LIBSO_INSTALLED);
  }
  else {
    snprintf(newval + ofst, PATH_MAX - ofst, " %s/" HPC_MONITOR_LIBSO_INSTALLED,
	     installpath);
  }
#endif
  oldval = getenv(LD_PRELOAD);
  if (oldval) {
    sz = PATH_MAX - (strlen(newval) + 1); /* 'path ' */
    snprintf(newval + strlen(newval), sz, " %s", oldval);
  }
  newval[PATH_MAX-1] = '\0';
  setenv(LD_PRELOAD, newval, 1);
  
  //fprintf(stderr, "%s\n", getenv(LD_LIBRARY_PATH));
  //fprintf(stderr, "%s\n", getenv(LD_PRELOAD));
  
  // -------------------------------------------------------
  // Prepare environment: Profiler options
  // -------------------------------------------------------
  if (opt_recursive) {
    setenv("HPCRUN_RECURSIVE", opt_recursive, 1);
  }
  if (opt_thread) {
    setenv("HPCRUN_THREAD", opt_thread, 1);
  }
  if (opt_eventlist[0] != '\0') {
    setenv("HPCRUN_EVENT_LIST", opt_eventlist, 1);
  }
  if (opt_out_path) {
    setenv("HPCRUN_OUTPUT", opt_out_path, 1);
    setenv("HPCRUN_OPTIONS", "DIR", 1); // hpcex extensions
  }
  if (opt_flag) {
    setenv("HPCRUN_EVENT_FLAG", opt_flag, 1);
  }
  if (opt_debug) {
    setenv("HPCRUN_DEBUG", opt_debug, 1);
  }
  
  return 0;
}



//***************************************************************************
// List profiling events
//***************************************************************************

static void
list_available_events_helper(event_list_t listType);

static int
check_and_prepare_env_for_eventlisting();


/*
 *  List available events.
 */
static int
list_available_events(char* argv[], event_list_t listType)
{
  static const char* HPCRUN_TAG = "HPCRUN_SUPER_SECRET_TAG";
  char* envtag = NULL;
  pid_t pid;
  int status = 0;
  
  // For a (security?) reason I do not understand, dlopen may *ignore*
  // a call to setenv() modifying LD_LIBRARY_PATH.  Thus we set the
  // environment (adding a special tag to prevent infinite recursion)
  // and then fork() and exec a call to ourself.
  envtag = getenv(HPCRUN_TAG);
  if (!envtag) {
    // 1. No hpcrun tag: prepare env, add the tag and fork/exec
    status |= check_and_prepare_env_for_eventlisting();
    status |= setenv(HPCRUN_TAG, "1", 1);
    if (status != 0) { 
      fprintf(stderr, "%s: Error preparing environment\n", args_command);
      return status; 
    }
    //fprintf(stderr, "%s\n", getenv(LD_LIBRARY_PATH));
    
    // Fork and exec
    if ((pid = fork()) == 0) {
      // Child process
      const char* cmd = argv[0];
      if (execvp(cmd, argv) == -1) {
	fprintf(stderr, "%s: Error exec'ing myself!: %s\n", 
		args_command, strerror(errno));
	return 1;
      }
      // never reached
    }
    
    // Parent process
    wait(&status);
    return WEXITSTATUS(status);
  }
  else {
    // 2. List the events
    dlopen_papi();
    list_available_events_helper(listType);    
    dlclose_papi();
    return 0;
  }
}


/*
 *  List available system, PAPI and native events.  (Mostly based on
 *  PAPI's src/ctests/avail.c)
 */
static void 
list_available_events_helper(event_list_t listType)
{
  int i, count;
  int isIntel, isP4;
  PAPI_event_info_t info;
  const PAPI_hw_info_t* hwinfo = NULL;
  static const char* separator_major = "\n=========================================================================\n\n";
  static const char* separator_minor = "-------------------------------------------------------------------------\n";

  static const char* HdrPAPIShort = "Name\t\tDescription\n";
  static const char* FmtPAPIShort = "%s\t%s\n";
  static const char* HdrPAPILong = 
    "Name\t     Profilable\tDescription (Implementation Note)\n";
  static const char* FmtPAPILong = "%s\t%s\t%s (%s)\n";

  static const char* HdrNtvShort = "Name\t\t\t\tDescription\n";
  static const char* FmtNtvShort = "%-31s %s\n";
  static const char* HdrNtvLong =  "Name\t\t\t\tDescription\n";
  //static const char* FmtNtvLong =  "%-31s %s\n";
  static const char* FmtNtvGrp    = "* %-29s %s\n";
  static const char* FmtNtvGrpMbr = "  %-29s %s\n";
  
  if (listType == LIST_NONE) {
    return;
  }
  
  // -------------------------------------------------------
  // Ensure PAPI is initialized
  // -------------------------------------------------------
  init_papi();

  // -------------------------------------------------------
  // Hardware information
  // -------------------------------------------------------
  if ((hwinfo = dl_PAPI_get_hardware_info()) == NULL) {
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
	 dl_PAPI_get_opt(PAPI_MAX_HWCTRS, NULL));
  printf("Max Multiplex Counters  : %d\n", PAPI_MPX_DEF_DEG);
  printf(separator_major);

  // -------------------------------------------------------
  // Wall clock time
  // -------------------------------------------------------
  printf("*** Wall clock time ***\n");
  printf(HPCRUN_EVENT_WALLCLK_STR"     wall clock time (1 millisecond period)\n");
  // printf(HPCRUN_EVENT_FWALLCLK_STR"    fast wall clock time (1 millisecond period)\n");
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
    if (dl_PAPI_get_event_info(i, &info) == PAPI_OK) {
      if (listType == LIST_SHORT) {
	printf(FmtPAPIShort, info.symbol, info.long_descr);
      } 
      else if (listType == LIST_LONG) {
	/* NOTE: Although clumsy, this test has official sanction. */
	int profilable = 1;
	if ((info.count > 1) && strcmp(info.derived, "DERIVED_CMPD") != 0) {
	  profilable = 0;
	}
	printf(FmtPAPILong, 
	       info.symbol, ((profilable) ? "Yes" : "No"),
	       info.long_descr, (info.note ? info.note : ""));
      }
      count++;
    }
  } while (dl_PAPI_enum_event(&i, PAPI_PRESET_ENUM_AVAIL) == PAPI_OK);
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
      if (dl_PAPI_get_event_info(i, &info) == PAPI_OK) {
	printf(FmtNtvGrp, info.symbol, info.long_descr);
	l = strlen(info.long_descr);
      }
      while (dl_PAPI_enum_event(&j, PAPI_PENT4_ENUM_BITS) == PAPI_OK) {
	if (dl_PAPI_get_event_info(j, &info) == PAPI_OK) {
	  printf(FmtNtvGrpMbr, info.symbol, info.long_descr + l + 1);
	  count++;
	}
      }
    } while (dl_PAPI_enum_event(&i, PAPI_PENT4_ENUM_GROUPS) == PAPI_OK);
  }
  else {
    do {
      if (dl_PAPI_get_event_info(i, &info) == PAPI_OK) {
	if (listType == LIST_SHORT || listType == LIST_LONG) {
	  const char* desc = info.long_descr;
	  if (strncmp(info.symbol, desc, strlen(info.symbol)) == 0) {
	    desc += strlen(info.symbol);
	  }
	  printf(FmtNtvShort, info.symbol, desc);
	} 
	count++;
      }
    } while (dl_PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK);
  }
  
  printf("Total native events reported: %d\n", count);
  printf(separator_major);
}


static int
check_and_prepare_env_for_eventlisting()
{
  return prepare_ld_lib_path_for_papi();
}


//***************************************************************************
// Misc
//***************************************************************************

static int
prepend_to_ld_lib_path(const char* str)
{
  char newval[PATH_MAX] = "";
  char *oldval;
  int sz;
  
  /* LD_LIBRARY_PATH */
  strncpy(newval, str, PATH_MAX);
  oldval = getenv(LD_LIBRARY_PATH);
  if (oldval) {
    sz = PATH_MAX - (strlen(newval) + 1); /* 'path:' */
    snprintf(newval + strlen(newval), sz, ":%s", oldval);
  }
  newval[PATH_MAX-1] = '\0';
  setenv(LD_LIBRARY_PATH, newval, 1);
  return 0;
}
