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
#include <fstream>
#include <new>

#include <string>
using std::string;

#include <sys/stat.h>
#include <sys/types.h>

//*************************** User Include Files ****************************

#include "Args.hpp"
#include "CSProfileUtils.hpp"

#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/PGMReader.hpp>

#include <lib/binutils/LM.hpp>

//*************************** Forward Declarations ***************************

PgmScopeTree*
readStructureData(const char* filenm);

static void
processCallingCtxtTree(CSProfile* profData, VMA begVMA, VMA endVMA, 
		       const std::string& lm_fnm, LoadModScope* lmScope);

static void
processCallingCtxtTree(CSProfile* profData, VMA begVMA, VMA endVMA, 
		       const std::string& lm_fnm);


//****************************************************************************

int realmain(int argc, char* const* argv);

int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
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

  return ret;
}


int
realmain(int argc, char* const* argv) 
{
  Args args(argc, argv);
  
  // ------------------------------------------------------------
  // Read 'profData', the profiling data file
  // ------------------------------------------------------------
  CSProfile* profData = NULL;
  try {
    //profData = TheProfileReader.ReadProfileFile(args.profileFile /*type*/);
    profData = ReadProfile_CSPROF(args.profileFile.c_str(),
				  args.progFile.c_str());
  } 
  catch (...) {
    DIAG_EMsg("While reading profile '" << args.profileFile << "'...");
    throw;
  }
  
  
  // ------------------------------------------------------------
  // Add source file info
  // ------------------------------------------------------------

  PgmScopeTree* scopeTree = NULL;
  PgmScope* pgmScope = NULL;
  if (!args.structureFile.empty()) {
    scopeTree = readStructureData(args.structureFile.c_str());
    pgmScope = scopeTree->GetRoot();
  }
  
  try { 
    // for each (used) load module, add information to the calling
    // context tree.
    
    // for efficiency, we add a flag in CSProfLDmodule "used"
    // add one more pass search the callstack tree, to set 
    // the flag -"alpha"- only, since we get info from binary 
    // profile file not from bfd  
    LdmdSetUsedFlag(profData); 

    // Note that this assumes iteration in reverse sorted order
    int num_lm = profData->GetEpoch()->GetNumLdModule();
    VMA endVMA = VMA_MAX;
    
    for (int i = num_lm - 1; i >= 0; i--) {
      CSProfLDmodule* csp_lm = profData->GetEpoch()->GetLdModule(i); 
      VMA begVMA = csp_lm->GetMapaddr(); // for next csploadmodule

      if (csp_lm->GetUsedFlag()) {
	const std::string& lm_fnm = csp_lm->GetName();
	LoadModScope* lmScope = NULL;
	if (pgmScope) {
	  lmScope = pgmScope->FindLoadMod(lm_fnm);
	}
	
	if (lmScope) {
	  DIAG_Msg(1, "Using STRUCTURE for: " << lm_fnm);
	  processCallingCtxtTree(profData, begVMA, endVMA, lm_fnm, lmScope);
	}
	else {
	  DIAG_Msg(1, "Using debug info for: " << lm_fnm);
	  processCallingCtxtTree(profData, begVMA, endVMA, lm_fnm);
	}
      }

      endVMA = begVMA - 1;
    } /* for each load module */ 
    
    NormalizeCSProfile(profData);
  }
  catch (...) {
    DIAG_EMsg("While preparing CSPROFILE...");
    throw;
  }
  
  delete scopeTree;

  // ------------------------------------------------------------
  // Dump
  // ------------------------------------------------------------

  // prepare output directory 
  args.createDatabaseDirectory();
    
  string dbSrcDir = args.dbDir + "/src";
  if (mkdir(dbSrcDir.c_str(),
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
    DIAG_Die("could not create database source code directory " << dbSrcDir);
  }
  
  copySourceFiles(profData, args.searchPaths, dbSrcDir);
  
  string experiment_fnm = args.dbDir + "/" + args.OutFilename_XML;
  WriteCSProfileInDatabase(profData, experiment_fnm);
  //WriteCSProfile(profData, std::cout, /* prettyPrint */ true);
  

  delete profData;
  return (0);
}

//****************************************************************************

  
class MyDocHandlerArgs : public DocHandlerArgs {
public:
  MyDocHandlerArgs()  { }
  ~MyDocHandlerArgs() { }
  
  virtual string ReplacePath(const char* oldpath) const { return oldpath; }
  virtual bool MustDeleteUnderscore() const { return false; }
};


PgmScopeTree*
readStructureData(const char* filenm)
{
  InitXerces();

  string path = ".";
  PgmScope* pgm = new PgmScope("");
  PgmScopeTree* scopeTree = new PgmScopeTree("", pgm);
  NodeRetriever scopeTreeInterface(pgm, path);
  MyDocHandlerArgs args;
  
  read_PGM(&scopeTreeInterface, filenm, PGMDocHandler::Doc_STRUCT, args);

  FiniXerces();
  
  return scopeTree;
}

//****************************************************************************

static void
processCallingCtxtTree(CSProfile* profData, VMA begVMA, VMA endVMA, 
		       const std::string& lm_fnm, LoadModScope* lmScope)
{
  VMA relocVMA = 0;

  try {
    binutils::LM* lm = new binutils::LM();
    lm->Open(lm_fnm.c_str());
    if (lm->GetType() != binutils::LM::Executable) {
      relocVMA = begVMA;
    }
    delete lm;
  }
  catch (...) {
    DIAG_EMsg("While reading '" << lm_fnm << "'...");
    throw;
  }
  
  InferCallFrames(profData, begVMA, endVMA, lmScope, relocVMA);
}


static void
processCallingCtxtTree(CSProfile* profData, VMA begVMA, VMA endVMA, 
		       const std::string& lm_fnm)
{
  binutils::LM* lm = NULL;
  try {
    lm = new binutils::LM();
    lm->Open(lm_fnm.c_str());
    lm->Read();
  }
  catch (...) {
    DIAG_EMsg("While reading '" << lm_fnm << "'...");
    throw;
  }
  
  // get the start and end PC from the text sections 
  if (lm->GetType() != binutils::LM::Executable) {
    lm->Relocate(begVMA);
  }
  
  InferCallFrames(profData, begVMA, endVMA, lm);
  
  delete lm;
}

