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

StringVec* 
getMyProfileFiles(StringVec& profileFiles, 
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

  StringVec* profFiles = 
    getMyProfileFiles(args.profileFiles, myRank, numRanks,rootRank);

  uint rFlags = Prof::CallPath::Profile::RFlg_onlyMetricDescs;
  profLcl = Analysis::CallPath::read(*profFiles, rFlags);

  // Obtain the metric manager (warning: replaces the manager in profLcl)
  Prof::Metric::Mgr* metricMgr = profLcl->metricMgr();
  profLcl->metricMgr(new Prof::Metric::Mgr);


  FILE* fs = hpcio_fopen_w("canonical-cct.hpcrun", 1);
  Prof::CallPath::Profile::fmt_fwrite(*profLcl, fs, 0);
  hpcio_fclose(fs);

  // ------------------------------------------------------------
  // Create canonical CCT (no metrics)
  // ------------------------------------------------------------
  Prof::CallPath::Profile* profGbl = NULL;

  // POST-INVARIANT: rank 0's 'profLcl' the canonical CCT
  ParallelAnalysis::reduce(profLcl, myRank, numRanks - 1);
  if (myRank == rootRank) {
    profGbl = profLcl;
    profLcl = NULL;
  }

  // POST-INVARIANT: 'profGbl' is the canonical CCT
  ParallelAnalysis::broadcast(profGbl, myRank, numRanks - 1);
  delete profLcl;

  // ------------------------------------------------------------
  // Add static structure to canonical CCT; form dense node ids
  // ------------------------------------------------------------

  Prof::Struct::Tree* structure = new Prof::Struct::Tree("");
  if (!args.structureFiles.empty()) {
    Analysis::CallPath::readStructure(structure, args);
  }
  profGbl->structure(structure);

  Analysis::CallPath::overlayStaticStructureMain(*profGbl, args.lush_agent);

  // TODO: renumber nodes densly

  // ------------------------------------------------------------
  // Create summary and thread-level metrics
  // ------------------------------------------------------------

  delete profFiles;
  delete metricMgr;
  delete profGbl;

  // -------------------------------------------------------
  // MPI_Finalize() called in parent
  // -------------------------------------------------------
  
  return 0;
}


//****************************************************************************

// getMyProfileFiles: creates canonical list of profiles files and
//   distributes chunks of size ceil(numFiles / numRanks) to each process.
//   The last process may have a smaller chunk than the others.
StringVec* 
getMyProfileFiles(StringVec& profileFiles, 
		  int myRank, int numRanks, int rootRank)
{
  char* sendFilesBuf = NULL;
  uint sendFilesBufSz = 0;
  uint sendFilesChunkSz = 0;
  uint pathLenMax = 0;
  
  // -------------------------------------------------------
  // root creates canonical and grouped list of files
  // -------------------------------------------------------

  if (myRank == rootRank) {
    std::pair<StringVec*, uint> pair = 
      Analysis::Util::normalizeProfileArgs(profileFiles);
    
    StringVec* canonicalFiles = pair.first;
    pathLenMax = pair.second;

    uint chunkSz = (uint)
      ceil( (double)canonicalFiles->size() / (double)numRanks);
    
    sendFilesChunkSz = chunkSz * (pathLenMax+1);
    sendFilesBufSz = sendFilesChunkSz * numRanks;
    sendFilesBuf = new char[sendFilesBufSz];
    memset(sendFilesBuf, '\0', sendFilesBufSz);

    for (uint i = 0, j = 0; i < canonicalFiles->size(); 
	 i++, j += (pathLenMax+1)) {
      const std::string& nm = (*canonicalFiles)[i];
      strncpy(&sendFilesBuf[j], nm.c_str(), pathLenMax);
      sendFilesBuf[j + pathLenMax] = '\0';
    }

    delete canonicalFiles;
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

  
  StringVec* myProfileFiles = new StringVec;
  for (uint i = 0; i < recvFilesChunkSz; i += (pathLenMax+1)) {
    string nm = &recvFilesBuf[i];
    if (!nm.empty()) {
      myProfileFiles->push_back(nm);
    }
  }

  delete[] recvFilesBuf;

  if (0) {
    for (uint i = 0; i < myProfileFiles->size(); ++i) {
      const std::string& nm = (*myProfileFiles)[i];
      std::cout << "[" << myRank << "]: " << nm << std::endl;
    }
  }

  return myProfileFiles;
}

//***************************************************************************
