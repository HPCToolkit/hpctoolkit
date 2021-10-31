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
// Copyright ((c)) 2002-2021, Rice University
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

// This file is the main program for hpcstruct.  This side just
// handles the argument list.  The real work is in makeStructure() in
// lib/banal/Struct.cpp.
//
// This side now handles the case of a measurements directory with
// GPU binaries.  We don't analyze anything here, just setup a Makefile and
// launch the work for each GPU binary.

//****************************** Include Files ******************************

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
#include <unistd.h>

#include <include/gpu-binary.h>
#include <include/hpctoolkit-config.h>

#include "Args.hpp"

#include <lib/banal/Struct.hpp>
#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/cpuset_hwthreads.h>
#include <lib/support/diagnostics.h>
#include <lib/support/realpath.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/RealPathMgr.hpp>

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

#define PRINT_ERROR(mesg)  \
  DIAG_EMsg(mesg << "\nTry 'hpcstruct --help' for more information.")

using namespace std;

static int
realmain(int argc, char* argv[]);


//***************************** Analyze Cubins ******************************

static const char* analysis_makefile =
#include "pmake.h"
;

//
// For a measurements directory, write a Makefile and launch hpcstruct
// to analyze CPU and GPU binaries associated with the measurements
//
static void
doMeasurementsDir(string measurements_dir, BAnal::Struct::Options & opts)
{
  measurements_dir = RealPath(measurements_dir.c_str());

  //
  // Check that 'measurements_dir' has at least one .gpubin file.
  //

  string gpubin_dir = measurements_dir + "/" GPU_BINARY_DIRECTORY;
  struct dirent *ent;
  bool has_gpubin = false;

  DIR *dir = opendir(gpubin_dir.c_str());
  if (dir != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      string file_name(ent->d_name);
      if (file_name.find(GPU_BINARY_SUFFIX) != string::npos) {
        has_gpubin = true;
        break;
      }
    }
    closedir(dir);
  }

  //
  // Put hpctoolkit on PATH
  //
  char *path = getenv("PATH");
  string new_path = string(HPCTOOLKIT_INSTALL_PREFIX) + "/bin" + ":" + path;

  if (has_gpubin) {
    // Put cuda (nvdisasm) on path.
    new_path = new_path +":" + CUDA_INSTALL_PREFIX + "/bin";
  }

  setenv("PATH", new_path.c_str(), 1);

  string hpcproftt_path = string(HPCTOOLKIT_INSTALL_PREFIX) 
    + "/libexec/hpctoolkit/hpcproftt";

  string struct_path = string(HPCTOOLKIT_INSTALL_PREFIX) 
    + "/bin/hpcstruct";


  //
  // Write Makefile and launch analysis.
  //
  string structs_dir = measurements_dir + "/structs";
  mkdir(structs_dir.c_str(), 0755);

  string makefile_name = structs_dir + "/Makefile";
  fstream makefile;
  makefile.open(makefile_name, fstream::out | fstream::trunc);

  if (! makefile.is_open()) {
    DIAG_EMsg("Unable to write file: " << makefile_name);
    exit(1);
  }

  unsigned int pthreads;
  unsigned int jobs;

  if (opts.jobs == 0) { // not specified
    unsigned int hwthreads = cpuset_hwthreads();
    jobs = std::max(hwthreads/2, 1U);
    pthreads = std::min(jobs, 16U);
  } else {
    jobs = opts.jobs;
    pthreads = jobs;
  }
    
  string gpucfg = opts.compute_gpu_cfg ? "yes" : "no";

  makefile << "MEAS_DIR =  "    << measurements_dir << "\n"
	   << "GPUBIN_CFG = "   << gpucfg << "\n"
	   << "CPU_ANALYZE = "  << opts.analyze_cpu_binaries << "\n"
	   << "GPU_ANALYZE = "  << opts.analyze_gpu_binaries << "\n"
	   << "PAR_SIZE = "     << opts.parallel_analysis_threshold << "\n"
	   << "JOBS = "         << jobs << "\n"
	   << "PTHREADS = "     << pthreads << "\n"
	   << "PROFTT = "       << hpcproftt_path << "\n"
	   << "STRUCT= "        << struct_path << "\n"
	   << analysis_makefile << endl;
  makefile.close();

  string make_cmd = string("make -C ") + structs_dir + " -k --silent "
      + " --no-print-directory all";

  if (system(make_cmd.c_str()) != 0) {
    DIAG_EMsg("Make hpcstruct files for measurement directory failed.");
    exit(1);
  }
}

//****************************** Main Program *******************************

int
main(int argc, char* argv[])
{
  try {
    return realmain(argc, argv);
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
realmain(int argc, char* argv[])
{
  Args args(argc, argv);

  RealPathMgr::singleton().searchPaths(args.searchPathStr);
  RealPathMgr::singleton().realpath(args.in_filenm);

  // ------------------------------------------------------------
  // Parameters on how to run hpcstruct
  // ------------------------------------------------------------

  unsigned int jobs_struct;
  unsigned int jobs_parse;
  unsigned int jobs_symtab;

#ifdef ENABLE_OPENMP
  //
  // Translate the args jobs to the struct opts jobs.  The specific
  // overrides the general: for example, -j sets all three phases,
  // --jobs-parse overrides just that one phase.
  //

  jobs_struct = (args.jobs_struct >= 1) ? args.jobs_struct : args.jobs;
  jobs_parse  = (args.jobs_parse >= 1)  ? args.jobs_parse  : args.jobs;

#ifndef ENABLE_OPENMP_SYMTAB
  jobs_symtab = 1;
#else
  jobs_symtab = (args.jobs_symtab >= 1) ? args.jobs_symtab : args.jobs;
#endif

  omp_set_num_threads(1);

#else
  jobs_struct = 1;
  jobs_parse = 1;
  jobs_symtab = 1;
#endif

  BAnal::Struct::Options opts;

  opts.set(args.jobs, jobs_struct, jobs_parse, jobs_symtab, args.show_time,
	   args.analyze_cpu_binaries, args.analyze_gpu_binaries,
	   args.compute_gpu_cfg, args.parallel_analysis_threshold);

  // ------------------------------------------------------------
  // If in_filenm is a directory, then analyze separately
  // ------------------------------------------------------------
  struct stat sb;

  if (stat(args.in_filenm.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
    if (!args.out_filenm.empty()) {
      DIAG_EMsg("Outfile file may not be specified when analyziing a measurement directory.");
      exit(1);
    }
    doMeasurementsDir(args.in_filenm, opts);
    return 0;
  }

  // ------------------------------------------------------------
  // Single application binary
  // ------------------------------------------------------------

  const char* osnm = (args.out_filenm == "-") ? NULL : args.out_filenm.c_str();
  std::ostream* outFile = IOUtil::OpenOStream(osnm);
  char* outBuf = new char[HPCIO_RWBufferSz];

  std::streambuf* os_buf = outFile->rdbuf();
  os_buf->pubsetbuf(outBuf, HPCIO_RWBufferSz);

  std::string gapsName = "";
  std::ostream* gapsFile = NULL;
  char* gapsBuf = NULL;
  std::streambuf* gaps_rdbuf = NULL;

  if (args.show_gaps) {
    // fixme: may want to add --gaps-name option
    if (args.out_filenm == "-") {
      DIAG_EMsg("Cannot make gaps file when hpcstruct file is stdout.");
      exit(1);
    }

    gapsName = RealPath(osnm) + std::string(".gaps");
    gapsFile = IOUtil::OpenOStream(gapsName.c_str());
    gapsBuf = new char[HPCIO_RWBufferSz];
    gaps_rdbuf = gapsFile->rdbuf();
    gaps_rdbuf->pubsetbuf(gapsBuf, HPCIO_RWBufferSz);
  }

  try {
    BAnal::Struct::makeStructure(args.in_filenm, outFile, gapsFile, gapsName,
			         args.searchPathStr, opts);
  } catch (int n) {
    IOUtil::CloseStream(outFile);
    if (osnm) {
      unlink(osnm);
    }
    if (gapsFile) {
      IOUtil::CloseStream(gapsFile);
      unlink(gapsName.c_str());
    }
    exit(n);
  }

  IOUtil::CloseStream(outFile);
  delete[] outBuf;

  if (gapsFile != NULL) {
    IOUtil::CloseStream(gapsFile);
    delete[] gapsBuf;
  }

  return (0);
}
