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
// Copyright ((c)) 2002-2022, Rice University
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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <iostream>
using std::cerr;
using std::endl;

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <streambuf>
#include <new>
#include <vector>

#include <string.h>
#include <unistd.h>

#include <string.h>

#include <include/gpu-binary.h>
#include "hpcstruct.hpp"
#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/cpuset_hwthreads.h>
#include <lib/support/realpath.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/IOUtil.hpp>
#include <include/hpctoolkit-config.h>

#include <lib/support/RealPathMgr.hpp>

using namespace std;

// Function prototypes
static void create_structs_directory ( string &structs_dir);
static void open_makefile ( string &makefile_name, fstream &makefile);
static void verify_measurements_directory(string &measurements_dir);

// Contents of Makefile to be generated for the directory
static const char* analysis_makefile =
#include "pmake.h"
;


//
// For a measurements directory, write a Makefile and launch hpcstruct
// to analyze CPU and GPU binaries associated with the measurements
//


void
doMeasurementsDir
(
 Args &args,
 struct stat *sb
)
{
  std::string measurements_dir = RealPath(args.in_filenm.c_str());

  verify_measurements_directory(measurements_dir);

  //
  // Put hpctoolkit on PATH
  //
  char *path = getenv("PATH");
  string new_path = string(HPCTOOLKIT_INSTALL_PREFIX) + "/bin" + ":" + path;

  // Assume that nvdisasm is on the user's path; if not BAnal will complain

  setenv("PATH", new_path.c_str(), 1);

  // Construct full paths for hpcproftt and hpcstruct
  //
  string hpcproftt_path = string(HPCTOOLKIT_INSTALL_PREFIX)
    + "/libexec/hpctoolkit/hpcproftt";

  string hpcstruct_path = string(HPCTOOLKIT_INSTALL_PREFIX)
    + "/bin/hpcstruct";

  //
  // Write Makefile and launch analysis.
  //
  string structs_dir = measurements_dir + "/structs";
  create_structs_directory(structs_dir);

  string makefile_name = structs_dir + "/Makefile";

  fstream makefile;
  open_makefile(makefile_name, makefile);

  // Figure out how many threads and jobs are to be used
  unsigned int pthreads;
  unsigned int jobs;

  if (args.jobs == 0) { // not specified
    // Set the default: half the number of CPUs
    unsigned int hwthreads = cpuset_hwthreads();
    jobs = std::max(hwthreads/2, 1U);
    pthreads = std::min(jobs, 16U);
  } else {
    jobs = args.jobs;
    pthreads = jobs;
  }

  string cache_path;

  // Initialize the structure cache, if specified
  //
  if (!args.nocache) {
    char *cpath = NULL;

    // Find or create the actual cache directory
    try {
      cpath = setup_cache_dir(args.cache_directory.c_str(), &args);
    } catch(const Diagnostics::FatalException &e) {
      exit(1);
    };

    // How can cpath still be NULL???
    if (cpath) {
      cache_path = cpath;
      //
      // check that the cache is writable
      //
      if (!hpcstruct_cache_writable(cpath)) {
	DIAG_EMsg("hpcstruct cache directory " << cpath << " not writable");
	exit(1);
      }
    }
  }

  string gpucfg = args.compute_gpu_cfg ? "yes" : "no";

  // two threads per small binary unless concurrency is 1
  int small_jobs = (jobs >= 2) ? jobs / 2 : jobs;
  int small_threads = (jobs == 1) ? 1 : 2;

  // Write the header with definitions to the makefile
  makefile << "MEAS_DIR =  "    << measurements_dir << "\n"
 	   << "GPUBIN_CFG = "   << gpucfg << "\n"
	   << "CPU_ANALYZE = "  << args.analyze_cpu_binaries << "\n"
	   << "GPU_ANALYZE = "  << args.analyze_gpu_binaries << "\n"
	   << "PAR_SIZE = "     << args.parallel_analysis_threshold << "\n"
	   << "JOBS = "         << jobs << "\n"
	   << "SJOBS = "        << small_jobs << "\n"
	   << "STHREADS = "     << small_threads << "\n"
	   << "LJOBS = "        << jobs/pthreads << "\n"
	   << "LTHREADS = "     << pthreads << "\n"
	   << "PROFTT = "       << hpcproftt_path << "\n"
	   << "STRUCT= "        << hpcstruct_path << "\n";

  if (!cache_path.empty()) {
    makefile << "CACHE= "       << cache_path << "\n";
  } else {
    makefile << "CACHE= " << "\n";
  }

  makefile << analysis_makefile << endl;

  makefile.close();

  // Construct the make command to invoke it
  string make_cmd = string("make -C ") + structs_dir + " -k --silent "
      + " --no-print-directory all";

  // Describe the parallelism and concurrency used
  cout << "INFO: Using a pool of " << jobs << " threads to analyze binaries in a measurement directory" << endl;
  cout << "INFO: Analyzing each large binary of >= " << args.parallel_analysis_threshold << " bytes in parallel using " << pthreads
       << " threads" << endl;
  cout << "INFO: Analyzing each small binary using " << small_threads <<
    " thread" << ((small_threads > 1) ? "s" : "") <<  "\n" << endl;

  // Run the make command
  //
  if (system(make_cmd.c_str()) != 0) {
    DIAG_EMsg("Running make to generate hpcstruct files for measurement directory failed.");
    exit(1);
  }

  // Write a blank line
  std::cout << std::endl;

  // and exit
  exit(0);
}

// Routine to verify that given measurements directory
// (1) is readable
// (2) contains measurement files
//
// It is invoked for a measurement-directory invocation, and for a single-binary
//    invocation with a "-M" argument
// It is never invoked for a single binary without the "-M" argument
//

void
verify_measurements_directory
(
  string &measurements_dir
)
{
  DIR *dir = opendir(measurements_dir.c_str());

  if (dir != NULL) {
    struct dirent *ent;
    bool has_hpcrun = false;
    while ((ent = readdir(dir)) != NULL) {
      string file_name(ent->d_name);
      if (file_name.find(".hpcrun") != string::npos) {
        has_hpcrun = true;
	break;
      }
    }
    closedir(dir);
    if (!has_hpcrun) {
      DIAG_EMsg("Measurements directory " << measurements_dir <<
                " does not contain any .hpcrun measurement files ");
      exit(1);
    }
  } else {
    DIAG_EMsg("Unable to open measurements directory " << measurements_dir
              << ": " << strerror(errno));
    exit(1);
  }
}


#if 0
// check if measurements directory contains a GPU binary
static bool
check_gpubin
(
  string &measurements_dir
)
{
  bool has_gpubin = false;

  string gpubin_dir = measurements_dir + "/" GPU_BINARY_DIRECTORY;

  DIR *dir = opendir(gpubin_dir.c_str());
  if (dir != NULL) {
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
      string file_name(ent->d_name);
      if (file_name.find(GPU_BINARY_SUFFIX) != string::npos) {
        has_gpubin = true;
        break;
      }
    }
    closedir(dir);
  }

  return has_gpubin;
}
#endif

// Invoked for a measurements-directory handling to ensure
//  that it has a subdirectory for structure files
static void
create_structs_directory
(
  string &structs_dir
)
{
  int result_dir = mkdir(structs_dir.c_str(), 0755);

  if (result_dir != 0 && errno != EEXIST) {
    DIAG_EMsg("Unable to create results directory " << structs_dir
              << ": " << strerror(errno));
    exit(1);
  }
}

// Open the Makefile to create it.
static void
open_makefile
(
  string &makefile_name,
  fstream &makefile
)
{
  makefile.open(makefile_name, fstream::out | fstream::trunc);

  if (! makefile.is_open()) {
    DIAG_EMsg("Unable to write file: " << makefile_name);
    exit(1);
  }
}
