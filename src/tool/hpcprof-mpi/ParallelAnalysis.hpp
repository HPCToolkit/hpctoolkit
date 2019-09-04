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
// Copyright ((c)) 2002-2019, Rice University
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

#ifndef ParallelAnalysis_hpp
#define ParallelAnalysis_hpp

//**************************** MPI Include Files ****************************

#include <mpi.h>

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <vector>

#include <cstring> // for memset()

#include <stdint.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof/CallPath-Profile.hpp>

#include <lib/support/Unique.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// PackedMetrics: a packable matrix
//***************************************************************************

namespace ParallelAnalysis {

class PackedMetrics
  : public Unique // prevent copying
{
public:
  // [mBegId, mEndId)
  PackedMetrics(uint numNodes, uint mBegId, uint mEndId,
		uint mDrvdBegId, uint mDrvdEndId)
    : m_numNodes(numNodes), m_numMetrics(mEndId - mBegId),
      m_mBegId(mBegId), m_mEndId(mEndId),
      m_mDrvdBegId(mDrvdBegId), m_mDrvdEndId(mDrvdEndId)
  {
    size_t sz = dataSize();
    m_packedData = new double[sz];

    // initialize first (unused) row to avoid bogus valgrind warnings
    memset(m_packedData, 0, (m_numHdr + m_numMetrics) * sizeof(double));

    m_packedData[m_numNodesIdx] = (double)m_numNodes;
    m_packedData[m_mBegIdIdx]   = (double)m_mBegId;
    m_packedData[m_mEndIdIdx]   = (double)m_mEndId;
  }

  // PackedMetrics(double* packedMatrix) { }

  ~PackedMetrics()
  { delete[] m_packedData; }


  // 0 based indexing (row-major layout)
  double
  idx(uint idxNodes, uint idxMetrics) const
  { return m_packedData[m_numHdr + (m_numMetrics * idxNodes) + idxMetrics]; }


  double&
  idx(uint idxNodes, uint idxMetrics)
  { return m_packedData[m_numHdr + (m_numMetrics * idxNodes) + idxMetrics]; }

  
  uint
  numNodes() const
  { return m_numNodes; }

  uint
  numMetrics() const
  { return m_numMetrics; }


  uint
  mBegId() const
  { return m_mBegId; }

  uint
  mEndId() const
  { return m_mEndId; }


  uint
  mDrvdBegId() const
  { return m_mDrvdBegId; }

  uint
  mDrvdEndId() const
  { return m_mDrvdEndId; }


  bool
  verify() const
  {
    return (m_numNodes      == (uint)m_packedData[m_numNodesIdx]
	    && m_mBegId     == (uint)m_packedData[m_mBegIdIdx]
	    && m_mEndId     == (uint)m_packedData[m_mEndIdIdx]);
  }

  
  double*
  data() const
  { return m_packedData; }

  // dataSize: size in terms of elements
  uint
  dataSize() const
  { return (m_numNodes * m_numMetrics) + m_numHdr; }

private:
  static const uint m_numHdr = 4;
  static const uint m_numNodesIdx   = 0;
  static const uint m_mBegIdIdx     = 1;
  static const uint m_mEndIdIdx     = 2;

  uint m_numNodes;   // rows
  uint m_numMetrics; // columns
  uint m_mBegId, m_mEndId; // [ )

  uint m_mDrvdBegId, m_mDrvdEndId; // [ )

  double* m_packedData; // use row-major layout

};

} // namespace ParallelAnalysis


//***************************************************************************
// reduce/broadcast
//***************************************************************************

namespace ParallelAnalysis {

// ------------------------------------------------------------------------
// recvMerge: merge profile on rank_y into profile on rank_x
// ------------------------------------------------------------------------

void
packSend(Prof::CallPath::Profile* profile,
	 int dest, int myRank, MPI_Comm comm = MPI_COMM_WORLD);
void
recvMerge(Prof::CallPath::Profile* profile,
	  int src, int myRank, MPI_Comm comm = MPI_COMM_WORLD);

void
packSend(std::pair<Prof::CallPath::Profile*,
	                ParallelAnalysis::PackedMetrics*> data,
	 int dest, int myRank, MPI_Comm comm = MPI_COMM_WORLD);
void
recvMerge(std::pair<Prof::CallPath::Profile*,
	  ParallelAnalysis::PackedMetrics*> data,
	  int src, int myRank, MPI_Comm comm = MPI_COMM_WORLD);

void
packSend(StringSet *stringSet,
	 int dest, int myRank, MPI_Comm comm = MPI_COMM_WORLD);
void
recvMerge(StringSet *stringSet,
	  int src, int myRank, MPI_Comm comm = MPI_COMM_WORLD);

// ------------------------------------------------------------------------
// reduce: Uses a tree-based reduction to reduce the profile at every
// rank into a canonical profile at the tree's root, rank 0.  Assumes
// 0-based ranks.
// 
// T: Prof::CallPath::Profile*
// T: std::pair<Prof::CallPath::Profile*, ParallelAnalysis::PackedMetrics*>
// ------------------------------------------------------------------------

template<typename T>
void
reduce(T object, int myRank, int numRanks, MPI_Comm comm = MPI_COMM_WORLD)
{
  int lchild = 2 * myRank + 1;
  if (lchild < numRanks) {
    recvMerge(object, lchild, myRank);
    int rchild = 2 * myRank + 2;
    if (rchild < numRanks) {
      recvMerge(object, rchild, myRank);
    }
  }
  if (myRank > 0) {
    int parent = (myRank - 1) / 2;
    packSend(object, parent, myRank);
  }
}


// ------------------------------------------------------------------------
// broadcast: Broadcast the profile at the tree's root (rank 0) to every
// other rank.  Assumes 0-based ranks.
// ------------------------------------------------------------------------
void
broadcast(Prof::CallPath::Profile*& profile, int myRank,
	  MPI_Comm comm = MPI_COMM_WORLD);
void
broadcast(StringSet &stringSet, int myRank,
	  MPI_Comm comm = MPI_COMM_WORLD);

// ------------------------------------------------------------------------
// pack/unpack a profile to/from a buffer
// ------------------------------------------------------------------------
void
packProfile(const Prof::CallPath::Profile& profile,
	    uint8_t** buffer, size_t* bufferSz);


Prof::CallPath::Profile*
unpackProfile(uint8_t* buffer, size_t bufferSz);


// ------------------------------------------------------------------------
// pack/unpack a metrics from/to a profile
// ------------------------------------------------------------------------

// packMetrics: pack the given metric values from 'profile' into
// 'packedMetrics'
void
packMetrics(const Prof::CallPath::Profile& profile,
	    ParallelAnalysis::PackedMetrics& packedMetrics);

// unpackMetrics: unpack 'packedMetrics' into profile and apply metric update
void
unpackMetrics(Prof::CallPath::Profile& profile,
	      const ParallelAnalysis::PackedMetrics& packedMetrics);

} // namespace ParallelAnalysis


//***************************************************************************

#endif // ParallelAnalysis_hpp 
