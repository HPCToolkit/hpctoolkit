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

// This file defines the format of the new binary db files.

//***************************************************************************

#ifndef Prof_DB_File_Format_h
#define Prof_DB_File_Format_h

#include <stdint.h>

#define SUMMARY_NAME  "hpctoolkit summary metrics"
#define TRACE_NAME    "hpctoolkit trace metrics"
#define PLOT_NAME     "hpctoolkit plot metrics"
#define THREADS_NAME  "hpctoolkit thread index"

#define FILE_VERSION  3

#define SUMMARY_TYPE  1
#define TRACE_TYPE    2
#define PLOT_TYPE     3
#define THREADS_TYPE  4

#define SUMMARY_FMT   1
#define TRACE_FMT     2
#define PLOT_FMT      3
#define THREADS_FMT   4

#define HEADER_SIZE  512
#define COMMON_SIZE  256
#define MESSAGE_SIZE  32

#define MAGIC  0x06870630

struct __attribute__ ((packed)) common_header {
  char      mesg[MESSAGE_SIZE];
  uint64_t  magic;
  uint64_t  version;
  uint64_t  type;
  uint64_t  format;
  uint64_t  num_cctid;
  uint64_t  num_metid;
  uint64_t  num_threads;
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
  uint64_t  index_start;
  uint64_t  index_length;
  uint64_t  trace_start;
  uint64_t  trace_length;
  uint64_t  min_time;
  uint64_t  max_time;
  uint32_t  size_offset;
  uint32_t  size_length;
  uint32_t  size_global_tid;
  uint32_t  size_time;
  uint32_t  size_cctid;
};

struct __attribute__ ((packed)) plot_header {
  uint64_t  index_start;
  uint64_t  index_length;
  uint64_t  plot_start;
  uint64_t  plot_length;
  uint32_t  size_cctid;
  uint32_t  size_metid;
  uint32_t  size_offset;
  uint32_t  size_count;
  uint32_t  size_tid;
  uint32_t  size_metval;
};

struct __attribute__ ((packed)) threads_header {
  uint64_t  string_start;
  uint64_t  string_length;
  uint64_t  index_start;
  uint64_t  index_length;
  uint32_t  num_fields;
  uint32_t  size_string;
  uint32_t  size_field;
};

struct __attribute__ ((packed)) metric_entry {
  uint32_t  metid;
  uint32_t  metval;
};

struct __attribute__ ((packed)) trace_index {
  uint64_t  offset;
  uint64_t  length;
  uint64_t  global_tid;
};

struct __attribute__ ((packed)) plot_index {
  uint32_t  cctid;
  uint32_t  metid;
  uint64_t  offset;
  uint64_t  count;
};

struct __attribute__ ((packed)) plot_entry {
  uint32_t  tid;
  uint32_t  metval;
};

typedef struct common_header   common_header_t;
typedef struct summary_header  summary_header_t;
typedef struct trace_header    trace_header_t;
typedef struct plot_header     plot_header_t;
typedef struct threads_header  threads_header_t;

typedef struct metric_entry    metric_entry_t;
typedef struct trace_index     trace_index_t;
typedef struct plot_index      plot_index_t;
typedef struct plot_entry      plot_entry_t;

#endif  // Prof_DB_File_Format_h
