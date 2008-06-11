// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#include <string>
using std::string;

#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/analysis/CallPath.hpp>

#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/PGMReader.hpp>

#include <lib/binutils/LM.hpp>

//*************************** Forward Declarations ***************************

static Prof::CSProfile* 
readProfileData(std::vector<string>& profileFiles);

static PgmScopeTree*
readStructure(std::vector<string>& structureFiles);

static void
dumpProfileData(std::ostream& os, std::vector<string>& profileFiles);

static void
processCallingCtxtTree(Prof::CSProfile* prof, Prof::Epoch::LM* epoch_lm,
		       LoadModScope* lmScope);

static void
processCallingCtxtTree(Prof::CSProfile* prof, Prof::Epoch::LM* epoch_lm);


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
  Prof::CSProfile* prof = readProfileData(args.profileFiles);

  // ------------------------------------------------------------
  // Add source file info
  // ------------------------------------------------------------

  PgmScopeTree* scopeTree = NULL;
  PgmScope* pgmScope = NULL;
  if (!args.structureFiles.empty()) {
    scopeTree = readStructure(args.structureFiles);
    pgmScope = scopeTree->GetRoot();
  }
  
  try { 
    Prof::Epoch* epoch = prof->epoch();
    for (Prof::Epoch::LMSet::iterator it = epoch->lm_begin();
	 it != epoch->lm_end(); ++it) {
      Prof::Epoch::LM* epoch_lm = *it;

      if (epoch_lm->isUsed()) { // FIXME
	const string& lm_fnm = epoch_lm->name();
	LoadModScope* lmScope = NULL;
	if (pgmScope) {
	  lmScope = pgmScope->FindLoadMod(lm_fnm);
	}
	
	if (lmScope) {
	  DIAG_Msg(1, "Using STRUCTURE for: " << lm_fnm);
	  processCallingCtxtTree(prof, epoch_lm, lmScope);
	}
	else {
	  DIAG_Msg(1, "Using debug info for: " << lm_fnm);
	  processCallingCtxtTree(prof, epoch_lm);
	}
      }
    }
    
    Analysis::CallPath::normalize(prof);
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
  std::pair<string, bool> ret = FileUtil::mkdirUnique(args.db_dir);
  args.db_dir = ret.first;  // // exits on failure...
    
  string dbSrcDir = args.db_dir + "/src";
  if (mkdir(dbSrcDir.c_str(),
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
    DIAG_Die("could not create database source code directory " << dbSrcDir);
  }
  
  Analysis::CallPath::copySourceFiles(prof, args.searchPaths, dbSrcDir);
  
  string experiment_fnm = args.db_dir + "/" + args.out_db_experiment;
  Analysis::CallPath::writeInDatabase(prof, experiment_fnm);
  //Analysis::CallPath::write(prof, std::cout, /* prettyPrint */ true);
  

  delete prof;
  return (0);
}

//****************************************************************************

static Prof::CSProfile* 
readProfileFile(const string& prof_fnm);


static Prof::CSProfile* 
readProfileData(std::vector<string>& profileFiles)
{
  Prof::CSProfile* prof = readProfileFile(profileFiles[0]);
  
  for (int i = 1; i < profileFiles.size(); ++i) {
    Prof::CSProfile* p = readProfileFile(profileFiles[i]);
    prof->merge(*p);
    delete p;
  }
  
  return prof;
}


static Prof::CSProfile* 
readProfileFile(const string& prof_fnm)
{
  Prof::CSProfile* prof = NULL;
  try {
    //prof = TheProfileReader.ReadProfileFile(args.profFnm /*type*/);
    prof = ReadProfile_CSPROF(prof_fnm.c_str());
  } 
  catch (...) {
    DIAG_EMsg("While reading profile '" << prof_fnm << "'...");
    throw;
  }
  return prof;
}


//****************************************************************************

  
class MyDocHandlerArgs : public DocHandlerArgs {
public:
  MyDocHandlerArgs()  { }
  ~MyDocHandlerArgs() { }
  
  virtual string ReplacePath(const char* oldpath) const { return oldpath; }
};


static PgmScopeTree*
readStructure(std::vector<string>& structureFiles)
{

  string searchPath = "."; // FIXME
  PgmScope* pgm = new PgmScope("");
  PgmScopeTree* structure = new PgmScopeTree("", pgm);
  NodeRetriever structIF(pgm, searchPath);
  MyDocHandlerArgs docargs; // FIXME

  Prof::Struct::readStructure(structIF, structureFiles, 
			      PGMDocHandler::Doc_STRUCT, docargs);
  
  return structure;
}

//****************************************************************************

static void
processCallingCtxtTree(Prof::CSProfile* prof, Prof::Epoch::LM* epoch_lm,
		       LoadModScope* lmScope)
{
  VMA relocVMA = 0;

  try {
    binutils::LM* lm = new binutils::LM();
    lm->open(epoch_lm->name().c_str());
    if (lm->type() != binutils::LM::TypeExe) {
      relocVMA = epoch_lm->loadAddr();
    }
    delete lm;
  }
  catch (...) {
    DIAG_EMsg("While reading '" << epoch_lm->name() << "'...");
    throw;
  }
  
  Analysis::CallPath::inferCallFrames(prof, epoch_lm, lmScope, relocVMA);
}


static void
processCallingCtxtTree(Prof::CSProfile* prof, Prof::Epoch::LM* epoch_lm)
{
  binutils::LM* lm = NULL;
  try {
    lm = new binutils::LM();
    lm->open(epoch_lm->name().c_str());
    lm->read(binutils::LM::ReadFlg_Proc);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << epoch_lm->name() << "'...");
    throw;
  }
  
  // get the start and end PC from the text sections 
  if (lm->type() != binutils::LM::TypeExe) {
    lm->relocate(epoch_lm->loadAddr());
  }
  
  Analysis::CallPath::inferCallFrames(prof, epoch_lm, lm);
  
  delete lm;
}

