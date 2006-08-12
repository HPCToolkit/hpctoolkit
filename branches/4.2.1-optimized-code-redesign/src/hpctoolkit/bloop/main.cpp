// -*-Mode: C++;-*-
// $Id$

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
//    main.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::cerr;
using std::endl;

#include <fstream>
#include <new>

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/Utils/Exception.hpp>

//*************************** User Include Files ****************************

#include "Args.hpp"
#include "PgmScopeTreeBuilder.hpp"
using namespace ScopeTreeBuilder;

#include <lib/binutils/LoadModule.hpp>

#include <lib/support/diagnostics.h>

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
  catch (const OA::Exception& x) {
    x.report(cerr);
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
  
  // ------------------------------------------------------------
  // Read executable
  // ------------------------------------------------------------
  LoadModule* lm = NULL;
  try {
    lm = new LoadModule(); // Executable(): use LoadModule for now
    if (!lm->Open(args.inputFile.c_str())) { 
      exit(1); // Error already printed 
    } 
    if (!lm->Read()) { exit(1); } // Error already printed 
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("Memory alloc failed while reading binary! " << x.what());
    exit(1);
  }
  
  if (args.dumpBinary) {
    // ------------------------------------------------------------
    // Dump load module without a scope tree
    // ------------------------------------------------------------
    lm->Dump(std::cout);
  } 
  else {
    // ------------------------------------------------------------
    // Build and print the ScopeTree
    // ------------------------------------------------------------

    // Build scope tree
    PgmScopeTree* pgmScopeTree =
      BuildFromLM(lm, args.canonicalPathList.c_str(),
		  args.normalizeScopeTree, 
		  args.unsafeNormalizations,
		  args.irreducibleIntervalIsLoop,
		  args.verboseMode);
    
    // Write scope tree
    WriteScopeTree(std::cout, pgmScopeTree, args.prettyPrintOutput);
    
    // Cleanup
    delete pgmScopeTree;
  }
  
  delete lm;
  return (0);
}

//****************************************************************************

