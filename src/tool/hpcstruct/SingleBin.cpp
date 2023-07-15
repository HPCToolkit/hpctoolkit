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
// Copyright ((c)) 2002-2023, Rice University
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

#include "hpcstruct.hpp"
#include <lib/banal/Struct.hpp>
#include <lib/prof-lean/gpu-binary-naming.h>
#include <lib/prof-lean/hpcio.h>
#include <lib/support/realpath.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/IOUtil.hpp>
#include <include/hpctoolkit-config.h>


#include "fileout.hpp"

#include <lib/support/RealPathMgr.hpp>

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

using namespace std;


//=====================================================================================
//***************** Function for processing a Single Binary ***************************

static void replace_lmname(std::string& line, const std::string& newname) {
  if(line.find("<LM") == std::string::npos) return;

  // Find the n=" and the corresponding "
  auto first = line.find("n=\"");
  if(first == std::string::npos) return;
  first += 3;
  auto last = line.find("\"", first);
  if(last == std::string::npos) return;

  // If the old filename ends in .gpubin.<hash>, don't remove the hash since
  // it's required for proper operation in Intel cases.
  {
    auto tail = line.find(".gpubin.");
    if(tail != std::string::npos) {
      tail += 7;  // ie. the . after gpubin

      // Check that there's a hash and it's all hex until the end
      if(tail < last) {
        bool all_hex = true;
        for(auto i = tail+1; i < last; i++) {
          if(!(('0' <= line[i] && line[i] <= '9')
               || ('a' <= line[i] && line[i] <= 'f'))) {
            all_hex = false;
            break;
          }
        }

        if(all_hex) {
          // Pull last back to the tail
          last = tail;
        }
      }
    }
  }

  line.replace(first, last-first, newname);
}

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
	   args.compute_gpu_cfg, args.parallel_analysis_threshold,
	   args.pretty_print_output);
  if (args.show_gaps && args.out_filenm == "-") {
    DIAG_EMsg("Cannot make gaps file when hpcstruct file is stdout.");
    exit(1);
  }

  // Now that we've set the BAnal options, we can force jobs to be non-zero
  //
  if (args.jobs == 0 ){
    args.jobs = 1;
  }

  bool gpu_binary = args.in_filenm.find(GPU_BINARY_SUFFIX) != string::npos;

  string binary_abspath = RealPath(args.in_filenm.c_str());
#if 0
  cerr << "DEBUG singleApplicationBinary  -- binary_abspath = " << binary_abspath.c_str() << endl;
  cerr << "DEBUG singleApplicationBinary  -- cache setup started" << args.cache_directory.c_str() << endl;
#endif

  string cache_path_directory;
  string cache_flat_entry;
  string cache_directory;

  // Make sure the file is readable
  if ( access(binary_abspath.c_str(), R_OK) != 0 ) {
    cerr << "ERROR -- input file " << args.in_filenm.c_str() << " is not readable" << endl;
    if ( args.is_from_makefile == true ) {
      cerr << "CACHESTAT (Input file is not readable) " << endl;
    }
    exit(1);
  }

  if (args.nocache) {
    // the user intentionally turned off the cache
    args.cache_stat = CACHE_DISABLED;

  }  else {
    //  using the cache; first open/create the directory
    //
    char *path = setup_cache_dir(args.cache_directory.c_str(), &args);

    if (path) {
      // We have either opened or created the cache directory; path is its absolute path.
      cache_directory = path;
      args.cache_stat = CACHE_ENABLED;
#if 0
      cerr << "DEBUG singleApplicationBinary  -- setup_cache_dir path = " << path
        <<" setting state to CACHE_ENABLED = " << CACHE_ENABLED << endl;
#endif

      // Compute a hash of the binary at the input absolute path
      char *hash = hpcstruct_cache_hash(binary_abspath.c_str());

      //  If it's a gpu binary and the user requested the cfg, set a suffix
      string suffix = "";
      if (gpu_binary && args.compute_gpu_cfg) {
        suffix = "+gpucfg";
      }

      // Compute the path in the cache for that binary
      cache_path_directory = hpcstruct_cache_path_directory(cache_directory.c_str(),
           binary_abspath.c_str(), hash, suffix.c_str() );

      // Compute the path for the entry in the FLAT subdirectory of the cache
      cache_flat_entry = hpcstruct_cache_flat_entry(cache_directory.c_str(), hash );

      string cache_path_link = hpcstruct_cache_path_link(binary_abspath.c_str(), hash);
      symlink(cache_path_link.c_str(), cache_flat_entry.c_str());
#if 0
      cerr << "DEBUG symlinked " << cache_path_link.c_str() << " to " << cache_flat_entry.c_str() << endl;
      cerr << "DEBUG state now = "  << args.cache_stat << endl;
#endif

    } else {
      //
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

    // Direct invocation, write a starting message
    if (gpu_binary == true ) {
      std::cout << " begin " << mode.c_str() <<" [gpucfg=" << (args.compute_gpu_cfg == true ? "yes" : "no")
        << "] analysis of " "GPU binary "
        << args.in_filenm.c_str() << " (size = " << sb->st_size
	<< ", threads = " << args.jobs << ")" << std::endl;
    } else {
      std::cout << " begin " << mode.c_str() << " analysis of CPU binary "
        << args.in_filenm.c_str() << " (size = " << sb->st_size
	<< ", threads = " << args.jobs << ")" << std::endl;
    }
  }

  // Initialize the output stream for the hpcstruct file
  //  Caching is embedded in this call
  //
  hpcstruct.init(cache_path_directory.c_str(), cache_flat_entry.c_str(),
		 structure_name.c_str(), hpcstruct_path.c_str());

#if 0
  std::cerr << "DEBUG hpcinit, path_dir " << cache_path_directory.c_str() << std::endl
	<< "DEBUG   flat entry " << cache_flat_entry.c_str() << "  name " << structure_name.c_str()
	<< "  path " <<  hpcstruct_path.c_str() << std::endl;
#endif

  // See if the user requested show_gaps
  //
  if (args.show_gaps) {
    // yes, initialize for that output
    //
    std::string gaps_path =
      std::string(hpcstruct_path) + std::string(".gaps");
    gaps.init(cache_path_directory.c_str(), cache_flat_entry.c_str(), "gaps",
	      gaps_path.c_str());
  }

#if 0
  cerr << "DEBUG singleApplicationBinary  -- hpcstruct.init done -- hpcstruct_path = " << hpcstruct_path.c_str() << endl;
#endif

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

  // If we're pulling from the cache, ensure the module path in the new file is correct
  if(!error) {
    auto cache = hpcstruct.cached();
    if(!cache.empty()) {
      std::ifstream infs(cache);
      std::ofstream outfs(hpcstruct_path);

      // Slurp, adjust and output lines one at a time
      for(std::string line; std::getline(infs, line); ) {
        replace_lmname(line, args.in_filenm);
        outfs << line << "\n";
      }
    }
  }

  hpcstruct.finalize(error);
  gaps.finalize(error);

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
      cache_stat_str = " (Cache enabled XXX )";  // Intermediate state, should never be seen
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
      cache_stat_str =  " (Added to cache + XXX)";  // Should never happen
      break;
    case CACHE_ENTRY_REPLACED:
      cache_stat_str =  " (Replaced in cache)";
      break;
    case CACHE_ENTRY_REMOVED:
      cache_stat_str =  " (Removed from cache XXX)";  //Intermediate state, should never be seen
      break;
    default:
      cache_stat_str = " (?? unknown XXX)";
      break;
  }
  //
  if ( args.is_from_makefile != true ) {
    // If this invocation was not from a Makefile, write a message to the user
    //
    if (gpu_binary == true ) {
      std::cout << "   end " << mode.c_str() << " [gpucfg=" << (args.compute_gpu_cfg == true ? "yes" : "no")
        << "] analysis of " "GPU binary "
        << args.in_filenm.c_str() << cache_stat_str << std::endl << std::endl ;
    } else {
      std::cout << "   end " << mode.c_str() << " analysis of CPU binary "
        << args.in_filenm.c_str() << cache_stat_str << std::endl << std::endl ;
    }

  } else {
    // if from a Makefile, write the key line to be read by script
    //
    cerr << "CACHESTAT " << cache_stat_str << std::endl;
  }

  if (error) exit(error);
}
