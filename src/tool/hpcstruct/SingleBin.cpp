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
#include <lib/banal/Struct.hpp>
#include <lib/prof-lean/hpcio.h>
#include <lib/support/realpath.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/IOUtil.hpp>

#include "fileout.hpp"

#include <lib/support/RealPathMgr.hpp>

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

using namespace std;


//=====================================================================================
//***************** Function for processing a Single Binary ***************************

void
doSingleBinary
(
 Args &args,
 struct stat *sb
)
{

  // ------------------------------------------------------------
  // Set Parameters on how to run the actual BAnal processing
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
  if (args.show_gaps && args.out_filenm == "-") {
    DIAG_EMsg("Cannot make gaps file when hpcstruct file is stdout.");
    exit(1);
  }

  // Now that we've set the BAnal options, we can force jobs to be non-zero
  if (args.jobs == 0 ){
    args.jobs = 1;
  }

  bool gpu_binary = args.in_filenm.find(GPU_BINARY_SUFFIX) != string::npos;

  string binary_abspath = RealPath(args.in_filenm.c_str());
#if 0
  cerr << "DEBUG singleApplicationBinary  -- binary_abspath = " << binary_abspath.c_str() << endl;
#endif

  string cache_path_directory;
  string cache_flat_directory;
  string cache_directory;

#if 0
  cerr << "DEBUG singleApplicationBinary  -- cache setup started" << args.cache_directory.c_str() << endl;
#endif

  if (args.nocache) {
    // the user intentionally turned off the cache
    args.cache_stat = CACHE_DISABLED;
  }  else {
    char *path = hpcstruct_cache_directory(args.cache_directory.c_str(), "", &args);
    // cerr << "DEBUG singleApplicationBinary  -- hpcstruct_cache_directory path = " << path << endl;

    if (path) {
      // We have either opened or created the cache directory
      cache_directory = path;

      char *hash = hpcstruct_cache_hash(binary_abspath.c_str());

      cache_path_directory = hpcstruct_cache_path_directory(path, binary_abspath.c_str(), hash);
      cache_flat_directory = hpcstruct_cache_flat_directory(path, hash);

      string cache_path_link = hpcstruct_cache_path_link(binary_abspath.c_str(), hash);
      symlink(cache_path_link.c_str(), cache_flat_directory.c_str());
      args.cache_stat = CACHE_ENABLED;
    } else {
      // the user did not specify a cache directory
      args.cache_stat = CACHE_NOT_NAMED;
    }

  }

#if 0
  cerr << "DEBUG singleApplicationBinary  -- cache setup done" << args.in_filenm.c_str() << endl;
#endif

  std::string hpcstruct_path =
    (args.out_filenm == "-") ? "" : RealPath(args.out_filenm.c_str());

  FileOutputStream gaps;
  FileOutputStream hpcstruct;

  string structure_name = "hpcstruct";

  if (gpu_binary && args.compute_gpu_cfg) structure_name += "+gpucfg";

  // set sequential or parallel mode
  std::string mode = "sequential";
  if (args.jobs > 1 ) {
    mode = "parallel";
  }

  // If this invocation was not from a Makefile, write a message to the user
  if ( args.is_from_makefile != true ) {
#if 0
    // Figure out a plausible maximum == 1/2 the number of HW threads in machine
    unsigned int maxthreads;
    unsigned int hwthreads = cpuset_hwthreads();
    if (args.jobs == 0) { // not specified
      maxthreads =  std::min(jobs, 16U);
    }
#endif

    // Write a starting message
    if (gpu_binary == true ) {
      std::cerr << " begin " << mode.c_str() <<" [gpucfg=" << (args.compute_gpu_cfg == true ? "yes" : "no")
        << "] analysis of " "GPU binary "
        << args.in_filenm.c_str() << " (size = " << sb->st_size
	<< ", threads = " << args.jobs << " )" << std::endl;
    } else {
      std::cerr << " begin " << mode.c_str() << " analysis of CPU binary "
        << args.in_filenm.c_str() << " (size = " << sb->st_size
	<< ", threads = " << args.jobs << " )" << std::endl;
    }
  }

  hpcstruct.init(cache_path_directory.c_str(), cache_flat_directory.c_str(),
		 structure_name.c_str(), hpcstruct_path.c_str());

#if 0
  cerr << "DEBUG singleApplicationBinary  -- hpcstruct.init done -- hpcstruct_path = " << hpcstruct_path.c_str() << endl;
#endif

  if (args.show_gaps) {
    std::string gaps_path =
      std::string(hpcstruct_path) + std::string(".gaps");
    gaps.init(cache_path_directory.c_str(), cache_flat_directory.c_str(), "gaps",
	      gaps_path.c_str());
  }

  hpcstruct.init(cache_path_directory.c_str(), cache_flat_directory.c_str(),
		 structure_name.c_str(), hpcstruct_path.c_str());

#if 0
  cerr << "DEBUG singleApplicationBinary  -- hpcstruct.init done -- hpcstruct_path = " << hpcstruct_path.c_str() << endl;
#endif

  if (args.show_gaps) {
    std::string gaps_path =
      std::string(hpcstruct_path) + std::string(".gaps");
    gaps.init(cache_path_directory.c_str(), cache_flat_directory.c_str(), "gaps",
	      gaps_path.c_str());
  }

  int error = 0;

  if (hpcstruct.needed() || gaps.needed()) {
    hpcstruct.open();
    gaps.open();
    try {
      BAnal::Struct::makeStructure(args.in_filenm, hpcstruct.getStream(),
				   gaps.getStream(), gaps.getName(),
				   args.searchPathStr, opts);
    } catch (int n) {
      error = n;
    }
  }

  hpcstruct.finalize(error);
  gaps.finalize(error);

  string checkname_cmd =  string(HPCTOOLKIT_INSTALL_PREFIX) + "/libexec/hpctoolkit/renamestruct.sh "
        + args.in_filenm.c_str() + " " + hpcstruct_path.c_str();

#if 0
  cerr << "DEBUG singleApplicationBinary : checkname_cmd  = " << checkname_cmd.c_str() << endl;
#endif

  // Ensure that the module path in the new .struct file is correct.
  //
  // Invoke the renamestuct shell script from the installation library
  //    Script is invoked with two arguments, $1 = path needed, $2 = structure-file
  //
  int retstat = system( checkname_cmd.c_str() );

  int renamestat = -1;

  if ( WIFEXITED(retstat) == true ) {
    // a normal exit
    renamestat = WEXITSTATUS (retstat);
    if (renamestat == 1 ) {
      //The names were really updated
      if ( args.cache_stat == CACHE_ENTRY_ADDED ) {
        args.cache_stat = CACHE_ENTRY_ADDED_RENAME;
      } else if ( args.cache_stat == CACHE_ENTRY_COPIED ) {
        args.cache_stat = CACHE_ENTRY_COPIED_RENAME;
      }


#if 0
  cerr << "DEBUG singleApplicationBinary : checkname_cmd replaced names in struct file " << endl;
#endif
      
    } else if (renamestat == 0 ) {
#if 0
  cerr << "DEBUG singleApplicationBinary : checkname_cmd did not need to replace names in struct file " << endl;
#endif
    } else {
#if 0
  cerr << "DEBUG singleApplicationBinary : checkname_cmd returned = " << renamestat << endl;
#endif
    }

  } else {
    // Not a normal exit -- a serious error
    DIAG_EMsg("Running renamepath to fix lines in structure files failed.");
    exit(1);
  }


  // Set cache usage status string
  const char * cache_stat_str;
  switch( args.cache_stat ) {
    case CACHE_NOT_NAMED:
      cache_stat_str = " (Cache not specified)";
      break;
    case CACHE_DISABLED:
      cache_stat_str = " (Cache disabled by user)";
      break;
    case CACHE_ENABLED:
      cache_stat_str = " (Cache enabled XXX )";
      break;
    case CACHE_ENTRY_COPIED:
      cache_stat_str =  " (Copied from cache)";
      break;
    case CACHE_ENTRY_COPIED_RENAME:
      cache_stat_str =  " (Copied from cache +)";
      break;
    case CACHE_ENTRY_ADDED:
      cache_stat_str =  " (Added to cache)";
      break;
    case CACHE_ENTRY_ADDED_RENAME:
      cache_stat_str =  " (Added to cache + XXX)";
      break;
    case CACHE_ENTRY_REPLACED:
      cache_stat_str =  " (Replaced in cache)";
      break;
    case CACHE_ENTRY_REMOVED:
      cache_stat_str =  " (Removed from cache)";
      break;
    default:
      cache_stat_str = " (?? unknown XXX)";
      break;
  }
  //
  if ( args.is_from_makefile != true ) {
    // If this invocation was not from a Makefile, write a message to the user
    if (gpu_binary == true ) {
      std::cerr << "   end " << mode.c_str() << " [gpucfg=" << (args.compute_gpu_cfg == true ? "yes" : "no")
        << "] analysis of " "GPU binary "
        << args.in_filenm.c_str() << cache_stat_str << std::endl << std::endl ;
    } else {
      std::cerr << "   end " << mode.c_str() << " analysis of CPU binary "
        << args.in_filenm.c_str() << cache_stat_str << std::endl << std::endl ;
    }

  } else {
   // if from a Makefile, write the key line to be read by script
    cerr << "CACHESTAT " << cache_stat_str << std::endl;
  }

  if (error) exit(error);
}
