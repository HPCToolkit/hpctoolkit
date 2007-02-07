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
  if (!args.structureFile.empty()) {
    scopeTree = readStructureData(args.structureFile.c_str());
  }
  
  try { 
    // for each "used" load module, go through the call stack tree 
    // to find the source line infornmation 
    
    // for efficiency, we add a flag in CSProfLDmodule "used"
    // add one more pass search the callstack tree, to set 
    // the flag -"alpha"- only, since we get info from binary 
    // profile file not from bfd  
    LdmdSetUsedFlag(profData); 

    // Note that this assumes iteration in reverse sorted order
    int num_lm = profData->GetEpoch()->GetNumLdModule();
    VMA endVMA = 0; 
    
    for (int i = num_lm - 1; i >= 0; i--) {
      CSProfLDmodule* csp_lm = profData->GetEpoch()->GetLdModule(i); 
      VMA begVMA = csp_lm->GetMapaddr(); // for next csploadmodule
      
      if (csp_lm->GetUsedFlag()) {
	binutils::LM* lm = NULL;
	try {
	  lm = new binutils::LM();
	  lm->Open(csp_lm->GetName());
	  lm->Read();
	}
	catch (...) {
	  DIAG_EMsg("While reading '" << csp_lm->GetName() << "'...");
	  throw;
	}
	
	// get the start and end PC from the text sections 
	DIAG_Msg(1, "Load Module: " << csp_lm->GetName());
	if (lm->GetType() != binutils::LM::Executable) {
	  lm->Relocate(begVMA);   
	}

	// FIXME: remove 'last' by initializing endVMA to the max value
	bool last = (i == (num_lm - 1));
	AddSourceFileInfoToCSProfile(profData, lm, begVMA, endVMA, last);
	NormalizeInternalCallSites(profData, lm, begVMA, endVMA, last);
	
	delete lm;
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
