// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __PERF_UTIL_H__
#define __PERF_UTIL_H__

#include <sys/syscall.h>

#include <unistd.h>
#include <linux/types.h>
#include <linux/perf_event.h>

#include "../../../common/lean/hpcrun-fmt.h"
#include "../../sample_event.h"

#include "perf_constants.h"
#include "event_custom.h"

/******************************************************************************
 * macros
 *****************************************************************************/



// the number of maximum frames (call chains)
// For kernel only call chain, I think 32 is a good number.
// If we include user call chains, it should be bigger than that.
#define MAX_CALLCHAIN_FRAMES 32


/******************************************************************************
 * Data types
 *****************************************************************************/

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
  u64    ips[MAX_CALLCHAIN_FRAMES];       /* if PERF_SAMPLE_CALLCHAIN */
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

  // header information in the buffer
  u32   header_misc; /* information about the sample */
  u32   header_type; /* either sample record or other */

} perf_mmap_data_t;


// --------------------------------------------------------------
// main data structure to store the information of an event.
// this structure is designed to be created once during the initialization.
// this code doesn't work if the number of events change dynamically.
// --------------------------------------------------------------
typedef struct event_info_s {
  int    id;
  struct perf_event_attr attr; // the event attribute
  int    perf_metric_id;
  int    hpcrun_metric_id;
  metric_desc_t *metric_desc;  // pointer on hpcrun metric descriptor

  // predefined metric
  event_custom_t *metric_custom;        // pointer to the predefined metric

} event_info_t;


typedef struct perf_event_mmap_page pe_mmap_t;


// --------------------------------------------------------------
// data perf event per thread per event
// this data is designed to be used within a thread
// --------------------------------------------------------------
typedef struct event_thread_s {

  pe_mmap_t    *mmap;  // mmap buffer
  int          fd;     // file descriptor of the event
  event_info_t *event; // pointer to main event description

} event_thread_t;



/******************************************************************************
 * Interfaces
 *****************************************************************************/

void
perf_util_init();

int
perf_util_attr_init(
  char *event_name,
  struct perf_event_attr *attr,
  bool usePeriod, u64 threshold,
  u64  sampletype
);

bool
perf_util_is_ksym_available();

int
perf_util_get_paranoid_level();

int
perf_util_get_max_sample_rate();

int
perf_util_check_precise_ip_suffix(char *event);

#endif
