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
// Copyright ((c)) 2002-2015, Rice University
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
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "Plot-Trace.hpp"
#include <lib/prof/DBFormat.h>

#include <include/uint.h>
#include <include/big-endian.h>
#include <lib/support/diagnostics.h>
#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Database.hpp>

#include <map>
#include <string>
#include <utility>

#define PLOT_INDEX_START  512
#define MEG  (1024 * 1024)

struct plot_pt_s {
  uint  cctid;
  uint  metid;
  uint  tid;
  float val;
};

typedef struct plot_pt_s plot_pt;

typedef std::pair <uint, uint> uint_pair;

// Plot graph global data

static plot_pt * plot_send_buf = NULL;
static plot_pt * plot_recv_buf = NULL;
static ulong send_buf_size = 0;

static uint * group_pos = NULL;
static uint * start_id = NULL;
static ulong group_size = 0;


static void
dumpCCTsizes(ulong *cct_sum, ulong *cct_prefix, uint *rank_start, uint maxid);

static void
dumpGroupSizes(ulong *cct_global, uint *rank_start, uint maxid, int numRanks);

static void
dumpPlotPoints(std::string & db_dir, plot_pt *buf, ulong num_points,
	       int myRank, int numRanks, int rootRank);

//***************************************************************************

// Internal helper functions

// Automatically restart short writes.
// Returns: 0 on success, -1 on failure.
//
static int
write_all(int fd, const void *buf, size_t count)
{
  ssize_t ret;
  size_t len;

  len = 0;
  while (len < count) {
    ret = write(fd, ((const char *) buf) + len, count - len);
    if (ret < 0 && errno != EINTR) {
      return -1;
    }
    if (ret > 0) {
      len += ret;
    }
  }

  return 0;
}

// Same as write_all() but call lseek() first.
//
static int
write_all_at(int fd, const void *buf, size_t count, off_t offset)
{
  off_t ret;

  ret = lseek(fd, offset, SEEK_SET);
  if (ret != offset) {
    return -1;
  }

  return write_all(fd, buf, count);
}

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
partition_cctids(ulong *cct_global, uint *start_id,
		 uint maxid, int numRanks)
{
  size_t num_elts = maxid + 1;
  int rank, group;
  uint id;

  ulong * cct_sum = (ulong *) malloc(num_elts * sizeof(ulong));
  ulong * cct_prefix = (ulong *) malloc(num_elts * sizeof(ulong));

  DIAG_Assert(cct_sum != NULL && cct_prefix != NULL,
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
  split(1, maxid, 0, numRanks - 1, cct_prefix, start_id);
  start_id[numRanks] = maxid + 1;

  // find max size of one group
  ulong group_size = 0;
  for (rank = 0; rank < numRanks; rank++) {
    for (group = 0; group < numRanks; group++) {
      ulong size = 0;
      for (id = start_id[group]; id < start_id[group + 1]; id++) {
	size += cct_global[rank * num_elts + id];
      }
      group_size = std::max(group_size, size);
    }
  }
  group_size += 2;  // space for sentinel

  if (0) {
    dumpCCTsizes(cct_sum, cct_prefix, start_id, maxid);
    dumpGroupSizes(cct_global, start_id, maxid, numRanks);
  }
  dumpGroupSizes(cct_global, start_id, maxid, numRanks);

  free(cct_sum);
  free(cct_prefix);

  return group_size;
}

//***************************************************************************

namespace Plot {

// Try to malloc the plot send/recv buffers for makeThreadMetrics().
// Treat unable to malloc as a soft failure: we can't make the plot
// graphs, but don't kill the whole program.  Note: it's important for
// all ranks to agree on whether to continue with plot graphs.
//
// Returns: 1 on success, 0 on failure.
//
int
allocBuffers(Prof::CallPath::Profile & prof,
	     int myRank, int numRanks, int rootRank)
{
  Prof::CCT::Tree * cct = prof.cct();
  uint maxid = cct->maxDenseId();
  uint id;  int ret, ret2, rank;

  ulong size_local = (maxid + 1) * sizeof(ulong);
  ulong size_global = numRanks * size_local;
  ulong * cct_local = (ulong *) malloc(size_local);
  ulong * cct_global = NULL;

  group_pos = (uint *) malloc(numRanks * sizeof(uint));
  start_id = (uint *) malloc((numRanks + 1) * sizeof(uint));

  DIAG_Assert(cct_local != NULL && group_pos != NULL && start_id != NULL,
	      "out of memory in Plot::allocBuffers");

  if (myRank == rootRank) {
    cct_global = (ulong *) malloc(size_global);
    DIAG_Assert(cct_global != NULL, "out of memory in Plot::allocBuffers");
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

  DIAG_Assert(ret == 0, "MPI_Gather for Plot::allocBuffers failed");

  // rank 0 partitions the cct ids, computes the max group size and
  // broadcasts the result.
  //
  if (myRank == rootRank) {
    group_size = partition_cctids(cct_global, start_id, maxid, numRanks);
  }
  ret = MPI_Bcast(start_id, numRanks + 1, MPI_INT, rootRank, MPI_COMM_WORLD);
  ret2 = MPI_Bcast(&group_size, 1, MPI_LONG, rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0 && ret2 == 0,
	      "MPI_Bcast for Plot::allocBuffers failed");

  for (rank = 0; rank < numRanks; rank++) {
    group_pos[rank] = rank * group_size;
  }

  // every rank tries to malloc the send/recv buffers.  rank 0 gathers
  // the success of each rank and broadcasts the result.
  //
  int *success_gbl = NULL;
  int success, my_success;

  if (myRank == rootRank) {
    success_gbl = (int *) malloc(numRanks * sizeof(int));
    DIAG_Assert(success_gbl != NULL, "out of memory in Plot::allocBuffers");
  }

  send_buf_size = group_size * numRanks * sizeof(plot_pt);

  plot_send_buf = (plot_pt *) malloc(send_buf_size);
  plot_recv_buf = (plot_pt *) malloc(send_buf_size);

  my_success = (plot_send_buf != NULL) && (plot_recv_buf != NULL);

  ret = MPI_Gather((void *) &my_success, 1, MPI_INT,
		   (void *) success_gbl, 1, MPI_INT,
		   rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Gather for Plot::allocBuffers failed");

  if (myRank == rootRank) {
    success = 1;
    for (rank = 0; rank < numRanks; rank++) {
      success = success && success_gbl[rank];
    }
  }

  ret = MPI_Bcast(&success, 1, MPI_INT, rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Bcast for Plot::allocBuffers failed");

  if (! success) {
    free(plot_send_buf);
    free(plot_recv_buf);
    plot_send_buf = NULL;
    plot_recv_buf = NULL;
  }

  if (myRank == rootRank) {
    double bufsize = send_buf_size / ((double) MEG);

    printf("\nplot graph buffer: %.2f meg, grand total: %.2f meg, in memory: %s\n",
	   bufsize, 2 * bufsize * numRanks,
	   success ? "yes" : "no");
  }

  return success;
}

void
addPlotPoints(Prof::CallPath::Profile & prof,
	      uint threadId, uint begMet, uint endMet)
{
  Prof::CCT::Tree * cct = prof.cct();
  uint maxid = cct->maxDenseId();
  long group = 0;

  for (uint id = 1; id <= maxid; id++) {
    Prof::CCT::ANode * node = cct->findNode(id);

    for (uint metid = begMet; metid < endMet; metid++) {
      if (metid < node->numMetrics() && node->metric(metid) != 0.0) {
	while (id >= start_id[group + 1]) {
	  group++;
	}
	uint pos = group_pos[group];

	plot_send_buf[pos].cctid = id;
	plot_send_buf[pos].metid = metid;
	plot_send_buf[pos].tid = threadId;
	plot_send_buf[pos].val = (float) node->metric(metid);
	group_pos[group]++;
      }
    }
  }
}

// The giant, in memory all-to-all of plot points.
//
int
sharePlotPoints(int myRank, int numRanks, int rootRank)
{
  int ret, group;

  // add sentinels and check for overflow.
  int success = 1;
  for (group = 0; group < numRanks; group++) {
    uint pos = group_pos[group];

    if (pos < group_size * (group + 1)) {
      plot_send_buf[pos].cctid = 0;
      plot_send_buf[pos].metid = 0;
      plot_send_buf[pos].tid = 0;
      plot_send_buf[pos].val = 0;
    }
    else {
      success = 0;
    }
  }

  DIAG_Assert(success, "incorrect count of plot points in Plot::sharePlotPoints");

  int size = group_size * sizeof(plot_pt);

  ret = MPI_Alltoall(plot_send_buf, size, MPI_BYTE,
		     plot_recv_buf, size, MPI_BYTE,
		     MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Alltoall failed in Plot::sharePlotPoints");

  return ret;
}

// Radix sort the plot points (cctid, metid, tid, val) using the
// send/recv buffers.  Also, count the total number of plot points,
// max metid and number of points per (cctid, metid) pair.
//
static void
sortPlotPoints(uint max_cctid, uint & max_metid, uint num_tid, int numRanks,
	       ulong & num_points, std::map <uint_pair, long> & cct_met_count)
{
  int group;  ulong n;

  //------------------------------------------------------------
  // Step 1 -- scan recv buf, count total number of plot points,
  // number per tid, number per (cctid, metid) pair, and recompute max
  // metric id (don't trust passed in value).  We sort three times, so
  // allocate one array of max size and reuse it.
  //------------------------------------------------------------

  ulong array_size = std::max(std::max(max_cctid, num_tid), (uint) 300) + 2;
  ulong * count = (ulong *) malloc(array_size * sizeof(ulong));
  ulong * start = (ulong *) malloc(array_size * sizeof(ulong));

  DIAG_Assert(count != NULL && start != NULL,
	      "out of memory in Plot::sortPlotPoints");

  for (n = 0; n < array_size; n++) {
    count[n] = 0;
  }
  cct_met_count.clear();
  num_points = 0;
  max_metid = 0;

  for (group = 0; group < numRanks; group++) {
    for (n = 0; n < group_size; n++) {
      plot_pt *elt = &plot_recv_buf[group * group_size + n];
      uint_pair mypair(elt->cctid, elt->metid);

      if (elt->cctid == 0) {
	break;
      }
      count[elt->tid]++;
      if (cct_met_count.find(mypair) != cct_met_count.end()) {
	cct_met_count[mypair]++;
      } else {
	cct_met_count[mypair] = 1;
      }
      num_points++;
      max_metid = std::max(max_metid, elt->metid);
    }
  }

  // extremely unlikely, but theoretically possible
  if (max_metid + 2 > array_size) {
    array_size = max_metid + 2;
    count = (ulong *) realloc(count, array_size * sizeof(ulong));
    start = (ulong *) realloc(start, array_size * sizeof(ulong));

    DIAG_Assert(count != NULL && start != NULL,
		"out of memory in Plot::sortPlotPoints");
  }

  // prefix sum of count[] is the start index per tid.
  start[0] = 0;
  for (n = 1; n <= num_tid; n++) {
    start[n] = start[n - 1] + count[n - 1];
  }

  //------------------------------------------------------------
  // Step 2 -- copy recv buf to send buf, use start[] to radix sort by
  // tid, and count number of points per metid.
  //------------------------------------------------------------

  for (n = 0; n < array_size; n++) {
    count[n] = 0;
  }

  for (group = 0; group < numRanks; group++) {
    for (n = 0; n < group_size; n++) {
      plot_pt *elt = &plot_recv_buf[group * group_size + n];

      if (elt->cctid == 0) {
	break;
      }
      plot_send_buf[start[elt->tid]] = *elt;
      start[elt->tid]++;
      count[elt->metid]++;
    }
  }

  start[0] = 0;
  for (n = 1; n <= max_metid; n++) {
    start[n] = start[n - 1] + count[n - 1];
  }

  //------------------------------------------------------------
  // Step 3 -- copy send buf back to recv buf, radix sort by metid,
  // and count number of points per cctid.
  //------------------------------------------------------------

  for (n = 0; n < array_size; n++) {
    count[n] = 0;
  }

  for (n = 0; n < num_points; n++) {
    plot_pt *elt = &plot_send_buf[n];

    plot_recv_buf[start[elt->metid]] = *elt;
    start[elt->metid]++;
    count[elt->cctid]++;
  }

  start[0] = 0;
  for (n = 1; n <= max_cctid; n++) {
    start[n] = start[n - 1] + count[n - 1];
  }

  //------------------------------------------------------------
  // Step 4 -- copy recv buf back to send buf and sort by cctid.
  // This completes the (cctid, metid, tid) sorted order.
  //------------------------------------------------------------

  for (n = 0; n < num_points; n++) {
    plot_pt *elt = &plot_recv_buf[n];

    plot_send_buf[start[elt->cctid]] = *elt;
    start[elt->cctid]++;
  }

  free(count);
  free(start);
}

int
writePlotGraphs(std::string & db_dir, uint max_cctid, uint max_tid,
		int myRank, int numRanks, int rootRank)
{
  std::map <uint_pair, long> cct_met_count;
  ulong num_points, n;
  uint max_metid;
  int rank, ret;

  //------------------------------------------------------------
  // Step 1 -- sort the plot points, result goes in send buf, and
  // count the number of plot points per (cctid, metid) pair.
  //------------------------------------------------------------

  sortPlotPoints(max_cctid, max_metid, max_tid, numRanks,
		 num_points, cct_met_count);

  // debug output to write plot points to a file.
  if (0) {
    dumpPlotPoints(db_dir, plot_send_buf, num_points,
		   myRank, numRanks, rootRank);
  }

  //------------------------------------------------------------
  // Step 2 -- send size of plot data and size of index to rank 0,
  // compute file offsets and scatter result.
  //------------------------------------------------------------

  ulong my_data_size = num_points * sizeof(plot_entry_t);
  ulong my_index_size = cct_met_count.size() * sizeof(plot_index_t);
  ulong my_size[2];
  ulong * global_size = NULL;
  ulong * global_ans = NULL;

  my_size[0] = my_data_size;
  my_size[1] = my_index_size;

  if (myRank == rootRank) {
    global_size = (ulong *) malloc(2 * numRanks * sizeof(ulong));
    global_ans = (ulong *) malloc(2 * numRanks * sizeof(ulong));

    DIAG_Assert(global_size != NULL && global_ans != NULL,
		"out of memory in Plot::writePlotGraphs");
  }

  ret = MPI_Gather((void *) my_size, 2, MPI_LONG,
		   (void *) global_size, 2, MPI_LONG,
		   rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Gather for Plot::writePlotGraphs failed");

  // for the header at rank 0
  ulong index_start = Prof::Database::alignOffset(PLOT_INDEX_START);
  ulong index_size = 0;
  ulong data_start = 0;
  ulong data_size = 0;

  if (myRank == rootRank) {
    ulong next_pos = index_start;
    ulong data_end = 0;

    // we don't align the index entries per mpi rank
    for (rank = 0; rank < numRanks; rank++) {
      global_ans[2*rank + 1] = next_pos;
      next_pos += global_size[2*rank + 1];
    }
    index_size = next_pos - index_start;

    // we do align the main plot data
    next_pos = Prof::Database::alignOffset(next_pos);
    data_start = next_pos;
    for (rank = 0; rank < numRanks; rank++) {
      global_ans[2*rank] = next_pos;
      data_end = next_pos + global_size[2*rank];
      next_pos = Prof::Database::alignOffset(data_end);
    }
    data_size = data_end - data_start;
  }

  ret = MPI_Scatter((void *) global_ans, 2, MPI_LONG,
		    (void *) my_size, 2, MPI_LONG,
		    rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Scatter for Plot::writePlotGraphs failed");

  ulong my_data_start = my_size[0];
  ulong my_index_start = my_size[1];

  //------------------------------------------------------------
  // Step 3 -- copy send buf back to recv buf, write (tid, val) pairs
  // in big endian, and write data to file.  After the write, the send
  // and recv buffers are unused.
  //------------------------------------------------------------

  plot_entry_t * plot_out_buf = (plot_entry_t *) plot_recv_buf;
  union { float fval; uint32_t ival; } u32;

  for (n = 0; n < num_points; n++) {
    u32.fval = plot_send_buf[n].val;
    plot_out_buf[n].tid = host_to_be_32(plot_send_buf[n].tid);
    plot_out_buf[n].metval = host_to_be_32(u32.ival);
  }

  std::string plot_fname = db_dir + "/" + "plot.db";
  int plot_fd = open(plot_fname.c_str(), O_WRONLY | O_CREAT, 0644);
  
  DIAG_Assert(plot_fd >= 0, "open plot metrics file failed");

  ret = write_all_at(plot_fd, plot_out_buf, my_data_size, my_data_start);

  DIAG_Assert(ret == 0, "write plot metrics file failed");

  //------------------------------------------------------------
  // Step 4 -- write the index data in big endian and write to file.
  // This probably fits in the send buf, if not, then resize.
  //------------------------------------------------------------

  plot_index_t * index_buf = (plot_index_t *) plot_send_buf;
  free(plot_recv_buf);

  if (my_index_size > send_buf_size) {
    free(plot_send_buf);
    plot_send_buf = NULL;
    index_buf = (plot_index_t *) malloc(my_index_size);

    DIAG_Assert(index_buf != NULL, "out of memory in Plot::writePlotGraphs");
  }

  std::map <uint_pair, long>::iterator it;
  ulong offset = my_data_start;

  n = 0;
  for (it = cct_met_count.begin(); it != cct_met_count.end(); it++) {
    index_buf[n].cctid = host_to_be_32(it->first.first);
    index_buf[n].metid = host_to_be_32(it->first.second);
    index_buf[n].offset = host_to_be_64(offset);
    index_buf[n].count =  host_to_be_64(it->second);
    offset += it->second * sizeof(plot_entry_t);
    n++;
  }

  ret = write_all_at(plot_fd, index_buf, my_index_size, my_index_start);

  DIAG_Assert(ret == 0, "write plot metrics index failed");

  free(index_buf);

  // finally, rank 0 writes the header
  if (myRank == rootRank) {
    ret = Prof::Database::writePlotHeader(plot_fd,
	    max_cctid + 1, max_metid + 1, max_tid,
	    index_start, index_size, data_start, data_size);

    DIAG_Assert(ret == 0, "write plot metrics header failed");
  }

  close(plot_fd);

  // temp debugging output
  if (1 && myRank == rootRank) {
    printf("\nfile offsets for index and data\n");
    for (rank = 0; rank < numRanks; rank++) {
      printf("rank: %d  index: 0x%08lx  data: 0x%08lx  points: %ld\n",
	     rank, global_ans[2*rank + 1], global_ans[2*rank],
	     global_size[2*rank] / sizeof(plot_entry_t));
    }
    printf("\n");
  }

  free(global_size);
  free(global_ans);

  return 0;
}

}  // namespace Plot

//***************************************************************************

namespace Trace {

// Perform a prefix sum on the lengths of the trace files to determine
// their starting offsets.  Rank 0 gathers the trace info from all
// ranks, computes the prefix sum (serial), and then scatters the
// result back to the other ranks.
//
int
mergeTraceInfo(Prof::Database::traceInfo * traceGbl,
	       Prof::Database::traceInfo * traceLcl,
	       long numPerRank, long & numActive,
	       int myRank, int numRanks, int rootRank)
{
  int chunkSize = numPerRank * sizeof(Prof::Database::traceInfo);
  long totalFiles = numPerRank * numRanks;
  off_t offset = 0;
  long k;
  int ret;

  ret = MPI_Gather((void *) traceLcl, chunkSize, MPI_CHAR,
		   (void *) traceGbl, chunkSize, MPI_CHAR,
		   rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Gather for mergeTraceInfo failed");

  numActive = 0;
  if (myRank == rootRank) {
    // count the number of active trace files
    for (k = 0; k < totalFiles; k++) {
      if (traceGbl[k].active) {
	numActive++;
      }
    }

    // compute starting offsets for all trace files
    offset = Prof::Database::firstTraceOffset(numActive);
    offset = Prof::Database::alignOffset(offset);

    for (k = 0; k < totalFiles; k++) {
      traceGbl[k].start_offset = 0;
      if (traceGbl[k].active) {
	traceGbl[k].start_offset = offset;
	offset += traceGbl[k].length;
	offset = Prof::Database::alignOffset(offset);
      }
    }
  }

  ret = MPI_Scatter((void *) traceGbl, chunkSize, MPI_CHAR,
		    (void *) traceLcl, chunkSize, MPI_CHAR,
		    rootRank, MPI_COMM_WORLD);

  DIAG_Assert(ret == 0, "MPI_Scatter for mergeTraceInfo failed");

  return 0;
}

}  // namespace Trace

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

  free(column);
}

static void
dumpPlotPoints(std::string & db_dir, plot_pt *buf, ulong num_points,
	       int myRank, int numRanks, int rootRank)
{
  std::string fname = db_dir + "/" + "plot-points.db";
  ulong my_size = num_points * sizeof(plot_pt);
  ulong my_prefix = 0;
  ulong * global_size = NULL;
  ulong * global_prefix = NULL;

  if (myRank == rootRank) {
    global_size = (ulong *) malloc(numRanks * sizeof(ulong));
    global_prefix = (ulong *) malloc(numRanks * sizeof(ulong));
  }

  MPI_Gather((void *) &my_size, 1, MPI_LONG,
	     (void *) global_size, 1, MPI_LONG,
	     rootRank, MPI_COMM_WORLD);

  if (myRank == rootRank) {
    global_prefix[0] = 0;
    for (int rank = 1; rank < numRanks; rank++) {
      global_prefix[rank] = global_prefix[rank - 1] + global_size[rank - 1];
    }
  }

  MPI_Scatter((void *) global_prefix, 1, MPI_LONG,
	      (void *) &my_prefix, 1, MPI_LONG,
	      rootRank, MPI_COMM_WORLD);

  free(global_size);
  free(global_prefix);

  int fd = open(fname.c_str(), O_WRONLY | O_CREAT, 0644);

  write_all_at(fd, buf, my_size, my_prefix);

  close(fd);
}
