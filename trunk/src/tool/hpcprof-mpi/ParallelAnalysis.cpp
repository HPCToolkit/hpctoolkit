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

//*************************** Forward Declarations **************************

//***************************************************************************


namespace ParallelAnalysis {

//***************************************************************************
// 
//***************************************************************************

void
broadcast(Prof::CallPath::Profile*& profile,
	  int myRank, int maxRank, MPI_Comm comm)
{
  if (myRank != RankTree::rootRank) {
    DIAG_Assert(!profile, "ParallelAnalysis::broadcast: " << DIAG_UnexpectedInput);
    profile = new Prof::CallPath::Profile("[ParallelAnalysis::broadcast]");
    profile->isMetricMgrVirtual(true);
  }
  
  int max_level = RankTree::level(maxRank);
  for (int level = 0; level < max_level; ++level) {
    int i_beg = RankTree::begNode(level);
    int i_end = std::min(maxRank, RankTree::endNode(level));
    
    for (int i = i_beg; i <= i_end; ++i) {
      // merge i into its left child (i_lchild)
      int i_lchild = RankTree::leftChild(i);
      if (i_lchild <= maxRank) {
	mergeNonLocal(profile, i_lchild, i, myRank);
      }

      // merge i into its right child (i_rchild)
      int i_rchild = RankTree::rightChild(i);
      if (i_rchild <= maxRank) {
	mergeNonLocal(profile, i_rchild, i, myRank);
      }
    }

    MPI_Barrier(comm);
  }
}


void
mergeNonLocal(Prof::CallPath::Profile* profile, int rank_x, int rank_y,
	      int myRank, MPI_Comm comm)
{
  MPI_Status mpistat;

  uint8_t* profileBuf = NULL;
  size_t profileBufSz = 0;

  Prof::CallPath::Profile* profile_x = NULL;
  Prof::CallPath::Profile* profile_y = NULL;

  if (myRank == rank_x) {
    profile_x = profile;

    // rank_x receives profile buffer size from rank_y
    MPI_Recv(&profileBufSz, 1, MPI_UNSIGNED_LONG, rank_y, 0, comm, &mpistat);

    profileBuf = new uint8_t[profileBufSz];

    // rank_x receives profile from rank_y
    MPI_Recv(profileBuf, profileBufSz, MPI_BYTE, rank_y, 0, comm, &mpistat);

    profile_y = unpackProfile(profileBuf, profileBufSz);
    delete[] profileBuf;
    
    int mergeTy = Prof::CallPath::Profile::Merge_mergeMetricByName;
    profile_x->merge(*profile_y, mergeTy);

    delete profile_y;
  }

  if (myRank == rank_y) {
    profile_y = profile;

    packProfile(*profile_y, &profileBuf, &profileBufSz);

    // rank_y sends profile buffer size to rank_x
    MPI_Send(&profileBufSz, 1, MPI_UNSIGNED_LONG, rank_x, 0, comm);

    // rank_y sends profile to rank_x
    MPI_Send(profileBuf, profileBufSz, MPI_BYTE, rank_x, 0, comm);

    free(profileBuf);
  }
}


void
mergeNonLocal(std::pair<Prof::CallPath::Profile*,
	                ParallelAnalysis::PackedMetrics*> data,
	      int rank_x, int rank_y, int myRank, MPI_Comm comm)
{
  MPI_Status mpistat;

  if (myRank == rank_x) {
    Prof::CallPath::Profile* profile_x = data.first;
    ParallelAnalysis::PackedMetrics* packedMetrics_x = data.second;

    // rank_x receives metric data from rank_y
    MPI_Recv(packedMetrics_x->data(), packedMetrics_x->dataSize(),
	     MPI_DOUBLE, rank_y, 0, comm, &mpistat);
    DIAG_Assert(packedMetrics_x->verify(), DIAG_UnexpectedInput);
    
    unpackMetrics(*profile_x, *packedMetrics_x);
  }

  if (myRank == rank_y) {
    Prof::CallPath::Profile* profile_y = data.first;
    ParallelAnalysis::PackedMetrics* packedMetrics_y = data.second;

    packMetrics(*profile_y, *packedMetrics_y);
    
    // rank_y sends metric data to rank_x
    MPI_Send(packedMetrics_y->data(), packedMetrics_y->dataSize(),
	     MPI_DOUBLE, rank_x, 0, comm);
  }
}


//***************************************************************************

void
packProfile(const Prof::CallPath::Profile& profile,
	    uint8_t** buffer, size_t* bufferSz)
{
  // open_memstream: mallocs buffer and sets bufferSz
  FILE* fs = open_memstream((char**)buffer, bufferSz);

  uint wFlags = Prof::CallPath::Profile::WFlg_virtualMetrics;
  Prof::CallPath::Profile::fmt_fwrite(profile, fs, wFlags);

  fclose(fs);
}


Prof::CallPath::Profile*
unpackProfile(uint8_t* buffer, size_t bufferSz)
{
  FILE* fs = fmemopen(buffer, bufferSz, "r");

  Prof::CallPath::Profile* prof = NULL;
  uint rFlags = Prof::CallPath::Profile::RFlg_virtualMetrics;
  Prof::CallPath::Profile::fmt_fread(prof, fs, rFlags,
				     "ParallelAnalysis::unpack", NULL, NULL);

  fclose(fs);
  return prof;
}


//***************************************************************************

void
packMetrics(const Prof::CallPath::Profile& profile,
	    ParallelAnalysis::PackedMetrics& packedMetrics)
{
  Prof::CCT::Tree& cct = *profile.cct();

  uint mBegId = packedMetrics.mBegId(), mEndId = packedMetrics.mEndId();

  DIAG_Assert(packedMetrics.numNodes() == cct.maxDenseId() + 1, "");
  DIAG_Assert(packedMetrics.numMetrics() == mEndId - mBegId, "");

  for (Prof::CCT::ANodeIterator it(cct.root()); it.Current(); ++it) {
    Prof::CCT::ANode* n = it.current();
    for (uint mId1 = 0, mId2 = mBegId; mId2 < mEndId; ++mId1, ++mId2) {
      packedMetrics.idx(n->id(), mId1) = n->demandMetric(mId2);
    }
  }
}


void
unpackMetrics(Prof::CallPath::Profile& profile,
	      const ParallelAnalysis::PackedMetrics& packedMetrics)
{
  Prof::CCT::Tree& cct = *profile.cct();

  uint mBegId = packedMetrics.mBegId(), mEndId = packedMetrics.mEndId();

  DIAG_Assert(packedMetrics.numNodes() == cct.maxDenseId() + 1, "");
  DIAG_Assert(packedMetrics.numMetrics() == mEndId - mBegId, "");

  for (uint nodeId = 1; nodeId < packedMetrics.numNodes(); ++nodeId) {
    for (uint mId1 = 0, mId2 = mBegId; mId2 < mEndId; ++mId1, ++mId2) {
      Prof::CCT::ANode* n = cct.findNode(nodeId);
      n->demandMetric(mId2) = packedMetrics.idx(nodeId, mId1);
    }
  }

  // TODO: apply Metric::AExprItrv::update() [*** ids of original drvd metrics]
}



//***************************************************************************

} // namespace ParallelAnalysis
