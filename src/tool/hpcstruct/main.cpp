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
// Copyright ((c)) 2002-2024, Rice University
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

// This file is the main program for hpcstruct
//
// It handles its argument list, and decides what to do.
//
// It then manages the structure cache initialization.
//  If a cache argument is not supplied, it checks for the
//  environment variable for a cache name.  If none is found,
//  an ADVICE message is written to the user, advising cache use.
//  If a cache directory is specified and the directory exists,
//  it is checked for read and write access.  If it does not exist,
//  it is created.
//
// If the argument is a measurements directory, which may contain both
//  CPU and GPU binaries, it writes a Makefile with commands for
//  launching subsidiary hpcstruct commands, one for each binary.
//  It then runs make with Makefile, which invokes those commands.
//
// If the argument is a single binary, hpstruct may have been invoked
//  directly by a user or invoked for a binary in a measurements directory.
//  The latter case is distinguished by a "-M measurements-dir" argument,
//  which should never be used directly by a user.
//
//  In either case, if a cache is specified, and the binary's structure
//  file is found, it is copied to the output structure file.
//  If no cache is specified, or the binary is not found in the cache,
//  it sets up the data structures for the real processing, which is
//  done in makeStructure() in lib/banal/Struct.cpp.
//  After generating the structure file, if a cache is specified, the
//  structure file is entered into the cache.

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

#include <string.h>
#include <unistd.h>

#include "hpctoolkit-config.h"

#include "Args.hpp"
#include "Structure-Cache.hpp"

#include "../../lib/banal/Struct.hpp"
#include "../../lib/prof-lean/cpuset_hwthreads.h"
#include "../../lib/prof-lean/gpu-binary-naming.h"
#include "../../lib/prof-lean/hpcio.h"
#include "../../lib/support/diagnostics.h"
#include "../../lib/support/realpath.h"
#include "../../lib/support/FileUtil.hpp"
#include "../../lib/support/IOUtil.hpp"
#include "../../lib/support/RealPathMgr.hpp"

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

#include "hpcstruct.hpp"

#define PRINT_ERROR(mesg)  \
  DIAG_EMsg(mesg << "\nTry 'hpcstruct --help' for more information.")

using namespace std;

Args *global_args;

// Internal functions
static int realmain(int argc, char* argv[]);


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


//=====================================================================================

static int
realmain(int argc, char* argv[])
{
  // Process the arguments
  //
  Args args(argc, argv);
  global_args = &args;

  RealPathMgr::singleton().searchPaths(args.searchPathStr);
  RealPathMgr::singleton().realpath(args.in_filenm);

  // ------------------------------------------------------------
  // If in_filenm is a directory, then analyze entire directory
  // ------------------------------------------------------------
  struct stat sb;

  if ( stat(args.in_filenm.c_str(), &sb) != 0 ) {
    cerr << "ERROR: input argument " << args.in_filenm.c_str() << " is not a file or HPCToolkit measurement directory." << endl;
    exit(1);
  }

  // See if the argument is a directory, or a single binary
  //
  if ( S_ISDIR(sb.st_mode) ) {
    // it's a directory
    // Make sure output file was not specified
    //
    if (!args.out_filenm.empty()) {
      DIAG_EMsg("Outfile file may not be specified when analyzing an HPCToolkit measurement directory.");
      exit(1);
    }
    // Now process the measurements directory, passing it its stat result
    //
    doMeasurementsDir(args, &sb);

  } else {
    // Process a single binary, passing in its stat result
    //
    doSingleBinary(args, &sb );
  }
  return 0;
}
