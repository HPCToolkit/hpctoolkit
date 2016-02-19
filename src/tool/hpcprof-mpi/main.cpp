// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
#include <climits> // UCHAR_MAX, PATH_MAX
#include <cctype>  // isdigit()
#include <cstring> // strcpy()

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Args.hpp"
#include "ParallelAnalysis.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/banal/Struct.hpp>

#include <lib/binutils/VMAInterval.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/StrUtil.hpp>


//*************************** Forward Declarations ***************************

static int
realmain(int argc, char* const* argv);

static Analysis::Util::NormalizeProfileArgs_t
myNormalizeProfileArgs(const Analysis::Util::StringVec& profileFiles,
		       vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int numRanks, int rootRank = 0);


static void
makeSummaryMetrics(Prof::CallPath::Profile& profGbl,
		   const Analysis::Args& args,
		   const Analysis::Util::NormalizeProfileArgs_t& nArgs,
		   const vector<uint>& groupIdToGroupSizeMap,
		   int myRank, int numRanks, int rootRank);

static void
makeThreadMetrics(Prof::CallPath::Profile& profGbl,
		  const Analysis::Args& args,
		  const Analysis::Util::NormalizeProfileArgs_t& nArgs,
		  const vector<uint>& groupIdToGroupSizeMap,
		  int myRank, int numRanks, int rootRank);

static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       const Analysis::Args& args,
		       uint& mDrvdBeg, uint& mDrvdEnd,
		       uint& mXDrvdBeg, uint& mXDrvdEnd,
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
		       const vector<uint>& groupIdToGroupSizeMap,
		       int myRank, int rootRank);

static void
makeSummaryMetrics_Lcl(Prof::CallPath::Profile& profGbl,
		       const string& profileFile,
		       const Analysis::Args& args, uint groupId, uint groupMax,
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
		       int myRank);

static void
makeThreadMetrics_Lcl(Prof::CallPath::Profile& profGbl,
		      const string& profileFile,
		      const Analysis::Args& args, uint groupId, uint groupMax,
		      int myRank);

static string
makeDBFileName(const string& dbDir, uint groupId, const string& profileFile);

static void
writeMetricsDB(Prof::CallPath::Profile& profGbl, uint mBegId, uint mEndId,
	       const string& metricDBFnm);


static void
writeStructure(const Prof::Struct::Tree& structure, const char* baseNm,
	       int myRank);

static void
writeProfile(const Prof::CallPath::Profile& prof, const char* baseNm,
	     int myRank);

static std::string
makeFileName(const char* baseNm, const char* ext, int myRank);


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
  // 0. MPI initialize
  // -------------------------------------------------------
  MPI_Init(&argc, (char***)&argv);

  const int rootRank = 0;
  int myRank, numRanks;
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank); 
  MPI_Comm_size(MPI_COMM_WORLD, &numRanks);

  // -------------------------------------------------------
  // 0. Debugging hook
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
  // 0. Make empty Experiment database (ensure file system works)
  // -------------------------------------------------------
  char dbDirBuf[PATH_MAX];
  if (myRank == rootRank) {
    args.makeDatabaseDir();

    memset(dbDirBuf, '\0', PATH_MAX); // avoid artificial valgrind warnings
    strncpy(dbDirBuf, args.db_dir.c_str(), PATH_MAX);
    dbDirBuf[PATH_MAX - 1] = '\0';
  }

  MPI_Bcast((void*)dbDirBuf, PATH_MAX, MPI_CHAR, rootRank, MPI_COMM_WORLD);
  args.db_dir = dbDirBuf;

  // -------------------------------------------------------
  // 1a. Create local CCT (from local set of profile files)
  // -------------------------------------------------------
  Prof::CallPath::Profile* profLcl = NULL;

  vector<uint> groupIdToGroupSizeMap; // only initialized for rootRank

  Analysis::Util::NormalizeProfileArgs_t nArgs =
    myNormalizeProfileArgs(args.profileFiles, groupIdToGroupSizeMap,
			   myRank, numRanks, rootRank);

  int mergeTy = Prof::CallPath::Profile::Merge_MergeMetricByName;
  uint rFlags = (Prof::CallPath::Profile::RFlg_VirtualMetrics
		 | Prof::CallPath::Profile::RFlg_NoMetricSfx
		 | Prof::CallPath::Profile::RFlg_MakeInclExcl);
  Analysis::Util::UIntVec* groupMap =
    (nArgs.groupMax > 1) ? nArgs.groupMap : NULL;

  profLcl = Analysis::CallPath::read(*nArgs.paths, groupMap, mergeTy, rFlags);

  // -------------------------------------------------------
  // 1b. Create canonical CCT (metrics merged by <group>.<name>.*)
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

  // -------------------------------------------------------
  // 1c. Add static structure to canonical CCT; form dense node ids
  //
  // Post-INVARIANT: each process has canonical CCT with dense node
  // ids; corresponding nodes have idential ids.
  // -------------------------------------------------------

  Prof::Struct::Tree* structure = new Prof::Struct::Tree("");
  if (!args.structureFiles.empty()) {
    Analysis::CallPath::readStructure(structure, args);
  }
  profGbl->structure(structure);

  if (0) {
    writeProfile(*profGbl, "dbg-canonical-cct-a", myRank);
    writeStructure(*structure, "dbg-structure-a", myRank);
    string fnm = makeFileName("dbg-overlay-structure", "txt", myRank);
    Analysis::CallPath::dbgOs = IOUtil::OpenOStream(fnm.c_str());
  }

  // N.B.: Ensures that each rank adds static structure in the same
  // order so that new corresponding nodes have identical node ids.
  Analysis::CallPath::overlayStaticStructureMain(*profGbl, args.agent,
						 args.doNormalizeTy);

  // N.B.: Dense ids are assigned w.r.t. Prof::CCT::...::cmpByStructureInfo()
  profGbl->cct()->makeDensePreorderIds();

  if (0) {
    writeProfile(*profGbl, "dbg-canonical-cct-b", myRank);
    writeStructure(*structure, "dbg-structure-b", myRank);
    IOUtil::CloseStream(Analysis::CallPath::dbgOs);
  }

  // -------------------------------------------------------
  // 2a. Create summary metrics for canonical CCT
  //
  // Post-INVARIANT: rank 0's 'profGbl' contains summary metrics
  // -------------------------------------------------------
  makeSummaryMetrics(*profGbl, args, nArgs, groupIdToGroupSizeMap,
		     myRank, numRanks, rootRank);

  // -------------------------------------------------------
  // 2b. Prune and normalize canonical CCT
  // -------------------------------------------------------

  uint prunedNodesSz = profGbl->cct()->maxDenseId() + 1;
  uint8_t* prunedNodes = new uint8_t[prunedNodesSz];
  memset(prunedNodes, 0, prunedNodesSz * sizeof(uint8_t));

  if (myRank == rootRank) {
    // Disable pruning when making a metric database because it causes
    // makeThreadMetrics_Lcl(), which uses CCT::MrgFlg_CCTMergeOnly,
    // to under-compute values for thread-level metrics.
    if (!args.db_makeMetricDB) {
      Analysis::CallPath::pruneBySummaryMetrics(*profGbl, prunedNodes);
    }
  }
  
  MPI_Bcast(prunedNodes, prunedNodesSz, MPI_BYTE, rootRank, MPI_COMM_WORLD);

  if (myRank != rootRank) {
    profGbl->cct()->pruneCCTByNodeId(prunedNodes);
  }
  delete[] prunedNodes;

  Analysis::CallPath::normalize(*profGbl, args.agent, args.doNormalizeTy);

  if (myRank == rootRank) {
    // Apply after all CCT pruning/normalization is completed.
    Analysis::CallPath::applySummaryMetricAgents(*profGbl, args.agent);
  }

  // N.B.: Dense ids are assigned w.r.t. Prof::CCT::...::cmpByStructureInfo()
  profGbl->cct()->makeDensePreorderIds();

  // -------------------------------------------------------
  // 2c. Create thread-level metric DB // Normalize trace files
  // -------------------------------------------------------
  makeThreadMetrics(*profGbl, args, nArgs, groupIdToGroupSizeMap,
		    myRank, numRanks, rootRank);
  
  // ------------------------------------------------------------
  // 3. Generate Experiment database
  //    INVARIANT: database dir already exists
  // ------------------------------------------------------------

  Analysis::CallPath::pruneStructTree(*profGbl);

  if (myRank == rootRank) {
    if (args.title.empty()) {
      args.title = profGbl->name();
    }

    if (!args.db_makeMetricDB) {
      profGbl->metricMgr()->zeroDBInfo();
    }

    Analysis::CallPath::makeDatabase(*profGbl, args);
  }
  else {
    Analysis::Util::copyTraceFiles(args.db_dir, profGbl->traceFileNameSet());
  }

  // -------------------------------------------------------
  // Cleanup/MPI finalize
  // -------------------------------------------------------
  nArgs.destroy();

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
  const uint groupIdLen = 1; // see asssertion below
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
    pathLenMax       = metadataBuf[1];
    groupIdMax       = metadataBuf[2];
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
    const char* nm_cstr = &recvFilesBuf[i + groupIdLen];
    string nm = nm_cstr;
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

// makeSummaryMetrics: Assumes 'profGbl' is the canonical CCT (with
// structure and with canonical ids).
static void
makeSummaryMetrics(Prof::CallPath::Profile& profGbl,
		   const Analysis::Args& args,
		   const Analysis::Util::NormalizeProfileArgs_t& nArgs,
		   const vector<uint>& groupIdToGroupSizeMap,
		   int myRank, int numRanks, int rootRank)
{
  uint mDrvdBeg = 0, mDrvdEnd = 0;   // [ )
  uint mXDrvdBeg = 0, mXDrvdEnd = 0; // [ )

  vector<VMAIntervalSet*> groupIdToGroupMetricsMap(nArgs.groupMax + 1, NULL);

  makeDerivedMetricDescs(profGbl, args,
			 mDrvdBeg, mDrvdEnd, mXDrvdBeg, mXDrvdEnd,
			 groupIdToGroupMetricsMap, groupIdToGroupSizeMap,
			 myRank, rootRank);

  Prof::Metric::Mgr& mMgrGbl = *profGbl.metricMgr();
  Prof::CCT::ANode* cctRoot = profGbl.cct()->root();

  // -------------------------------------------------------
  // compute local contribution summary metrics (accumulate function)
  // -------------------------------------------------------
  cctRoot->computeMetricsIncr(mMgrGbl, mDrvdBeg, mDrvdEnd,
			      Prof::Metric::AExprIncr::FnInit);

  for (uint i = 0; i < nArgs.paths->size(); ++i) {
    const string& fnm = (*nArgs.paths)[i];
    uint groupId = (*nArgs.groupMap)[i];
    makeSummaryMetrics_Lcl(profGbl, fnm, args, groupId, nArgs.groupMax,
			   groupIdToGroupMetricsMap, myRank);
  }

  // -------------------------------------------------------
  // create summary metrics via reduction (combine function)
  // -------------------------------------------------------

  // 1. Change definitions of derived metrics [mDrvdBeg, mDrvdEnd)
  //    since the 'combine' function will be used during the metric
  //    reduction.  Metric inputs point to accumulators [mXDrvdBeg,
  //    mXDrvdEnd) rather input values.
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

  // 2. Initialize temporary accumulators [mXDrvdBeg, mXDrvdEnd) used
  //    during the summary metrics reduction
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

  // -------------------------------------------------------
  // finalize metrics
  // -------------------------------------------------------

  if (myRank == rootRank) {
    
    // We now generate non-finalized metrics, so no need to do this
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
      m->computedType(Prof::Metric::ADesc::ComputedTy_NonFinal);
    }
  }

  for (uint grpId = 1; grpId < groupIdToGroupMetricsMap.size(); ++grpId) {
    delete groupIdToGroupMetricsMap[grpId];
  }

  delete packedMetrics;
}


static void
makeThreadMetrics(Prof::CallPath::Profile& profGbl,
		  const Analysis::Args& args,
		  const Analysis::Util::NormalizeProfileArgs_t& nArgs,
		  const vector<uint>& groupIdToGroupSizeMap,
		  int myRank, int numRanks, int rootRank)
{
  for (uint i = 0; i < nArgs.paths->size(); ++i) {
    string& fnm = (*nArgs.paths)[i];
    uint groupId = (*nArgs.groupMap)[i];
    makeThreadMetrics_Lcl(profGbl, fnm, args, groupId, nArgs.groupMax, myRank);
  }
}


static uint
makeDerivedMetricDescs(Prof::CallPath::Profile& profGbl,
		       const Analysis::Args& args,
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

  bool needAllStats =
    Analysis::Args::MetricFlg_isSet(args.prof_metrics,
				    Analysis::Args::MetricFlg_StatsAll);

  mDrvdBeg = mMgrGbl.makeSummaryMetricsIncr(needAllStats, mSrcBeg, mSrcEnd);
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
      uint numInputs = groupIdToGroupSizeMap[groupId]; // / <n> TODO:threads
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
  mXDrvdBeg = mMgrGbl.makeSummaryMetricsIncr(needAllStats, mSrcBeg, mSrcEnd);
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


// makeSummaryMetrics_Lcl: Make summary metrics.
//
// Assumes:
// - 'profGbl' is the canonical CCT (with structure and with
//   canonical ids)
// - each thread-level CCT is always a subset of 'profGbl' (the
//   canonical CCT); in other words, 'profGbl' should not be pruned
//   in any way!
// - 'args' contains the correct final experiment database.
//
// FIXME: abstract between makeSummaryMetrics_Lcl() & makeThreadMetrics_Lcl()
static void
makeSummaryMetrics_Lcl(Prof::CallPath::Profile& profGbl,
		       const string& profileFile,
		       const Analysis::Args& args, uint groupId, uint groupMax,
		       vector<VMAIntervalSet*>& groupIdToGroupMetricsMap,
		       int myRank)
{
  Prof::Metric::Mgr* mMgrGbl = profGbl.metricMgr();
  Prof::CCT::Tree* cctGbl = profGbl.cct();
  Prof::CCT::ANode* cctRootGbl = cctGbl->root();

  // -------------------------------------------------------
  // read profile file
  // -------------------------------------------------------
  uint rFlags = (Prof::CallPath::Profile::RFlg_NoMetricSfx
		 | Prof::CallPath::Profile::RFlg_MakeInclExcl);
  uint rGroupId = (groupMax > 1) ? groupId : 0;

  Prof::CallPath::Profile* prof =
    Analysis::CallPath::read(profileFile, rGroupId, rFlags);

  // -------------------------------------------------------
  // merge into canonical CCT
  // -------------------------------------------------------
  int mergeTy  = Prof::CallPath::Profile::Merge_MergeMetricByName;
  int mergeFlg = (Prof::CCT::MrgFlg_AssertCCTMergeOnly);

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

  cctRootGbl->aggregateMetricsIncl(ivalsetIncl);
  cctRootGbl->aggregateMetricsExcl(ivalsetExcl);


  // 2. Batch compute local derived metrics
  const VMAIntervalSet* ivalsetDrvd = groupIdToGroupMetricsMap[groupId];
  if (ivalsetDrvd) {
    DIAG_Assert(ivalsetDrvd->size() == 1, DIAG_UnexpectedInput);
    const VMAInterval& ival = *(ivalsetDrvd->begin());
    uint mDrvdBeg = (uint)ival.beg();
    uint mDrvdEnd = (uint)ival.end();

    DIAG_MsgIf(0, "[" << myRank << "] grp " << groupId << ": [" << mDrvdBeg << ", " << mDrvdEnd << ")");
    cctRootGbl->computeMetricsIncr(*mMgrGbl, mDrvdBeg, mDrvdEnd,
				   Prof::Metric::AExprIncr::FnAccum);
  }

  // -------------------------------------------------------
  // reinitialize metric values for next time
  // -------------------------------------------------------

  // TODO: This really should (a) come immediately after the MetricMgr
  // merge above and (b) use FnInitSrc (not 0).  However, to do this
  // we would need to (a) split the MetricMgr merge and CCT merge into
  // two; and (b) use a CCT init (which whould initialize using
  // assignment) instead of CCT::merge() (which initializes based on
  // addition against 0).
  cctRootGbl->zeroMetricsDeep(mBeg, mEnd); // cf. FnInitSrc
  
  delete prof;
}


// makeThreadMetrics_Lcl: Make thread-level metric database.
//
// Makes same assumptions as makeSummaryMetrics_Lcl but with one key
// exception: Each thread-level CCT does not have to be a subset of
// 'profGbl' (the canonical CCT); in other words, 'profGbl' may be
// pruned.
static void
makeThreadMetrics_Lcl(Prof::CallPath::Profile& profGbl,
		      const string& profileFile,
		      const Analysis::Args& args, uint groupId, uint groupMax,
		      int myRank)
{
  Prof::Metric::Mgr* mMgrGbl = profGbl.metricMgr();
  Prof::CCT::Tree* cctGbl = profGbl.cct();
  Prof::CCT::ANode* cctRootGbl = cctGbl->root();

  // -------------------------------------------------------
  // read profile file
  // -------------------------------------------------------
  uint rFlags = (Prof::CallPath::Profile::RFlg_NoMetricSfx
		 | Prof::CallPath::Profile::RFlg_MakeInclExcl);
  uint rGroupId = (groupMax > 1) ? groupId : 0;

  Prof::CallPath::Profile* prof =
    Analysis::CallPath::read(profileFile, rGroupId, rFlags);

  // -------------------------------------------------------
  // merge into canonical CCT
  // -------------------------------------------------------
  int mergeTy  = Prof::CallPath::Profile::Merge_MergeMetricByName;
  int mergeFlg = (Prof::CCT::MrgFlg_NormalizeTraceFileY
		  | Prof::CCT::MrgFlg_CCTMergeOnly);

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

  if (args.db_makeMetricDB) {
    uint mEnd = mBeg + prof->metricMgr()->size(); // open end)

    // -------------------------------------------------------
    // compute local incl/excl sampled metrics
    // -------------------------------------------------------

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
    
    cctRootGbl->aggregateMetricsIncl(ivalsetIncl);
    cctRootGbl->aggregateMetricsExcl(ivalsetExcl);

    // -------------------------------------------------------
    // write local sampled metric values into database
    // -------------------------------------------------------

    string dbFnm = makeDBFileName(args.db_dir, groupId, profileFile);
    writeMetricsDB(profGbl, mBeg, mEnd, dbFnm);

    // -------------------------------------------------------
    // reinitialize metric values for next time
    // -------------------------------------------------------
    
    // TODO: see corresponding comments in makeSummaryMetrics_Lcl()
    cctRootGbl->zeroMetricsDeep(mBeg, mEnd); // cf. FnInitSrc
  }

  delete prof;
}


static string
makeDBFileName(const string& dbDir, uint groupId, const string& profileFile)
{
  string grpStr = StrUtil::toStr(groupId);
 
  string fnm_base = FileUtil::rmSuffix(FileUtil::basename(profileFile.c_str()));

  string fnm = grpStr + "." + fnm_base + "." + HPCPROF_MetricDBSfx;

  string metricDBFnm = dbDir + "/" + fnm;
  return metricDBFnm;
}


// [mBegId, mEndId)
static void
writeMetricsDB(Prof::CallPath::Profile& profGbl, uint mBegId, uint mEndId,
	       const string& metricDBFnm)
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
  // write data
  // -------------------------------------------------------

  FILE* fs = hpcio_fopen_w(metricDBFnm.c_str(), 1);
  if (!fs) {
    DIAG_Throw("error opening file '" << metricDBFnm << "'");
  }
  DIAG_MsgIf(0, "writeMetricsDB: " << metricDBFnm);

  uint numNodes = packedMetrics.numNodes() - 1;

  // 1. header
  hpcmetricDB_fmt_hdr_t hdr;
  hdr.numNodes = numNodes;
  hdr.numMetrics = mEndId - mBegId; // [mBegId mEndId)

  hpcmetricDB_fmt_hdr_fwrite(&hdr, fs);

  // 2. metric values
  //    - first row corresponds to node 1.
  //    - first column corresponds to first sampled metric.
  // cf. ParallelAnalysis::unpackMetrics: 

  for (uint nodeId = 1; nodeId < numNodes + 1; ++nodeId) {
    for (uint mId1 = 0, mId2 = mBegId; mId2 < mEndId; ++mId1, ++mId2) {
      double mval = packedMetrics.idx(nodeId, mId1);
      DIAG_MsgIf(0,  "  " << nodeId << " -> " << mval);
      hpcfmt_real8_fwrite(mval, fs);
    }
  }

  hpcio_fclose(fs);
}


//***************************************************************************

static void
writeStructure(const Prof::Struct::Tree& structure, const char* baseNm,
	       int myRank)
{
  string fnm = makeFileName(baseNm, "xml", myRank);
  std::ostream* os = IOUtil::OpenOStream(fnm.c_str());
  Prof::Struct::writeXML(*os, structure, true);
  IOUtil::CloseStream(os);
}


static void
writeProfile(const Prof::CallPath::Profile& prof, const char* baseNm,
	     int myRank)
{
  // Only safe if static structure has not been added
  //string fnm_hpcrun = makeFileName(baseNm, "txt", myRank);
  //FILE* fs = hpcio_fopen_w(fnm_hpcrun.c_str(), 1);
  //Prof::CallPath::Profile::fmt_fwrite(prof, fs, 0);
  //hpcio_fclose(fs);

  string fnm_xml = makeFileName(baseNm, "xml", myRank);
  std::ostream* os = IOUtil::OpenOStream(fnm_xml.c_str());
  prof.cct()->writeXML(*os, 0, 0, Prof::CCT::Tree::OFlg_Debug);
  IOUtil::CloseStream(os);
}


static std::string
makeFileName(const char* baseNm, const char* ext, int myRank)
{
  return string(baseNm) + "-" + StrUtil::toStr(myRank) + "." + ext;
}

//****************************************************************************

