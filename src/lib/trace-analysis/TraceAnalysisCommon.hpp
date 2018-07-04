// -*-Mode: C++;-*-

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
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

/* 
 * File:   TraceAnalysisCommon.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 2, 2018, 11:59 AM
 * 
 * Trace analysis common types and utilities.
 */

#ifndef TRACEANALYSISCOMMON_HPP
#define TRACEANALYSISCOMMON_HPP

#include <string>
using std::string;

namespace TraceAnalysis {
  typedef unsigned long VMA; // Virtual Memory Address
  typedef int64_t Time; // Virtual Memory Address
  
  // Parameters and functions for printing messages.
  const int MSG_PRIO_LOW = 0;
  const int MSG_PRIO_NORMAL = 1;
  const int MSG_PRIO_HIGH = 2;
  const int MSG_PRIO_MAX = 3;
  void print_msg(int level, const char *fmt,...);
  string vmaToHexString(VMA vma);
  string timeToString(Time time);
  
  // Parameters for iteration identification.
  const int ITER_CHILD_DUR_ACC = 5; // 5 samples
  const int ITER_NUM_CHILD_ACC = 5;
  const int ITER_NUM_FUNC_ACC = 1;
  const int ITER_NUM_LOOP_ACC = 2;
  // If (duration of rejected iterations / duration of loop) is larger than
  // LOOP_REJ_THRESHOLD, the whole loop will be rejected.
  const double LOOP_REJ_THRESHOLD = 0.3; 

  // Parameters and functions for difference quantification.
  const uint MAX_SAMPLE_NOISE = 10; // 10 samples
  const Time computeWeightedAverage(Time time1, long w1, Time time2, long w2);
  
  // Parameters for clustering.
  const int MAX_NUM_CLUSTER = 10;
  const double MIN_DIFF_RATIO = 0.01;
  const double REL_DIFF_RATIO = 0.25;
  
  const int RSD_DETECTION_WINDOW = 16;
  const int RSD_DETECTION_LENGTH = 5;
  const int PRSD_DETECTION_LENGTH = 3;
}

#endif /* TRACEANALYSISCOMMON_HPP */

