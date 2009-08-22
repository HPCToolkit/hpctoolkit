// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
using std::cerr;
using std::endl;

#include <fstream>
#include <new>

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/banal/bloop.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/IOUtil.hpp>
#include <lib/support/RealPathMgr.hpp>

//*************************** Forward Declarations ***************************

int
real_main(int argc, char* argv[]);

//****************************************************************************

int
main(int argc, char* argv[])
{
  try {
    return real_main(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  } 
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  } 
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }
}


int
real_main(int argc, char* argv[])
{
  Args args(argc, argv);
  RealPathMgr::singleton().searchPaths(args.searchPathStr);
  
  // ------------------------------------------------------------
  // Read executable
  // ------------------------------------------------------------
  BinUtil::LM* lm = NULL;
  try {
    lm = new BinUtil::LM();
    lm->open(args.in_filenm.c_str());
    lm->read(BinUtil::LM::ReadFlg_ALL);
  } 
  catch (...) {
    DIAG_EMsg("Exception encountered while reading " << args.in_filenm);
    throw;
  }
  
  // ------------------------------------------------------------
  // Build and print the ScopeTree
  // ------------------------------------------------------------
  { 
    ProcNameMgr* procNameMgr = NULL;
    if (args.lush_agent == "agent-c++") {
      procNameMgr = new CppNameMgr;
    }
    else if (args.lush_agent == "agent-cilk") {
      procNameMgr = new CilkNameMgr;
    }

    Prof::Struct::Root* rootStrct = new Prof::Struct::Root("");
    Prof::Struct::Tree* strctTree = new Prof::Struct::Tree("", rootStrct);

    using namespace banal::bloop;
    Prof::Struct::LM* lmStrct = makeStructure(lm, args.doNormalizeTy, 
					      args.isIrreducibleIntervalLoop,
					      args.isForwardSubstitution,
					      procNameMgr,
					      args.dbgProcGlob);
    lmStrct->Link(rootStrct);
    
    const char* osnm = 
      (args.out_filenm == "-") ? NULL : args.out_filenm.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    writeStructure(*os, strctTree, args.prettyPrintOutput);
    IOUtil::CloseStream(os);

    delete strctTree;
  }
  
  // Cleanup
  delete lm;
  
  return (0);
}

//****************************************************************************

