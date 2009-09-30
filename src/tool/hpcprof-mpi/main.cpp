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

static Analysis::Util::NormalizeProfileArgs_t
myNormalizeProfileArgs(StringVec& profileFiles,
		       int myRank, int numRanks, int rootRank = 0);

static void
makeMetrics(Analysis::Util::NormalizeProfileArgs_t& nArgs,
	    Prof::CallPath::Profile& profGbl,
	    int myRank, int numRanks, int rootRank);

static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       uint& mDrvdBeg, uint& mDrvdEnd,
		       uint& mXDrvdBeg, uint& mXDrvdEnd);

static void
processProfile(Prof::CallPath::Profile& profGbl,
	       string& profileFile, uint groupId,
	       uint mDrvdBeg, uint mDrvdEnd);

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

  // Post-INVARIANT: rank 0's 'profLcl' is the canonical CCT.  Metrics
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

  profGbl->cct()->makeDensePreorderIds();

  // -------------------------------------------------------
  // Create summary metrics and thread-level metrics
  // -------------------------------------------------------

  makeMetrics(nArgs, *profGbl, myRank, numRanks, rootRank);

  nArgs.destroy();

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
static Analysis::Util::NormalizeProfileArgs_t
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

static void
makeMetrics(Analysis::Util::NormalizeProfileArgs_t& nArgs,
	    Prof::CallPath::Profile& profGbl,
	    int myRank, int numRanks, int rootRank)
{
  uint mDrvdBeg, mDrvdEnd;   // [ )
  uint mXDrvdBeg, mXDrvdEnd; // [ )
  makeDerivedMetricDescs(profGbl, mDrvdBeg, mDrvdEnd, mXDrvdBeg, mXDrvdEnd);

  Prof::Metric::Mgr& metricMgr = *profGbl.metricMgr();

  // -------------------------------------------------------
  // create local summary metrics (and thread-level metrics)
  // -------------------------------------------------------

  Prof::CCT::ANode* cctRoot = profGbl.cct()->root();
  cctRoot->computeMetricsItrv(metricMgr, mDrvdBeg, mDrvdEnd,
			      Prof::Metric::AExprItrv::FnInit, 0);

  for (uint i = 0; i < nArgs.paths->size(); ++i) {
    string& fnm = (*nArgs.paths)[i];
    uint groupId = (*nArgs.groupMap)[i];
    processProfile(profGbl, fnm, groupId, mDrvdBeg, mDrvdEnd);
  }

  // -------------------------------------------------------
  // create global summary metrics
  // -------------------------------------------------------

  // 1. change definitions of derived metrics [mDrvdBeg, mDrvdEnd) to
  //    point to the local summary values that will be in [mXDrvdBeg,
  //    mXDrvdEnd) durring the reduction
  for (uint i = mDrvdBeg, j = mXDrvdBeg; i < mDrvdEnd; ++i, ++j) {
    Prof::Metric::ADesc* m = metricMgr.metric(i);
    Prof::Metric::DerivedItrvDesc* mm =
      dynamic_cast<Prof::Metric::DerivedItrvDesc*>(m);
    DIAG_Assert(mm, DIAG_UnexpectedInput);

    Prof::Metric::AExprItrv* expr = mm->expr();
    expr->srcId(j);
  }

  uint maxCCTId = profGbl.cct()->maxDenseId();

  ParallelAnalysis::PackedMetrics* packedMetrics =
    new ParallelAnalysis::PackedMetrics(maxCCTId + 1, mXDrvdBeg, mXDrvdEnd,
					mDrvdBeg, mDrvdEnd);
  uint numUpdatesLcl = nArgs.paths->size();

  // 2. Reduction: Post-INVARIANT: rank 0's 'profGbl' contains summary metrics
  ParallelAnalysis::reduce(std::make_pair(&profGbl, packedMetrics),
			   myRank, numRanks - 1);
  delete packedMetrics;

  // 3. Finalize metrics
  if (myRank == rootRank) {
    uint numUpdates = numUpdatesLcl; // TODO: reduction
    cctRoot->computeMetricsItrv(metricMgr, mDrvdBeg, mDrvdEnd,
				Prof::Metric::AExprItrv::FnFini, numUpdates);
  }
}


static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       uint& mDrvdBeg, uint& mDrvdEnd,
		       uint& mXDrvdBeg, uint& mXDrvdEnd)
{
  uint numDrvd = 0;
  mDrvdBeg = Prof::Metric::Mgr::npos; // [ )
  mDrvdEnd = Prof::Metric::Mgr::npos;

  mXDrvdBeg = Prof::Metric::Mgr::npos; // [ )
  mXDrvdEnd = Prof::Metric::Mgr::npos;

  uint numSrc = profGbl.metricMgr()->size();

  // create derived metrics
  if (numSrc > 0) {
    uint mSrcBeg = 0;
    uint mSrcEnd = numSrc;

    // official set of derived metrics
    mDrvdBeg = profGbl.metricMgr()->makeItrvSummaryMetrics(mSrcBeg, mSrcEnd);
    if (mDrvdBeg != Prof::Metric::Mgr::npos) {
      mDrvdEnd = profGbl.metricMgr()->size();
      numDrvd = (mDrvdEnd - mDrvdBeg);
    }

    // temporary set of extra derived metrics (for reduction)
    mXDrvdBeg = profGbl.metricMgr()->makeItrvSummaryMetrics(mSrcBeg, mSrcEnd);
    if (mXDrvdBeg != Prof::Metric::Mgr::npos) {
      mXDrvdEnd = profGbl.metricMgr()->size();
    }
    
    for (uint i = mXDrvdBeg; i < mXDrvdEnd; ++i) {
      Prof::Metric::ADesc* m = profGbl.metricMgr()->metric(i);
      m->isVisible(false);
    }
  }

  profGbl.isMetricMgrVirtual(false);

  return numDrvd;
}


static void
processProfile(Prof::CallPath::Profile& profGbl,
	       string& profileFile, uint groupId,
	       uint mDrvdBeg, uint mDrvdEnd)
{
  Prof::Metric::Mgr* mMgrGbl = profGbl.metricMgr();
  Prof::CCT::Tree* cctGbl = profGbl.cct();

  // -------------------------------------------------------
  // read profile file
  // -------------------------------------------------------
  uint rFlags = Prof::CallPath::Profile::RFlg_noMetricSfx;
  Prof::CallPath::Profile* prof = 
    Analysis::CallPath::read(profileFile, groupId, rFlags);

  // -------------------------------------------------------
  // merge into canonical CCT
  // -------------------------------------------------------
  uint mergeTy = Prof::CallPath::Profile::Merge_mergeMetricByName;

  // Add some structure information to leaves so that 'prof' can be
  // merged with the structured canonical CCT.
  //
  // (Background: When CCT::Stmts are merged in
  // Analysis::CallPath::coalesceStmts(), IP/LIP information is not
  // retained.  This means that many leaves in 'prof' will not
  // correctly find their corresponding node in profGbl.
  prof->structure(profGbl.structure());
  Analysis::CallPath::noteStaticStructureOnLeaves(*prof);
  prof->structure(NULL);

  uint mBeg = profGbl.merge(*prof, mergeTy);    // [closed begin
  uint mEnd = mBeg + prof->metricMgr()->size(); //  open end)

  // -------------------------------------------------------
  // compute local metrics and update local derived metrics
  // -------------------------------------------------------
  cctGbl->root()->accumulateMetrics(mBeg, mEnd);


  cctGbl->root()->computeMetricsItrv(*mMgrGbl, mDrvdBeg, mDrvdEnd,
				     Prof::Metric::AExprItrv::FnUpdate, 1);

  // -------------------------------------------------------
  // TODO: write local values to disk
  // -------------------------------------------------------

  // -------------------------------------------------------
  // reinitialize metric values since space may be used again
  // -------------------------------------------------------
  cctGbl->root()->zeroMetricsDeep(mBeg, mEnd);
  
  delete prof;
}

//***************************************************************************

