// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2017, Rice University
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

#ifndef __PERF_UTIL_H__
#define __PERF_UTIL_H__

#include <linux/types.h>
#include <linux/perf_event.h>

#include <lib/prof-lean/hpcrun-fmt.h>

#ifndef u32
typedef __u32 u32;
#endif


#ifndef u64
typedef __u64 u64;
#endif

// data from perf's mmap. See perf_event_open man page
typedef struct perf_mmap_data_s {
  struct perf_event_header header;
  u64    sample_id;  /* if PERF_SAMPLE_IDENTIFIER */
  u64    ip;         /* if PERF_SAMPLE_IP */
  u32    pid, tid;   /* if PERF_SAMPLE_TID */
  u64    time;       /* if PERF_SAMPLE_TIME */
  u64    addr;       /* if PERF_SAMPLE_ADDR */
  u64    id;         /* if PERF_SAMPLE_ID */
  u64    stream_id;  /* if PERF_SAMPLE_STREAM_ID */
  u32    cpu, res;   /* if PERF_SAMPLE_CPU */
  u64    period;     /* if PERF_SAMPLE_PERIOD */
                     /* if PERF_SAMPLE_READ */
  u64    nr;         /* if PERF_SAMPLE_CALLCHAIN */
  u64    *ips;       /* if PERF_SAMPLE_CALLCHAIN */
  u32    size;       /* if PERF_SAMPLE_RAW */
  char   *data;      /* if PERF_SAMPLE_RAW */
  /* if PERF_SAMPLE_BRANCH_STACK */
  
                     /* if PERF_SAMPLE_BRANCH_STACK */
  u64    abi;        /* if PERF_SAMPLE_REGS_USER */
  u64    *regs;
                     /* if PERF_SAMPLE_REGS_USER */
  u64    stack_size;             /* if PERF_SAMPLE_STACK_USER */
  char   *stack_data; /* if PERF_SAMPLE_STACK_USER */
  u64    stack_dyn_size;         /* if PERF_SAMPLE_STACK_USER &&
                                     size != 0 */
  u64    weight;     /* if PERF_SAMPLE_WEIGHT */
  u64    data_src;   /* if PERF_SAMPLE_DATA_SRC */
  u64    transaction;/* if PERF_SAMPLE_TRANSACTION */
  u64    intr_abi;        /* if PERF_SAMPLE_REGS_INTR */
  u64    *intr_regs;
                     /* if PERF_SAMPLE_REGS_INTR */
} perf_mmap_data_t;


// main data structure to store the information of an event.
// this structure is designed to be created once during the initialization.
// this code doesn't work if the number of events change dynamically.
typedef struct event_info_s {
  int    id;
  struct perf_event_attr attr; // the event attribute
  int    metric;               // metric ID of the event (raw counter)
  metric_desc_t *metric_desc;  // pointer on hpcrun metric descriptor
  int  	 index_predefined;	   // index of the predefined metric (if this event is
  	  	  	  	  	  	  	   // a customized hpcrun event)
  int    metric_predefined;
  metric_desc_t *metric_desc_predefined; // pointer to predefined metric
} event_info_t;


typedef struct perf_event_mmap_page pe_mmap_t;


// data perf event per thread per event
// this data is designed to be used within a thread
typedef struct event_thread_s {
  pe_mmap_t   *mmap;    // mmap buffer
  int    fd;            // file descriptor of the event
  event_info_t  *event; // pointer to main event description
} event_thread_t;

#endif
