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

#include "../common/lean/cpuset_hwthreads.h"
#include "../common/lean/gpu-binary-naming.h"
#include "../common/lean/hpcio.h"
#include "../common/realpath.h"
#include "../common/FileUtil.hpp"
#include "../common/IOUtil.hpp"

#include "../common/RealPathMgr.hpp"

using namespace std;

// Function prototypes
static void create_structs_directory ( string &structs_dir);
static void open_makefile ( string &makefile_name, fstream &makefile);
static void verify_measurements_directory(string &measurements_dir);

// Contents of Makefile to be generated for the directory
// editorconfig-checker-disable
static const char* analysis_makefile = R"EOF(
#*******************************************************************************
# a helper template makefile used by hpcstruct at runtime
#
# if hpcstruct is passed the name of a measurements directory that contains
# a gpubins subdirectory, this makefile will be used to orchestrate parallel
# analysis of all gpubins within the subdirectory.
#
# to simplify things at execution time, this makefile will be incorporated
# into hpcstruct as a string and written to a temporary file if it is needed.
# this avoids the need for hpcstruct to know how to find a copy of this
# makefile at runtime in an hpctoolkit installation.
#*******************************************************************************

#-------------------------------------------------------------------------------
# set up subdirectory paths
#-------------------------------------------------------------------------------
GPUBIN_DIR  = $(MEAS_DIR)/gpubins
STRUCTS_DIR = $(MEAS_DIR)/structs


#*******************************************************************************
# calculate the alternate kind of GPU CFG analysis
#*******************************************************************************
ifeq ($(GPUBIN_CFG),yes)
GPUBIN_CFG_ALT = no
else
GPUBIN_CFG_ALT = yes
endif


#*******************************************************************************
# use a measurement cache, if available
#*******************************************************************************
ifneq ($(CACHE)x,x)
CACHE_ARGS = -c $(CACHE)
else
CACHE_ARGS = --nocache
endif


#-------------------------------------------------------------------------------
# create $(MEAS_DIR)/all.lm: a list of all load modules involved in the execution
#-------------------------------------------------------------------------------
$(MEAS_DIR)/all.lm:
	@echo INFO: identifying load modules that need binary analysis
	@echo
	$(PROFLM) $(MEAS_DIR) > $(MEAS_DIR)/all.lm


#*******************************************************************************
# enable analysis of GPU binaries
#*******************************************************************************
ifeq ($(GPU_ANALYZE),1)
GPUBIN_USED_DIR  = $(MEAS_DIR)/gpubins-used

#-------------------------------------------------------------------------------
# create gpubins-used directory containing links to all GPU binaries used
#-------------------------------------------------------------------------------
$(GPUBIN_USED_DIR): $(MEAS_DIR)/all.lm
	-@mkdir $(GPUBIN_USED_DIR) >&- 2>&-
	-@cd $(GPUBIN_USED_DIR) >&- 2>&-; for i in `cat $(MEAS_DIR)/all.lm | grep gpubin`; do ln -s $(MEAS_DIR)/$$i; done >&- 2>&-

#-------------------------------------------------------------------------------
# $(GB): gpubin files
#-------------------------------------------------------------------------------
GB := $(wildcard $(GPUBIN_USED_DIR)/*)

#-------------------------------------------------------------------------------
# $(GS): hpcstruct files for gpubins
#-------------------------------------------------------------------------------
GS := $(patsubst $(GPUBIN_USED_DIR)/%,$(STRUCTS_DIR)/%-gpucfg-$(GPUBIN_CFG).hpcstruct,$(GB))

#-------------------------------------------------------------------------------
# $(GW): warning files that may be generated during structure analysis of gpubins
#-------------------------------------------------------------------------------
GW := $(patsubst %.hpcstruct,%.warnings,$(GS))

endif

#*******************************************************************************
#*******************************************************************************


#*******************************************************************************
# enable analysis of CPU binaries
#*******************************************************************************
ifeq ($(CPU_ANALYZE),1)
CPUBIN_DIR  = $(MEAS_DIR)/cpubins

#-------------------------------------------------------------------------------
# create cpubins directory containing links to all CPU binaries
#-------------------------------------------------------------------------------
$(CPUBIN_DIR): $(MEAS_DIR)/all.lm
	-@mkdir $(CPUBIN_DIR) >&- 2>&-
	-@cd $(CPUBIN_DIR) >&- 2>&-; for i in `cat $(MEAS_DIR)/all.lm | grep ^/`; do ln -s $$i; done >&- 2>&-
	-@cd $(CPUBIN_DIR) >&- 2>&-; for i in `cat $(MEAS_DIR)/all.lm | grep -v gpubin | grep -v ^/`; do ln -s $(MEAS_DIR)/$$i; done >&- 2>&-

#-------------------------------------------------------------------------------
# $(CB): cpubin files
#-------------------------------------------------------------------------------
CB := $(wildcard $(CPUBIN_DIR)/*)

#-------------------------------------------------------------------------------
# $(CS): hpcstruct files for cpubins
#-------------------------------------------------------------------------------
CS := $(patsubst $(CPUBIN_DIR)/%,$(STRUCTS_DIR)/%.hpcstruct,$(CB))

#-------------------------------------------------------------------------------
# $(CW): warning files that may be generated during structure analysis of cpubins
#-------------------------------------------------------------------------------
CW := $(patsubst %.hpcstruct,%.warnings,$(CS))

endif
#-------------------------------------------------------------------------------
# execute the sequence of commands for each target in a single shell
#-------------------------------------------------------------------------------
.ONESHELL:
.SILENT:

.DEFAULT_GOAL := all

#-------------------------------------------------------------------------------
# rule for analyzing a cpu binary
# 1. analyze a cpu binary file in $(CPUBIN)
# 2. produce a hpcstruct file in $(STRUCTS_DIR)
# 3. leave a warnings file in $(STRUCTS_DIR) if trouble arises
# 4. announce when analysis of a cpu binary begins and ends
#-------------------------------------------------------------------------------
$(STRUCTS_DIR)/%.hpcstruct: $(CPUBIN_DIR)/%
	@cpubin_name=`basename -s x $<`
	@input_name=`cat $(MEAS_DIR)/all.lm | grep $$cpubin_name`
	struct_name=$@
	warn_name=$(STRUCTS_DIR)/$$cpubin_name.warnings
	# @echo  DEBUG cpubin = $$cpu_bin_name
	nbytes=`du -b -L $< | tail -1 | awk '{ print $$1 }'`
	meas_dir=$(MEAS_DIR)
	# echo DEBUG meas_dir = $$meas_dir

	if test $$nbytes -gt $(CPAR_SIZE) ; then
		# inform the user the analysis is starting
		PARSTAT=concurrent
		if test $(THREADS) -gt 1 ; then
			echo  \ begin parallel analysis of CPU binary $$cpubin_name \(size = $$nbytes, threads = $(THREADS)\)
			PARSTAT=parallel
		else
			echo \ begin concurrent analysis of CPU binary $$cpubin_name \(size = $$nbytes, threads = 1\)
		fi

		#  invoke hpcstruct on the CPU binary in the measurements directory
		$(STRUCT) $(CACHE_ARGS) -j $(THREADS) -o $$struct_name -M $$meas_dir $$input_name > $$warn_name 2>&1 || { err=$$?; egrep 'ERROR|WARNING' $$warn_name >&2; }
		# echo DEBUG: hpcstruct for analysis of CPU binary $$cpubin_name returned

		# See if there is anything to worry about in the warnings file
		#  suppress any ADVICE, INFO, DEBUG, and CACHESTAT lines and any blank lines;
		#  it's an error if anything remains
		#
		errs=`sed 's/^$//INFO/g;' $$warn_name |grep -v DEBUG | grep -v CACHESTAT | grep -v INFO | grep -v ADVICE | wc -l`

		# echo DEBUG errs = XX $$errs XX
		if [ $${errs} -eq 1 ] ; then
			echo WARNING: incomplete analysis of $$cpubin_name';' see $$warn_name for details
		fi

		# extract the status relative to the cache
		CACHE_STAT=`grep CACHESTAT $$warn_name | sed 's/CACHESTAT// ' `
		# echo DEBUG CACHE_STAT = XX $$CACHE_STAT XX
		echo \ \ \ end  $$PARSTAT analysis of CPU binary $$cpubin_name $$CACHE_STAT

	fi

#-------------------------------------------------------------------------------
# rule  for analyzing a gpubin
# 1. analyze a gpubin file in $(GPUBIN_DIR)
# 2. produce a hpcstruct file in $(STRUCTS_DIR)
# 3. leave a warnings file in $(STRUCTS_DIR) if trouble arises
# 4. announce when analysis of a gpubin begins and ends
#-------------------------------------------------------------------------------
$(STRUCTS_DIR)/%-gpucfg-$(GPUBIN_CFG).hpcstruct: $(GPUBIN_DIR)/%
	@gpubin_name=`basename -s x $<`
	@input_name=`cat $(MEAS_DIR)/all.lm | grep $$gpubin_name`
	struct_name=$@
	rm -f $(STRUCTS_DIR)/$$gpubin_name-gpucfg-$(GPUBIN_CFG_ALT).hpcstruct
	rm -f $(STRUCTS_DIR)/$$gpubin_name-gpucfg-$(GPUBIN_CFG_ALT).warnings
	warn_name=$(STRUCTS_DIR)/$$gpubin_name-gpucfg-$(GPUBIN_CFG).warnings
	nbytes=`du -b -L $< | tail -1 | awk '{ print $$1 }'`
	meas_dir=$(MEAS_DIR)
	# echo DEBUG meas_dir = $$meas_dir

	if test $$nbytes -gt $(GPAR_SIZE) ; then
		# tell user we're starting
		PARSTAT=concurrent
		if test $(THREADS) -gt 1 ; then
			PARSTAT=parallel
			echo \ begin parallel [gpucfg=$(GPUBIN_CFG)] analysis of GPU binary $$gpubin_name \(size = $$nbytes, threads = $(THREADS)\)
		else
			echo \ begin concurrent [gpucfg=$(GPUBIN_CFG)] analysis of GPU binary $$gpubin_name \(size = $$nbytes, threads = 1\)
		fi

		# invoke hpcstruct to process the gpu binary
		$(STRUCT) $(CACHE_ARGS) -j $(THREADS) --gpucfg $(GPUBIN_CFG) -o $$struct_name -M $$meas_dir $$input_name > $$warn_name 2>&1 || { err=$$?; egrep 'ERROR|WARNING' $$warn_name >&2; }
		# echo debug: hpcstruct for analysis of GPU binary $$gpubin_name returned

		# See if there is anything to worry about in the warnings file
		#  suppress any ADVICE, INFO, DEBUG, and CACHESTAT lines and any blank lines;
		#  it's an error if anything remains
		#
		errs=`sed 's/^$//INFO/g;' $$warn_name |grep -v DEBUG | grep -v CACHESTAT | grep -v INFO | grep -v ADVICE | wc -l`
		# echo DEBUG errs = XX $$errs XX

		if [ $${errs} -eq 1 ] ; then
			echo WARNING: incomplete analysis of $$gpubin_name';' see $$warn_name for details
		fi

		# extract the status relative to the cache
		CACHE_STAT=`grep CACHESTAT $$warn_name  | sed 's/CACHESTAT// ' `
		# echo DEBUG CACHE_STAT = XX $$CACHE_STAT XX

		echo \ \ \ end  $$PARSTAT [gpucfg=$(GPUBIN_CFG)] analysis of GPU binary $$gpubin_name $$CACHE_STAT
	fi

#-------------------------------------------------------------------------------
# analyze files to create structure files
#-------------------------------------------------------------------------------
DOMAKE=1

ifeq ($(DOMAKE),1)
all: $(CPUBIN_DIR) $(GPUBIN_USED_DIR)
	$(MAKE) -j $(LJOBS) THREADS=$(LTHREADS) GPAR_SIZE=$(PAR_SIZE) CPAR_SIZE=$(PAR_SIZE) analyze
	$(MAKE) -j $(SJOBS) THREADS=$(STHREADS) GPAR_SIZE=0 CPAR_SIZE=0 analyze
endif

analyze: $(GS) $(CS)

#-------------------------------------------------------------------------------
# remove all generated files
#-------------------------------------------------------------------------------
clean:
	@echo removing all hpcstruct files in $(STRUCTS_DIR)
	@rm -f $(GS)
	@rm -f $(CS)
	@echo removing all links to CPU binaries in $(CPUBIN_DIR)
	@rm -rf $(CPUBIN_DIR)
	@rm -rf $(MEAS_DIR)/all.lm
	@echo removing all warnings files in $(STRUCTS_DIR)
	@rm -f $(CW)
	@rm -f $(GW)
)EOF";
// editorconfig-checker-enable


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

  setenv("PATH", new_path.c_str(), 1);

  // Construct full paths for hpcproflm and hpcstruct
  //
  string hpcproflm_path;
  {
    char* path = getenv("HPCTOOLKIT_HPCPROFLM");
    hpcproflm_path = path != NULL && path[0] != '\0' ? path : HPCTOOLKIT_INSTALL_PREFIX "/libexec/hpctoolkit/hpcproflm";
  }

  string hpcstruct_path;
  {
    char* path = getenv("HPCTOOLKIT_HPCSTRUCT");
    hpcstruct_path = path != NULL && path[0] != '\0' ? path : HPCTOOLKIT_INSTALL_PREFIX "/bin/hpcstruct";
  }

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
           << "PROFLM = "       << hpcproflm_path << "\n"
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
