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
// Copyright ((c)) 2002-2010, Rice University 
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
#include <typeinfo>

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

#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/RealPathMgr.hpp>


//*************************** Forward Declarations ***************************

static int
realmain(int argc, char* const* argv);

static Analysis::Util::NormalizeProfileArgs_t
myNormalizeProfileArgs(const Analysis::Util::StringVec& profileFiles,
		       vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int numRanks, int rootRank = 0);

static void
writeProfile(Prof::CallPath::Profile& prof, const char* baseNm, int myRank);

static void
makeMetrics(const Analysis::Args& args,
	    const Analysis::Util::NormalizeProfileArgs_t& nArgs,
	    const vector<uint>& groupIdToGroupSizeMap,
	    Prof::CallPath::Profile& profGbl,
	    int myRank, int numRanks, int rootRank);

static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       uint& mDrvdBeg, uint& mDrvdEnd,
		       uint& mXDrvdBeg, uint& mXDrvdEnd,
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
		       const vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int rootRank);

static void
processProfile(Prof::CallPath::Profile& profGbl,
	       const Analysis::Args& args,
	       string& profileFile, uint groupId, uint groupMax,
	       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
	       int myRank);

static void
writeMetrics(Prof::CallPath::Profile& profGbl, uint mBegId, uint mEndId,
	     const Analysis::Args& args, uint groupId, string& profileFile);

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

  vector<uint> groupIdToGroupSizeMap; // only initialized for rootRank

  Analysis::Util::NormalizeProfileArgs_t nArgs = 
    myNormalizeProfileArgs(args.profileFiles, groupIdToGroupSizeMap,
			   myRank, numRanks, rootRank);

  int mergeTy = Prof::CallPath::Profile::Merge_mergeMetricByName;
  uint rFlags = (Prof::CallPath::Profile::RFlg_virtualMetrics
		 | Prof::CallPath::Profile::RFlg_noMetricSfx
		 | Prof::CallPath::Profile::RFlg_makeInclExcl);
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
  
  if (0) { writeProfile(*profGbl, "canonical-cct", myRank); }

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

  // N.B.: Ensures that each rank adds static structure in the same
  // order so that new corresponding nodes have identical node ids.
  Analysis::CallPath::overlayStaticStructureMain(*profGbl, args.agent,
						 args.doNormalizeTy);

  // N.B.: Dense ids are assigned w.r.t. relative magnitude of structure ids
  profGbl->cct()->makeDensePreorderIds();

  // -------------------------------------------------------
  // Create summary metrics and thread-level metrics
  // -------------------------------------------------------
  if (myRank == rootRank) {
    args.makeDatabaseDir();
  }
  MPI_Barrier(MPI_COMM_WORLD);

  makeMetrics(args, nArgs, groupIdToGroupSizeMap, *profGbl,
	      myRank, numRanks, rootRank);

  nArgs.destroy();

  // ------------------------------------------------------------
  // Generate Experiment database
  // ------------------------------------------------------------
  if (myRank == rootRank) {
    Analysis::CallPath::makeDatabase(*profGbl, args);
  }

  // -------------------------------------------------------
  // Cleanup/MPI finalize
  // -------------------------------------------------------
  delete profGbl;

  MPI_Finalize();

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


static void
writeProfile(Prof::CallPath::Profile& prof, const char* baseNm, int myRank)
{
  string fnm_base = string(baseNm) + "-" + StrUtil::toStr(myRank);
  string fnm_hpcrun = fnm_base + ".txt";
  string fnm_xml    = fnm_base + ".xml";

  FILE* fs = hpcio_fopen_w(fnm_hpcrun.c_str(), 1);
  Prof::CallPath::Profile::fmt_fwrite(prof, fs, 0);
  hpcio_fclose(fs);

  std::ostream* os = IOUtil::OpenOStream(fnm_xml.c_str());
  prof.cct()->writeXML(*os, 0, 0, Prof::CCT::Tree::OFlg_Debug);
  IOUtil::CloseStream(os);
}


//***************************************************************************

// makeMetrics: Assumes 'profGbl' is the canonical CCT (with
// structure and with canonical ids).
static void
makeMetrics(const Analysis::Args& args,
	    const Analysis::Util::NormalizeProfileArgs_t& nArgs,
	    const vector<uint>& groupIdToGroupSizeMap,
	    Prof::CallPath::Profile& profGbl,
	    int myRank, int numRanks, int rootRank)
{
  uint mDrvdBeg = 0, mDrvdEnd = 0;   // [ )
  uint mXDrvdBeg = 0, mXDrvdEnd = 0; // [ )

  vector<VMAIntervalSet*> groupIdToGroupMetricsMap(nArgs.groupMax + 1, NULL);

  makeDerivedMetricDescs(profGbl, mDrvdBeg, mDrvdEnd, mXDrvdBeg, mXDrvdEnd,
			 groupIdToGroupMetricsMap, groupIdToGroupSizeMap,
			 myRank, rootRank);

  Prof::Metric::Mgr& mMgrGbl = *profGbl.metricMgr();
  Prof::CCT::ANode* cctRoot = profGbl.cct()->root();

  // -------------------------------------------------------
  // create local summary metrics (and thread-level metrics)
  // -------------------------------------------------------
  cctRoot->computeMetricsIncr(mMgrGbl, mDrvdBeg, mDrvdEnd,
			      Prof::Metric::AExprIncr::FnInit);

  for (uint i = 0; i < nArgs.paths->size(); ++i) {
    string& fnm = (*nArgs.paths)[i];
    uint groupId = (*nArgs.groupMap)[i];
    processProfile(profGbl, args, fnm, groupId, nArgs.groupMax,
		   groupIdToGroupMetricsMap, myRank);
  }

  // -------------------------------------------------------
  // create global summary metrics
  // -------------------------------------------------------

  // 1. Change definitions of derived metrics [mDrvdBeg, mDrvdEnd) to
  //    point to the local summary values that will be in [mXDrvdBeg,
  //    mXDrvdEnd) durring the reduction
  for (uint i = mDrvdBeg, j = mXDrvdBeg; i < mDrvdEnd; ++i, ++j) {
    Prof::Metric::ADesc* m = mMgrGbl.metric(i);
    Prof::Metric::DerivedIncrDesc* mm =
      dynamic_cast<Prof::Metric::DerivedIncrDesc*>(m);
    DIAG_Assert(mm, DIAG_UnexpectedInput);

    Prof::Metric::AExprIncr* expr = mm->expr();
    if (expr) {
      expr->srcId(j);
      if (expr->hasAccum2()) {
	expr->src2Id(j + 1); // cf. Metric::Mgr::makeSummaryMetricIncr()
      }
    }
  }

  // 2. Initialize extra derived metric storage [mXDrvdBeg, mXDrvdEnd)
  //    since it will serve as an input during the summary metrics
  //    reduction
  cctRoot->computeMetricsIncr(mMgrGbl, mXDrvdBeg, mXDrvdEnd,
			      Prof::Metric::AExprIncr::FnInitSrc);

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
    
    // hpcprof-mpi can now generate non-finalized metrics
    if (0) {
      for (uint grpId = 1; grpId < groupIdToGroupMetricsMap.size(); ++grpId) {
	const VMAIntervalSet* ivalset = groupIdToGroupMetricsMap[grpId];
	
	// degenerate case: group has no metrics => no derived metrics
	if (!ivalset) { continue; }
	
	DIAG_Assert(ivalset->size() == 1, DIAG_UnexpectedInput);
	
	const VMAInterval& ival = *(ivalset->begin());
	uint mBeg = (uint)ival.beg(), mEnd = (uint)ival.end();
	
	cctRoot->computeMetricsIncr(mMgrGbl, mBeg, mEnd,
				    Prof::Metric::AExprIncr::FnFini);
      }
    }

    for (uint i = 0; i < mMgrGbl.size(); ++i) {
      Prof::Metric::ADesc* m = mMgrGbl.metric(i);
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
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
		       const vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int rootRank)
{
  Prof::Metric::Mgr& mMgrGbl = *(profGbl.metricMgr());

  uint numSrc = mMgrGbl.size();
  uint mSrcBeg = 0, mSrcEnd = numSrc; // [ )

  uint numDrvd = 0;
  mDrvdBeg = mDrvdEnd = 0;   // [ )
  mXDrvdBeg = mXDrvdEnd = 0; // [ )

  // -------------------------------------------------------
  // official set of derived metrics
  // -------------------------------------------------------
  mDrvdBeg = mMgrGbl.makeSummaryMetricsIncr(mSrcBeg, mSrcEnd);
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

    uint groupId = 1; // default group-id

    // find groupId embedded in metric descriptor name
    const string& nmPfx = m->namePfx();
    if (!nmPfx.empty()) {
      groupId = (uint)StrUtil::toUInt64(nmPfx);
    }
    DIAG_Assert(groupId > 0, DIAG_UnexpectedInput);
    DIAG_Assert(groupId < groupIdToGroupMetricsMap.size(), DIAG_UnexpectedInput);

    // rootRank: set the number of inputs
    if (myRank == rootRank) {
      Prof::Metric::DerivedIncrDesc* mm = 
	dynamic_cast<Prof::Metric::DerivedIncrDesc*>(m);
      DIAG_Assert(mm, DIAG_UnexpectedInput);
    
      // N.B.: groupIdToGroupSizeMap is only initialized for rootRank  
      uint numInputs = groupIdToGroupSizeMap[groupId];
      if (mm->expr()) {
        mm->expr()->numSrcFxd(numInputs);
      }
    }
   
    // populate groupIdToGroupMetricsMap map
    VMAIntervalSet*& ivalset = groupIdToGroupMetricsMap[groupId];
    if (!ivalset) {
      ivalset = new VMAIntervalSet;
    }
    ivalset->insert(i, i + 1); // [ )
  }
  
  // -------------------------------------------------------
  // make temporary set of extra derived metrics (for reduction)
  // -------------------------------------------------------
  mXDrvdBeg = mMgrGbl.makeSummaryMetricsIncr(mSrcBeg, mSrcEnd);
  if (mXDrvdBeg != Prof::Metric::Mgr::npos) {
    mXDrvdEnd = mMgrGbl.size();
  }
  
  for (uint i = mXDrvdBeg; i < mXDrvdEnd; ++i) {
    Prof::Metric::ADesc* m = mMgrGbl.metric(i);
    m->isVisible(false);
  }

  profGbl.isMetricMgrVirtual(false);

  return numDrvd;
}


// processProfile: Assumes 'profGbl' is the canonical CCT (with
// structure and with canonical ids).
static void
processProfile(Prof::CallPath::Profile& profGbl,
	       const Analysis::Args& args,
	       string& profileFile, uint groupId, uint groupMax,
	       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
	       int myRank)
{
  Prof::Metric::Mgr* mMgrGbl = profGbl.metricMgr();
  Prof::CCT::Tree* cctGbl = profGbl.cct();
  Prof::CCT::ANode* cctRoot = cctGbl->root();

  // -------------------------------------------------------
  // read profile file
  // -------------------------------------------------------
  uint rFlags = (Prof::CallPath::Profile::RFlg_noMetricSfx
		 | Prof::CallPath::Profile::RFlg_makeInclExcl);
  uint rGroupId = (groupMax > 1) ? groupId : 0;

  Prof::CallPath::Profile* prof =
    Analysis::CallPath::read(profileFile, rGroupId, rFlags);

  // -------------------------------------------------------
  // merge into canonical CCT
  // -------------------------------------------------------
  int mergeTy  = Prof::CallPath::Profile::Merge_mergeMetricByName;
  int mergeFlg = Prof::CCT::Tree::OFlg_MergeOnly; // nodes may only be merged

  // Add *some* structure information to the leaves of 'prof' so that
  // it will be merged successfully with the structured canonical CCT
  // 'profGbl'.
  //
  // Background: When CCT::Stmts are merged in
  // Analysis::CallPath::coalesceStmts(CallPath::Profile), IP/LIP
  // information is not retained.  This means that when merging 'prof'
  // into 'profGbl' (using CallPath::Profile::merge()), many leaves in
  // 'prof' will not find their corresponding node in 'profGbl' unless
  // corrective measures are taken.
  prof->structure(profGbl.structure());
  Analysis::CallPath::noteStaticStructureOnLeaves(*prof);
  prof->structure(NULL);

  uint mBeg = profGbl.merge(*prof, mergeTy, mergeFlg); // [closed begin
  uint mEnd = mBeg + prof->metricMgr()->size();        //  open end)

  // -------------------------------------------------------
  // compute local incl/excl sampled metrics and update local derived metrics
  // -------------------------------------------------------

  // 1. Batch compute local sampled metrics
  VMAIntervalSet ivalsetIncl;
  VMAIntervalSet ivalsetExcl;

  for (uint mId = mBeg; mId < mEnd; ++mId) {
    Prof::Metric::ADesc* m = mMgrGbl->metric(mId);
    if (m->type() == Prof::Metric::ADesc::TyIncl) {
      ivalsetIncl.insert(VMAInterval(mId, mId + 1)); // [ )
    }
    else if (m->type() == Prof::Metric::ADesc::TyExcl) {
      ivalsetExcl.insert(VMAInterval(mId, mId + 1)); // [ )
    }
  }

  cctRoot->aggregateMetricsIncl(ivalsetIncl);
  cctRoot->aggregateMetricsExcl(ivalsetExcl);


  // 2. Batch compute local derived metrics
  const VMAIntervalSet* ivalsetDrvd = groupIdToGroupMetricsMap[groupId];
  if (ivalsetDrvd) {
    DIAG_Assert(ivalsetDrvd->size() == 1, DIAG_UnexpectedInput);
    const VMAInterval& ival = *(ivalsetDrvd->begin());
    uint mDrvdBeg = (uint)ival.beg();
    uint mDrvdEnd = (uint)ival.end();

    DIAG_MsgIf(0, "[" << myRank << "] grp " << groupId << ": [" << mDrvdBeg << ", " << mDrvdEnd << ")");
    cctRoot->computeMetricsIncr(*mMgrGbl, mDrvdBeg, mDrvdEnd,
				Prof::Metric::AExprIncr::FnAccum);
  }

  // -------------------------------------------------------
  // write local sampled metric values to disk
  // -------------------------------------------------------
  //writeMetrics(profGbl, mBeg, mEnd, args, groupId, profileFile);

  // -------------------------------------------------------
  // reinitialize metric values since space may be used again
  // -------------------------------------------------------
  cctRoot->zeroMetricsDeep(mBeg, mEnd); // TODO: not strictly necessary
  
  delete prof;
}


static void
writeMetrics(Prof::CallPath::Profile& profGbl, uint mBegId, uint mEndId,
	     const Analysis::Args& args, uint groupId, string& profileFile)
{
  const Prof::CCT::Tree& cct = *(profGbl.cct());

  // -------------------------------------------------------
  // pack metrics into dense matrix
  // -------------------------------------------------------
  uint maxCCTId = cct.maxDenseId();

  ParallelAnalysis::PackedMetrics packedMetrics(maxCCTId + 1, mBegId, mEndId,
						mBegId, mEndId);

  ParallelAnalysis::packMetrics(profGbl, packedMetrics);

  // -------------------------------------------------------
  // generate file name and open file
  // -------------------------------------------------------
  string fnm_base = FileUtil::basename(profileFile.c_str());
  string fnm = StrUtil::toStr(groupId) + "." + fnm_base + ".hpcprof-metrics";
  string pathNm = args.db_dir + "/" + fnm;

  FILE* fs = hpcio_fopen_w(pathNm.c_str(), 1);
  if (!fs) {
    DIAG_Throw("error opening file");
  }

  // -------------------------------------------------------
  // write data
  // -------------------------------------------------------

  // TODO: header: Tag: HPCPROF_____ {16b}, num nodes {4b}

  // N.B.: first row corresponds to node 1.
  //       first column corresponds to first sampled metric.

  // cf. ParallelAnalysis::unpackMetrics
  for (uint nodeId = 1; nodeId < packedMetrics.numNodes(); ++nodeId) {
    for (uint mId1 = 0, mId2 = mBegId; mId2 < mEndId; ++mId1, ++mId2) {
      double mval = packedMetrics.idx(nodeId, mId1);
      hpcfmt_byte8_fwrite(mval, fs); // TODO: HPCFMT_ThrowIfError()
    }
  }

  hpcio_fclose(fs);
}


//***************************************************************************
