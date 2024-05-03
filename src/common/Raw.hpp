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

#ifndef Analysis_Raw_Raw_hpp
#define Analysis_Raw_Raw_hpp

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

#include "lean/hpcrun-fmt.h"
#include "lean/id-tuple.h"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace Raw {

void
writeAsText(/*destination,*/ const char* filenm, bool sm_easyToGrep);
//YUMENG: second arg: if more flags, maybe build a struct to include all flags and pass the struct around

void
writeAsText_callpath(/*destination,*/ const char* filenm, bool sm_easyToGrep);

void
writeAsText_profiledb(const char* filenm, bool sm_easyToGrep);

void
writeAsText_cctdb(const char* filenm, bool sm_easyToGrep);

void
writeAsText_tracedb(const char* filenm);

void
writeAsText_metadb(const char* filenm);

void
writeAsText_callpathMetricDB(/*destination,*/ const char* filenm);

void
writeAsText_callpathTrace(/*destination,*/ const char* filenm);

} // namespace Raw

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Raw_Raw_hpp
