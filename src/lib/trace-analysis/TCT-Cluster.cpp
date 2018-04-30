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
 * File:   TCTCluster.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on April 23, 2018, 12:47 AM
 */

#include "TCT-Cluster.hpp"
#include "data/TCT-Node.hpp"

namespace TraceAnalysis {
  // Compute difference between two duration ranges (min1, max1) and (min2, max2).
  // Noise from sampling is considered.
  // Noise should be no more than MAX_SAMPLE_NOISE times sampling period.
  Time AbstractTCTCluster::computeRangeDiff(Time min1, Time max1, Time min2, Time max2) {
    // Adjust duration ranges when noise is greater than MAX_SAMPLE_NOISE times sampling period.
    if (max1 - min1 > samplingPeriod * MAX_SAMPLE_NOISE) {
      Time mid1 = (max1 + min1) / 2;
      min1 = mid1 - samplingPeriod * MAX_SAMPLE_NOISE / 2;
      min1 = mid1 + samplingPeriod * MAX_SAMPLE_NOISE / 2;
    }
    if (max2 - min2 > samplingPeriod * MAX_SAMPLE_NOISE) {
      Time mid2 = (max2 + min2) / 2;
      min2 = mid2 - samplingPeriod * MAX_SAMPLE_NOISE / 2;
      min2 = mid2 + samplingPeriod * MAX_SAMPLE_NOISE / 2;
    }
    
    // If the two ranges don't intersect, return their minimum possible difference.
    if (max2 < min1) return min1 - max2;
    if (max1 < min2) return min2 - max1;
    
    // If the two ranges intersect, return 0.
    return 0;
  }
  
  TCTANode* AbstractTCTCluster::mergeNode(TCTANode* node1, int weight1, TCTANode* node2, int weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    if (!(node1->id == node2->id))
      print_msg(MSG_PRIO_MAX, "ERROR: merging two nodes with different id: %s vs %s.\n", 
              node1->id.toString().c_str(), node2->id.toString().c_str());
    
    if (node1->getWeight() != weight1)
      print_msg(MSG_PRIO_HIGH, "ERROR: weight doesn't match %d vs %d for \n %s", node1->getWeight(), weight1, 
              node1->toString(node1->getDepth(), 0, 0).c_str());
    if (node2->getWeight() != weight2)
      print_msg(MSG_PRIO_HIGH, "ERROR: weight doesn't match %d vs %d for \n %s", node2->getWeight(), weight2, 
              node2->toString(node2->getDepth(), 0, 0).c_str());
    if (node1->getDepth() != node2->getDepth())
      print_msg(MSG_PRIO_HIGH, "ERROR: merging node at difference depth %d vs %d.\n", node1->getDepth(), node2->getDepth());
    
    if (node1->type == TCTANode::Prof || node2->type == TCTANode::Prof) {
      TCTProfileNode* prof1 = node1->type == TCTANode::Prof ? (TCTProfileNode*)node1 : TCTProfileNode::newProfileNode(node1);
      TCTProfileNode* prof2 = node2->type == TCTANode::Prof ? (TCTProfileNode*)node2 : TCTProfileNode::newProfileNode(node2);
      TCTProfileNode* merged = mergeProfileNode(prof1, weight1, prof2, weight2, ifAccumulate, isScoreOnly);
      if (node1->type != TCTANode::Prof) delete prof1;
      if (node2->type != TCTANode::Prof) delete prof2;
      return merged;
    }
    else {
      if (node1->type != node2->type) {
        print_msg(MSG_PRIO_MAX, "ERROR: merging non-profile nodes but their types don't match %d vs %d.\n", node1->type, node2->type);
        return node1->voidDuplicate();
      }
      if (node1->type == TCTANode::Loop)
        return mergeLoopNode((TCTLoopNode*)node1, weight1, (TCTLoopNode*)node2, weight2, ifAccumulate, isScoreOnly);
      else 
        return mergeTraceNode((TCTATraceNode*)node1, weight1, (TCTATraceNode*)node2, weight2, ifAccumulate, isScoreOnly);
    }
  }
  
  TCTProfileNode* AbstractTCTCluster::mergeProfileNode(TCTProfileNode* prof1, int weight1, TCTProfileNode* prof2, int weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    TCTProfileNode* mergedProf = NULL;
    if (!isScoreOnly) {
      
    }
    
    return NULL;
  }
  
  TCTATraceNode* AbstractTCTCluster::mergeTraceNode(TCTATraceNode* trace1, int weight1, TCTATraceNode* trace2, int weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    
    return NULL;
  }
}