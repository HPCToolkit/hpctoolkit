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

#ifndef binutils_BinUtils_hpp
#define binutils_BinUtils_hpp

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************


#include "../common/ProcNameMgr.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace BinUtil {


std::string
canonicalizeProcName(const std::string& name, ProcNameMgr* procNameMgr = NULL);


std::string
demangleProcName(const std::string& name);


} // namespace BinUtil


//****************************************************************************

#endif // binutils_BinUtils_hpp
