// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef CSPROF_METRICS_H
#define CSPROF_METRICS_H

#include <sys/types.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

// tallent: I have moved flags into hpcfile_csprof.h.  The flags don't
// really belong there but:
// 1) metrics.c uses hpcfile_csprof_data_t to implement metrics
//    info, which already confuses boundaries
// 2) metric info needs to exist in a library so csprof (hpcrun),
//    xcsprof (hpcprof) and hpcfile can use it.  hpcfile at least
//    satisfies this.

int 
csprof_get_max_metrics();

/* Set the maximum number of metrics that can be tracked by the library
   simultaneously.  This function can only be called for effect once. */
int 
csprof_set_max_metrics(int max_metrics);

/* Return the number of metrics being recorded. */
int 
csprof_num_recorded_metrics();

/* Request a new metric id.  Returns the new metric id on success, -1
   on failure (that is, you are already tracking the maximum number of
   metrics possible). */
int 
csprof_new_metric();

/* Associate `name' with `metric_id' for later processing.  The sample
   period defaults to `1' for this metric. */
void 
csprof_set_metric_info(int metric_id, char *name, hpcrun_metricFlags_t flags);

/* A more detailed version of `csprof_set_metric_info'. */
void 
csprof_set_metric_info_and_period(int metric_id, char *name,
				  hpcrun_metricFlags_t flags, size_t period);

/* Record `value' for `metric_id'. */
void 
csprof_record_metric(int metric_id, size_t value);

metric_tbl_t* 
hpcrun_get_metric_data();

#endif
