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

#define __STDC_LIMIT_MACROS

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
#include <lib/support/diagnostics.h>
#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/CCT-Merge.hpp>
#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Database.hpp>
#include <lib/prof/DBFormat.h>
#include <lib/prof/Metric-Mgr.hpp>
#include <include/uint.h>
#include <include/big-endian.h>

//***************************************************************************

#define METRIC_BUFFER_SIZE  (4 * 1024 * 1024)
#define TRACE_BUFFER_SIZE   (8 * 1024 * 1024)

#define OFFSET_ALIGN  512

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

static void
make_common_header(struct common_header *hdr, const char *mesg,
		   uint64_t type, uint64_t format,
		   uint64_t num_cctid, uint64_t num_metid, uint64_t num_threads)
{
  memset(hdr, 0, sizeof(struct common_header));

  strncpy(hdr->mesg, mesg, MESSAGE_SIZE);
  hdr->mesg[MESSAGE_SIZE - 1] = 0;
  hdr->magic = host_to_be_64(MAGIC);
  hdr->version = host_to_be_64(FILE_VERSION);
  hdr->type =   host_to_be_64(type);
  hdr->format = host_to_be_64(format);
  hdr->num_cctid = host_to_be_64(num_cctid);
  hdr->num_metid = host_to_be_64(num_metid);
  hdr->num_threads = host_to_be_64(num_threads);
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
  uint num_metid = prof.metricMgr()->size();
  uint beg_metric, end_metric, last_vis;

  desc = mgr->findFirstVisible();
  beg_metric = (desc) ? desc->id() : 0;
  desc = mgr->findLastVisible();
  last_vis = (desc) ? (desc->id() + 1) : Metric::Mgr::npos;


  std::cout << "writing summary binary db: "
	    << "max-cct: " << num_cctid
	    << ", num-metrics: " << num_metid << ", ...\n";
  std::cout << "first-vis: " << beg_metric
	    << ", last-vis: " << last_vis << "\n";


  //------------------------------------------------------------
  // compute sizes and allocate buffers
  //------------------------------------------------------------

  uint64_t header_size = HEADER_SIZE;
  uint64_t metric_entry_size = sizeof(struct metric_entry);
  uint64_t offset_start = alignOffset(header_size);
  uint64_t offset_size =  sizeof(uint64_t) * (num_cctid + 2);
  uint64_t metric_start = alignOffset(offset_start + offset_size);

  uint64_t * offset_table;
  struct metric_entry * metric_table;

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
	  metric_table[next_idx].metid = host_to_be_32(metid);
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

  char header[HEADER_SIZE];

  struct common_header * common_hdr = (struct common_header *) header;
  struct summary_header * summary_hdr = (struct summary_header *) (header + COMMON_SIZE);

  memset(header, 0, header_size);
  make_common_header(common_hdr, SUMMARY_NAME, SUMMARY_TYPE, SUMMARY_FMT,
		     num_cctid, num_metid, 0);

  summary_hdr->offset_start = host_to_be_64(offset_start);
  summary_hdr->offset_size =  host_to_be_64(offset_size);
  summary_hdr->metric_start = host_to_be_64(metric_start);
  summary_hdr->metric_size =  host_to_be_64(next_off);
  summary_hdr->size_offset =  host_to_be_32(8);
  summary_hdr->size_metid =   host_to_be_32(4);
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

  free(offset_table);
  free(metric_table);
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

// Open trace file lazily.  We don't know at initdb time if there are
// trace files.  So, we open trace.db and malloc buffer on first
// write.
//
static void
openTraceFile(void)
{
  if (trace_fd >= 0) {
    return;
  }

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

int
writeTraceHeader(traceInfo *trace, long num_threads, long num_active)
{
  uint64_t header_size = HEADER_SIZE;
  uint64_t index_start = alignOffset(header_size);
  uint64_t index_length = (num_active + 1) * sizeof(struct trace_index);
  uint64_t min_time, max_time;
  uint64_t min_start, max_end;

  openTraceFile();

  min_time = UINT64_MAX;
  max_time = 0;
  min_start = UINT64_MAX;
  max_end = 0;

  for (long n = 0; n < num_threads; n++) {
    if (trace[n].active) {
      min_time = std::min(min_time, trace[n].min_time);
      max_time = std::max(max_time, trace[n].max_time);
      min_start = std::min(min_start, (uint64_t) trace[n].start_offset);
      max_end = std::max(max_end, (uint64_t) (trace[n].start_offset + trace[n].length));
    }
  }

  char header[HEADER_SIZE];

  struct common_header * common_hdr = (struct common_header *) header;
  struct trace_header * trace_hdr = (struct trace_header *) (header + COMMON_SIZE);

  memset(header, 0, header_size);
  make_common_header(common_hdr, TRACE_NAME, TRACE_TYPE, TRACE_FMT,
		     0, 0, num_active);

  trace_hdr->index_start =  host_to_be_64(index_start);
  trace_hdr->index_length = host_to_be_64(index_length);
  trace_hdr->trace_start =  host_to_be_64(min_start);
  trace_hdr->trace_length = host_to_be_64(max_end - min_start);
  trace_hdr->min_time = host_to_be_64(min_time);
  trace_hdr->max_time = host_to_be_64(max_time);
  trace_hdr->size_offset = host_to_be_32(8);
  trace_hdr->size_length = host_to_be_32(8);
  trace_hdr->size_global_tid = host_to_be_32(8);
  trace_hdr->size_time =  host_to_be_32(8);
  trace_hdr->size_cctid = host_to_be_32(4);

  int ret = write_all_at(trace_fd, header, header_size, 0);

  DIAG_Assert(ret == 0, "write trace header failed in Prof::Database::writeTraceHeader");

  return 0;
}

// Experiment.xml includes the full list of threads, but trace.db
// includes only those threads with active trace files.
//
int
writeTraceIndex(traceInfo *trace, long num_threads, long num_active)
{
  struct trace_index *table;
  size_t start = alignOffset(HEADER_SIZE);
  size_t size = (num_threads + 1) * sizeof(struct trace_index);
  long n, pos;

  openTraceFile();

  table = (struct trace_index *) malloc(size);

  DIAG_Assert(table != NULL, "out of memory in Prof::Database::writeTraceIndex");

  pos = 0;
  for (n = 0; n < num_threads; n++) {
    if (trace[n].active) {
      table[pos].offset = host_to_be_64(trace[n].start_offset);
      table[pos].length = host_to_be_64(trace[n].length);
      table[pos].global_tid = host_to_be_64(n);
      pos++;
    }
  }
  table[pos].offset = 0;
  table[pos].length = 0;
  table[pos].global_tid = 0;
  size = (pos + 1) * sizeof(struct trace_index);

  int ret = write_all_at(trace_fd, table, size, start);

  DIAG_Assert(ret == 0, "write trace index failed in Prof::Database::writeTraceIndex");

  free(table);

  return 0;
}

int
writeTraceFile(Prof::CallPath::Profile *prof,
	       Prof::CCT::MergeEffectList *effects)
{
  off_t offset = prof->m_traceInfo.start_offset;

  openTraceFile();

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

//***************************************************************************

// Thread ID File

int
writeThreadIDFile(traceInfo *trace, long num_threads)
{
  long header_size = HEADER_SIZE;
  char header[HEADER_SIZE];
  long k, pos;

  // open the threads.db file

  std::string thr_fname = db_dir + "/" + "threads.db";
  int flags = O_WRONLY | O_CREAT | O_TRUNC;

  int thr_fd = open(thr_fname.c_str(), flags, 0644);

  DIAG_Assert(thr_fd >= 0, "open thread id file failed");

  // check if threads have multiple mpi ranks and/or multiple thread
  // ids, where 'multiple' means any non-zero value.

  int use_rank = 0;
  int use_tid = 0;
  int num_fields;

  for (k = 0; k < num_threads; k++) {
    if (trace[k].rank > 0) {
      use_rank = 1;
    }
    if (trace[k].tid > 0) {
      use_tid = 1;
    }
  }
  if (use_tid == 0) { use_rank = 1; }
  num_fields = use_rank + use_tid;

  // write the string table

  uint64_t string_start = alignOffset(header_size);
  uint64_t string_length;

  memset(header, 0, header_size);

  pos = 0;
  if (use_rank) {
    strncpy(&header[pos], "Process", MESSAGE_SIZE);
    header[pos + MESSAGE_SIZE - 1] = 0;
    pos += MESSAGE_SIZE;
  }
  if (use_tid) {
    strncpy(&header[pos], "Thread", MESSAGE_SIZE);
    header[pos + MESSAGE_SIZE - 1] = 0;
    pos += MESSAGE_SIZE;
  }
  string_length = pos;

  int ret = write_all_at(thr_fd, header, string_length, string_start);

  DIAG_Assert(ret == 0, "write thread index file failed");

  // write the thread index data
  // fixme: may want to buffer this

  long index_start = alignOffset(string_start + string_length);
  long index_size = num_threads * num_fields * sizeof(uint32_t);

  uint32_t *index = (uint32_t *) malloc(index_size);

  DIAG_Assert(index != NULL, "malloc for thread id table failed");

  pos = 0;
  for (k = 0; k < num_threads; k++) {
    if (use_rank) {
      index[pos] = host_to_be_32(trace[k].rank);
      pos++;
    }
    if (use_tid) {
      index[pos] = host_to_be_32(trace[k].tid);
      pos++;
    }
  }

  ret = write_all_at(thr_fd, index, index_size, index_start);

  DIAG_Assert(ret == 0, "write thread id file failed");

  free(index);

  // lastly, write the header

  struct common_header * common_hdr = (struct common_header *) header;
  struct threads_header * thr_hdr = (struct threads_header *) (header + COMMON_SIZE);

  memset(header, 0, header_size);
  make_common_header(common_hdr, THREADS_NAME, THREADS_TYPE, THREADS_FMT,
		     0, 0, num_threads);

  thr_hdr->string_start =  host_to_be_64(string_start);
  thr_hdr->string_length = host_to_be_64(string_length);
  thr_hdr->index_start =  host_to_be_64(index_start);
  thr_hdr->index_length = host_to_be_64(index_size);
  thr_hdr->num_fields =  host_to_be_32(num_fields);
  thr_hdr->size_string = host_to_be_32(MESSAGE_SIZE);
  thr_hdr->size_field =  host_to_be_32(4);

  ret = write_all_at(thr_fd, header, header_size, 0);

  DIAG_Assert(ret == 0, "write thread id file failed");

  close(thr_fd);

  return 0;
}

//***************************************************************************

// Plot Graph Header

int
writePlotHeader(int fd, ulong num_cctid, ulong num_metid, ulong num_threads,
		ulong index_start, ulong index_length,
		ulong plot_start, ulong plot_length)
{
  long header_size = HEADER_SIZE;
  char header[HEADER_SIZE];

  struct common_header * common_hdr = (struct common_header *) header;
  struct plot_header * plot_hdr = (struct plot_header *) (header + COMMON_SIZE);

  memset(header, 0, header_size);
  make_common_header(common_hdr, PLOT_NAME, PLOT_TYPE, PLOT_FMT,
		     num_cctid, num_metid, num_threads);

  plot_hdr->index_start =  host_to_be_64(index_start);
  plot_hdr->index_length = host_to_be_64(index_length);
  plot_hdr->plot_start =  host_to_be_64(plot_start);
  plot_hdr->plot_length = host_to_be_64(plot_length);
  plot_hdr->size_cctid = host_to_be_32(4);
  plot_hdr->size_metid = host_to_be_32(4);
  plot_hdr->size_offset = host_to_be_32(8);
  plot_hdr->size_count =  host_to_be_32(8);
  plot_hdr->size_tid = host_to_be_32(4);
  plot_hdr->size_metval = host_to_be_32(4);

  int ret = write_all_at(fd, header, header_size, 0);

  return ret;
}

}  // namespace Prof, Database
}
