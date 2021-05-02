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

static const char* gpubin_analysis_makefile =
#include "gpubin-analysis.h"
;

//
// For a measurements directory, write a Makefile and launch hpcstruct
// for each GPU binary (gpubin).
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
  bool found = false;

  DIR *dir = opendir(gpubin_dir.c_str());
  if (dir == NULL) {
    PRINT_ERROR("Unable to open measurements directory: " << gpubin_dir);
    exit(1);
  }

  while ((ent = readdir(dir)) != NULL) {
    string file_name(ent->d_name);
    if (file_name.find(GPU_BINARY_SUFFIX) != string::npos) {
      found = true;
      break;
    }
  }

  if (! found) {
    PRINT_ERROR("Measurements directory does not contain gpubin: " << gpubin_dir);
    exit(1);
  }
  closedir(dir);

  //
  // Put hpctoolkit and cuda (nvdisasm) on path.
  //
  char *path = getenv("PATH");
  string new_path = string(HPCTOOLKIT_INSTALL_PREFIX) + "/bin/"
    + ":" + path + ":" + CUDA_INSTALL_PREFIX + "/bin/";

  setenv("PATH", new_path.c_str(), 1);

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

  string gpucfg = opts.compute_gpu_cfg ? "yes" : "no";
  string du_graph = opts.du_graph ? "yes" : "no";

  makefile << "GPUBIN_DIR =  " << gpubin_dir << "\n"
	   << "STRUCTS_DIR = " << structs_dir << "\n"
	   << "GPUBIN_CFG = " << gpucfg << "\n"
	   << "DU_GRAPH = " << du_graph << "\n"
	   << "GPU_SIZE = " << opts.gpu_size << "\n"
	   << "JOBS = " << opts.jobs << "\n\n"
	   << gpubin_analysis_makefile << endl;
  makefile.close();

  string make_cmd = string("make -C ") + structs_dir + " -k --silent "
      + " --no-print-directory all";

  if (system(make_cmd.c_str()) != 0) {
    DIAG_EMsg("Make hpcstruct files for GPU binaries failed.");
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
  BAnal::Struct::Options opts;

  RealPathMgr::singleton().searchPaths(args.searchPathStr);
  RealPathMgr::singleton().realpath(args.in_filenm);

  // ------------------------------------------------------------
  // Parameters on how to run hpcstruct
  // ------------------------------------------------------------

#ifdef ENABLE_OPENMP
  //
  // Translate the args jobs to the struct opts jobs.  The specific
  // overrides the general: for example, -j sets all three phases,
  // --jobs-parse overrides just that one phase.
  //
  int jobs = (args.jobs >= 1) ? args.jobs : 1;

  opts.jobs = jobs;
  opts.jobs_struct = jobs;
  opts.jobs_parse = jobs;
  opts.jobs_symtab = jobs;

  if (args.jobs_struct >= 1) {
    opts.jobs_struct = args.jobs_struct;
  }
  if (args.jobs_parse >= 1) {
    opts.jobs_parse = args.jobs_parse;
  }
  if (args.jobs_symtab >= 1) {
    opts.jobs_symtab = args.jobs_symtab;
  }

#ifndef ENABLE_OPENMP_SYMTAB
  opts.jobs_symtab = 1;
#endif

  omp_set_num_threads(1);

#else
  opts.jobs = 1;
  opts.jobs_struct = 1;
  opts.jobs_parse = 1;
  opts.jobs_symtab = 1;
#endif

  opts.show_time = args.show_time;
  opts.compute_gpu_cfg = args.compute_gpu_cfg;
  opts.du_graph = args.du_graph;
  opts.gpu_size = args.gpu_size;

  // ------------------------------------------------------------
  // If in_filenm is a directory, then analyze separately
  // ------------------------------------------------------------
  struct stat sb;

  if (stat(args.in_filenm.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
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
