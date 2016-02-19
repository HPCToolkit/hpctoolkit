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
// Copyright ((c)) 2002-2016, Rice University
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

//************************* System Include Files ****************************

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <iomanip>

#include <fstream>
#include <sstream>

#include <string>
using std::string;

#include <map>
#include <list>
#include <vector>

#include <algorithm>

#include <cstring>

//*************************** User Include Files ****************************

#include "StructSimple.hpp"

#include <lib/prof/Struct-Tree.hpp>
using namespace Prof;

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Insn.hpp>

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations ***************************

//****************************************************************************
// 
//****************************************************************************

// makeStructureSimple: Uses the line map to make structure
Prof::Struct::Stmt*
BAnal::Struct::makeStructureSimple(Prof::Struct::LM* lmStrct,
				   BinUtil::LM* lm, VMA vma)
{
  string procnm, filenm;
  SrcFile::ln line = Prof::Struct::Tree::UnknownLine;
  lm->findSrcCodeInfo(vma, 0 /*opIdx*/, procnm, filenm, line);
  procnm = BinUtil::canonicalizeProcName(procnm);
  
  if (filenm.empty()) {
    filenm = Prof::Struct::Tree::UnknownFileNm;
  }
  if (procnm.empty()) {
    procnm = Prof::Struct::Tree::UnknownProcNm;
  }
  
  Prof::Struct::File* fileStrct = Prof::Struct::File::demand(lmStrct, filenm);
  Prof::Struct::Proc* procStrct = Prof::Struct::Proc::demand(fileStrct, procnm,
							     "", line, line);

  VMA begVMA = vma, endVMA = vma + 1;
  BinUtil::Insn* insn = lm->findInsn(vma, 0 /*opIdx*/);
  if (insn) {
    endVMA = insn->endVMA();
  }
  Prof::Struct::Stmt* stmtStrct = demandStmtStructure(lmStrct, procStrct, line,
						      begVMA, endVMA);
  
  return stmtStrct;
}


Struct::Stmt*
BAnal::Struct::demandStmtStructure(Prof::Struct::LM* lmStrct,
				   Prof::Struct::Proc* procStrct,
				   SrcFile::ln line, VMA begVMA, VMA endVMA)
{
  Prof::Struct::Stmt* stmtStrct = procStrct->findStmt(line);

  if (stmtStrct) {
    if (0) {
      // disable: potentially expensive to maintain
      lmStrct->eraseStmtIf(stmtStrct);
    }
    stmtStrct->vmaSet().insert(begVMA, endVMA);
    if (0) {
      // disable: potentially expensive to maintain
      lmStrct->insertStmtIf(stmtStrct);
    }
  }
  else {
    // N.B.: calls lmStrct->insertStmtIf()
    stmtStrct = new Prof::Struct::Stmt(procStrct, line, line, begVMA, endVMA);
  }

  return stmtStrct;
}

