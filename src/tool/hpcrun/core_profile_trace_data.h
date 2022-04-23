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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef CORE_PROFILE_TRACE_DATA_H
#define CORE_PROFILE_TRACE_DATA_H

#include "cct2metrics.h"
#include "epoch.h"

#include "lib/prof-lean/hpcfmt.h"  // for metric_aux_info_t
#include "lib/prof-lean/hpcio-buffer.h"

#include <stdint.h>
#include <stdio.h>

enum perf_ksym_e { PERF_UNDEFINED, PERF_AVAILABLE, PERF_UNAVAILABLE };

typedef struct core_profile_trace_data_t {
  int id;
  id_tuple_t id_tuple;
  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------
  epoch_t* epoch;

  // metrics: this is needed otherwise
  // hpcprof does not pick them up
  cct2metrics_t* cct2metrics_map;

  // for metric scale (openmp uses)
  void (*scale_fn)(void*);
  // ----------------------------------------
  // tracing
  // ----------------------------------------
  uint64_t trace_min_time_us;
  uint64_t trace_max_time_us;

  // Whether the trace is ordered, and if not how disordered we expect it to be
  bool trace_is_ordered;
  unsigned int trace_expected_disorder;
  // Last timestamp in the trace, so we can tell if its ordered
  uint64_t trace_last_time;

  // ----------------------------------------
  // IO support
  // ----------------------------------------
  FILE* hpcrun_file;
  void* trace_buffer;
  hpcio_outbuf_t* trace_outbuf;
} core_profile_trace_data_t;

#endif
