// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Args_hpp
#define Args_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <string>

//*************************** User Include Files ****************************

#include "../common/CmdLineParser.hpp"
#include "../hpcprof/stdshim/filesystem.hpp"

//*************************** Forward Declarations **************************

typedef enum { CACHE_DISABLED, CACHE_ENABLED, CACHE_NOT_NAMED, CACHE_ENTRY_COPIED, CACHE_ENTRY_COPIED_RENAME, CACHE_ENTRY_ADDED,
        CACHE_ENTRY_ADDED_RENAME, CACHE_ENTRY_REPLACED, CACHE_ENTRY_REMOVED } cachestat_t;

//***************************************************************************

class Args {
public:
  Args();
  Args(int argc, const char* const argv[]);
  ~Args();

  // Parse the command line
  void
  parse(int argc, const char* const argv[]);

  void
  printUsage(std::ostream& os) const;

  // Error
  void
  printError(std::ostream& os, const char* msg) const;

  void
  printError(std::ostream& os, const std::string& msg) const;

  // Dump
  void
  dump(std::ostream& os = std::cerr) const;

  void
  ddump() const;

public:
  // Parsed Data: Command
  const std::string& getCmd() const;

  int jobs;
  int jobs_struct;
  int jobs_parse;
  int jobs_symtab;
  bool show_time;
  long parallel_analysis_threshold;
  bool analyze_cpu_binaries ;     // default: true
  bool analyze_gpu_binaries ;     // default: true
  bool compute_gpu_cfg;
  std::string meas_dir;
  bool is_from_makefile;        // set true if -M argument is seen
  cachestat_t cache_stat;       // reflects cache interactions for the binary

  // Parsed Data: optional arguments
  std::string searchPathStr;          // default: "."
  std::string dbgProcGlob;

  bool pretty_print_output;       // default: false
  bool useBinutils;               // default: false
  bool show_gaps;                 // default: false
  bool nocache;                   // default: false

  // Parsed Data: arguments
  std::string in_filenm;
  hpctoolkit::stdshim::filesystem::path full_filenm;
  std::string out_filenm;
  std::string cache_directory;

private:
  void
  Ctor();

private:
  static CmdLineParser::OptArgDesc optArgs[];
  CmdLineParser parser;
};

extern Args *global_args;

#endif // Args_hpp
