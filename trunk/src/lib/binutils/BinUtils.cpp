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

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
# include <string.h>
#else
# include <cstdlib> // for 'free'
# include <cstring> // for 'strlen', 'strcpy'
using namespace std; // For compatibility with non-std C headers
#endif

#include <sys/types.h> // for 'stat'
#include <sys/stat.h>

//*************************** User Include Files ****************************

#include "BinUtils.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************


//***************************************************************************
// System Dependent Helpers
//***************************************************************************

// 'GetBestFuncName': If 'name' is non-nil, uses 'GetDemangledFuncName' 
// to attempt to demangle it.  If there is an error in demangling,
// return 'name'; otherwise return the demangled version.
const char*
GetBestFuncName(const char* name)
{
  if (!name) { return NULL; }
  if (name[0] == '\0') { return name; }

  const char* demangledName = GetDemangledFuncName(name);
  if (demangledName && (strlen(demangledName) > 0)) {
    return demangledName;
  } 
  else {
    return name;
  }
}


// Include the system demangle
#if defined(OS_IRIX64)
# include <dem.h>      // demangle
  const int DEMANGLE_BUF_SZ = MAXDBUF;
#elif defined(OS_OSF1)
# include <../../usr/include/demangle.h> // demangle (don't confuse with GNU)
  const int DEMANGLE_BUF_SZ = 32768; // see MAXDBUF in SGI's dem.h
#elif defined(OS_SUNOS)
# include <../../usr/include/demangle.h> // demangle (don't confuse with GNU)
  const int DEMANGLE_BUF_SZ = 32768; // see MAXDBUF in SGI's dem.h
#elif defined(OS_LINUX)
  // the system demangle is GNU's demangle
  const int DEMANGLE_BUF_SZ = 32768; // see MAXDBUF in SGI's dem.h
#else
# error "binutils::BinUtils does not recognize your platform."
#endif

const int MANGLE_BUF_SZ = 4096;

// Include GNU's demangle
#include <include/gnu_demangle.h> // GNU's demangle


// Returns the demangled function name.  The return value is stored in
// 'outbuf' and will be clobbered the next time the function is called.
const char*
GetDemangledFuncName(const char* name)
{
  static char inbuf[MANGLE_BUF_SZ];
  static char outbuf[DEMANGLE_BUF_SZ];
  strncpy(inbuf, name, MANGLE_BUF_SZ-1);
  inbuf[MANGLE_BUF_SZ-1] = '\0';

  // -------------------------------------------------------
  // Try the system demangler first
  // -------------------------------------------------------
  strcpy(outbuf, ""); // clear outbuf
  bool demSuccess = false;
#if defined(OS_IRIX64)
  if (demangle(inbuf, outbuf) == 0) {
    demSuccess = true;
  } // else: demangling failed
#elif defined(OS_OSF1)
  if (MLD_demangle_string(inbuf, outbuf,
			  DEMANGLE_BUF_SZ, MLD_SHOW_DEMANGLED_NAME) == 1) {
    demSuccess = true; 
  } // else: error or useless
#elif defined(OS_SUNOS)
  if (cplus_demangle(inbuf, outbuf, DEMANGLE_BUF_SZ) == 0) {
    demSuccess = true;
  } // else: invalid mangled name or output buffer too small
#elif defined(OS_LINUX)
  // System demangler is same as GNU's
#endif

  // Definitions of a 'valid encoded name' differ.  This causes a
  // problem if the system demangler deems a string to be encoded,
  // reports a successful demangle, but simply *copies* the string into
  // 'outbuf' (the 'identity demangle').  (Sun, in particular, has a
  // liberal interpretation of 'valid encoded name'.)  In such cases,
  // we want to give the GNU demangler a chance.
  if (strcmp(inbuf, outbuf) == 0) { demSuccess = false; }
  
  if (demSuccess) { return outbuf; }

  // -------------------------------------------------------
  // Now try GNU's demangler
  // -------------------------------------------------------
  strcpy(outbuf, ""); // clear outbuf
  char* ret = GNU_CPLUS_DEMANGLE(inbuf, DMGL_PARAMS | DMGL_ANSI);
  if (!ret) {
    return NULL; // error!
  }
  strncpy(outbuf, ret, DEMANGLE_BUF_SZ-1);
  outbuf[DEMANGLE_BUF_SZ-1] = '\0';
  free(ret); // gnu_cplus_demangle caller is responsible for memory cleanup
  return outbuf; // success!
}

