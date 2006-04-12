// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    LoadModuleInfo.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <iostream>
#include <fstream>
#include <map>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include <include/general.h>

#include "LoadModuleInfo.h"
#include <lib/support/Assertion.h>

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/PCToSrcLineMap.h>

//************************ Forward Declarations ******************************

using std::cerr;
using std::hex;
using std::dec;
using std::endl;

//****************************************************************************

//***************************************************************************
// LoadModuleInfo
//***************************************************************************

LoadModuleInfo::LoadModuleInfo(LoadModule* _lm, PCToSrcLineXMap* _map)
  : lm(_lm), map(_map)
{
  BriefAssertion(lm != NULL);
}

LoadModuleInfo::~LoadModuleInfo()
{
  lm = NULL; // does not own member data
  map = NULL;
}

bool
LoadModuleInfo::GetSymbolicInfo(Addr pc, ushort opIndex,
				String& func, String& file, SrcLineX& srcLn)
{
  // 'PCToSrcLineXMap' has priority over 'LoadModule'.
  bool foundInfo = false;
  if (map) {
    // FIXME: add opindex to map
    // Look in 'PCToSrcLineXMap'.
    Addr strta = map->GetStartAddr(), enda = map->GetEndAddr();
    if ( strta <= pc && pc <= enda ) {
      ProcPCToSrcLineXMap* pmap = map->FindProc(pc);
      SrcLineX* s = pmap->Find(pc);
      func = pmap->GetProcName();
      file = pmap->GetFileName();
      srcLn = *s;
      foundInfo = true;
    } else {
      // FIXME: what can we expect from pcmap?
      cerr << hex << "Error: GetSymbolicInfo: pc " << pc << " is out of "
	"range [" << strta << ", " << enda << ")" << dec << endl;
      foundInfo = false;
    }
  } 

  if (!foundInfo) {
    // Look in 'LoadModule'.
    suint line;
    lm->GetSourceFileInfo(pc, opIndex, func, file, line);
    func = GetBestFuncName(func);
    srcLn.SetSrcLine(line);
  }
  return true;
}


bool 
LoadModuleInfo::GetProcedureFirstLineInfo(Addr pc, 
					  ushort opIndex, 
					  suint &line) {
  return lm->GetProcedureFirstLineInfo(pc, opIndex, line);
}
