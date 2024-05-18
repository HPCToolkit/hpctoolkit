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

#ifndef Analysis_Util_hpp
#define Analysis_Util_hpp

//************************* System Include Files ****************************

#include <string>

#include <vector>
#include <set>

//*************************** User Include Files ****************************


#include "Args.hpp"

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
  ProfType_ProfileDB,
  ProfType_CctDB,
  ProfType_TraceDB,
  ProfType_MetaDB,
};


ProfType_t
getProfileType(const std::string& filenm);


// --------------------------------------------------------------------------
// Output options
// --------------------------------------------------------------------------

enum OutputOption_t {
   Print_All,
   Print_LoadModule_Only
};

extern OutputOption_t option;

} // namespace Util

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Util_hpp
