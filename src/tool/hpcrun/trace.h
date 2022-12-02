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

#ifndef hpcrun_trace_h
#define hpcrun_trace_h
#include <stdint.h>
#include "files.h"
#include "core_profile_trace_data.h"


typedef enum hpcrun_trace_type_masks {
    HPCRUN_CPU_TRACE_FLAG = 1,
    HPCRUN_GPU_TRACE_FLAG = 2
} hpcrun_trace_type_masks_t;

typedef enum hpcrun_trace_type {
    HPCRUN_NO_TRACE,
    HPCRUN_SAMPLE_TRACE,
    HPCRUN_CALL_TRACE,
} hpcrun_trace_type_t;

void trace_other_close(void *thread_data);

void hpcrun_trace_init();
void hpcrun_trace_open(core_profile_trace_data_t * cptd, hpcrun_trace_type_t type);
void hpcrun_trace_append(core_profile_trace_data_t *cptd, cct_node_t* node, unsigned int metric_id, uint32_t dLCA, uint64_t sampling_period);
void hpcrun_trace_append_with_time(core_profile_trace_data_t *st, unsigned int call_path_id, unsigned int metric_id, uint64_t nanotime);
void hpcrun_trace_close(core_profile_trace_data_t * cptd);

void hpcrun_trace_append_stream(core_profile_trace_data_t *cptd, cct_node_t *node, unsigned int metric_id, uint32_t dLCA, uint64_t nanotime);

int hpcrun_trace_isactive();

// When a sample source finds that a metric suitable for tracing
// is specified, we call this function to indicate we can do tracing
void hpcrun_set_trace_metric(hpcrun_trace_type_masks_t);

// After processing all command line events, we call this function
// to check whether a metric suitable for tracing exists
int hpcrun_has_trace_metric();

int hpcrun_cpu_trace_on();
int hpcrun_gpu_trace_on();

#endif // hpcrun_trace_h
