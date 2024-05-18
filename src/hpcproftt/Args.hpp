// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
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
#include <vector>

//*************************** User Include Files ****************************


#include "../common/Args.hpp"

#include "../common/diagnostics.h"
#include "../common/CmdLineParser.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

class Args : public Analysis::Args {
public:

  class Exception : public Diagnostics::Exception {
  public:
    Exception(const char* x,
              const char* filenm = NULL, unsigned int lineno = 0)
      : Diagnostics::Exception(x, filenm, lineno)
      { }

    Exception(std::string x,
              const char* filenm = NULL, unsigned int lineno = 0)
      : Diagnostics::Exception(x, filenm, lineno)
      { }

    ~Exception() { }
  };


public:
  Args();
  Args(int argc, const char* const argv[]);
  virtual ~Args();

  // Parse the command line
  void
  parse(int argc, const char* const argv[]);

  void
  printUsage(std::ostream& os) const;

  // Error
  static void
  printError(std::ostream& os, const char* msg) /*const*/;

  static void
  printError(std::ostream& os, const std::string& msg) /*const*/;

  // Dump
  virtual void
  dump(std::ostream& os = std::cerr) const;

public:
  // Parsed Data: Command
  static const std::string&
  getCmd() /*const*/;

  static void
  parseArg_metric(Args* args, const std::string& opts, const char* errTag);

public:

  // Object Correlation args
  std::vector<std::string> obj_procGlobs;
  uint64_t obj_procThreshold;

  bool obj_metricsAsPercents;
  bool obj_showSourceCode;

public:

  // Sparse metrics data format version - YUMENG
  bool sm_easyToGrep = false; //default

private:
  void Ctor();
  void setHPCHome();

private:
  static CmdLineParser::OptArgDesc optArgs[];
  CmdLineParser parser;
};

#endif // Args_hpp
