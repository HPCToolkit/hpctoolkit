// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Analysis_Args_hpp
#define Analysis_Args_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <vector>

//*************************** User Include Files ****************************

#include <include/general.h>

//*************************** Forward Declarations **************************

//***************************************************************************

namespace Analysis {

// PathTuple: a {path, viewname} pair.
//   PathTuple.first = path; PathTuple.second = path target or viewname
// PathTupleVec: the vector of all 'PathTuple'
typedef std::pair<std::string, std::string> PathTuple;
typedef std::vector<PathTuple> PathTupleVec;

const std::string DefaultPathTupleTarget = "src";

} // namespace Analysis

//***************************************************************************

namespace Analysis {

//---------------------------------------------------------------------------
// Arguments for source code correlation
//---------------------------------------------------------------------------

class Args {
public: 
  Args();
  virtual ~Args();

  // Dump
  virtual std::string toString() const;

  virtual void dump(std::ostream& os = std::cerr) const;
  void ddump() const;

public:
  // -------------------------------------------------------
  // Correlation arguments
  // -------------------------------------------------------

  // Title
  std::string title;

  // Search paths
  //std::vector<std::string> searchPaths;
  PathTupleVec searchPathTpls;

  // Structure files
  std::vector<std::string> structureFiles;

  // Group files
  std::vector<std::string> groupFiles;

  // Replace paths
  std::vector<std::string> replaceInPath;
  std::vector<std::string> replaceOutPath;

  // Profile files
  std::vector<std::string> profileFiles;

  // FIXME: computed metrics require interior values (implications for
  // hpcviewer?)... perhaps this should only be an output option.
  bool metrics_computeInteriorValues;

  // LUSH agent (prototype)
  std::string lush_agent;

  // -------------------------------------------------------
  // Output arguments: database output
  // -------------------------------------------------------

#define Analysis_OUT_DB_EXPERIMENT "experiment.xml"
#define Analysis_OUT_DB_CSV        "experiment.csv"
#define Analysis_DB_DIR            "experiment-db"

  std::string out_db_experiment; // disable: "", stdout: "-"
  std::string out_db_csv;        // disable: "", stdout: "-"

  std::string db_dir;            // disable: ""
  bool db_copySrcFiles;

  // -------------------------------------------------------
  // Output arguments: textual output
  // -------------------------------------------------------

#define Analysis_OUT_TXT           ""

  std::string out_txt;           // disable: "", stdout: "-"

  enum TxtSum { 
    TxtSum_NULL  = 0,
    
    // individual flags
    TxtSum_fPgm  = 0x00000001,
    TxtSum_fLM   = 0x00000010,
    TxtSum_fFile = 0x00000100,
    TxtSum_fProc = 0x00001000,
    TxtSum_fLoop = 0x00010000,
    TxtSum_fStmt = 0x00100000,

    // composite flags
    TxtSum_ALL  = (TxtSum_fPgm | TxtSum_fLM | TxtSum_fFile | TxtSum_fProc 
		   | TxtSum_fLoop | TxtSum_fStmt)
  };

  int/*TxtSum*/ txt_summary;

  bool txt_srcAnnotation;
  std::vector<std::string> txt_srcFileGlobs;


public:
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  void normalizeSearchPaths();

  std::string searchPathStr() const;
  

private:
  void Ctor();
}; 

} // namespace Analysis

//***************************************************************************

#endif // Analysis_Args_hpp 
