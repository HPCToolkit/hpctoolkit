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
#include <fstream>
#include <new>

//*************************** User Include Files ****************************

#include "Args.h"
#include "PgmScopeTree.h"
#include "PgmScopeTreeUtils.h"
#include <lib/binutils/LoadModule.h>
#include <lib/binutils/PCToSrcLineMap.h>

//*************************** Forward Declarations ***************************

using std::cerr;

bool WriteMapFile(PCToSrcLineXMap* map, String& fname);

//****************************************************************************

int
main(int argc, char* argv[])
{
  Args args(argc, argv);

  // ------------------------------------------------------------
  // Read executable
  // ------------------------------------------------------------
  Executable* exe = NULL;
  try {
    exe = new Executable();
    if (!exe->Open(args.inputFile)) { exit(1); } // Error already printed 
    if (!exe->Read()) { exit(1); }               // Error already printed 
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading binary!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading binary!\n";
    exit(2);
  }

  if (args.debugMode) {
    // ------------------------------------------------------------
    // Dump executable without a scope tree
    // ------------------------------------------------------------
    try {
      exe->Dump(std::cout);
    } catch (...) {
      cerr << "Error: Exception encountered while dumping binary!\n";
      exit(2);
    }
  } else {
    // ------------------------------------------------------------
    // Build and print the ScopeTree
    // ------------------------------------------------------------
    try {
      PCToSrcLineXMap* map = NULL;
      if (args.pcMapFile.Length() > 0) {
	map = (PCToSrcLineXMap*)1; // FIXME:indicate that map should be created
      }
      
      // Build scope tree
      PgmScopeTree* pgmScopeTree =
	BuildScopeTreeFromExe(exe, map, args.canonicalPathList,
			      args.normalizeScopeTree,
			      args.verboseMode);
      
      // Write map (map should now be a valid pointer) & scope tree
      if (map) { WriteMapFile(map, args.pcMapFile); }
      WriteScopeTree(std::cout, pgmScopeTree, args.prettyPrintOutput);
      
      // Cleanup
      delete pgmScopeTree;
      delete map;
    } catch (std::bad_alloc& x) {
      cerr << "Error: Memory alloc failed while preparing PGM tree!\n";
      exit(1);
    } catch (...) {
      cerr << "Error: Exception encountered while preparing PGM tree!\n";
      exit(2);
    }
  }

  delete exe;
  return (0);
}

//****************************************************************************

bool WriteMapFile(PCToSrcLineXMap* map, String& fname)
{

  std::ofstream ofile(fname);
  if ( !ofile.is_open() || ofile.fail() ) {
    cerr << "Error opening file `" << fname << "'\n";
    return false;
  }
  
  bool status = map->Write(ofile);
  ofile.close();
  
  if (!status) {
    cerr << "Error writing file `" << fname << "'\n";
    return false;
  }
  
#if 0
  std::ifstream file1(fname);
  std::ofstream file2((fname+".tmp"));
  
  // Try reading the map and writing it to a second file for comparison
  PCToSrcLineXMap* map1 = new PCToSrcLineXMap();
  map1->Read(file1);
  map1->Write(file2);
  delete map1;
  file1.close();
  file2.close();
#endif
  return true;
}
