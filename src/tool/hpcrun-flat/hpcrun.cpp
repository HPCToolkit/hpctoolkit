// -*-Mode: C++;-*-

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

//***************************************************************************
//
// File: 
//    $HeadURL$
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

#include <iostream>
using std::ostream;

#include <iomanip>

#include <string>
using std::string;

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h> /* for wait() */
#include <sys/wait.h>  /* for wait() */
#include <unistd.h>    /* for getpid(), fork(), etc. */

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>
#include <include/uint.h>

#include "Args.hpp"
#include "hpcpapi.h" /* <papi.h>, etc. */
#include "dlpapi.h"
#include "hpcrun.h"

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
    char* installpath = findinstall(argv[0], HPCRUN_NAME);
    DIAG_Assert(installpath, "Cannot locate installation path for '"HPCRUN_NAME"'");
    
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
  for (uint i = 0; i < args.profArgV.size(); ++i) {
    profArgV[i] = (char*)args.profArgV[i].c_str();
  }
  profArgV[args.profArgV.size()] = NULL;
  
  DIAG_Msg(1, HPCRUN_NAME" (pid " << getpid() << ") ==> " << profArgV[0]);
  
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

  // -------------------------------------------------------
  // Prepare LD_LIBRARY_PATH (in reverse order)
  // -------------------------------------------------------

  // To support multi-lib we pack LIB_LIBRARY_PATH with all versions
  
  // LD_LIBRARY_PATH for libpapi (even though we link with libpapi,
  // this may be needed to resolve PAPI dependencies such as libpfm)
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
  
  DIAG_Msg(1, HPCRUN_NAME" (pid " << getpid() << "): LD_LIBRARY_PATH=" << getenv(LD_LIBRARY_PATH));
  DIAG_Msg(1, HPCRUN_NAME" (pid " << getpid() << "): LD_PRELOAD=" << getenv(LD_PRELOAD));

  
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
  DIAG_If(1) {
    // PAPI_DEBUG=PROFILE | SUBSTRATE | THREADS | OVERFLOW
    //setenv("PAPI_DEBUG",  "PROFILE, OVERFLOW, SUBSTRATE", 1);
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
    DIAG_Msg(1, "LD_LIBRARY_PATH=" << getenv(LD_LIBRARY_PATH));
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
  using std::setfill;
  using std::setw;

#define SEPARATOR_MAJOR setfill('=') << setw(77) << "" << "\n\n"
#define SEPARATOR_MINOR setfill('-') << setw(77) << "" << "\n"

  ostream& os = std::cout;

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
  const PAPI_hw_info_t* hwinfo = NULL;
  if ((hwinfo = dl_PAPI_get_hardware_info()) == NULL) {
    DIAG_Throw("PAPI_get_hardware_info failed");
  }
  os << "*** Hardware information ***\n";
  os << SEPARATOR_MINOR;
  os << "Vendor string and code  : " 
     << hwinfo->vendor_string << " (" << hwinfo->vendor <<")\n";
  os << "Model string and code   : " 
     << hwinfo->model_string << " (" << hwinfo->model << ")\n";
  os << "CPU Revision            : " << hwinfo->revision << "\n";
  os << "CPU Megahertz           : " << hwinfo->mhz << "\n";
  os << "CPU's in this Node      : " << hwinfo->ncpu << "\n";
  os << "Nodes in this System    : " << hwinfo->nnodes << "\n";
  os << "Total CPU's             : " << hwinfo->totalcpus << "\n";
  os << "Number Hardware Counters: " << dl_PAPI_get_opt(PAPI_MAX_HWCTRS, NULL) << "\n";
  os << "Max Multiplex Counters  : " << dl_PAPI_get_opt(PAPI_MAX_MPX_CTRS, NULL) << "\n";
  os << SEPARATOR_MAJOR;

  // -------------------------------------------------------
  // Wall clock time
  // -------------------------------------------------------
  os << "*** Wall clock time ***\n";
  os << HPCRUN_EVENT_WALLCLK_STR << "     wall clock time (1 millisecond period)\n";
  // os << HPCRUN_EVENT_FWALLCLK_STR << "    fast wall clock time (1 millisecond period)\n";
  os << SEPARATOR_MAJOR;

  // -------------------------------------------------------
  // PAPI events
  // -------------------------------------------------------
  os << "*** Available PAPI preset events ***\n";
  os << SEPARATOR_MINOR;
  if (listType == Args::LIST_SHORT) {
    os << "Name\t\tDescription\n";
  }
  else if (listType == Args::LIST_LONG) {
    os << "Name\t     Profilable\tDescription (Implementation Note)\n";
  }
  else {
    DIAG_Die(DIAG_Unimplemented);
  }
  os << SEPARATOR_MINOR;

  int i = PAPI_PRESET_MASK;
  int count = 0;
  do {
    PAPI_event_info_t info;
    if (dl_PAPI_query_event(i) != PAPI_OK) {
      continue;
    }
    if (dl_PAPI_get_event_info(i, &info) == PAPI_OK) {
      /* NOTE: Although clumsy, this test has official sanction. */
      const char* profilable = "Yes";
      if ((info.count > 1) && strcmp(info.derived, "DERIVED_CMPD") != 0) {
	profilable = "No";
      }
      
      if (listType == Args::LIST_SHORT) {
	os << info.symbol << "\t" << info.long_descr << "\n";
      } 
      else if (listType == Args::LIST_LONG) {
	os << info.symbol << "\t" << profilable << "\t" << info.long_descr
	   << " (" << info.note << ")\n";
      }
      else {
	DIAG_Die(DIAG_Unimplemented);
      }
      count++;
    }
  } while (dl_PAPI_enum_event(&i, PAPI_PRESET_ENUM_AVAIL) == PAPI_OK);
  os << "Total PAPI events reported: " << count << "\n";
  os << SEPARATOR_MAJOR;

  // -------------------------------------------------------
  // Native events
  // -------------------------------------------------------

  /* PAPI does not always correctly return a vendor id */
  os << "*** Available native events ***\n";
  os << SEPARATOR_MINOR;
  if (listType == Args::LIST_SHORT) { 
    os << "Name\t\t\t\tDescription\n";
  }
  else if (listType == Args::LIST_LONG) { 
    os << "Name\t\t\t\tDescription\n";
  }
  else {
    DIAG_Die(DIAG_Unimplemented);
  }
  os << SEPARATOR_MINOR;
  os << std::left << setfill(' ');
  
  i = PAPI_NATIVE_MASK;
  count = 0;
  do {
    PAPI_event_info_t info;
    if (dl_PAPI_get_event_info(i, &info) == PAPI_OK) {
      const char* desc = (info.long_descr) ? info.long_descr : "";
      
      if (listType == Args::LIST_SHORT || listType == Args::LIST_LONG) {
	if (strncmp(info.symbol, desc, strlen(info.symbol)) == 0) {
	  desc += strlen(info.symbol);
	}
	os << setw(31) << info.symbol << " " << desc << "\n";
      }
      else {
	DIAG_Die(DIAG_Unimplemented);
      }
      count++;
    }
  } while (dl_PAPI_enum_event(&i, PAPI_ENUM_EVENTS) == PAPI_OK);
  
  os << "Total native events reported: " << count << "\n";
  os << SEPARATOR_MAJOR;
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
  return prepend_to_ld_lib_path(HPC_PAPI "/lib");
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

