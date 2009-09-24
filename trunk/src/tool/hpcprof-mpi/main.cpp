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

//**************************** MPI Include Files ****************************

#include <mpi.h>

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
using std::string;

#include <cstdlib> // getenv()
#include <cmath>   // ceil()
#include <climits> // UCHAR_MAX

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Args.hpp"
#include "ParallelAnalysis.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.hpp>


//*************************** Forward Declarations ***************************

typedef std::vector<std::string> StringVec;

Analysis::Util::NormalizeProfileArgs_t
myNormalizeProfileArgs(StringVec& profileFiles, 
		       int myRank, int numRanks, int rootRank = 0);


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
    ret = 1;
    goto exit;
  }
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    ret = 1;
    goto exit;
  }
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    ret = 1;
    goto exit;
  }
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    ret = 2;
    goto exit;
  }

 exit:
  MPI_Finalize();
  return ret;
}


int
realmain(int argc, char* const* argv) 
{
  Args args;
  args.parse(argc, argv); // may call exit()

  RealPathMgr::singleton().searchPaths(args.searchPathStr());

  // -------------------------------------------------------
  // MPI initialize
  // -------------------------------------------------------
  MPI_Init(&argc, (char***)&argv);

  const int rootRank = 0;
  int myRank, numRanks;
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank); 
  MPI_Comm_size(MPI_COMM_WORLD, &numRanks);

  // -------------------------------------------------------
  // Debugging hook
  // -------------------------------------------------------
  const char* HPCPROF_WAIT = getenv("HPCPROF_WAIT");
  if (HPCPROF_WAIT) {
    int waitRank = rootRank;
    if (strlen(HPCPROF_WAIT) > 0) {
      waitRank = atoi(HPCPROF_WAIT);
    }

    volatile int DEBUGGER_WAIT = 1;
    if (myRank == waitRank) {
      while (DEBUGGER_WAIT);
    }
  }

  // -------------------------------------------------------
  // Form local CCT from my set of profile files
  // -------------------------------------------------------
  Prof::CallPath::Profile* profLcl = NULL;

  Analysis::Util::NormalizeProfileArgs_t nArgs = 
    myNormalizeProfileArgs(args.profileFiles, myRank, numRanks, rootRank);

  int mergeTy = Prof::CallPath::Profile::Merge_mergeMetricByName;
  uint rFlags = (Prof::CallPath::Profile::RFlg_virtualMetrics 
		 | Prof::CallPath::Profile::RFlg_noMetricSfx);
  profLcl = Analysis::CallPath::read(*nArgs.paths, nArgs.groupMap,
				     mergeTy, rFlags);

  // -------------------------------------------------------
  // Create canonical CCT (metrics merged by <group>.<name>.*)
  // -------------------------------------------------------
  Prof::CallPath::Profile* profGbl = NULL;

  // Post-INVARIANT: rank 0's 'profLcl' the canonical CCT.  Metrics
  // are merged (and sorted by always merging left-child before right)
  ParallelAnalysis::reduce(profLcl, myRank, numRanks - 1);

  if (myRank == rootRank) {
    profGbl = profLcl;
    profLcl = NULL;
  }
  delete profLcl;

  // Post-INVARIANT: 'profGbl' is the canonical CCT
  ParallelAnalysis::broadcast(profGbl, myRank, numRanks - 1);
  
  if (0 && myRank == rootRank) {
    FILE* fs = hpcio_fopen_w("canonical-cct.hpcrun", 1);
    Prof::CallPath::Profile::fmt_fwrite(*profGbl, fs, 0);
    hpcio_fclose(fs);
  }

  // -------------------------------------------------------
  // Add static structure to canonical CCT; form dense node ids
  //
  // Post-INVARIANT: each process has canonical CCT with dense node
  // ids; corresponding nodes have idential ids.
  // -------------------------------------------------------

  Prof::Struct::Tree* structure = new Prof::Struct::Tree("");
  if (!args.structureFiles.empty()) {
    Analysis::CallPath::readStructure(structure, args);
  }
  profGbl->structure(structure);

  // FIXME: iterator should sort by lm/ip so static structure is added
  // in same way
  Analysis::CallPath::overlayStaticStructureMain(*profGbl, args.lush_agent);

  profGbl->cct()->renumberIdsDensly();

  // -------------------------------------------------------
  // Create summary and thread-level metrics
  // -------------------------------------------------------

  //Prof::Metric::Mgr* mrgMetricMgr = profGbl->metricMgr();
  //Prof::Metric::Mgr* sumMetricMgr = new Prof::Metric::Mgr;

  // TODO: create summary metrics descriptors

  //profGbl->metricMgr(sumMetricMgr);
  profGbl->isMetricMgrVirtual(false);

  for (uint i = 0; i < nArgs.paths->size(); ++i) {
    string& fnm = (*nArgs.paths)[i];
    uint groupId = (*nArgs.groupMap)[i];
    uint rFlags = Prof::CallPath::Profile::RFlg_noMetricSfx;

    Prof::CallPath::Profile* prof =
      Analysis::CallPath::read(fnm, groupId, rFlags);
    profGbl->merge(*prof, Prof::CallPath::Profile::Merge_mergeMetricByName);

    // TODO: create local metrics and incrementally update summary metrics

    delete prof;
  }

  nArgs.destroy();
  //delete mrgMetricMgr;

  // ------------------------------------------------------------
  // Generate Experiment database
  // ------------------------------------------------------------

  if (myRank == rootRank) {
    Analysis::CallPath::makeDatabase(*profGbl, args);
  }

  // -------------------------------------------------------
  // Cleanup: MPI_Finalize() called in parent
  // -------------------------------------------------------

  delete profGbl;

  return 0;
}


//****************************************************************************

// myNormalizeProfileArgs: creates canonical list of profiles files and
//   distributes chunks of size ceil(numFiles / numRanks) to each process.
//   The last process may have a smaller chunk than the others.
Analysis::Util::NormalizeProfileArgs_t
myNormalizeProfileArgs(StringVec& profileFiles,
		       int myRank, int numRanks, int rootRank)
{
  Analysis::Util::NormalizeProfileArgs_t out;

  char* sendFilesBuf = NULL;
  uint sendFilesBufSz = 0;
  uint sendFilesChunkSz = 0;
  uint groupIdLen = 1;
  uint pathLenMax = 0;
  
  // -------------------------------------------------------
  // root creates canonical and grouped list of files
  // -------------------------------------------------------

  if (myRank == rootRank) {
    Analysis::Util::NormalizeProfileArgs_t nArgs = 
      Analysis::Util::normalizeProfileArgs(profileFiles);
    
    StringVec* canonicalFiles = nArgs.paths;
    pathLenMax = nArgs.pathLenMax;

    DIAG_Assert(nArgs.groupMax <= UCHAR_MAX, "myNormalizeProfileArgs: 'groupMax' cannot be packed into a uchar!");

    uint chunkSz = (uint)
      ceil( (double)canonicalFiles->size() / (double)numRanks);
    
    sendFilesChunkSz = chunkSz * (groupIdLen + pathLenMax + 1);
    sendFilesBufSz = sendFilesChunkSz * numRanks;
    sendFilesBuf = new char[sendFilesBufSz];
    memset(sendFilesBuf, '\0', sendFilesBufSz);

    for (uint i = 0, j = 0; i < canonicalFiles->size(); 
	 i++, j += (groupIdLen + pathLenMax + 1)) {
      const std::string& nm = (*canonicalFiles)[i];
      uint groupId = (*nArgs.groupMap)[i];

      sendFilesBuf[j] = (char)groupId;
      strncpy(&(sendFilesBuf[j + groupIdLen]), nm.c_str(), pathLenMax);
      sendFilesBuf[j + groupIdLen + pathLenMax] = '\0';
    }

    nArgs.destroy();
  }

  // -------------------------------------------------------
  // prepare parameters for scatter
  // -------------------------------------------------------
  
  // TODO: broadcast groups
  const uint metadataBufSz = 2;
  uint metadataBuf[metadataBufSz];
  metadataBuf[0] = sendFilesChunkSz;
  metadataBuf[1] = pathLenMax;

  MPI_Bcast((void*)metadataBuf, metadataBufSz, MPI_UNSIGNED, 
	    rootRank, MPI_COMM_WORLD);

  if (myRank != rootRank) {
    sendFilesChunkSz = metadataBuf[0];
    pathLenMax = metadataBuf[1];
  }

  // -------------------------------------------------------
  // evenly distribute profile files across all processes
  // -------------------------------------------------------

  uint recvFilesChunkSz = sendFilesChunkSz;
  const char* recvFilesBuf = new char[recvFilesChunkSz];
  
  MPI_Scatter((void*)sendFilesBuf, sendFilesChunkSz, MPI_CHAR,
	      (void*)recvFilesBuf, recvFilesChunkSz, MPI_CHAR,
	      rootRank, MPI_COMM_WORLD);
  
  delete[] sendFilesBuf;


  for (uint i = 0; i < recvFilesChunkSz; i += (groupIdLen + pathLenMax + 1)) {
    uint groupId = recvFilesBuf[i];
    string nm = &recvFilesBuf[i + groupIdLen];
    if (!nm.empty()) {
      out.paths->push_back(nm);
      out.groupMap->push_back(groupId);
    }
  }

  delete[] recvFilesBuf;

  if (0) {
    for (uint i = 0; i < out.paths->size(); ++i) {
      const std::string& nm = (*out.paths)[i];
      std::cout << "[" << myRank << "]: " << nm << std::endl;
    }
  }

  return out;
}

//***************************************************************************
