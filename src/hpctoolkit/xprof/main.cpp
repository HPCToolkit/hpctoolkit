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
#include "ProfileReader.h"
#include "ProfileWriter.h"
#include "PCProfile.h"
#include "DerivedProfile.h"

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/binutils/LoadModuleInfo.h>

//*************************** Forward Declarations ***************************

using std::cerr;

PCProfileFilterList*
CreateFilterList(LoadModule* lm);

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
    if (!exe->Open(args.progFile)) { exit(1); } // Error already printed 
    if (!exe->Read()) { exit(1); }              // Error already printed 
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading binary!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading binary!\n";
    exit(2);
  }
  
  // ------------------------------------------------------------
  // Read 'PCToSrcLineXMap', if available
  // ------------------------------------------------------------
  PCToSrcLineXMap* map = NULL;
  try {
    if (!args.pcMapFile.Empty()) {
      map = ReadPCToSrcLineMap(args.pcMapFile);
      if (!map) {
	cerr << "Error reading file `" << args.pcMapFile << "'\n";
	exit(1); 
      }
    }
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading map!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading map!\n";
    exit(2);
  }
  
  LoadModuleInfo modInfo(exe, map);
  
  // ------------------------------------------------------------
  // Read 'rawprofData', the raw profiling data file
  // ------------------------------------------------------------
  PCProfile* rawprofData = NULL;
  try {
    rawprofData = ProfileReader::ReadProfileFile(args.profFile /*filetype*/);
    if (!rawprofData) { exit(1); }
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading profile!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading profile!\n";
    exit(2);
  }
  
  // ------------------------------------------------------------
  // Construct some metrics, derived in some way from the raw
  // profiling data, that we are interested in.
  // ------------------------------------------------------------
  DerivedProfile* profData = NULL;
  try {
    // We currently assume DCPI mode
    PCProfileFilterList* filtList = NULL;
    if (!args.outputRawMetrics) {
      filtList = CreateFilterList(modInfo.GetLM());
    }
    
    profData = new DerivedProfile(rawprofData, filtList);

  } catch (...) {
    cerr << "Error: Exception encountered while constructing derived profile!\n";
    exit(2);
  }

  // ------------------------------------------------------------
  // Translate the derived metrics to a PROFILE file
  // ------------------------------------------------------------
  try {
    // We currently assume DCPI mode
    //DumpAsPROFILE(std::cout, rawprofData, &modInfo);
    ProfileWriter::WriteProfile(std::cout, profData, &modInfo);
  } catch (...) {
    cerr << "Error: Exception encountered while translating to PROFILE!\n";
    exit(2);
  }
  
  delete exe;
  delete map;
  delete rawprofData;
  delete profData;
  return (0);
}

//****************************************************************************

#include "DCPIProfile.h"

PCProfileFilterList*
CreateFilterList(LoadModule* lm)
{
  PCProfileFilterList* flist = new PCProfileFilterList;
  flist->push_back(DCPIProfileFilter::RetiredInsn(lm));
  flist->push_back(DCPIProfileFilter::RetiredFPInsn(lm));

  return flist;
}
