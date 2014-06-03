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

// Todo:
//
// 1. Write metric table (and possibly offset table) in chunks with
// buffer.
//
// 2. Settle on format of headers.
//
// 3. Replace err() with hpcprof errors and warnings.
//

//***************************************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include <lib/analysis/Args.hpp>
#include <lib/analysis/ArgsHPCProf.hpp>
#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <include/uint.h>
#include <include/big-endian.h>

//***************************************************************************

#define MESSAGE_SIZE  32
#define SUMMARY_NAME  "hpcprof summary metrics"

#define MAGIC  0x06870630

#define METRIC_BUFFER_SIZE  (4 * 1024 * 1024)
#define TABLE_ALIGN   512

#define round_up(x, y)  ((y) * (((x) + (y) - 1)/(y)))

struct __attribute__ ((packed)) common_header {
  char      mesg[MESSAGE_SIZE];
  uint32_t  magic;
  uint32_t  type;
  uint32_t  format;
  uint64_t  num_threads;
  uint64_t  num_cctid;
  uint64_t  num_metric;
};

struct __attribute__ ((packed)) summary_header {
  uint64_t  offset_start;
  uint64_t  offset_size;
  uint64_t  metric_start;
  uint64_t  metric_size;
  uint32_t  size_offset;
  uint32_t  size_metid;
  uint32_t  size_metval;
};

struct __attribute__ ((packed)) metric_entry {
  uint16_t  metid;
  uint32_t  metval;
};

//***************************************************************************

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

//***************************************************************************

namespace Prof {
namespace Database {

bool
makeSummaryDB(Prof::CallPath::Profile & prof, const Analysis::Args & args)
{
  Prof::CCT::Tree * cct_tree = prof.cct();
  Prof::Metric::Mgr * mgr = prof.metricMgr();
  Metric::ADesc * desc;
  uint num_cctid = cct_tree->maxDenseId() + 1;
  uint num_metrics = prof.metricMgr()->size();
  uint beg_metric, end_metric, last_vis;

  desc = mgr->findFirstVisible();
  beg_metric = (desc) ? desc->id() : 0;
  desc = mgr->findLastVisible();
  last_vis = (desc) ? (desc->id() + 1) : Metric::Mgr::npos;


  std::cout << "writing summary binary db: "
	    << "max-cct: " << num_cctid
	    << ", num-metrics: " << num_metrics << ", ...\n";
  std::cout << "first-vis: " << beg_metric
	    << ", last-vis: " << last_vis << "\n";


  //------------------------------------------------------------
  // compute sizes and allocate buffers
  //------------------------------------------------------------

  uint64_t common_size = sizeof(struct common_header);
  uint64_t summary_size = sizeof(struct summary_header);
  uint64_t metric_entry_size = sizeof(struct metric_entry);
  uint64_t header_size =  common_size + summary_size;
  uint64_t offset_start = round_up(header_size + 8, TABLE_ALIGN);
  uint64_t offset_size =  sizeof(uint64_t) * (num_cctid + 2);
  uint64_t metric_start = round_up(offset_start + offset_size + 8, TABLE_ALIGN);

  struct common_header * common_hdr;
  struct summary_header * summary_hdr;
  uint64_t * offset_table;
  struct metric_entry * metric_table;

  char * header = (char *) malloc(header_size);
  if (header == NULL) {
    err(1, "malloc for summary metrics header failed");
  }
  common_hdr = (struct common_header *) header;
  summary_hdr = (struct summary_header *) (header + common_size);

  offset_table = (uint64_t *) malloc(offset_size);
  if (offset_table == NULL) {
    err(1, "malloc for summary metrics offset table failed");
  }

  metric_table = (struct metric_entry *) malloc(METRIC_BUFFER_SIZE);
  if (metric_table == NULL) {
    err(1, "malloc for summary metrics metric table failed");
  }

  //------------------------------------------------------------
  // open output file
  //------------------------------------------------------------

  std::string summary_fname = args.db_dir + "/" + "summary.db";
  int flags = O_WRONLY | O_CREAT | O_TRUNC;

  int sum_fd = open(summary_fname.c_str(), flags, 0644);
  if (sum_fd < 0) {
    err(1, "open summary metrics file failed");
  }

  //------------------------------------------------------------
  // fill in offset and metric tables
  //------------------------------------------------------------

  // FIXME: write metric table with chunks and buffer.

  uint64_t next_idx = 0;
  uint64_t next_off = 0;
  union { float fval; uint32_t ival; } u32;

  for (uint cctid = 0; cctid < num_cctid; cctid++) {
    Prof::CCT::ANode * node = cct_tree->findNode(cctid);

    offset_table[cctid] = host_to_be_64(next_off);
    if (node != NULL) {
      end_metric = std::min(node->numMetrics(), last_vis);
      for (uint metid = beg_metric; metid < end_metric; metid++) {
	u32.fval = (float) node->metric(metid);
	if (u32.fval != 0.0) {
	  metric_table[next_idx].metid = host_to_be_16(metid);
	  metric_table[next_idx].metval = host_to_be_32(u32.ival);
	  next_idx++;
	  next_off += metric_entry_size;
	}
      }
    }
  }
  offset_table[num_cctid] = host_to_be_64(next_off);
  offset_table[num_cctid + 1] = 0;

  //------------------------------------------------------------
  // fill in common and summary headers
  //------------------------------------------------------------

  memset(header, 0, header_size);
  strncpy((char *) common_hdr, SUMMARY_NAME, MESSAGE_SIZE);

  common_hdr->mesg[MESSAGE_SIZE - 1] = 0;
  common_hdr->magic = host_to_be_32(MAGIC);
  common_hdr->type =  host_to_be_32(1);
  common_hdr->format =  host_to_be_32(1);
  common_hdr->num_threads = 0;
  common_hdr->num_cctid =  host_to_be_64(num_cctid);
  common_hdr->num_metric = host_to_be_64(num_metrics);

  summary_hdr->offset_start = host_to_be_64(offset_start);
  summary_hdr->offset_size =  host_to_be_64(offset_size);
  summary_hdr->metric_start = host_to_be_64(metric_start);
  summary_hdr->metric_size =  host_to_be_64(next_off);
  summary_hdr->size_offset =  host_to_be_32(8);
  summary_hdr->size_metid =   host_to_be_32(2);
  summary_hdr->size_metval =  host_to_be_32(4);

  //------------------------------------------------------------
  // write file
  //------------------------------------------------------------

  if (write_all_at(sum_fd, metric_table, next_off, metric_start) != 0) {
    err(1, "write summary metric metric table failed");
  }

  if (write_all_at(sum_fd, offset_table, offset_size, offset_start) != 0) {
    err(1, "write summary metric offset table failed");
  }

  if (write_all_at(sum_fd, header, header_size, 0) != 0) {
    err(1, "write summary metric header failed");
  }

  close(sum_fd);

  return true;
}

}  // namespace Prof, Database
}
