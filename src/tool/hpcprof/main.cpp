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

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#include <string>
using std::string;

#include <vector>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "Args.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.hpp>


//*************************** Forward Declarations ***************************

static int
realmain(int argc, char* const* argv);

static void
makeMetrics(Prof::CallPath::Profile& prof,
	    const Analysis::Args& args,
	    const Analysis::Util::NormalizeProfileArgs_t& nArgs);


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
  args.parse(argc, argv);

  RealPathMgr::singleton().searchPaths(args.searchPathStr());

  Analysis::Util::NormalizeProfileArgs_t nArgs =
    Analysis::Util::normalizeProfileArgs(args.profileFiles);

  // ------------------------------------------------------------
  // 0. Special checks
  // ------------------------------------------------------------

  if (nArgs.paths->size() == 1 && !args.hpcprof_isMetricArg) {
    args.prof_metrics = Analysis::Args::MetricFlg_Thread;
  }

  if (Analysis::Args::MetricFlg_isThread(args.prof_metrics)
      && nArgs.paths->size() > 16
      && !args.hpcprof_forceMetrics) {
    DIAG_Throw("You have requested thread-level metrics for " << nArgs.paths->size() << " profile files.  Because this may result in an unusable database, to continue you must use the --force-metric option.");
  }

  // -------------------------------------------------------
  // 0. Make empty Experiment database (ensure file system works)
  // -------------------------------------------------------

  args.makeDatabaseDir();

  // ------------------------------------------------------------
  // 1a. Create canonical CCT // Normalize trace files
  // ------------------------------------------------------------

  int mergeTy = Prof::CallPath::Profile::Merge_CreateMetric;
  Analysis::Util::UIntVec* groupMap =
    (nArgs.groupMax > 1) ? nArgs.groupMap : NULL;

  uint rFlags = 0;
  if (Analysis::Args::MetricFlg_isSum(args.prof_metrics)) {
    rFlags |= Prof::CallPath::Profile::RFlg_MakeInclExcl;
  }
  uint mrgFlags = (Prof::CCT::MrgFlg_NormalizeTraceFileY);

  Prof::CallPath::Profile* prof =
    Analysis::CallPath::read(*nArgs.paths, groupMap, mergeTy, rFlags, mrgFlags);

  prof->disable_redundancy(args.remove_redundancy);


  // ------------------------------------------------------------
  // 1b. Add static structure to canonical CCT
  // ------------------------------------------------------------

  Prof::Struct::Tree* structure = new Prof::Struct::Tree("");
  if (!args.structureFiles.empty()) {
    Analysis::CallPath::readStructure(structure, args);
  }
  prof->structure(structure);

  Analysis::CallPath::overlayStaticStructureMain(*prof, args.agent,
						 args.doNormalizeTy);
  
  // -------------------------------------------------------
  // 2a. Create summary metrics for canonical CCT
  // -------------------------------------------------------

  if (Analysis::Args::MetricFlg_isSum(args.prof_metrics)) {
    makeMetrics(*prof, args, nArgs);
  }

  // -------------------------------------------------------
  // 2b. Prune and normalize canonical CCT
  // -------------------------------------------------------

  if (Analysis::Args::MetricFlg_isSum(args.prof_metrics)) {
    Analysis::CallPath::pruneBySummaryMetrics(*prof, NULL);
  }

  Analysis::CallPath::normalize(*prof, args.agent, args.doNormalizeTy);

  if (Analysis::Args::MetricFlg_isSum(args.prof_metrics)) {
    // Apply after all CCT pruning/normalization is completed.
    //TODO: Analysis::CallPath::applySummaryMetricAgents(*prof, args.agent);
  }

  prof->cct()->makeDensePreorderIds();

  // -------------------------------------------------------
  // 2c. Create thread-level metric DB
  // -------------------------------------------------------
  // Currently we use --metric=thread as a proxy for the metric database

  // ------------------------------------------------------------
  // 3. Generate Experiment database
  //    INVARIANT: database dir already exists
  // ------------------------------------------------------------

  Analysis::CallPath::pruneStructTree(*prof);

  if (args.title.empty()) {
    args.title = prof->name();
  }

  if (!args.db_makeMetricDB) {
    prof->metricMgr()->zeroDBInfo();
  }

  Analysis::CallPath::makeDatabase(*prof, args);


  // -------------------------------------------------------
  // Cleanup
  // -------------------------------------------------------
  nArgs.destroy();

  delete prof;

  return 0;
}


//****************************************************************************

static void
makeMetrics(Prof::CallPath::Profile& prof,
	    const Analysis::Args& args,
	    const Analysis::Util::NormalizeProfileArgs_t& GCC_ATTR_UNUSED nArgs)
{
  Prof::Metric::Mgr& mMgr = *prof.metricMgr();

  Prof::CCT::ANode* cctRoot = prof.cct()->root();

  // -------------------------------------------------------
  // create derived metrics
  // -------------------------------------------------------
  uint numSrc = mMgr.size();
  uint mSrcBeg = 0, mSrcEnd = numSrc; // [ )

  uint numDrvd = 0;
  uint mDrvdBeg = 0, mDrvdEnd = 0; // [ )
  
  bool needAllStats =
    Analysis::Args::MetricFlg_isSet(args.prof_metrics,
				    Analysis::Args::MetricFlg_StatsAll);
  bool needMultiOccurance =
    Analysis::Args::MetricFlg_isThread(args.prof_metrics);

  mDrvdBeg = mMgr.makeSummaryMetrics(needAllStats, needMultiOccurance,
                                     mSrcBeg, mSrcEnd);
  if (mDrvdBeg != Prof::Metric::Mgr::npos) {
    mDrvdEnd = mMgr.size();
    numDrvd = (mDrvdEnd - mDrvdBeg);
  }

  if (!Analysis::Args::MetricFlg_isThread(args.prof_metrics)) {
    for (uint mId = mSrcBeg; mId < mSrcEnd; ++mId) {
      Prof::Metric::ADesc* m = mMgr.metric(mId);
      m->isVisible(false);
    }
  }

  // -------------------------------------------------------
  // aggregate sampled metrics (in batch)
  // -------------------------------------------------------

  VMAIntervalSet ivalsetIncl;
  VMAIntervalSet ivalsetExcl;

  for (uint mId = mSrcBeg; mId < mSrcEnd; ++mId) {
    Prof::Metric::ADesc* m = mMgr.metric(mId);
    if (m->type() == Prof::Metric::ADesc::TyIncl) {
      ivalsetIncl.insert(VMAInterval(mId, mId + 1)); // [ )
    }
    else if (m->type() == Prof::Metric::ADesc::TyExcl) {
      ivalsetExcl.insert(VMAInterval(mId, mId + 1)); // [ )
    }
    m->computedType(Prof::Metric::ADesc::ComputedTy_Final); // proleptic
  }

  cctRoot->aggregateMetricsIncl(ivalsetIncl);
  cctRoot->aggregateMetricsExcl(ivalsetExcl);


  // -------------------------------------------------------
  // compute derived metrics
  // -------------------------------------------------------
  cctRoot->computeMetrics(mMgr, mDrvdBeg, mDrvdEnd, /*doFinal*/false);

  for (uint i = mDrvdBeg; i < mDrvdEnd; ++i) {
    Prof::Metric::ADesc* m = mMgr.metric(i);
    m->computedType(Prof::Metric::ADesc::ComputedTy_NonFinal);
  }
}
