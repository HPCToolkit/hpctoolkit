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
// Copyright ((c)) 2002-2020, Rice University
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
// cubins.  We don't analyze anything here, just setup a Makefile and
// launch the work for each cubin.

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

#include <include/hpctoolkit-config.h>

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

static const char* cubins_analysis_makefile =
#include "cubins-analysis.h"
;

//
// For a measurements directory, write a Makefile and launch hpcstruct
// for each .cubin file.
//
static void
doMeasurementsDir(string measurements_dir, BAnal::Struct::Options & opts)
{
  measurements_dir = RealPath(measurements_dir.c_str());

  //
  // Check that 'measurements_dir' has at least one .cubin file.
  //
#ifndef OPT_HAVE_CUDA
  PRINT_ERROR("Hpcstruct is not compiled with cuda.");
  exit(1);
#endif

  string cubins_dir = measurements_dir + "/cubins";
  struct dirent *ent;
  bool found = false;

  DIR *dir = opendir(cubins_dir.c_str());
  if (dir == NULL) {
    PRINT_ERROR("Unable to open measurements directory: " << cubins_dir);
    exit(1);
  }

  while ((ent = readdir(dir)) != NULL) {
    string file_name(ent->d_name);
    if (file_name.find(".cubin") != string::npos) {
      found = true;
      break;
    }
  }

  if (! found) {
    PRINT_ERROR("Measurements directory does not contain cubins: " << cubins_dir);
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

  makefile << "CUBINS_DIR =  " << cubins_dir << "\n"
	   << "STRUCTS_DIR = " << structs_dir << "\n"
	   << "CUBIN_CFG = " << gpucfg << "\n\n"
	   << cubins_analysis_makefile << endl;
  makefile.close();

  string make_cmd = string("make -C ") + structs_dir + " -k -j " + to_string(opts.jobs)
    + " --silent --no-print-directory analyze";

  if (system(make_cmd.c_str()) != 0) {
    DIAG_EMsg("Make hpcstruct files for cubins failed.");
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
  opts.jobs = args.jobs;
  opts.jobs_parse = args.jobs_parse;
  opts.jobs_symtab = args.jobs_symtab;

  // default is to run serial (for correctness), unless --jobs is
  // specified.
  if (opts.jobs < 1) {
    opts.jobs = 1;
  }
  if (opts.jobs_parse < 1) {
    opts.jobs_parse = opts.jobs;
  }

  // libdw is not yet thread-safe, so run symtab serial unless
  // specifically requested.
  if (opts.jobs_symtab < 1) {
    opts.jobs_symtab = 1;
  }
  omp_set_num_threads(1);
#else
  opts.jobs = 1;
  opts.jobs_parse = 1;
  opts.jobs_symtab = 1;
#endif

  opts.show_time = args.show_time;
  opts.compute_gpu_cfg = args.compute_gpu_cfg;

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
