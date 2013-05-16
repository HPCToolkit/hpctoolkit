#ifndef CORE_PROFILE_TRACE_DATA_H
#define CORE_PROFILE_TRACE_DATA_H

#include <stdint.h>
#include <stdio.h>
#include <lib/prof-lean/hpcio-buffer.h>

#include "epoch.h"
#include "cct2metrics.h"

typedef struct core_profile_trace_data_t {
  int id;
  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------
  epoch_t* epoch;

  //metrics: this is needed otherwise 
  //hpcprof does not pick them up
  cct2metrics_t* cct2metrics_map;

  // for metric scale (openmp uses)
  void (*scale_fn)(void*);
  // ----------------------------------------
  // tracing
  // ----------------------------------------
  uint64_t trace_min_time_us;
  uint64_t trace_max_time_us;
  // ----------------------------------------
  // IO support
  // ----------------------------------------
  FILE* hpcrun_file;
  void* trace_buffer;
  hpcio_outbuf_t trace_outbuf;
} core_profile_trace_data_t;


#endif























