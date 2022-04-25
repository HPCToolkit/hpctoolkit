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
    auto tail = line.find(GPU_BINARY_SUFFIX ".");
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
 const std::string& cache,
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

  bool gpu_binary = isGpuBinary(args);

  string binary_abspath = RealPath(args.in_filenm.c_str());
#if 0
  cerr << "DEBUG singleApplicationBinary  -- binary_abspath = " << binary_abspath.c_str() << endl;
  cerr << "DEBUG singleApplicationBinary  -- cache setup started" << args.cache_directory.c_str() << endl;
#endif

  string cache_path_directory;
  string cache_flat_entry;

  // Make sure the file is readable
  if ( access(binary_abspath.c_str(), R_OK) != 0 ) {
    cerr << "ERROR -- input file " << args.in_filenm.c_str() << " is not readable" << endl;
    exit(1);
  }

  if (!cache.empty()) {
    // Compute a hash of the binary at the input absolute path
    char *hash = hpcstruct_cache_hash(binary_abspath.c_str());

    //  If it's a gpu binary and the user requested the cfg, set a suffix
    string suffix = "";
    if (gpu_binary && args.compute_gpu_cfg) {
      suffix = "+gpucfg";
    }

    // Compute the path in the cache for that binary
    cache_path_directory = hpcstruct_cache_path_directory(cache.c_str(),
         binary_abspath.c_str(), hash, suffix.c_str() );

    // Compute the path for the entry in the FLAT subdirectory of the cache
    cache_flat_entry = hpcstruct_cache_flat_entry(cache.c_str(), hash );

    string cache_path_link = hpcstruct_cache_path_link(binary_abspath.c_str(), hash);
    symlink(cache_path_link.c_str(), cache_flat_entry.c_str());
#if 0
    cerr << "DEBUG symlinked " << cache_path_link.c_str() << " to " << cache_flat_entry.c_str() << endl;
    cerr << "DEBUG state now = "  << args.cache_stat << endl;
#endif

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

  // Initialize the output stream for the hpcstruct file
  //  Cacheing is embedded in this call
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
    if(args.cacheonly) {
      error = 1;
    } else {
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

  if (error) exit(error);
}
