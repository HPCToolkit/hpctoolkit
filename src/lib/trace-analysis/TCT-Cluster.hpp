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
 * File:   TCT-Cluster.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on April 23, 2018, 12:35 AM
 */

#ifndef TCT_CLUSTER_HPP
#define TCT_CLUSTER_HPP

#include "data/TCT-Node.hpp"

namespace TraceAnalysis {
  class AbstractTCTCluster {
  public:
    AbstractTCTCluster(Time samplingPeriod): samplingPeriod(samplingPeriod){}
    
    TCTANode* mergeNode(TCTANode* node1, int weight1, TCTANode* node2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
    TCTProfileNode* mergeProfileNode(TCTProfileNode* prof1, int weight1, TCTProfileNode* prof2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
    TCTATraceNode* mergeTraceNode(TCTATraceNode* trace1, int weight1, TCTATraceNode* trace2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
    virtual TCTLoopNode* mergeLoopNode(TCTLoopNode* loop1, int weight1, TCTLoopNode* loop2, int weight2, 
            bool ifAccumulate, bool isScoreOnly) = 0;
  private:
    Time samplingPeriod;
    
    Time computeRangeDiff(Time min1, Time max1, Time min2, Time max2);
  };
}

#endif /* TCT_CLUSTER_HPP */

