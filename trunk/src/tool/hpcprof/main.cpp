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
#include <lib/analysis/Util.hpp>

#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/PGMReader.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/realpath.h>

//*************************** Forward Declarations ***************************

static Prof::CallPath::Profile* 
readProfileData(std::vector<string>& profileFiles);

static void
readStructure(Prof::Struct::Tree* structure, const Analysis::Args& args);

static void
overlayStaticStructure(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm,
		       Prof::Struct::LM* lmStrct);

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
  RealPathMgr::singleton().searchPaths(args.searchPathStr());

  // ------------------------------------------------------------
  // Read profile data
  // ------------------------------------------------------------
  Prof::CallPath::Profile* prof = readProfileData(args.profileFiles);

  // ------------------------------------------------------------
  // Seed structure information
  // ------------------------------------------------------------
  Prof::Struct::Tree* structure = new Prof::Struct::Tree("");
  if (!args.structureFiles.empty()) {
    readStructure(structure, args);
  }
  prof->structure(structure);

  // ------------------------------------------------------------
  // Add static structure to dynamic call paths
  // ------------------------------------------------------------
  try { 
    Prof::Epoch* epoch = prof->epoch();
    for (uint i = 0; i < epoch->lm_size(); ++i) {
      // NOTE(tallent): This method will not iterate over all load modules if
      // two map to the same load address (such as may happen in a
      // scalability study)...
      // Prof::Epoch::LMSet::iterator it = epoch->lm_begin(); it != epoch->lm_end(); ++it
      Prof::Epoch::LM* epoch_lm = epoch->lm(i); // *it;

      // tallent (FIXME): See note on Epoch::LM::isUsed()
      // FIXME: emit warning if not available, but process anyway
      if (epoch_lm->isAvail() && epoch_lm->isUsed()) {
	const string& lm_nm = epoch_lm->name();

	Prof::Struct::Tree* structure = prof->structure();
	Prof::Struct::Root* rootStrct = structure->root();
	Prof::Struct::LM* lmStrct = Prof::Struct::LM::demand(rootStrct, lm_nm);
	
	overlayStaticStructure(prof, epoch_lm, lmStrct);
      }
    }
    
    Analysis::CallPath::normalize(prof, args.lush_agent);
  }
  catch (...) {
    DIAG_EMsg("While preparing hpc-experiment...");
    throw;
  }
  
  // ------------------------------------------------------------
  // Generate Experiment database
  // ------------------------------------------------------------

  // prepare output directory (N.B.: chooses a unique name!)
  string db_dir = args.db_dir; // make copy
  std::pair<string, bool> ret = FileUtil::mkdirUnique(db_dir);
  db_dir = RealPath(ret.first.c_str());  // exits on failure...
    
  DIAG_Msg(1, "Copying source files reached by PATH option to " << db_dir);
  // NOTE: makes file names in prof relative to database
  Analysis::Util::copySourceFiles(prof, args.searchPathTpls, db_dir);

  string experiment_fnm = db_dir + "/" + args.out_db_experiment;
  std::ostream* os = IOUtil::OpenOStream(experiment_fnm.c_str());
  bool prettyPrint = (Diagnostics_GetDiagnosticFilterLevel() >= 5);
  Analysis::CallPath::write(prof, *os, prettyPrint);
  IOUtil::CloseStream(os);

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


static void
readStructure(Prof::Struct::Tree* structure, const Analysis::Args& args)
{
  DocHandlerArgs docargs; // NOTE: override for replacePath()

  Prof::Struct::readStructure(*structure, args.structureFiles,
			      PGMDocHandler::Doc_STRUCT, docargs);
}

//****************************************************************************

static void
overlayStaticStructure(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm,
		       Prof::Struct::LM* lmStrct)
{
  const string& lm_nm = epoch_lm->name();
  binutils::LM* lm = NULL;

  bool useStruct = (lmStrct->ChildCount() > 0);

  if (useStruct) {
    DIAG_Msg(1, "STRUCTURE: " << lm_nm);
  }
  else {
    DIAG_Msg(1, "Line map : " << lm_nm);

    try {
      lm = new binutils::LM();
      lm->open(lm_nm.c_str());
      lm->read(binutils::LM::ReadFlg_Proc);
    }
    catch (...) {
      DIAG_EMsg("While reading '" << lm_nm << "'...");
      throw;
    }
  }

  Analysis::CallPath::overlayStaticStructure(prof, epoch_lm, lmStrct, lm);

  delete lm;
}
