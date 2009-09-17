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

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "CCTAnalysis.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************


namespace CCTAnalysis {

#if 0
//***************************************************************************
// 
//***************************************************************************

// reduce: form a canonical CCT structure
//
// use lg(n) barriers, level-by-level
void
reduce(Prof::CallPath::Profile* profile,
       int myRank, int maxRank, MPI_Comm comm)
{
  for (int level = RankTree::level(maxRank); level >= 1; --level) {
    int i_beg = RankTree::begNode(level);
    int i_end = std::min(maxRank, RankTree::endNode(level));
    
    for (int i = i_beg; i <= i_end; i += 2) {

      // INVARIANTS:
      //   - i is left child of i_parent
      //   - (i + 1) is right child of i_parent (if it exists)
      int i_parent = RankTree::parent(i);

      // merge i into i_parent
      mergeNonLocal(profile, i_parent, i, myRank);

      // merge (i + 1) into i_parent
      if ((i + 1) <= i_end) {
	mergeNonLocal(profile, i_parent, i + 1, myRank);
      }
    }
    
    MPI_Barrier(comm);
  }
}


void
broadcast(Prof::CallPath::Profile* profile,
	  int myRank, int maxRank, MPI_Comm comm) 
{
  // use lg(n) barriers, level-by-level

  int max_level = RankTree::level(maxRank);
  for (int level = 0; level < max_level; ++level) {
    int i_beg = RankTree::begNode(level);
    int i_end = std::min(maxRank, RankTree::endNode(level));
    
    for (int i = i_beg; i <= i_end; ++i) {
      int i_lchild = RankTree::leftChild(i);
      if (i_lchild <= maxRank) {
	mergeNonLocal(profile, i_lchild, i, myRank);
      }

      int i_rchild = RankTree::rightChild(i);
      if (i_rchild <= maxRank) {
	mergeNonLocal(profile, i_rchild, i, myRank);
      }
    }

    MPI_Barrier(comm);
  }
}


// merge from y into x
void
mergeNonLocal(Prof::CallPath::Profile* profile, int myRank, 
	      int rank_x, int rank_y, MPI_Comm comm)
{
  if (myRank == rank_x) {
    // TODO: create CCT receive buffer

    // rank_x receives from rank_y
    MPI_Status status;
    MPI_Recv(buf, count, datatype, rank_y, tag, comm, status);

    Prof::CallPath::Profile* profile_y = cct_unpack(buffer, bufferSz);
    // TODO: merge profile_y into profile
  }

  if (myRank == rank_y) {
    void* buffer = NULL;
    size_t buferSz = 0;
    int ret = cct_pack(profile, buffer, bufferSz);

    // rank_y sends to rank_x
    MPI_Send(buffer, bufferSz, MPI_BYTE, rank_x, tag, comm);
  }
}


//***************************************************************************

// Tie a memory buffer to a FILE stream!

void
cct_pack(Prof::CallPath::Profile* profile, void** buffer, size_t* bufferSz)
{
  // FILE *open_memstream(char **ptr, size_t *sizeloc);
  FILE* fs = open_memstream(&buffer, &bufferSz);
  profile->write(fs); // TODO: Prof::CallPath::Profile::write()
}


Prof::CallPath::Profile*
cct_unpack(void* buffer, size_t bufferSz)
{
  // FILE *fmemopen(void *buf, size_t size, const char *mode);
  FILE* fs = fmemopen(buffer, bufferSz, "r");
  Prof::CallPath::Profile* prof = Prof::CallPath::Profile::make(fs);
  return prof;
}

//***************************************************************************
#endif

} // namespace CCTAnalysis
