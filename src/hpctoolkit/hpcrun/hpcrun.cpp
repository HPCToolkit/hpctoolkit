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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h> /* for wait() */
#include <sys/wait.h>  /* for wait() */

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "Args.hpp"
#include "hpcpapi.h" /* <papi.h>, etc. */
#include "dlpapi.h"
#include "hpcrun.h"

#include <include/general.h>

#include <lib/support/diagnostics.h>
#include <lib/support/findinstall.h>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

#define LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#define LD_PRELOAD      "LD_PRELOAD"

//*************************** Forward Declarations **************************

static int
list_available_events(char* argv[], Args::EventList_t listType);

static void 
print_external_lib_paths();

static int
launch_with_profiling(const char* installpath, const Args& args);

static int
prepare_ld_lib_path_for_papi();

static int
prepend_to_ld_lib_path(const char* str);

static int
prepend_to_ld_preload(const char* str);

//***************************************************************************
//
//***************************************************************************

static int
real_main(int argc, char* argv[]);


int
main(int argc, char* argv[])
{
  try {
    return real_main(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  } 
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  } 
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }
}


static int
real_main(int argc, char* argv[])
{
  int ret = 0;
  Args args(argc, argv);
  
  if (args.listEvents != Args::LIST_NONE) {
    ret = list_available_events(argv, args.listEvents);
  }
  else if (args.printPaths) {
    print_external_lib_paths();
  }
  else {
    // Launch and profile
    char* installpath = findinstall(argv[0], "hpcrun");
    DIAG_Assert(installpath, "Cannot locate installation path for 'hpcrun'");
    
    ret = launch_with_profiling(installpath, args);
    // only returns on error
  }
  
  return ret;
}


//***************************************************************************
// Profile
//***************************************************************************

static int
prepare_env_for_profiling(const char* installpath, const Args& args);


static int
launch_with_profiling(const char* installpath, const Args& args)
{
  pid_t pid;
  int status;

  // Gather <command> into a NULL-terminated argv list
  char** profArgV = new char*[args.profArgV.size() + 1];
  for (uint i; i < args.profArgV.size(); ++i) {
    profArgV[i] = (char*)args.profArgV[i].c_str();
  }
  profArgV[args.profArgV.size()] = NULL;
  
  DIAG_Msg(1, "hpcrun (pid " << getpid() << ") ==> " << profArgV[0]);
  
  prepare_env_for_profiling(installpath, args);
  
  // Fork and exec the command to profile
  if ((pid = fork()) == 0) {
    // Child process
    const char* cmd = profArgV[0];
    if (execvp(cmd, profArgV) == -1) {
      DIAG_Throw("Error exec'ing '" << cmd << "': " << strerror(errno));
    }
    // never reached
  }
  
  // Parent process
  wait(&status);
  return WEXITSTATUS(status);
}


static int
prepare_env_for_profiling(const char* installpath, const Args& args)
{  
  char buf[PATH_MAX] = "";
  int sz;

  // -------------------------------------------------------
  // Prepare LD_LIBRARY_PATH
  // -------------------------------------------------------

  // To support multi-lib we pack LIB_LIBRARY_PATH with all versions
  
  // LD_LIBRARY_PATH for libpapi
  prepare_ld_lib_path_for_papi();

  // LD_LIBRARY_PATH for hpcrun (dynamically determined)
#if defined(HAVE_OS_MULTILIB)
  snprintf(buf, PATH_MAX, "%s/lib64/hpctoolkit", installpath);
  prepend_to_ld_lib_path(buf);
  snprintf(buf, PATH_MAX, "%s/lib32/hpctoolkit", installpath);
  prepend_to_ld_lib_path(buf);
#endif
  snprintf(buf, PATH_MAX, "%s/lib/hpctoolkit", installpath);
  prepend_to_ld_lib_path(buf);

  // LD_LIBRARY_PATH for libmonitor (statically or dynamically determined)
#ifdef HAVE_MONITOR
  const char* MON = HPC_MONITOR;
  if (MON[0] == '/') { 
    // statically determined
#if defined(HAVE_OS_MULTILIB)
    prepend_to_ld_lib_path(HPC_MONITOR "/lib32/:" HPC_MONITOR "/lib64/");
#endif
    prepend_to_ld_lib_path(HPC_MONITOR "/lib/");
  }
  else {
    // dynamically determined
#if defined(HAVE_OS_MULTILIB)
    snprintf(buf, PATH_MAX, "%s/lib64/" HPC_MONITOR, installpath);
    prepend_to_ld_lib_path(buf);
    snprintf(buf, PATH_MAX, "%s/lib32/" HPC_MONITOR, installpath);
    prepend_to_ld_lib_path(buf);
#endif
    snprintf(buf, PATH_MAX, "%s/lib/" HPC_MONITOR, installpath);
    prepend_to_ld_lib_path(buf);
  }
#endif /* HAVE_MONITOR */

  
  // -------------------------------------------------------
  // Prepare LD_PRELOAD
  // -------------------------------------------------------

  prepend_to_ld_preload(HPCRUN_LIB " " HPC_LIBMONITOR_SO);
  
  DIAG_Msg(1, "hpcrun (pid " << getpid() << "): LD_LIBRARY_PATH=" << getenv(LD_LIBRARY_PATH));
  DIAG_Msg(1, "hpcrun (pid " << getpid() << "): LD_PRELOAD=" << getenv(LD_PRELOAD));

  
  // -------------------------------------------------------
  // Prepare environment: Profiler options
  // -------------------------------------------------------
  if (!args.profRecursive.empty()) {
    const char* val = (args.profRecursive == "yes") ? "1" : "0";
    setenv("HPCRUN_RECURSIVE", val, 1);
  }
  if (!args.profThread.empty()) {
    const char* val = HPCRUN_THREADPROF_EACH_STR;
    if (args.profThread == "all") {
      val = HPCRUN_THREADPROF_ALL_STR;
    }
    setenv("HPCRUN_THREAD", val, 1);
  }
  if (!args.profEvents.empty()) {
    setenv("HPCRUN_EVENT_LIST", args.profEvents.c_str(), 1);
  }
  if (!args.profOutput.empty()) {
    setenv("HPCRUN_OUTPUT", args.profOutput.c_str(), 1);
    setenv("HPCRUN_OPTIONS", "DIR", 1); // hpcex extensions
  }
  if (!args.profPAPIFlag.empty()) {
    setenv("HPCRUN_EVENT_FLAG", args.profPAPIFlag.c_str(), 1);
  }
  DIAG_If(1) {
    string val = StrUtil::toStr(Diagnostics_GetDiagnosticFilterLevel());
    setenv("HPCRUN_DEBUG",  val.c_str(), 1);
    setenv("MONITOR_DEBUG", val.c_str(), 1);
  }
  
  return 0;
}



//***************************************************************************
// List profiling events
//***************************************************************************

static void
list_available_events_helper(Args::EventList_t listType);

static int
check_and_prepare_env_for_eventlisting();

static void
init_papi(); 


/*
 *  List available events.
 */
static int
list_available_events(char* argv[], Args::EventList_t listType)
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
      DIAG_Throw("Error preparing environment.");
    }
    
    // Fork and exec
    if ((pid = fork()) == 0) {
      // Child process
      const char* cmd = argv[0];
      if (execvp(cmd, argv) == -1) {
	DIAG_Throw("Error exec'ing myself: " << strerror(errno));
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
list_available_events_helper(Args::EventList_t listType)
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
  
  if (listType == Args::LIST_NONE) {
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
    DIAG_Throw("PAPI_get_hardware_info failed");
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
  if (listType == Args::LIST_SHORT) {
    printf(HdrPAPIShort);
  }
  else if (listType == Args::LIST_LONG) {
    printf(HdrPAPILong);
  }
  printf(separator_minor);
  i = 0 | PAPI_PRESET_MASK;
  count = 0;
  do {
    if (dl_PAPI_get_event_info(i, &info) == PAPI_OK) {
      if (listType == Args::LIST_SHORT) {
	printf(FmtPAPIShort, info.symbol, info.long_descr);
      } 
      else if (listType == Args::LIST_LONG) {
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
  if (listType == Args::LIST_SHORT) { 
    printf(HdrNtvShort); 
  }
  else if (listType == Args::LIST_LONG) { 
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
	if (listType == Args::LIST_SHORT || listType == Args::LIST_LONG) {
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


static void
init_papi() 
{
  /* Initialize PAPI library */
  if (hpc_init_papi(dl_PAPI_is_initialized, dl_PAPI_library_init) != 0) {
    exit(-1); /* message already printed */
  }
}


//***************************************************************************
// Misc
//***************************************************************************

static int
prepare_ld_lib_path_for_papi()
{
#if defined(HAVE_OS_MULTILIB)
  prepend_to_ld_lib_path(HPC_PAPI "/lib32:" HPC_PAPI "/lib64");
#endif
  prepend_to_ld_lib_path(HPC_PAPI "/lib");
}


static void 
print_external_lib_paths()
{
  DIAG_Msg(0, "Using PAPI installation: '"HPC_PAPI"'");
  const char* MON = HPC_MONITOR;
  if (MON[0] == '/') {
    DIAG_Msg(0, "Using MONITOR installation: '"HPC_MONITOR"'\n");
  }
}


static int
prepend_to_env_var(const char* env_var, const char* str, char sep);

static int
prepend_to_ld_lib_path(const char* str)
{
  return prepend_to_env_var(LD_LIBRARY_PATH, str, ':');
}


static int
prepend_to_ld_preload(const char* str)
{
  return prepend_to_env_var(LD_PRELOAD, str, ' ');
}


static int
prepend_to_env_var(const char* env_var, const char* str, char sep)
{
  char newval[PATH_MAX] = "";
  char *oldval;
  int sz;
  
  strncpy(newval, str, PATH_MAX);
  oldval = getenv(env_var);
  if (oldval) {
    sz = PATH_MAX - (strlen(newval) + 1); /* 'path:' */
    snprintf(newval + strlen(newval), sz, "%c%s", sep, oldval);
  }
  newval[PATH_MAX-1] = '\0';
  setenv(env_var, newval, 1);
  return 0;
}

