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
 * File:   TCT-Metrics.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on June 29, 2018, 10:11 AM
 */

#include "TCT-Metrics.hpp"
#include "TCT-Node.hpp"

namespace TraceAnalysis {
  void TCTPerfLossMetric::initDurationMetric(Time duration, int weight) {
    maxDuration = duration;
    minDuration = duration;
    totalDuration = (double)duration * (double)weight;
  }
  
  void TCTPerfLossMetric::setDuratonMetric(const TCTPerfLossMetric& rep1, const TCTPerfLossMetric& rep2) {
    maxDuration = std::max(rep1.maxDuration, rep2.maxDuration);
    minDuration = std::min(rep1.minDuration, rep2.minDuration);
    totalDuration = rep1.totalDuration + rep2.totalDuration;
  }
}
  