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
// Copyright ((c)) 2002-2021, Rice University
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

#ifndef Analysis_Util_hpp 
#define Analysis_Util_hpp

//************************* System Include Files ****************************

#include <string>

#include <vector>
#include <set>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Args.hpp"

#include <lib/prof/CallPath-Profile.hpp>

#include <lib/prof/Struct-Tree.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace Util {

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum ProfType_t {
  ProfType_NULL,
  ProfType_Callpath,
  ProfType_CallpathMetricDB,
  ProfType_CallpathTrace,
  ProfType_Flat,
  ProfType_SparseDBtmp, //YUMENG: for development purpose only, check the output files from prof2 first round
  ProfType_SparseDBthread, //YUMENG
  ProfType_SparseDBcct, //YUMENG
  ProfType_TraceDB //YUMENG
};


ProfType_t
getProfileType(const std::string& filenm);


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef std::vector<std::string> StringVec;
typedef std::vector<uint> UIntVec;

class NormalizeProfileArgs_t {
public:
  NormalizeProfileArgs_t()
  {
    paths = new StringVec;
    pathLenMax = 0;
    groupMap = new UIntVec;
    groupMax = 0; // 1-based group numbering
  }

  ~NormalizeProfileArgs_t()
  { /* no delete b/c no deep copy constructor */ }

  void
  destroy()
  {
    delete paths;
    paths = NULL;
    delete groupMap;
    groupMap = NULL;
  }

public:
  StringVec* paths;
  uint       pathLenMax;

  UIntVec* groupMap;
  uint     groupMax; // 1-based group numbering
};


NormalizeProfileArgs_t
normalizeProfileArgs(const StringVec& inPaths);


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int
parseReplacePath(const std::string& arg);


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Prof::Struct::ACodeNode*
demandStructure(VMA vma, Prof::Struct::LM* lmStrct, BinUtil::LM* lm, 
		bool useStruct, const string* unknownProcNm = NULL);


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void 
copySourceFiles(Prof::Struct::Root* structure,
		const Analysis::PathTupleVec& pathVec,
		const std::string& dstDir);

void
copyTraceFiles(const std::string& dstDir,
	       const std::set<std::string>& srcFiles);


} // namespace Util

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Util_hpp
