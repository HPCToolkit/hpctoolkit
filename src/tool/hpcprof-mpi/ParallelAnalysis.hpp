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

#ifndef ParallelAnalysis_hpp
#define ParallelAnalysis_hpp

//**************************** MPI Include Files ****************************

#include <mpi.h>

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <vector>

#include <cmath>
#include <cstring> // for memset()

#include <stdint.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof/CallPath-Profile.hpp>

#include <lib/support/Unique.hpp>

//*************************** Forward Declarations **************************


//***************************************************************************
// RankTree: Implicit tree representation for reductions/broadcasts
//***************************************************************************

namespace RankTree {

// Representing a 1-based list of ranks (from 1 to n) as a binary tree:
//
//             1               |  level 0
//          __/ \__            |
//        2         3          |  level 1
//       / \       / \         |
//      4   5     6   7        |  level 2
//    / |  / \   / \  | \      |
//   8  9 10 11 12 13 14 15    |  level 3
//
// For rank i:
//   parent(i):      floor(i/2), if i > 1
//   left-child(i):  2i
//   right-child(i): 2i + 1
//   
// Let the root have level 0.  Then, level l:
//   begins with node 2^l
//   ends with node 2^(l + 1) - 1 (assuming a complete level)
// 
// Given node i, its level is floor(log2(i))

const int rootRank = 0;

inline int 
make1BasedRank(int rank0)
{
  return (rank0 + 1);
}


inline int 
make0BasedRank(int rank1)
{
  return (rank1 - 1);
}


// Given a 0-based rank for a node in the binary tree, returns its level
inline int 
parent(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int parent1 = rank1 / 2; //  floor() via truncation
  return make0BasedRank(parent1);
}


// Given a 0-based rank for a node in the binary tree, returns a
// 0-based rank for its left child
inline int 
leftChild(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int child1 = 2 * rank1;
  return make0BasedRank(child1);
}


// Given a 0-based rank for a node in the binary tree, returns a
// 0-based rank for its right child
inline int 
rightChild(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int child1 = 2 * rank1 + 1;
  return make0BasedRank(child1);
}


// level: Given a 0-based rank for a node in the binary tree, returns
// its parent's 0-based rank
inline int 
level(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int level = (int) log2(rank1); // floor() via truncation
  return level;
}


// begNode: Given a tree level, return the first node (0-based rank)
// on that level
inline int 
begNode(int level)
{
  int rank1 = (int) pow(2.0, level);
  return make0BasedRank(rank1);
}


inline int 
endNode(int level)
{
  int rank1 = (int) pow(2.0, level + 1) - 1;
  return make0BasedRank(rank1);
}


} // namespace RankTree


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
// mergeNonLocal: merge profile on rank_y into profile on rank_x
// ------------------------------------------------------------------------

void
mergeNonLocal(Prof::CallPath::Profile* profile, int rank_x, int rank_y,
	      int myRank, MPI_Comm comm = MPI_COMM_WORLD);

void
mergeNonLocal(std::pair<Prof::CallPath::Profile*,
                        ParallelAnalysis::PackedMetrics*> data,
	      int rank_x, int rank_y, int myRank,
	      MPI_Comm comm = MPI_COMM_WORLD);


// ------------------------------------------------------------------------
// reduce: Uses a tree-based reduction to reduce the profile at every
// rank into a canonical profile at the tree's root, rank 0.  Assumes
// 0-based ranks.  Uses lg(maxRank) barriers, one at each level of the
// binary tree.
// 
// T: Prof::CallPath::Profile*
// T: std::pair<Prof::CallPath::Profile*, ParallelAnalysis::PackedMetrics*>
// ------------------------------------------------------------------------

template<typename T>
void
reduce(T object, int myRank, int maxRank, MPI_Comm comm = MPI_COMM_WORLD)
{
  for (int level = RankTree::level(maxRank); level >= 1; --level) {
    int i_beg = RankTree::begNode(level);
    int i_end = std::min(maxRank, RankTree::endNode(level));
    
    for (int i = i_beg; i <= i_end; i += 2) {
      int parent = RankTree::parent(i);
      int lchild = i;     // left child of parent
      int rchild = i + 1; // right child of parent (if it exists)

      // merge lchild into parent (merging left child first maintains
      // metric order for CallPath::Profiles)
      mergeNonLocal(object, parent, lchild, myRank);

      // merge rchild into parent
      if (rchild <= i_end) {
	mergeNonLocal(object, parent, rchild, myRank);
      }
    }
    
    MPI_Barrier(comm);
  }
}


// ------------------------------------------------------------------------
// broadcast: Use a tree-based broadcast to broadcast the profile at
// the tree's root (rank 0) to every other rank.  Assumes 0-based
// ranks.  Uses lg(maxRank) barriers, one at each level of the binary
// tree.
// ------------------------------------------------------------------------
void
broadcast(Prof::CallPath::Profile*& profile, int myRank, int maxRank, 
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
