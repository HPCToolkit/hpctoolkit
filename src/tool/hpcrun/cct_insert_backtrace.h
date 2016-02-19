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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef CCT_INSERT_BACKTRACE_H
#define CCT_INSERT_BACKTRACE_H

#include <cct/cct_bundle.h>
#include <cct/cct.h>
#include <unwind/common/backtrace.h>
#include "metrics.h"

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
							cct_metric_data_t datum);

extern cct_node_t* hpcrun_cct_record_backtrace(cct_bundle_t* bndl, bool partial, 
backtrace_info_t *bt,
#if 0
bool thread_stop,
					       frame_t* bt_beg, frame_t* bt_last,
#endif
					       bool tramp_found);

extern cct_node_t* hpcrun_cct_record_backtrace_w_metric(cct_bundle_t* bndl, bool partial, 
backtrace_info_t *bt,
#if 0
bool thread_stop,
							frame_t* bt_beg, frame_t* bt_last,
#endif
							bool tramp_found,
							int metricId, uint64_t metricIncr);

extern cct_node_t* hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context, 
                                        void **trace_pc,
                                        int metricId, uint64_t metricIncr,
                                        int skipInner, int isSync);
//
// debug version of hpcrun_backtrace2cct:
//   simulates errors to test partial unwind capability
//

extern cct_node_t* hpcrun_dbg_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
                                            void **trace_pc,
                                            int metricId, uint64_t metricIncr,
                                            int skipInner);
#endif // CCT_INSERT_BACKTRACE_H
