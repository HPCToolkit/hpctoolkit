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
// Copyright ((c)) 2002-2021, Rice University
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

#include <string>
using std::string;

#include <algorithm>

#include <stdint.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "ParallelAnalysis.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>


//*************************** Forward Declarations **************************

#define DBG_CCT_MERGE 0

//***************************************************************************


namespace ParallelAnalysis {

//***************************************************************************
// forward declarations
//***************************************************************************

static void
packStringSet(const StringSet& profile,
		 uint8_t** buffer, size_t* bufferSz);

static StringSet*
unpackStringSet(uint8_t* buffer, size_t bufferSz);

//***************************************************************************
// private functions
//***************************************************************************

static void 
broadcast_sizet
(
  size_t &size, 
  MPI_Comm comm
)
{ 
  long size_l = size;
  MPI_Bcast(&size_l, 1, MPI_LONG, 0, comm);
  size = size_l;
} 



//***************************************************************************
// interface functions
//***************************************************************************

void
broadcast
(
  Prof::CallPath::Profile*& profile,
  int myRank, 
  MPI_Comm comm
)
{
  size_t size = 0;
  uint8_t* buf = NULL;

  if (myRank == 0) {
    packProfile(*profile, &buf, &size);
  }

  broadcast_sizet(size, comm);

  if (myRank != 0) {
    buf = (uint8_t *)malloc(size * sizeof(uint8_t));
  }

  MPI_Bcast(buf, size, MPI_BYTE, 0, comm);

  if (myRank != 0) {
    profile = unpackProfile(buf, size);
  }

  free(buf);
}

void
broadcast
(
  StringSet &stringSet,
  int myRank, 
  MPI_Comm comm
)
{
  size_t size = 0;
  uint8_t* buf = NULL;

  if (myRank == 0) {
    packStringSet(stringSet, &buf, &size);
  }

  broadcast_sizet(size, comm);

  if (myRank != 0) {
    buf = (uint8_t *)malloc(size * sizeof(uint8_t));
  }

  MPI_Bcast(buf, size, MPI_BYTE, 0, comm);

  if (myRank != 0) {
    StringSet *rhs = unpackStringSet(buf, size);
    stringSet += *rhs;
    delete rhs;
  }

  free(buf);
}


void
packSend(Prof::CallPath::Profile* profile,
	 int dest, int myRank, MPI_Comm comm)
{
  uint8_t* profileBuf = NULL;
  size_t profileBufSz = 0;
  packProfile(*profile, &profileBuf, &profileBufSz);
  MPI_Send(profileBuf, (int)profileBufSz, MPI_BYTE, dest, myRank, comm);
  free(profileBuf);
}

void
recvMerge(Prof::CallPath::Profile* profile,
	  int src, int myRank, MPI_Comm comm)
{
  // probe src
  MPI_Status mpistat;
  MPI_Probe(src, src, comm, &mpistat);
  int profileBufSz;
  MPI_Get_count(&mpistat, MPI_BYTE, &profileBufSz);

  // receive profile from src
  uint8_t *profileBuf = new uint8_t[profileBufSz];
  MPI_Recv(profileBuf, profileBufSz, MPI_BYTE, src, src, comm, &mpistat);
  Prof::CallPath::Profile* new_profile =
    unpackProfile(profileBuf, (size_t)profileBufSz);
  delete[] profileBuf;

  if (DBG_CCT_MERGE) {
    string pfx0 = "[" + StrUtil::toStr(myRank) + "]";
    string pfx1 = "[" + StrUtil::toStr(src) + "]";
    DIAG_DevMsgIf(1, profile->metricMgr()->toString(pfx0.c_str()));
    DIAG_DevMsgIf(1, new_profile->metricMgr()->toString(pfx1.c_str()));
  }
    
  int mergeTy = Prof::CallPath::Profile::Merge_MergeMetricByName;
  profile->merge(*new_profile, mergeTy);

  // merging the perf event statistics
  profile->metricMgr()->mergePerfEventStatistics(new_profile->metricMgr());

  if (DBG_CCT_MERGE) {
    string pfx = ("[" + StrUtil::toStr(src)
		  + " => " + StrUtil::toStr(myRank) + "]");
    DIAG_DevMsgIf(1, profile->metricMgr()->toString(pfx.c_str()));
  }

  delete new_profile;
}

void
packSend(std::pair<Prof::CallPath::Profile*,
	                ParallelAnalysis::PackedMetrics*> data,
	 int dest, int myRank, MPI_Comm comm)
{
  Prof::CallPath::Profile* profile = data.first;
  ParallelAnalysis::PackedMetrics* packedMetrics = data.second;
  packMetrics(*profile, *packedMetrics);
  MPI_Send(packedMetrics->data(), packedMetrics->dataSize(),
	   MPI_DOUBLE, dest, myRank, comm);
}

void
recvMerge(std::pair<Prof::CallPath::Profile*,
	  ParallelAnalysis::PackedMetrics*> data,
	  int src, int myRank, MPI_Comm comm)
{
  Prof::CallPath::Profile* profile = data.first;
  ParallelAnalysis::PackedMetrics* packedMetrics = data.second;

  // receive new metric data from src
  MPI_Status mpistat;
  MPI_Recv(packedMetrics->data(), packedMetrics->dataSize(),
	   MPI_DOUBLE, src, src, comm, &mpistat);
  DIAG_Assert(packedMetrics->verify(), DIAG_UnexpectedInput);
  unpackMetrics(*profile, *packedMetrics);
}

void
packSend(StringSet *stringSet,
	 int dest, int myRank, MPI_Comm comm)
{
  uint8_t* stringSetBuf = NULL;
  size_t stringSetBufSz = 0;
  packStringSet(*stringSet, &stringSetBuf, &stringSetBufSz);
  MPI_Send(stringSetBuf, (int)stringSetBufSz, MPI_BYTE, 
	   dest, myRank, comm);
  free(stringSetBuf);
}

void
recvMerge(StringSet *stringSet,
	  int src, int myRank, MPI_Comm comm)
{
  // determine size of incoming packed directory set from src
  MPI_Status mpistat;
  MPI_Probe(src, src, comm, &mpistat);
  int stringSetBufSz = 0;
  MPI_Get_count(&mpistat, MPI_BYTE, &stringSetBufSz);

  // receive new stringSet from src
  uint8_t *stringSetBuf = new uint8_t[stringSetBufSz];
  MPI_Recv(stringSetBuf, stringSetBufSz, MPI_BYTE, 
	   src, src, comm, &mpistat);
  StringSet *new_stringSet =
    unpackStringSet(stringSetBuf, (size_t) stringSetBufSz);
  delete[] stringSetBuf;
  *stringSet += *new_stringSet;
  delete new_stringSet;
}


//***************************************************************************

void
packProfile(const Prof::CallPath::Profile& profile,
	    uint8_t** buffer, size_t* bufferSz)
{
  // open_memstream: mallocs buffer and sets bufferSz
  FILE* fs = open_memstream((char**)buffer, bufferSz);

  uint wFlags = Prof::CallPath::Profile::WFlg_VirtualMetrics;
  Prof::CallPath::Profile::fmt_fwrite(profile, fs, wFlags);

  fclose(fs);
}


Prof::CallPath::Profile*
unpackProfile(uint8_t* buffer, size_t bufferSz)
{
  FILE* fs = fmemopen(buffer, bufferSz, "r");

  Prof::CallPath::Profile* prof = NULL;
  uint rFlags = Prof::CallPath::Profile::RFlg_VirtualMetrics;
  Prof::CallPath::Profile::fmt_fread(prof, fs, rFlags,
				     "(ParallelAnalysis::unpackProfile)",
				     NULL, NULL, false);

  fclose(fs);
  return prof;
}



//***************************************************************************

static void
packStringSet(const StringSet& stringSet,
	      uint8_t** buffer, size_t* bufferSz)
{
  // open_memstream: malloc buffer and sets bufferSz
  FILE* fs = open_memstream((char**)buffer, bufferSz);

  StringSet::fmt_fwrite(stringSet, fs);

  fclose(fs);
}


static StringSet*
unpackStringSet(uint8_t* buffer, size_t bufferSz)
{
  FILE* fs = fmemopen(buffer, bufferSz, "r");

  StringSet* stringSet = NULL;

  StringSet::fmt_fread(stringSet, fs);

  fclose(fs);

  return stringSet;
}


//***************************************************************************

void
packMetrics(const Prof::CallPath::Profile& profile,
	    ParallelAnalysis::PackedMetrics& packedMetrics)
{
  Prof::CCT::Tree& cct = *profile.cct();

  // pack derived metrics [mDrvdBeg, mDrvdEnd) from 'profile' into
  // 'packedMetrics'
  uint mDrvdBeg = packedMetrics.mDrvdBegId();
  uint mDrvdEnd = packedMetrics.mDrvdEndId();

  DIAG_Assert(packedMetrics.numNodes() == cct.maxDenseId() + 1, "");
  DIAG_Assert(packedMetrics.numMetrics() == mDrvdEnd - mDrvdBeg, "");

  for (Prof::CCT::ANodeIterator it(cct.root()); it.Current(); ++it) {
    Prof::CCT::ANode* n = it.current();
    for (uint mId1 = 0, mId2 = mDrvdBeg; mId2 < mDrvdEnd; ++mId1, ++mId2) {
      packedMetrics.idx(n->id(), mId1) = n->metric(mId2);
    }
  }
}


void
unpackMetrics(Prof::CallPath::Profile& profile,
	      const ParallelAnalysis::PackedMetrics& packedMetrics)
{
  Prof::CCT::Tree& cct = *profile.cct();

  // 1. unpack 'packedMetrics' into temporary derived metrics [mBegId,
  //    mEndId) in 'profile'
  uint mBegId = packedMetrics.mBegId(), mEndId = packedMetrics.mEndId();

  DIAG_Assert(packedMetrics.numNodes() == cct.maxDenseId() + 1, "");
  DIAG_Assert(packedMetrics.numMetrics() == mEndId - mBegId, "");

  for (uint nodeId = 1; nodeId < packedMetrics.numNodes(); ++nodeId) {
    for (uint mId1 = 0, mId2 = mBegId; mId2 < mEndId; ++mId1, ++mId2) {
      Prof::CCT::ANode* n = cct.findNode(nodeId);
      n->demandMetric(mId2) = packedMetrics.idx(nodeId, mId1);
    }
  }

  // 2. update derived metrics [mDrvdBeg, mDrvdEnd) based on new
  //    values in [mBegId, mEndId)
  uint mDrvdBeg = packedMetrics.mDrvdBegId();
  uint mDrvdEnd = packedMetrics.mDrvdEndId();
  cct.root()->computeMetricsIncr(*profile.metricMgr(), mDrvdBeg, mDrvdEnd,
				 Prof::Metric::AExprIncr::FnCombine);
}



//***************************************************************************

} // namespace ParallelAnalysis
