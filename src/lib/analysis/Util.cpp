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

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "Util.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

Analysis::Util::ProfType_t 
Analysis::Util::getProfileType(const std::string& filenm)
{
  // FIXME: a better way of doing this would be to read the first 32
  // bytes and test for the magic cookie.
  
  // FIXME: not yet abstracted since csprof is still a mess
  static const string CALLPATH_SFX = ".csp";
  
  if (filenm.length() > CALLPATH_SFX.length()) {
    uint begpos = filenm.length() - CALLPATH_SFX.length();
    if (filenm.find(CALLPATH_SFX, begpos) != string::npos) {
      return ProfType_CALLPATH;
    }
  }
  
  return ProfType_FLAT;
}

