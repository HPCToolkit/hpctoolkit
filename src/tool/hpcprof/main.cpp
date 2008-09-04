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

static Prof::CallPath::Profile* 
readProfileData(std::vector<string>& profileFiles);

static Prof::Struct::Tree*
readStructure(const Analysis::Args& args);

static void
dumpProfileData(std::ostream& os, std::vector<string>& profileFiles);

static void
processCallingCtxtTree(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm,
		       Prof::Struct::LM* lmStrct);

static void
processCallingCtxtTree(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm);


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
  Prof::CallPath::Profile* prof = readProfileData(args.profileFiles);

  // ------------------------------------------------------------
  // Add source file info
  // ------------------------------------------------------------

  Prof::Struct::Tree* structure = NULL;
  if (!args.structureFiles.empty()) {
    structure = readStructure(args);
  }
  
  try { 
    Prof::Epoch* epoch = prof->epoch();
    for (Prof::Epoch::LMSet::iterator it = epoch->lm_begin();
	 it != epoch->lm_end(); ++it) {
      Prof::Epoch::LM* epoch_lm = *it;

      if (epoch_lm->isUsed()) { // FIXME
	const string& lm_fnm = epoch_lm->name();
	Prof::Struct::LM* lmStrct = NULL;
	if (structure) {
	  Prof::Struct::Pgm* pgmStrct = structure->GetRoot();
	  lmStrct = pgmStrct->findLM(lm_fnm);
	}
	
	if (lmStrct) {
	  DIAG_Msg(1, "Using STRUCTURE for: " << lm_fnm);
	  processCallingCtxtTree(prof, epoch_lm, lmStrct);
	}
	else {
	  DIAG_Msg(1, "Using debug info for: " << lm_fnm);
	  processCallingCtxtTree(prof, epoch_lm);
	}
      }
    }
    
    Analysis::CallPath::normalize(prof, args.lush_agent);
  }
  catch (...) {
    DIAG_EMsg("While preparing CSPROFILE...");
    throw;
  }
  
  delete structure;

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

static Prof::CallPath::Profile* 
readProfileFile(const string& prof_fnm);


static Prof::CallPath::Profile* 
readProfileData(std::vector<string>& profileFiles)
{
  Prof::CallPath::Profile* prof = readProfileFile(profileFiles[0]);
  
  for (int i = 1; i < profileFiles.size(); ++i) {
    Prof::CallPath::Profile* p = readProfileFile(profileFiles[i]);
    prof->merge(*p);
    delete p;
  }
  
  return prof;
}


static Prof::CallPath::Profile* 
readProfileFile(const string& prof_fnm)
{
  Prof::CallPath::Profile* prof = NULL;
  try {
    prof = Prof::CallPath::Profile::make(prof_fnm.c_str());
  }
  catch (...) {
    DIAG_EMsg("While reading profile '" << prof_fnm << "'...");
    throw;
  }
  return prof;
}


//****************************************************************************

static string 
searchPathStr(const Analysis::Args& args)
{
  // cf. Driver::searchPathStr (Flat-SrcCorrelation.*)
  string path = ".";
  for (uint i = 0; i < args.searchPaths.size(); ++i) { 
    path += string(":") + args.searchPaths[i];
  }
  return path;
}


static Prof::Struct::Tree*
readStructure(const Analysis::Args& args)
{
  string searchPath = searchPathStr(args);

  Prof::Struct::Pgm* pgmStrct = new Prof::Struct::Pgm("");
  Prof::Struct::Tree* structure = new Prof::Struct::Tree("", pgmStrct);

  Prof::Struct::TreeInterface structIF(structure->GetRoot(), searchPath);
  DocHandlerArgs docargs; // NOTE: override for replacePath()

  Prof::Struct::readStructure(structIF, args.structureFiles, 
			      PGMDocHandler::Doc_STRUCT, docargs);
  return structure;
}

//****************************************************************************

static void
processCallingCtxtTree(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm,
		       Prof::Struct::LM* lmStrct)
{
  Analysis::CallPath::inferCallFrames(prof, epoch_lm, lmStrct);
}


static void
processCallingCtxtTree(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm)
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
  Analysis::CallPath::inferCallFrames(prof, epoch_lm, lm);
  delete lm;
}

