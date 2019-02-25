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
// Copyright ((c)) 2002-2019, Rice University
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

#include <include/uint.h>
#include <lib/support/CmdLineParser.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************

class Args {
public:
  enum EventList_t {
    LIST_NONE  = 0, // none
    LIST_SHORT,     // 'short'
    LIST_LONG       // 'long'
  };

public: 
  Args(); 
  Args(int argc, const char* const argv[]);
  ~Args(); 

  // Parse the command line
  void parse(int argc, const char* const argv[]);

  // Version and Usage information
  void printVersion(std::ostream& os) const;
  void printUsage(std::ostream& os) const;
  
  // Error
  void printError(std::ostream& os, const char* msg) const;
  void printError(std::ostream& os, const std::string& msg) const;

  // Dump
  void dump(std::ostream& os = std::cerr) const;
  void ddump() const;

public:  
  // Parsed Data: Command
  const std::string& getCmd() const;

  // Parsed Data: optional arguments
  EventList_t listEvents; // default: LIST_NONE
  bool        printPaths; // default: false

  // Parsed Data: optional arguments
  std::string profRecursive; // default: libhpcrun's default
  std::string profThread;    // default: libhpcrun's default
  std::string profEvents;    // default: libhpcrun default
  std::string profOutput;    // default: libhpcrun default
  std::string profPAPIFlag;  // default: libhpcrun default

  // Parsed Data:
  std::vector<std::string> profArgV;
  
private:
  void Ctor();

private:
  static CmdLineParser::OptArgDesc optArgs[];
  CmdLineParser parser;
}; 

#endif // Args_hpp
