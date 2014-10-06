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
// Copyright ((c)) 2002-2014, Rice University
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

// This file implements the prof-mpi support for making the metric db
// (plot) graphs and the trace db file in the new metric DB format.


//***************************************************************************

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "Plot-Trace.hpp"

#include <include/uint.h>
#include <lib/support/diagnostics.h>
#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/CCT-Tree.hpp>

#define MEG  (1024 * 1024)

struct plot_pt_s {
  uint  cctid;
  uint  metid;
  uint  tid;
  float val;
};

typedef struct plot_pt_s plot_pt;

// global data

static plot_pt * send_buf = NULL;
static plot_pt * recv_buf = NULL;

static void
dumpCCTsizes(ulong *cct_sum, ulong *cct_prefix, uint *rank_start, uint maxid);

static void
dumpGroupSizes(ulong *cct_global, uint *rank_start, uint maxid, int numRanks);

//***************************************************************************

// Internal helper functions

// Use ranks [ranklo, rankhi] to partition CCT ids [cctlo, ccthi] into
// subranges and try to minimize the size of the largest interval.
// prefix[k] is the prefix sum of the cct sizes 1..k (inclusive), and
// both cct and rank ranges must be non-empty.
//
// Returns: start[k] is the lowest cctid assigned to rank k.
//
static void
split(uint cctlo, uint ccthi, int ranklo, int rankhi,
      ulong *prefix, uint *start)
{
  // only one rank
  if (ranklo == rankhi) {
    start[ranklo] = cctlo;
    return;
  }

  // only one cct node, but must assign all ranks.
  if (cctlo == ccthi) {
    start[ranklo] = cctlo;
    for (int r = ranklo + 1; r <= rankhi; r++) {
      start[r] = cctlo + 1;
    }
    return;
  }

  // split the cct sizes as evenly as possible.  if num ranks is odd,
  // then split proportionately and minimize the ratio of cct size per
  // num ranks.  we could use binary search here.
  //
  int rankmid = (ranklo + rankhi)/2;
  int N1 = rankmid - ranklo + 1;
  int N2 = rankhi - rankmid;
  uint cctmid = cctlo;
  ulong min_score = (N1 + N2) * (prefix[ccthi] - prefix[cctlo - 1]);

  for (uint id = cctlo; id < ccthi; id++) {
    ulong S1 = prefix[id] - prefix[cctlo - 1];
    ulong S2 = prefix[ccthi] - prefix[id];
    ulong score = std::max(N2*S1, N1*S2);

    if (score < min_score) {
      min_score = score;
      cctmid = id;
    }
  }

  // split into [cctlo, cctmid] and [cctmid+1, ccthi]
  split(cctlo, cctmid, ranklo, rankmid, prefix, start);
  split(cctmid + 1, ccthi, rankmid + 1, rankhi, prefix, start);
}

// Returns: the size of the largest group for the send/recv buffers
// across all ranks and groups.  Note: size is number of plot points,
// not bytes.
//
static ulong
partition_cctids(ulong *cct_global, uint maxid, int numRanks)
{
  size_t num_elts = maxid + 1;
  int rank, group;
  uint id;

  ulong * cct_sum = (ulong *) malloc(num_elts * sizeof(ulong));
  ulong * cct_prefix = (ulong *) malloc(num_elts * sizeof(ulong));
  uint * rank_start = (uint *) malloc((numRanks + 1) * sizeof(uint));

  DIAG_Assert(cct_sum != NULL && cct_prefix != NULL && rank_start != NULL,
	      "out of memory in Plot::initArrays");

  // cct_sum[] is the number of plot points per CCT node summed over
  // all ranks.
  for (id = 0; id <= maxid; id++) {
    cct_sum[id] = 0;
  }
  for (rank = 0; rank < numRanks; rank++) {
    for (id = 1; id <= maxid; id++) {
      cct_sum[id] += cct_global[rank * num_elts + id];
    }
  }

  // cct_prefix[] is the prefix sum of cct_sum[].
  cct_prefix[0] = 0;
  for (id = 1; id <= maxid; id++) {
    cct_prefix[id] = cct_prefix[id - 1] + cct_sum[id];
  }

  // partition the cct nodes into approx equal-sized groups
  split(1, maxid, 0, numRanks - 1, cct_prefix, rank_start);
  rank_start[numRanks] = maxid + 1;

  // find max size of one group
  ulong group_size = 0;
  for (rank = 0; rank < numRanks; rank++) {
    for (group = 0; group < numRanks; group++) {
      ulong size = 0;
      for (id = rank_start[group]; id < rank_start[group + 1]; id++) {
	size += cct_global[rank * num_elts + id];
      }
      group_size = std::max(group_size, size);
    }
  }

  if (0) {
    dumpCCTsizes(cct_sum, cct_prefix, rank_start, maxid);
    dumpGroupSizes(cct_global, rank_start, maxid, numRanks);
  }

  free(cct_sum);
  free(cct_prefix);
  free(rank_start);

  return group_size;
}

//***************************************************************************

namespace Plot {

int
allocBuffers(Prof::CallPath::Profile & prof,
	     int myRank, int numRanks, int rootRank)
{
  Prof::CCT::Tree * cct = prof.cct();
  uint maxid = cct->maxDenseId();
  uint id;  int ret, rank;

  ulong size_local = (maxid + 1) * sizeof(ulong);
  ulong size_global = numRanks * size_local;
  ulong * cct_local = (ulong *) malloc(size_local);
  ulong * cct_global = NULL;

  DIAG_Assert(cct_local != NULL, "out of memory in Plot::initArrays");

  if (myRank == rootRank) {
    cct_global = (ulong *) malloc(size_global);
    DIAG_Assert(cct_global != NULL, "out of memory in Plot::initArrays");
  }

  // rank 0 gathers the local plot counts and divides the CCT nodes
  // into groups.
  //
  cct_local[0] = 0;
  for (id = 1; id <= maxid; id++) {
    cct_local[id] = cct->findNode(id)->m_numPlotMetrics;
  }

  ret = MPI_Gather((void *) cct_local,  maxid + 1, MPI_LONG,
		   (void *) cct_global, maxid + 1, MPI_LONG,
		   rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Gather for Plot::initArrays failed");

  // rank 0 partitions the cct ids, computes the max group size and
  // broadcasts the result.
  //
  ulong group_size = 0;

  if (myRank == rootRank) {
    group_size = partition_cctids(cct_global, maxid, numRanks);
  }

  ret = MPI_Bcast(&group_size, 1, MPI_LONG, rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Bcast for Plot::initArrays failed");

  // every rank tries to malloc the send/recv buffers.  rank 0 gathers
  // the success of each rank and broadcasts the result.
  //
  int *success_gbl = NULL;
  int success, my_success;

  if (myRank == rootRank) {
    success_gbl = (int *) malloc(numRanks * sizeof(int));
    DIAG_Assert(success_gbl != NULL, "out of memory in Plot::initArrays");
  }

  send_buf = (plot_pt *) malloc(group_size * numRanks * sizeof(plot_pt));
  recv_buf = (plot_pt *) malloc(group_size * numRanks * sizeof(plot_pt));

  my_success = (send_buf != NULL) && (recv_buf != NULL);

  ret = MPI_Gather((void *) &my_success, 1, MPI_INT,
		   (void *) success_gbl, 1, MPI_INT,
		   rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Gather for Plot::initArrays failed");

  if (myRank == rootRank) {
    success = 1;
    for (rank = 0; rank < numRanks; rank++) {
      success = success && success_gbl[rank];
    }
  }

  ret = MPI_Bcast(&success, 1, MPI_INT, rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Bcast for Plot::initArrays failed");

  if (myRank == rootRank) {
    double bufsize = (group_size * numRanks * sizeof(plot_pt))/((double) MEG);

    printf("\nplot graph buffer: %.2f meg, grand total: %.2f meg, in memory: %s\n",
	   bufsize, 2 * bufsize * numRanks,
	   success ? "yes" : "no");
  }

  return 0;
}

}  // namespace Plot

//***************************************************************************

// Debugging output

static void
dumpCCTsizes(ulong *cct_sum, ulong *cct_prefix, uint *rank_start,
	     uint maxid)
{
  uint id;  int rank;

  rank = 0;
  printf("\ncct size and prefix table\n");
  for (id = 1; id <= maxid; id++) {
    while (id >= rank_start[rank + 1]) {
      rank++;
    }
    printf("cct: %d, num: %ld, prefix: %ld, rank: %d\n",
	   id, cct_sum[id], cct_prefix[id], rank);
  }
}

static void
dumpGroupSizes(ulong *cct_global, uint *rank_start, uint maxid, int numRanks)
{
  uint num_elts = maxid + 1;
  ulong group_size, total;
  uint id;  int rank, group;

  ulong *column = (ulong *) malloc(numRanks * sizeof(ulong));

  for (rank = 0; rank < numRanks; rank++) {
    column[rank] = 0;
  }

  printf("\ngroup sizes table (plot points)\n");
  group_size = 0;

  for (rank = 0; rank < numRanks; rank++) {
    printf("rank: %3d  sizes:", rank);

    for (group = 0; group < numRanks; group++) {
      ulong size = 0;

      for (id = rank_start[group]; id < rank_start[group + 1]; id++) {
	size += cct_global[rank * num_elts + id];
      }
      printf(" %6ld", size);

      group_size = std::max(group_size, size);
      column[group] += size;
    }
    printf("  cct: %d--%d\n", rank_start[rank], rank_start[rank + 1] - 1);
  }

  printf("column sizes:");

  total = 0;
  for (rank = 0; rank < numRanks; rank++) {
    printf(" %6ld", column[rank]);
    total += column[rank];
  }
  printf("\n");
  printf("group size: %ld (%ld), per proc: %ld, total: %ld (%ld)\n",
	 group_size, total/(numRanks * numRanks) + 1,
	 group_size * numRanks,
	 group_size * numRanks * numRanks, total);
}
