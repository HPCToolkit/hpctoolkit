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

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/IOUtil.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/realpath.h>


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
  Prof::CallPath::Profile* prof = Analysis::CallPath::read(args.profileFiles);

  // ------------------------------------------------------------
  // Seed structure information
  // ------------------------------------------------------------
  Prof::Struct::Tree* structure = new Prof::Struct::Tree("");
  if (!args.structureFiles.empty()) {
    Analysis::CallPath::readStructure(structure, args);
  }
  prof->structure(structure);

  // ------------------------------------------------------------
  // Add static structure to dynamic call paths
  // ------------------------------------------------------------

  try { 
    Prof::LoadMapMgr* loadmap = prof->loadMapMgr();
    Prof::Struct::Tree* structure = prof->structure();
    Prof::Struct::Root* rootStrct = structure->root();

    for (Prof::LoadMapMgr::LMSet_nm::iterator it = loadmap->lm_begin_nm();
	 it != loadmap->lm_end_nm(); ++it) {
      Prof::ALoadMap::LM* loadmap_lm = *it;

      // tallent:TODO: The call to LoadMap::compute_relocAmt() in
      // Profile::hpcrun_fmt_epoch_fread has emitted a warning if a
      // load module is unavailable.  Probably should be here...
      if (loadmap_lm->isAvail() && loadmap_lm->isUsed()) {
	const string& lm_nm = loadmap_lm->name();

	Prof::Struct::LM* lmStrct = Prof::Struct::LM::demand(rootStrct, lm_nm);
	Analysis::CallPath::overlayStaticStructureMain(prof, loadmap_lm, 
						       lmStrct);
      }
    }
    
    Analysis::CallPath::normalize(prof, args.lush_agent);

    // Note: Use StructMetricIdFlg to flag that static structure is used
    rootStrct->accumulateMetrics(Prof::CallPath::Profile::StructMetricIdFlg);
    rootStrct->pruneByMetrics();
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
  // NOTE: makes file names in structure relative to database
  Analysis::Util::copySourceFiles(prof->structure()->root(), 
				  args.searchPathTpls, db_dir);

  string experiment_fnm = db_dir + "/" + args.out_db_experiment;
  std::ostream* os = IOUtil::OpenOStream(experiment_fnm.c_str());
  bool prettyPrint = (Diagnostics_GetDiagnosticFilterLevel() >= 5);
  Analysis::CallPath::write(prof, *os, args.title, prettyPrint);
  IOUtil::CloseStream(os);

  delete prof;
  return (0);
}

//****************************************************************************

