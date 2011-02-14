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
// Copyright ((c)) 2002-2011, Rice University
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

#ifndef BAnal_Struct_hpp
#define BAnal_Struct_hpp

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/uint.h> 

#include <lib/prof/Struct-Tree.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/ProcNameMgr.hpp>


//***************************************************************************

namespace BAnal {

namespace Struct {

  enum NormTy {
    // TODO: redo along the lines of BinUtil::LM::ReadFlg
    NormTy_None,
    NormTy_Safe, // Safe-only
    NormTy_All
  };

  Prof::Struct::LM* 
  makeStructure(BinUtil::LM* lm, 
		NormTy doNormalizeTy,
		bool isIrrIvalLoop = false,
		bool isFwdSubst = false,
		ProcNameMgr* procNameMgr = NULL,
		const std::string& dbgProcGlob = "");
  
  Prof::Struct::Stmt*
  makeStructureSimple(Prof::Struct::LM* lmStrct, BinUtil::LM* lm, VMA vma);


  bool 
  normalize(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe = true);

  void
  writeStructure(std::ostream& os, Prof::Struct::Tree* strctTree,
		 bool prettyPrint = true);

} // namespace Struct

} // namespace BAnal

//****************************************************************************

#endif // BAnal_Struct_hpp
