// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-omp/src/tool/hpcrun/sample-sources/memleak.h $
// $Id: memleak.h 3680 2012-02-25 22:14:00Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

//
// MEMLEAK sample source public interface:
//
//  The allocate/free wrappers for the MEMLEAK sample source are not 
//  housed with the rest of the MEMLEAK code. They will be separate 
//  so that they are not linked into all executables. 
//
//  To avoid exposing the details of the MEMLEAK handler via global variables,
//  the following procedural interfaces are provided.
//
//

#ifndef sample_source_memleak_h
#define sample_source_memleak_h

/******************************************************************************
 * local includes 
 *****************************************************************************/
/******************************************************************************
 * external variables 
 *****************************************************************************/
// set the sampling period glablly used by all threads
// using external variable rather than a query function 
// to reduce the call overhead
extern long ma_period;
/******************************************************************************
 * interface operations
 *****************************************************************************/

int hpcrun_ma_metric_id();
int hpcrun_numa_match_metric_id();
int hpcrun_numa_mismatch_metric_id();
int hpcrun_low_offset_metric_id();
int hpcrun_high_offset_metric_id();
int *hpcrun_location_metric_id();

void hpcrun_metric_min(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr);
void hpcrun_metric_max(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr);

#endif // sample_source_memleak_h
