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
 * File:   TCT-Metrics.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on June 29, 2018, 10:02 AM
 */

#ifndef TCT_METRICS_HPP
#define TCT_METRICS_HPP

#include "../TraceAnalysisCommon.hpp"

namespace TraceAnalysis {
  class TCTDiffScore {
  public:
    TCTDiffScore() : inclusive(0), exclusive(0) {}
    TCTDiffScore(const TCTDiffScore& other): inclusive(other.inclusive), exclusive(other.exclusive) {}
    virtual ~TCTDiffScore() {}
    
    double getInclusive() const {
      return inclusive;
    }
    
    double getExclusive() const {
      return exclusive;
    }
    
    void setScores(double inclusive, double exclusive) {
      this->inclusive = inclusive;
      this->exclusive = exclusive;
    }
    
    void clear() {
      inclusive = 0;
      exclusive = 0;
    }
    
  private:
    double inclusive;
    double exclusive;
  };
  
  // Records performance loss metrics
  class TCTPerfLossMetric {
  public:
    TCTPerfLossMetric(): minDuration(0), maxDuration(0), totalDuration(0) {}
    TCTPerfLossMetric(const TCTPerfLossMetric& other): minDuration(other.minDuration), 
        maxDuration(other.maxDuration), totalDuration(other.totalDuration){}
    virtual ~TCTPerfLossMetric() {}
    
    void initDurationMetric(Time duration, int weight);
    void setDuratonMetric(const TCTPerfLossMetric& rep1, const TCTPerfLossMetric& rep2);
    
    void clear() {
      minDuration = 0;
      maxDuration = 0;
      totalDuration = 0;
    }
    
  private:
    // Metrics that records durations of all instances represented by this TCTANode
    Time minDuration;
    Time maxDuration;
    double totalDuration;
  };
}

#endif /* TCT_METRICS_HPP */

