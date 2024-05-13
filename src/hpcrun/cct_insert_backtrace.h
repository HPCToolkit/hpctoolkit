// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef CCT_INSERT_BACKTRACE_H
#define CCT_INSERT_BACKTRACE_H

#include "cct/cct_bundle.h"
#include "cct/cct.h"
#include "unwind/common/backtrace.h"
#include "metrics.h"

typedef  cct_node_t *(*hpcrun_kernel_callpath_t)(cct_node_t *path, void *data_aux);

//
// interface routines
//
//

//
// Backtrace insertion
//
// Given a call path of the following form, insert the path into the
// calling context tree and, if successful, return the leaf node
// representing the sample point (innermost frame).
//
//               (low VMAs)                       (high VMAs)
//   backtrace: [inner-frame......................outer-frame]
//              ^ path_end                        ^ path_beg
//              ^ bt_beg                                       ^ bt_end
//
extern cct_node_t* hpcrun_cct_insert_backtrace(cct_node_t* cct, frame_t* path_beg, frame_t* path_end);

extern cct_node_t* hpcrun_cct_insert_backtrace_w_metric(cct_node_t* cct,
                                                        int metric_id,
                                                        frame_t* path_beg, frame_t* path_end,
                                                        cct_metric_data_t datum, void *data);

extern cct_node_t* hpcrun_cct_record_backtrace(cct_bundle_t* bndl, bool partial,
backtrace_info_t *bt,
                                               bool tramp_found);

extern cct_node_t* hpcrun_cct_record_backtrace_w_metric(cct_bundle_t* bndl, bool partial,
backtrace_info_t *bt,
                                bool tramp_found,
                                int metricId, hpcrun_metricVal_t metricIncr,
                                void *data);

extern cct_node_t* hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
        int metricId, hpcrun_metricVal_t metricIncr,
        int skipInner, int isSync, void *data);


extern void hpcrun_kernel_callpath_register(hpcrun_kernel_callpath_t kcp);

//
// debug version of hpcrun_backtrace2cct:
//   simulates errors to test partial unwind capability
//

#endif // CCT_INSERT_BACKTRACE_H
