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
 * File:   TraceCluster.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on April 23, 2018, 12:35 AM
 */

#ifndef TRACECLUSTER_HPP
#define TRACECLUSTER_HPP

#include "data/TCT-Node.hpp"

namespace TraceAnalysis {
  // Forward declarations
  class TCTANode;
  class TCTATraceNode;
  class TCTLoopNode;
  class TCTProfileNode;
  
  class AbstractTraceCluster {
  public:
    AbstractTraceCluster(const Time& samplingPeriod) : samplingPeriod(samplingPeriod){}
    
    const Time& getSamplingPeriod() const {
      return samplingPeriod;
    }
    
    /* Return the merged node of the input node1 and node2.
     * Callee responsible for deallocating the merged node.
     */
    TCTANode* mergeNode(const TCTANode* node1, int weight1, const TCTANode* node2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
    TCTProfileNode* mergeProfileNode(const TCTProfileNode* prof1, int weight1, const TCTProfileNode* prof2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
    TCTANode* mergeTraceNode(const TCTATraceNode* trace1, int weight1, const TCTATraceNode* trace2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
    virtual TCTANode* mergeLoopNode(const TCTLoopNode* loop1, int weight1, const TCTLoopNode* loop2, int weight2, 
            bool ifAccumulate, bool isScoreOnly) = 0;
  protected:
    const Time& samplingPeriod;
    
    Time computeRangeDiff(Time min1, Time max1, Time min2, Time max2);
  };
  
  class LocalTraceCluster : public AbstractTraceCluster {
  public:
    LocalTraceCluster(const Time& samplingPeriod) : AbstractTraceCluster(samplingPeriod) {}
    
    virtual TCTANode* mergeLoopNode(const TCTLoopNode* loop1, int weight1, const TCTLoopNode* loop2, int weight2, 
            bool ifAccumulate, bool isScoreOnly);
  };
}

#endif /* TRACECLUSTER_HPP */

