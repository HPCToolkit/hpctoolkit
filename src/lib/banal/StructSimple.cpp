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
// Copyright ((c)) 2002-2020, Rice University
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

// This file builds a Struct Simple tree of Prof::Struct nodes from a
// binutils load module.  This is for load modules without a full
// structure file.
//
// The tree consists of File, Proc, Stmt and single (guard) Alien
// nodes, but not loops or full inline sequences.  The tree is built
// incrementally only for vma's for which we take a sample.

//***************************************************************************

#include <iostream>
#include <sstream>
#include <string>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>

#include <lib/prof/Struct-Tree.hpp>

#include <lib/support/dictionary.h>
#include <lib/support/FileUtil.hpp>

#include "StructSimple.hpp"

using namespace std;

#define DEBUG_STRUCT_SIMPLE  0

//****************************************************************************

//
// makeStructureSimple -- make a Prof::Struct::Stmt node and path up
// to lmStruct for vma.
//
Prof::Struct::Stmt *
BAnal::Struct::makeStructureSimple(Prof::Struct::LM * lmStruct,
				   BinUtil::LM * lm, VMA vma)
{
  //
  // begin address for proc containing vma, and proc and file name
  //
  string prettynm, linknm, proc_filenm;
  SrcFile::ln proc_line = 0;
  VMA proc_vma = vma;

  BinUtil::Proc * bproc = lm->findProc(vma);

  if (bproc != NULL) {
    proc_vma = bproc->begVMA();
    lm->findSrcCodeInfo(proc_vma, 0, linknm, proc_filenm, proc_line);
  } else {
    lm->findSimpleFunction(proc_vma, linknm);
  }

  if (proc_filenm.empty()) {
    proc_filenm = string(UNKNOWN_FILE)
        + " [" + FileUtil::basename(lm->name().c_str()) + "]";
  }

  if (! linknm.empty()) {
    prettynm = BinUtil::demangleProcName(linknm);
  }
  else {
    stringstream buf;

    buf << UNKNOWN_PROC << " 0x" << hex << proc_vma << dec
	<< " [" << FileUtil::basename(lm->name().c_str()) << "]";
    prettynm = buf.str();
  }

  Prof::Struct::File * fileStruct =
    Prof::Struct::File::demand(lmStruct, proc_filenm);

  Prof::Struct::Proc * procStruct =
    Prof::Struct::Proc::demand(fileStruct, prettynm, linknm, proc_line, proc_line);

  //
  // file and line for vma (stmt), and end vma
  //
  string stmt_procnm, stmt_filenm;
  SrcFile::ln stmt_line = 0;
  VMA end_vma = vma + 1;

  lm->findSrcCodeInfo(vma, 0, stmt_procnm, stmt_filenm, stmt_line);

  BinUtil::Insn * insn = lm->findInsn(vma, 0);
  if (insn) {
    end_vma = insn->endVMA();
  }

  Prof::Struct::Stmt * stmt = NULL;

  // stmts with known file and line that differs from proc need a
  // guard alien
  if ((! stmt_filenm.empty()) && stmt_line != 0
      && (stmt_filenm != proc_filenm || stmt_line < proc_line))
  {
    Prof::Struct::Alien * alien = procStruct->demandGuardAlien(stmt_filenm, stmt_line);
    stmt = alien->demandStmt(stmt_line, vma, end_vma);
  }
  else {
    stmt = procStruct->demandStmtSimple(stmt_line, vma, end_vma);
  }

#if DEBUG_STRUCT_SIMPLE
  cout << "------------------------------------------------------------\n"
       << "0x" << hex << vma << "--0x" << end_vma << dec << "  (struct simple)\n"
       << "line:  " << stmt_line << "\n"
       << "file:  " << stmt_filenm << "\n"
       << "name:  " << linknm << "\n\n";

  stmt->dumpmePath(cout, 0, "");
  cout << "\n";
#endif

  return stmt;
}
