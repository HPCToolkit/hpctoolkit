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

//************************* System Include Files ****************************

#include <iostream>
#include <string>
using std::string;

#include <cstdlib> // for 'free'
#include <cstring> // for 'strlen', 'strcpy'


//*************************** User Include Files ****************************

#include "BinUtils.hpp"


//****************************************************************************

// Include the system demangle
#if defined(HOST_OS_SOLARIS)
# include <../../usr/include/demangle.h> // demangle (don't confuse with GNU)
  const int DEMANGLE_BUF_SZ = 32768; // see MAXDBUF in SGI's dem.h
#elif defined(HOST_OS_LINUX) || defined(HOST_OS_MACOS)
  // the system demangle is GNU's demangle
#else
# error "binutils::BinUtils does not recognize your platform."
#endif

// Include GNU's demangle
#include <include/gnu_demangle.h> // GNU's demangle

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace BinUtil {

//***************************************************************************
// 
//***************************************************************************

// 'canonicalizeProcName': If 'name' is non-empty, uses 'demangleProcName' 
// to attempt to demangle it.  If there is an error in demangling,
// return 'name'; otherwise return the demangled version.
string
canonicalizeProcName(const std::string& name, ProcNameMgr* procNameMgr)
{
  if (name.empty()) {
    return name; 
  }

  string bestname = demangleProcName(name.c_str());
  if (procNameMgr) {
    bestname = procNameMgr->canonicalize(bestname);
  }
  
  return bestname;
}


// Returns the demangled function name (if possible) or the original name.
string
demangleProcName(const std::string& name)
{
  string bestname = name;

  // -------------------------------------------------------
  // Try the system demangler first
  // -------------------------------------------------------
#if defined(HOST_OS_SOLARIS)
  static char outbuf[DEMANGLE_BUF_SZ] = { '\0' };
  if (cplus_demangle(name, outbuf, DEMANGLE_BUF_SZ) == 0) {
    // Sun has a liberal interpretation of 'valid encoded name'.)
    // Ensure, a change has occured.
    if (strcmp(name, outbuf) != 0) {
      bestname = outbuf;
    }
  }
  if (!bestname.empty()) {
    return bestname;
  }
#endif
  // Note: on Linux, system demangler is same as GNU's

  // -------------------------------------------------------
  // Now try GNU's demangler
  // -------------------------------------------------------
  char* str = GNU_CPLUS_DEMANGLE(name.c_str(), DMGL_PARAMS | DMGL_ANSI);
  if (str) {
    bestname = str;
    free(str); // gnu_cplus_demangle caller is responsible for memory cleanup
  }

  return bestname;
}


} // namespace BinUtil
