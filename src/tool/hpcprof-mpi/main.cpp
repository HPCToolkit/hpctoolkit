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

#include <vector>
using std::vector;

#include <cstdlib> // getenv()
#include <cmath>   // ceil()
#include <climits> // UCHAR_MAX
#include <cctype>  // isdigit()

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Args.hpp"
#include "ParallelAnalysis.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.hpp>


//*************************** Forward Declarations ***************************

static int
realmain(int argc, char* const* argv);

static Analysis::Util::NormalizeProfileArgs_t
myNormalizeProfileArgs(const Analysis::Util::StringVec& profileFiles,
		       vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int numRanks, int rootRank = 0);

static void
makeMetrics(const Analysis::Util::NormalizeProfileArgs_t& nArgs,
	    const vector<uint>& groupIdToGroupSizeMap,
	    Prof::CallPath::Profile& profGbl,
	    int myRank, int numRanks, int rootRank);

static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       uint& mDrvdBeg, uint& mDrvdEnd,
		       uint& mXDrvdBeg, uint& mXDrvdEnd,
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap);

static void
processProfile(Prof::CallPath::Profile& profGbl,
	       string& profileFile, uint groupId,
	       uint mDrvdBeg, uint mDrvdEnd);


//****************************************************************************

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


static int
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

  vector<uint> groupIdToGroupSizeMap; // only filled for rootRank

  Analysis::Util::NormalizeProfileArgs_t nArgs = 
    myNormalizeProfileArgs(args.profileFiles, groupIdToGroupSizeMap,
			   myRank, numRanks, rootRank);

  int mergeTy = Prof::CallPath::Profile::Merge_mergeMetricByName;
  uint rFlags = (Prof::CallPath::Profile::RFlg_virtualMetrics
		 | Prof::CallPath::Profile::RFlg_noMetricSfx);
  Analysis::Util::UIntVec* groupMap =
    (nArgs.groupMax > 1) ? nArgs.groupMap : NULL;

  profLcl = Analysis::CallPath::read(*nArgs.paths, groupMap, mergeTy, rFlags);

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

  makeMetrics(nArgs, groupIdToGroupSizeMap, *profGbl,
	      myRank, numRanks, rootRank);

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
myNormalizeProfileArgs(const Analysis::Util::StringVec& profileFiles,
		       vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int numRanks, int rootRank)
{
  Analysis::Util::NormalizeProfileArgs_t out;

  char* sendFilesBuf = NULL;
  uint sendFilesBufSz = 0;
  uint sendFilesChunkSz = 0;
  uint groupIdLen = 1;
  uint pathLenMax = 0;
  uint groupIdMax = 0;

  // -------------------------------------------------------
  // root creates canonical and grouped list of files
  // -------------------------------------------------------

  if (myRank == rootRank) {
    Analysis::Util::NormalizeProfileArgs_t nArgs = 
      Analysis::Util::normalizeProfileArgs(profileFiles);
    
    Analysis::Util::StringVec* canonicalFiles = nArgs.paths;
    pathLenMax = nArgs.pathLenMax;
    groupIdMax = nArgs.groupMax;

    DIAG_Assert(nArgs.groupMax <= UCHAR_MAX, "myNormalizeProfileArgs: 'groupMax' cannot be packed into a uchar!");

    uint chunkSz = (uint)
      ceil( (double)canonicalFiles->size() / (double)numRanks);
    
    sendFilesChunkSz = chunkSz * (groupIdLen + pathLenMax + 1);
    sendFilesBufSz = sendFilesChunkSz * numRanks;
    sendFilesBuf = new char[sendFilesBufSz];
    memset(sendFilesBuf, '\0', sendFilesBufSz);

    groupIdToGroupSizeMap.resize(groupIdMax + 1);
    
    for (uint i = 0, j = 0; i < canonicalFiles->size(); 
	 i++, j += (groupIdLen + pathLenMax + 1)) {
      const std::string& nm = (*canonicalFiles)[i];
      uint groupId = (*nArgs.groupMap)[i];

      groupIdToGroupSizeMap[groupId]++;

      // pack into sendFilesBuf
      sendFilesBuf[j] = (char)groupId;
      strncpy(&(sendFilesBuf[j + groupIdLen]), nm.c_str(), pathLenMax);
      sendFilesBuf[j + groupIdLen + pathLenMax] = '\0';
    }

    nArgs.destroy();
  }

  // -------------------------------------------------------
  // prepare parameters for scatter
  // -------------------------------------------------------
  
  const uint metadataBufSz = 3;
  uint metadataBuf[metadataBufSz];
  metadataBuf[0] = sendFilesChunkSz;
  metadataBuf[1] = pathLenMax;
  metadataBuf[2] = groupIdMax;

  MPI_Bcast((void*)metadataBuf, metadataBufSz, MPI_UNSIGNED, 
	    rootRank, MPI_COMM_WORLD);

  if (myRank != rootRank) {
    sendFilesChunkSz = metadataBuf[0];
    pathLenMax = metadataBuf[1];
    groupIdMax = metadataBuf[2];
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

  out.pathLenMax = pathLenMax;
  out.groupMax = groupIdMax;

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
makeMetrics(const Analysis::Util::NormalizeProfileArgs_t& nArgs,
	    const vector<uint>& groupIdToGroupSizeMap,
	    Prof::CallPath::Profile& profGbl,
	    int myRank, int numRanks, int rootRank)
{
  uint mDrvdBeg, mDrvdEnd;   // [ )
  uint mXDrvdBeg, mXDrvdEnd; // [ )
  vector<VMAIntervalSet*> groupIdToGroupMetricsMap(nArgs.groupMax + 1, NULL);
  makeDerivedMetricDescs(profGbl, mDrvdBeg, mDrvdEnd, mXDrvdBeg, mXDrvdEnd,
			 groupIdToGroupMetricsMap);

  Prof::Metric::Mgr& metricMgr = *profGbl.metricMgr();

  // -------------------------------------------------------
  // create local summary metrics (and thread-level metrics)
  // -------------------------------------------------------

  Prof::CCT::ANode* cctRoot = profGbl.cct()->root();
  cctRoot->computeMetricsItrv(metricMgr, mDrvdBeg, mDrvdEnd,
			      Prof::Metric::AExprItrv::FnInit, 0);

  Analysis::Util::UIntVec* groupMap =
    (nArgs.groupMax > 1) ? nArgs.groupMap : NULL;

  for (uint i = 0; i < nArgs.paths->size(); ++i) {
    string& fnm = (*nArgs.paths)[i];
    uint groupId = (groupMap) ? (*groupMap)[i] : 0;
    processProfile(profGbl, fnm, groupId, mDrvdBeg, mDrvdEnd);
  }

  // -------------------------------------------------------
  // create global summary metrics
  // -------------------------------------------------------

  // 1. Change definitions of derived metrics [mDrvdBeg, mDrvdEnd) to
  //    point to the local summary values that will be in [mXDrvdBeg,
  //    mXDrvdEnd) durring the reduction
  for (uint i = mDrvdBeg, j = mXDrvdBeg; i < mDrvdEnd; ++i, ++j) {
    Prof::Metric::ADesc* m = metricMgr.metric(i);
    Prof::Metric::DerivedItrvDesc* mm =
      dynamic_cast<Prof::Metric::DerivedItrvDesc*>(m);
    DIAG_Assert(mm, DIAG_UnexpectedInput);

    Prof::Metric::AExprItrv* expr = mm->expr();
    if (expr) {
      expr->srcId(j);
    }
  }

  // 2. Initialize extra derived metric storage [mXDrvdBeg, mXDrvdEnd)
  //    since it will serve as an input during the summary metrics
  //    reduction
  cctRoot->computeMetricsItrv(metricMgr, mXDrvdBeg, mXDrvdEnd,
			      Prof::Metric::AExprItrv::FnInitSrc, 0);

  // 3. Reduction
  uint maxCCTId = profGbl.cct()->maxDenseId();

  ParallelAnalysis::PackedMetrics* packedMetrics =
    new ParallelAnalysis::PackedMetrics(maxCCTId + 1, mXDrvdBeg, mXDrvdEnd,
					mDrvdBeg, mDrvdEnd);

  // Post-INVARIANT: rank 0's 'profGbl' contains summary metrics
  ParallelAnalysis::reduce(std::make_pair(&profGbl, packedMetrics),
			   myRank, numRanks - 1);

  // 4. Finalize metrics
  if (myRank == rootRank) {
    for (uint grpId = 1; grpId < groupIdToGroupMetricsMap.size(); ++grpId) {
      const VMAIntervalSet* ivalset = groupIdToGroupMetricsMap[grpId];

      // degenerate case: group has no metrics and therefore no derived metrics
      if (!ivalset) { continue; }

      DIAG_Assert(ivalset->size() == 1, DIAG_UnexpectedInput);
      
      const VMAInterval& ival = *(ivalset->begin());
      uint mBeg = (uint)ival.beg(), mEnd = (uint)ival.end();
      
      uint numInputs = groupIdToGroupSizeMap[grpId];
      
      cctRoot->computeMetricsItrv(metricMgr, mBeg, mEnd,
				  Prof::Metric::AExprItrv::FnFini, numInputs);
    }

    for (uint i = 0; i < metricMgr.size(); ++i) {
      Prof::Metric::ADesc* m = metricMgr.metric(i);
      m->isComputed(true);
    }
  }

  for (uint grpId = 1; grpId < groupIdToGroupMetricsMap.size(); ++grpId) {
    delete groupIdToGroupMetricsMap[grpId];
  }

  delete packedMetrics;
}


static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       uint& mDrvdBeg, uint& mDrvdEnd,
		       uint& mXDrvdBeg, uint& mXDrvdEnd,
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap)
{
  Prof::Metric::Mgr& mMgrGbl = *(profGbl.metricMgr());

  uint numDrvd = 0;
  mDrvdBeg = Prof::Metric::Mgr::npos; // [ )
  mDrvdEnd = Prof::Metric::Mgr::npos;

  mXDrvdBeg = Prof::Metric::Mgr::npos; // [ )
  mXDrvdEnd = Prof::Metric::Mgr::npos;

  uint numSrc = mMgrGbl.size();

  // create derived metrics
  if (numSrc > 0) {
    uint mSrcBeg = 0;
    uint mSrcEnd = numSrc;

    // -------------------------------------------------------
    // official set of derived metrics
    // -------------------------------------------------------
    mDrvdBeg = mMgrGbl.makeSummaryMetricsItrv(mSrcBeg, mSrcEnd);
    if (mDrvdBeg != Prof::Metric::Mgr::npos) {
      mDrvdEnd = mMgrGbl.size();
      numDrvd = (mDrvdEnd - mDrvdBeg);
    }

    for (uint i = mSrcBeg; i < mSrcEnd; ++i) {
      Prof::Metric::ADesc* m = mMgrGbl.metric(i);
      m->isVisible(false);
    }

    for (uint i = mDrvdBeg; i < mDrvdEnd; ++i) {
      Prof::Metric::ADesc* m = mMgrGbl.metric(i);

      // FIXME: clutzy, but after the CCT redution, metric prefixes
      // have been collapsed into the name.
      string nm = m->name();

      // extract group id
      size_t endGroupId = nm.find_first_of('.');
      DIAG_Assert(endGroupId != string::npos, DIAG_UnexpectedInput);

      size_t begGroupId = endGroupId;
      while (begGroupId > 0 && isdigit(nm[begGroupId - 1]) ) {
	begGroupId--;
      }
      DIAG_Assert(begGroupId < endGroupId, DIAG_UnexpectedInput);

      string grpStr = nm.substr(begGroupId, (endGroupId - begGroupId));
      uint groupId = (uint)StrUtil::toUInt64(grpStr);

      DIAG_Assert(groupId > 0, DIAG_UnexpectedInput);
      DIAG_Assert(groupId < groupIdToGroupMetricsMap.size(), DIAG_UnexpectedInput);

      VMAIntervalSet*& ivalset = groupIdToGroupMetricsMap[groupId];
      if (!ivalset) {
	ivalset = new VMAIntervalSet;
      }
      ivalset->insert(i, i + 1); // [ )
    }

    // -------------------------------------------------------
    // temporary set of extra derived metrics (for reduction)
    // -------------------------------------------------------
    mXDrvdBeg = mMgrGbl.makeSummaryMetricsItrv(mSrcBeg, mSrcEnd);
    if (mXDrvdBeg != Prof::Metric::Mgr::npos) {
      mXDrvdEnd = mMgrGbl.size();
    }
    
    for (uint i = mXDrvdBeg; i < mXDrvdEnd; ++i) {
      Prof::Metric::ADesc* m = mMgrGbl.metric(i);
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
  cctGbl->root()->aggregateMetrics(mBeg, mEnd);

  if (0) {
    profGbl.cct()->writeXML(std::cerr, mBeg, mEnd);
  }

  cctGbl->root()->computeMetricsItrv(*mMgrGbl, mDrvdBeg, mDrvdEnd,
				     Prof::Metric::AExprItrv::FnUpdate, 0);

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

