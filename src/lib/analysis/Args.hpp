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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef Analysis_Args_hpp
#define Analysis_Args_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <vector>

//*************************** User Include Files ****************************

#include <include/uint.h>

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
  // Agent options
  // -------------------------------------------------------

  std::string agent;

  // -------------------------------------------------------
  // Attribution/Correlation arguments: general
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

  bool doNormalizeTy;

  // -------------------------------------------------------
  // Attribution/Correlation arguments: special
  // -------------------------------------------------------

  enum MetricFlg {
    MetricFlg_NULL     = 0,
    MetricFlg_Thread   = (1 << 1),
    MetricFlg_StatsSum = (1 << 2),
    MetricFlg_StatsAll = (1 << 3)
  };

  static inline bool
  MetricFlg_isSet(uint flags, MetricFlg x)
  { return (flags & x); }

  static inline void
  MetricFlg_set(uint& flags, MetricFlg x)
  { flags |= x; }

  static inline void
  MetricFlg_clear(uint& flags, MetricFlg x)
  { flags &= ~x; }

  static inline bool
  MetricFlg_isThread(uint flags)
  { return MetricFlg_isSet(flags, MetricFlg_Thread); }

  static inline bool
  MetricFlg_isSum(uint flags)
  {
    return (MetricFlg_isSet(flags,  MetricFlg_StatsSum)
	    || MetricFlg_isSet(flags, MetricFlg_StatsAll));
  }

  uint prof_metrics;

  // TODO: Currently this is always true even though we only need to
  // compute final metric values for (1) hpcproftt (flat) and (2)
  // hpcprof-flat when it computes derived metrics.  However, at the
  // moment this is a sinking ship and not worth the time investment.
  bool profflat_computeFinalMetricValues;

  // -------------------------------------------------------
  // Output arguments: experiment database output
  // -------------------------------------------------------

#define Analysis_OUT_DB_EXPERIMENT "experiment.xml"
#define Analysis_OUT_DB_CSV        "experiment.csv"

#define Analysis_DB_DIR_pfx        "hpctoolkit"
#define Analysis_DB_DIR_nm         "database"
#define Analysis_DB_DIR            "hpctoolkit-<app>-database"


  std::string out_db_experiment; // disable: "", stdout: "-"
  std::string out_db_csv;        // disable: "", stdout: "-"

  std::string db_dir;            // disable: ""
  bool db_copySrcFiles;

  std::string out_db_config;     // disable: "", stdout: "-"

  bool db_makeMetricDB;
  bool db_addStructId;

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

  // flag to remove procedure name redundancy
  // this feature is not fully reliable to remove procedure
  // name redundancy, especially if compiled with g++ -O2
  // since the compiler doesn't generate function parameters
  // and cannot distinguish between foo(int) and foo(int, int)
  bool remove_redundancy;

public:
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  void
  normalizeSearchPaths();

  // makes a unique database dir
  void
  makeDatabaseDir();

  std::string
  searchPathStr() const;
  

private:
  void Ctor();
}; 

} // namespace Analysis

//***************************************************************************

#endif // Analysis_Args_hpp 
