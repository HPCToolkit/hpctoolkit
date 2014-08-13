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
// 4. Settle on alignment (if any) in trace and other files.
//
// 5. Add trace file to hpcprof-mpi.
//
// 6. Add way to choose subset of trace files.
//
// 7. Add min/max times to trace header.
//
// 8. Add fields to experiment.xml for the .db files and their
// properties.
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
#include <lib/prof/CCT-Merge.hpp>
#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <include/uint.h>
#include <include/big-endian.h>

//***************************************************************************

#define MESSAGE_SIZE  32
#define SUMMARY_NAME  "hpctoolkit summary file"
#define TRACE_NAME    "hpctoolkit trace file"

#define MAGIC  0x06870630

#define METRIC_BUFFER_SIZE  (4 * 1024 * 1024)
#define TRACE_BUFFER_SIZE   (8 * 1024 * 1024)

#define OFFSET_ALIGN  512

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

struct __attribute__ ((packed)) trace_header {
  uint64_t  min_time;
  uint64_t  max_time;
  uint64_t  index_start;
  uint64_t  index_length;
  uint32_t  size_offset;
  uint32_t  size_length;
  uint32_t  size_time;
  uint32_t  size_cctid;
};

struct __attribute__ ((packed)) metric_entry {
  uint16_t  metid;
  uint32_t  metval;
};

struct __attribute__ ((packed)) trace_index {
  uint64_t  offset;
  uint64_t  length;
};

// Global data for all files.

static bool new_db_format = false;
static std::string db_dir;

static int trace_fd = -1;
static unsigned char *trace_buf = NULL;

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

void
initdb(const Analysis::Args & args)
{
  db_dir = std::string(args.db_dir);
  new_db_format = true;
}

bool
newDBFormat(void)
{
  return new_db_format;
}

off_t
alignOffset(off_t offset)
{
  return OFFSET_ALIGN * ((offset + OFFSET_ALIGN - 1)/OFFSET_ALIGN);
}

//***************************************************************************

// Summary Metrics File

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
  uint64_t offset_start = alignOffset(header_size);
  uint64_t offset_size =  sizeof(uint64_t) * (num_cctid + 2);
  uint64_t metric_start = alignOffset(offset_start + offset_size);

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

//***************************************************************************

// Mega Trace File

off_t
firstTraceOffset(long num_files)
{
  off_t offset;

  offset = sizeof(struct common_header) + sizeof(struct trace_header);
  offset = alignOffset(offset);
  offset += (num_files + 2) * sizeof(struct trace_index);
  offset = alignOffset(offset);

  return offset;
}

int
writeTraceHeader(long num_threads)
{
  struct common_header * common_hdr;
  struct trace_header * trace_hdr;

  if (trace_fd < 0) {
    return 0;
  }

  uint64_t common_size = sizeof(struct common_header);
  uint64_t trace_size = sizeof(struct trace_header);
  uint64_t header_size =  common_size + trace_size;
  uint64_t index_start = alignOffset(header_size);
  uint64_t index_length = (num_threads + 2) * sizeof(struct trace_index);

  char * header = (char *) malloc(header_size);
  if (header == NULL) {
    err(1, "malloc for trace header failed");
  }
  common_hdr = (struct common_header *) header;
  trace_hdr = (struct trace_header *) (header + common_size);

  memset(header, 0, header_size);
  strncpy((char *) common_hdr, TRACE_NAME, MESSAGE_SIZE);

  common_hdr->mesg[MESSAGE_SIZE - 1] = 0;
  common_hdr->magic = host_to_be_32(MAGIC);
  common_hdr->type =  host_to_be_32(2);
  common_hdr->format =  host_to_be_32(2);
  common_hdr->num_threads = host_to_be_64(num_threads);
  common_hdr->num_cctid =  0;
  common_hdr->num_metric = 0;

  trace_hdr->min_time = 0;
  trace_hdr->max_time = 0;
  trace_hdr->index_start =  host_to_be_64(index_start);
  trace_hdr->index_length = host_to_be_64(index_length);
  trace_hdr->size_offset =  host_to_be_32(8);
  trace_hdr->size_length =  host_to_be_32(8);
  trace_hdr->size_time =    host_to_be_32(8);
  trace_hdr->size_cctid =   host_to_be_32(4);

  if (write_all_at(trace_fd, header, header_size, 0) != 0) {
    err(1, "write trace header failed");
  }

  return 0;
}

int
writeTraceIndex(traceInfo *trace, long num_threads)
{
  struct trace_index *table;
  size_t start = alignOffset(sizeof(struct common_header) + sizeof(struct trace_header));
  size_t size = (num_threads + 1) * sizeof(*table);
  long n;

  if (trace_fd < 0) {
    return 0;
  }

  table = (struct trace_index *) malloc(size);
  if (table == NULL) {
    err(1, "unable to malloc trace index table");
  }

  for (n = 0; n < num_threads; n++) {
    table[n].offset = host_to_be_64(trace[n].start_offset);
    table[n].length = host_to_be_64(trace[n].length);
  }
  table[num_threads].offset = 0;
  table[num_threads].length = 0;

  if (write_all_at(trace_fd, table, size, start) != 0) {
    err(1, "write trace index table failed");
  }

  return 0;
}

int
writeTraceFile(Prof::CallPath::Profile *prof,
	       Prof::CCT::MergeEffectList *effects)
{
  off_t offset = prof->m_traceInfo.start_offset;

  // open trace.db file lazily.
  if (trace_fd < 0) {
    std::string trace_fname = db_dir + "/" + "trace.db";

    trace_fd = open(trace_fname.c_str(), O_WRONLY | O_CREAT, 0644);
    if (trace_fd < 0) {
      err(1, "open trace metrics file failed");
    }

    trace_buf = (unsigned char *) malloc(TRACE_BUFFER_SIZE);
    if (trace_buf == NULL) {
      err(1, "malloc for trace buffer failed");
    }
  }

  // read the original trace file.
  const char *name = prof->traceFileName().c_str();
  int old_fd = open(name, O_RDONLY);
  if (old_fd < 0) {
    err(1, "open trace file failed: %s", name);
  }

  ssize_t len = read(old_fd, trace_buf, TRACE_BUFFER_SIZE);
  if (len < 0) {
    err(1, "read trace file failed: %s", name);
  }
  close(old_fd);

  // translate the cctids.
  if (effects != NULL) {
    std::map <uint, uint> cctid_map;

    // convert merge effects list to map.
    for (CCT::MergeEffectList::iterator it = effects->begin();
	 it != effects->end(); ++it) {
      cctid_map.insert(std::make_pair(it->old_cpId, it->new_cpId));
    }

    // walk through original trace file and translate the cctids.
    for (int k = 40; k < len; k += 12) {
      int32_t *p = (int32_t *) &trace_buf[k];
      uint oldid = be_to_host_32(*p);
      std::map <uint, uint> :: iterator it = cctid_map.find(oldid);

      if (it != cctid_map.end()) {
	*p = host_to_be_32(it->second);
      }
    }
  }

  // write the new trace file.
  if (write_all_at(trace_fd, &trace_buf[32], len - 32, offset) != 0) {
    err(1, "write trace file failed: %s", name);
  }

  prof->m_traceInfo.length = len - 32;

  return 0;
}

void
endTraceFiles(void)
{
  close(trace_fd);

  if (trace_buf != NULL) {
    free(trace_buf);
  }
}

}  // namespace Prof, Database
}
