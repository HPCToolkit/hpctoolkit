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
extern cct_node_t* hpcrun_cct_insert_backtrace(cct_bundle_t* bndl, cct_node_t* cct,
                                               int metric_id,
                                               frame_t* path_beg, frame_t* path_end,
                                               cct_metric_data_t datum);

extern cct_node_t* hpcrun_cct_record_backtrace(cct_bundle_t* bndl, bool partial,
                                               frame_t* bt_beg, frame_t* bt_last,
                                               bool tramp_found,
                                               int metricId, uint64_t metricIncr);

extern cct_node_t* hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context, 
                                        int metricId, uint64_t metricIncr,
                                        int skipInner, int isSync);
//
// debug version of hpcrun_backtrace2cct:
//   simulates errors to test partial unwind capability
//

extern cct_node_t* hpcrun_dbg_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
                                            int metricId, uint64_t metricIncr,
                                            int skipInner);

#endif // CCT_INSERT_BACKTRACE_H
