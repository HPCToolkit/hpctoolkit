// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Args.hpp"

#include <lib/prof-juicy/CallPath-Profile.hpp>

#include <lib/prof-juicy/Struct-Tree.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace Util {

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum ProfType_t {
  ProfType_NULL,
  ProfType_CALLPATH,
  ProfType_FLAT
};

ProfType_t
getProfileType(const std::string& filenm);

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Prof::Struct::ACodeNode*
demandStructure(VMA vma, Prof::Struct::LM* lmStrct, BinUtil::LM* lm, 
		bool useStruct);


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void 
copySourceFiles(Prof::Struct::Root* structure, 
		const Analysis::PathTupleVec& pathVec, 
		const string& dstDir);

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


} // namespace Util

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Util_hpp
