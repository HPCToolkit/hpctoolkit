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

#ifndef CCTAnalysis_hpp
#define CCTAnalysis_hpp

//**************************** MPI Include Files ****************************

#include <mpi.h>

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <vector>

#include <cmath>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof-juicy/CallPath-Profile.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************


namespace CCTAnalysis {

void
reduce(Prof::CallPath::Profile* profile, int myRank, int maxRank, 
       MPI_Comm comm = MPI_COMM_WORLD);

void
broadcast(Prof::CallPath::Profile* profile, int myRank, int maxRank, 
	  MPI_Comm comm = MPI_COMM_WORLD);

void
mergeNonLocal(Prof::CallPath::Profile* profile, int myRank, 
	      int rank_x, int rank_y, MPI_Comm comm = MPI_COMM_WORLD);


} // namespace CCTAnalysis


//***************************************************************************
// RankTree: Implicit tree representation for reductions/broadcasts
//***************************************************************************

namespace RankTree {

// Representing a 1-based list of ranks (from 1 to n) as a binary tree:
//
//             1               |
//          __/ \__            |
//        2         3          |
//       / \       / \         |
//      4   5     6   7        |
//    / |  / \   / \  | \      |
//   8  9 10 11 12 13 14 15    |
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

static inline int 
make1BasedRank(int rank0)
{
  return (rank0 + 1);
}


static inline int 
make0BasedRank(int rank1)
{
  return (rank1 - 1);
}


// Given a 0-based rank for a node in the binary tree, returns its level
static inline int 
parent(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int parent1 = rank1 / 2; //  floor() via truncation
  return make0BasedRank(parent1);
}


// Given a 0-based rank for a node in the binary tree, returns a
// 0-based rank for its left child
static inline int 
leftChild(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int child1 = 2 * rank1;
  return make0BasedRank(child1);
}


// Given a 0-based rank for a node in the binary tree, returns a
// 0-based rank for its right child
static inline int 
rightChild(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int child1 = 2 * rank1 + 1;
  return make0BasedRank(child1);
}


// level: Given a 0-based rank for a node in the binary tree, returns
// its parent's 0-based rank
static inline int 
level(int rank0)
{
  int rank1 = make1BasedRank(rank0);
  int level = (int) log2(rank1); // floor() via truncation
  return level;
}


// begNode: Given a tree level, return the first node (0-based rank)
// on that level
static inline int 
begNode(int level)
{
  int rank1 = (int) pow(2.0, level);
  return make0BasedRank(rank1);
}


static inline int 
endNode(int level)
{
  int rank1 = (int) pow(2.0, level + 1) - 1;
  return make0BasedRank(rank1);
}


} // namespace RankTree



#endif // CCTAnalysis_hpp 
