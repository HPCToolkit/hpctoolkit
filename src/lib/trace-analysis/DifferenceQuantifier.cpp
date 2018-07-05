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
 * File:   DifferenceQuantifier.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on April 23, 2018, 12:47 AM
 */

#include "TraceAnalysisCommon.hpp"
#include "DifferenceQuantifier.hpp"
#include "data/TCT-Node.hpp"

namespace TraceAnalysis {
  // Compute difference between two duration ranges (min1, max1) and (min2, max2).
  // Noise from sampling is considered.
  // Noise should be no more than MAX_SAMPLE_NOISE times sampling period.
  Time AbstractDifferenceQuantifier::computeRangeDiff(Time min1, Time max1, Time min2, Time max2) {
    // Adjust duration ranges when noise is greater than MAX_SAMPLE_NOISE times sampling period.
    if (max1 - min1 > getSamplingPeriod() * MAX_SAMPLE_NOISE) {
      Time mid1 = (max1 + min1) / 2;
      min1 = mid1 - getSamplingPeriod() * MAX_SAMPLE_NOISE / 2;
      max1 = mid1 + getSamplingPeriod() * MAX_SAMPLE_NOISE / 2;
    }
    if (max2 - min2 > getSamplingPeriod() * MAX_SAMPLE_NOISE) {
      Time mid2 = (max2 + min2) / 2;
      min2 = mid2 - getSamplingPeriod() * MAX_SAMPLE_NOISE / 2;
      max2 = mid2 + getSamplingPeriod() * MAX_SAMPLE_NOISE / 2;
    }
    
    // If the two ranges don't intersect, return their minimum possible difference.
    if (max2 < min1) return min1 - max2;
    if (max1 < min2) return min2 - max1;
    
    // If the two ranges intersect, return 0.
    return 0;
  }
  
  TCTANode* AbstractDifferenceQuantifier::mergeNode(const TCTANode* node1, long weight1, const TCTANode* node2, long weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    if (!(node1->id == node2->id))
      print_msg(MSG_PRIO_MAX, "ERROR: merging two nodes with different id: %s vs %s.\n", 
              node1->id.toString().c_str(), node2->id.toString().c_str());
    
    if (node1->getWeight() != weight1)
      print_msg(MSG_PRIO_HIGH, "ERROR: weight doesn't match %d vs %d for \n %s", node1->getWeight(), weight1, 
              node1->toString(node1->getDepth(), 0).c_str());
    if (node2->getWeight() != weight2)
      print_msg(MSG_PRIO_HIGH, "ERROR: weight doesn't match %d vs %d for \n %s", node2->getWeight(), weight2, 
              node2->toString(node2->getDepth(), 0).c_str());
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
      if (node1->type == TCTANode::Loop) {
        return mergeLoopNode((TCTLoopNode*)node1, weight1, (TCTLoopNode*)node2, weight2, ifAccumulate, isScoreOnly);
      }
      else 
        return mergeTraceNode((TCTATraceNode*)node1, weight1, (TCTATraceNode*)node2, weight2, ifAccumulate, isScoreOnly);
    }
  }
  
  TCTProfileNode* AbstractDifferenceQuantifier::mergeProfileNode(const TCTProfileNode* prof1, long weight1, const TCTProfileNode* prof2, long weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    TCTProfileNode* mergedProf = (TCTProfileNode*)prof1->voidDuplicate();
    if (!isScoreOnly) {
      mergedProf->setWeight(weight1 + weight2);
      mergedProf->getTime().setAsAverageTime(prof1->getTime(), weight1, prof2->getTime(), weight2);
      mergedProf->getPerfLossMetric().setDuratonMetric(prof1->getPerfLossMetric(), prof2->getPerfLossMetric());
    }

    /**
     * When accumulating difference scores, the inclusive difference score of a node is the sum of difference scores in its subtree.
     * Inclusive difference score of the merged profile node consists of the following components --
     *  1) the inclusive difference score of all children, which can be divided to:
     *    1.1) the inclusive difference score of all children in prof1;
     *    1.2) the inclusive difference score of all children in prof2;
     *    1.3) the difference score of children between prof1 and prof2;
     *  2) the difference of exclusive duration among all nodes represented by the merged profile node.
     *    2.1) the difference of exclusive duration among all nodes represented by prof1
     *    2.2) the difference of exclusive duration among all nodes represented by prof2
     *    2.3) the difference of exclusive duration between nodes represented by prof1 and prof2
     *  
     *  When not accumulating, only 1.3) and 2.3) needs to be added up.
     */
    double inclusiveDiff = 0;
    
    const map<TCTID, TCTProfileNode*>& map1 = prof1->getChildMap();
    const map<TCTID, TCTProfileNode*>& map2 = prof2->getChildMap();
    
    for (auto it = map1.begin(); it != map1.end(); it++) {
      const TCTProfileNode* child1 = it->second;
      // Children that both profile nodes have
      if (map2.find(child1->id) != map2.end()) {
        const TCTProfileNode* child2 = map2.find(child1->id)->second;
        TCTProfileNode* mergedChild = (TCTProfileNode*)mergeNode(child1, weight1, child2, weight2, ifAccumulate, isScoreOnly);
        inclusiveDiff += mergedChild->getDiffScore().getInclusive(); // belongs to 1.3) when not accumulating and 1) when accumulating
        
        if (isScoreOnly) delete mergedChild;
        else mergedProf->addChild(mergedChild);
      }
      else { // Children that only profile 1 has.
        TCTProfileNode* child2 = (TCTProfileNode*) child1->voidDuplicate();
        child2->setWeight(weight2);
        child2->getTime().setDuration(-getSamplingPeriod(), getSamplingPeriod());
        
        TCTProfileNode* mergedChild = (TCTProfileNode*)mergeNode(child1, weight1, child2, weight2, ifAccumulate, isScoreOnly);
        inclusiveDiff += mergedChild->getDiffScore().getInclusive(); // belongs to 1.3) when not accumulating and 1) when accumulating
        
        if (isScoreOnly) delete mergedChild;
        else mergedProf->addChild(mergedChild);
        
        delete child2;
      }
    }
    
    for (auto it = map2.begin(); it != map2.end(); it++)
      // Children that only profile 2 has.
      if (map1.find(it->first) == map1.end()) {
        const TCTProfileNode* child2 = it->second;
        TCTProfileNode* child1 = (TCTProfileNode*) child2->voidDuplicate();
        child1->setWeight(weight1);
        child1->getTime().setDuration(-getSamplingPeriod(), getSamplingPeriod());
        
        TCTProfileNode* mergedChild = (TCTProfileNode*)mergeNode(child1, weight1, child2, weight2, ifAccumulate, isScoreOnly);
        inclusiveDiff += mergedChild->getDiffScore().getInclusive(); // belongs to 1.3) when not accumulating and 1) when accumulating

        if (isScoreOnly) delete mergedChild;
        else mergedProf->addChild(mergedChild);
        
        delete child1;
      }
    
    Time minExclusive1, maxExclusive1, minExclusive2, maxExclusive2;
    prof1->getExclusiveDuration(minExclusive1, maxExclusive1);
    prof2->getExclusiveDuration(minExclusive2, maxExclusive2);
    
    inclusiveDiff += (double)computeRangeDiff(minExclusive1, maxExclusive1, minExclusive2, maxExclusive2)
            * (double) weight1 * (double) weight2;  // belongs to 2.3)

    /**
     * The exclusive difference score of a node is the sum of difference at this node (not counting is children),
     * which is the sum of difference of inclusive duration among all nodes represented by the merged profile node.
     * When accumulating, exclusive difference score of the merged profile node consists of the following components -- 
     *  1) the difference of inclusive duration among all nodes represented by prof1
     *  2) the difference of inclusive duration among all nodes represented by prof2
     *  3) the difference of inclusive duration between nodes represented by prof1 and prof2
     * 
     * When not accumulating, only 3) is needed.
     */
    double exclusiveDiff = (double)computeRangeDiff(prof1->getMinDuration(), prof1->getMaxDuration(), 
                                                    prof2->getMinDuration(), prof2->getMaxDuration())
            * (double) weight1 * (double) weight2; // Add 3)
    
    if (ifAccumulate) {
      // Add 1) and 2) to exclusive diff score.
      exclusiveDiff += prof1->getDiffScore().getExclusive() + prof2->getDiffScore().getExclusive();
      
      // The above code has added 1) and 2.3) to inclusive diff score. Add 2.1) and 2.2) to inclusive diff score below.
      // Add 2.1)
      inclusiveDiff += prof1->getDiffScore().getInclusive();
      for (auto it = map1.begin(); it != map1.end(); it++)
        inclusiveDiff -= it->second->getDiffScore().getInclusive();
      
      // Add 2.2)
      inclusiveDiff += prof2->getDiffScore().getInclusive();
      for (auto it = map2.begin(); it != map2.end(); it++)
        inclusiveDiff -= it->second->getDiffScore().getInclusive();
    }
    
    /* Inclusive diff score should be no less than "exclusive diff score" plus minimum accumulation.
     * (As we hide noises from sampling, the resulting inclusive diff score could be smaller than 
     * the minimum threshold)
     */
    double minInclusiveDiff = (double)computeRangeDiff(prof1->getMinDuration(), prof1->getMaxDuration(), 
                                                    prof2->getMinDuration(), prof2->getMaxDuration())
            * (double) weight1 * (double) weight2;
    if (ifAccumulate)
      minInclusiveDiff += prof1->getDiffScore().getInclusive() + prof2->getDiffScore().getInclusive();
    
    if (inclusiveDiff < minInclusiveDiff) inclusiveDiff = minInclusiveDiff;
    
    mergedProf->getDiffScore().setScores(inclusiveDiff, exclusiveDiff);
    
    return mergedProf;
  }
  
  TCTANode* AbstractDifferenceQuantifier::mergeTraceNode(const TCTATraceNode* trace1, long weight1, const TCTATraceNode* trace2, long weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    TCTATraceNode* mergedTrace = (TCTATraceNode*)trace1->voidDuplicate();
    if (!isScoreOnly) {
      mergedTrace->setWeight(weight1 + weight2);
      mergedTrace->getTime().setAsAverageTime(trace1->getTime(), weight1, trace2->getTime(), weight2);
      mergedTrace->getPerfLossMetric().setDuratonMetric(trace1->getPerfLossMetric(), trace2->getPerfLossMetric());
    }
    
    /**
     * Inclusive difference score of the merged trace node consists of the following components --
     *  1) the inclusive difference score of all children, which can be divided to:
     *    1.1) the inclusive difference score of all children in trace1;
     *    1.2) the inclusive difference score of all children in trace2;
     *    1.3) the difference score of children between trace1 and trace2;
     *  2) the difference of gaps, which can be divided to:
     *    2.1) the difference of gaps among all nodes represented by prof1
     *    2.2) the difference of gaps among all nodes represented by prof2
     *    2.3) the difference of gaps between nodes represented by prof1 and prof2
     *  
     *  When not accumulating, only 1.3) and 2.3) needs to be added up.
     */
    double inclusiveDiff = 0;
    
    Time gapDiffMin = 0; // minimun value of gapDiff = (gap1 - gap2)
    Time gapDiffMax = 0; // maximun value of gapDiff = (gap1 - gap2)
    
    int k1 = 0, k2 = 0;
    
    while (k1 < trace1->getNumChild() || k2 < trace2->getNumChild()) {
      // When children of trace1 and trace2 match
      if (k1 < trace1->getNumChild() && k2 < trace2->getNumChild()
              && trace1->getChild(k1)->id == trace2->getChild(k2)->id) {
        // Add gap diff 2.3)
        Time minGap1, maxGap1, minGap2, maxGap2;
        trace1->getGapBeforeChild(k1, minGap1, maxGap1);
        trace2->getGapBeforeChild(k2, minGap2, maxGap2);
        inclusiveDiff += (double)computeRangeDiff(minGap1+gapDiffMin, maxGap1+gapDiffMax, minGap2, maxGap2)
            * (double) weight1 * (double) weight2;
        gapDiffMin = 0;
        gapDiffMax = 0;
        
        // Add child diff: 1.3) when not accumulating and 1) when accumulating
        const TCTANode* child1 = trace1->getChild(k1);
        const TCTANode* child2 = trace2->getChild(k2);
        TCTANode* mergedChild = mergeNode(child1, weight1, child2, weight2, ifAccumulate, isScoreOnly);
        inclusiveDiff += mergedChild->getDiffScore().getInclusive();
        
        if (isScoreOnly) delete mergedChild;
        else mergedTrace->addChild(mergedChild);
        
        k1++; k2++;
      }
      // When children of trace1 and trace2 don't match
      else {
        int compare = 0; // compare < 0 means trace1[k1] should be processed first; > 0 otherwise.
        if (k1 == trace1->getNumChild()) compare = 1;
        if (k2 == trace2->getNumChild()) compare = -1;
        
        if (compare == 0  
            && (trace1->cfgGraph == NULL // When cfgGraph is NULL
               || !trace1->cfgGraph->hasChild(trace1->getChild(k1)->ra) // or RA of trace1[k1] is not found in cfgGraph
               || !trace1->cfgGraph->hasChild(trace2->getChild(k2)->ra) // or RA of trace2[k2] is not found in cfgGraph
               )) { // Return a merged profile node.
          delete mergedTrace;
          TCTProfileNode* prof1 = TCTProfileNode::newProfileNode(trace1);
          TCTProfileNode* prof2 = TCTProfileNode::newProfileNode(trace2);
          TCTProfileNode* mergedProf = mergeProfileNode(prof1, weight1, prof2, weight2, ifAccumulate, isScoreOnly);
          delete prof1;
          delete prof2;
          return mergedProf;
        }
        
        if (compare == 0) // Process the child that proceeds in control flow.
          compare = trace1->cfgGraph->compareChild(trace1->getChild(k1)->ra, trace2->getChild(k2)->ra);
        
        if (compare == 0) { // Tie breaker with TCTID.
          if (trace1->getChild(k1)->id < trace2->getChild(k2)->id) compare = -1;
          else compare = 1;
        }
        
        if (compare < 0) { // Process trace1[k1] first
          const TCTANode* child1 = trace1->getChild(k1);
          
          // Create dummy child2 node
          TCTANode* child2 = child1->voidDuplicate();
          child2->setWeight(trace2->getWeight());
          // Set start/end time for the dummy child2 node.
          Time startExclusiveMin, startInclusiveMin; // child2 should never start earlier than the last child of trace2.
          trace2->getLastChildEndTime(k2, startExclusiveMin, startInclusiveMin);
          Time startExclusiveMax, startInclusiveMax; // child2 should never start later than the current child of trace2.
          trace2->getCurrChildStartTime(k2, startExclusiveMax, startInclusiveMax);
          // Set the midpoints as start/end time for the dummy node.
          Time startExclusive = (startExclusiveMin + startExclusiveMax) / 2;
          Time startInclusive = (startInclusiveMin + startInclusiveMax) / 2;
          child2->getTime().setStartTime(startExclusive, startInclusive);
          child2->getTime().setEndTime(startExclusive, startInclusive);
          
          TCTANode* mergedChild = mergeNode(child1, weight1, child2, weight2, ifAccumulate, isScoreOnly);
          inclusiveDiff += mergedChild->getDiffScore().getInclusive(); // Add child diff: 1.3) when not accumulating and 1) when accumulating

          if (isScoreOnly) delete mergedChild;
          else mergedTrace->addChild(mergedChild);
          
          delete child2;
          
          // Accumulate gap for next match
          Time gapMin, gapMax;
          trace1->getGapBeforeChild(k1, gapMin, gapMax);
          gapDiffMin += gapMin;
          gapDiffMax += gapMax;
          
          k1++;
        }
        else { // Process trace2[k2] first
          const TCTANode* child2 = trace2->getChild(k2);
          
          // Create dummy child1 node
          TCTANode* child1 = child2->voidDuplicate();
          child1->setWeight(trace1->getWeight());
          // Set start/end time for the dummy child1 node.
          Time startExclusiveMin, startInclusiveMin; // child1 should never start earlier than the last child of trace1.
          trace1->getLastChildEndTime(k1, startExclusiveMin, startInclusiveMin);
          Time startExclusiveMax, startInclusiveMax; // child1 should never start later than the current child of trace1.
          trace1->getCurrChildStartTime(k1, startExclusiveMax, startInclusiveMax);
          // Set the midpoints as start/end time for the dummy node.
          Time startExclusive = (startExclusiveMin + startExclusiveMax) / 2;
          Time startInclusive = (startInclusiveMin + startInclusiveMax) / 2;
          child1->getTime().setStartTime(startExclusive, startInclusive);
          child1->getTime().setEndTime(startExclusive, startInclusive);
          
          TCTANode* mergedChild = mergeNode(child1, weight1, child2, weight2, ifAccumulate, isScoreOnly);
          inclusiveDiff += mergedChild->getDiffScore().getInclusive(); // Add child diff: 1.3) when not accumulating and 1) when accumulating

          if (isScoreOnly) delete mergedChild;
          else mergedTrace->addChild(mergedChild);
          
          delete child1;
          
          // Accumulate gap for next match
          Time gapMin, gapMax;
          trace2->getGapBeforeChild(k2, gapMin, gapMax);
          gapDiffMin -= gapMax;
          gapDiffMax -= gapMin;
          
          k2++;
        }
      }
    }
    
    // Final gap
    Time gapMin, gapMax;
    trace1->getGapBeforeChild(k1, gapMin, gapMax);
    gapDiffMin += gapMin;
    gapDiffMax += gapMax;
    
    trace2->getGapBeforeChild(k2, gapMin, gapMax);
    gapDiffMin -= gapMax;
    gapDiffMax -= gapMin;
    
    inclusiveDiff += (double)computeRangeDiff(gapDiffMin, gapDiffMax, 0, 0)
            * (double) weight1 * (double) weight2; // Add gap diff 2.3)
    
     /* When accumulating, exclusive difference score of the merged trace node consists of the following components -- 
     *  1) the difference of inclusive duration among all nodes represented by trace1
     *  2) the difference of inclusive duration among all nodes represented by trace2
     *  3) the difference of inclusive duration between nodes represented by trace1 and trace2
     * 
     * When not accumulating, only 3) is needed.
     */
    double exclusiveDiff = (double)computeRangeDiff(trace1->getMinDuration(), trace1->getMaxDuration(), 
                                                    trace2->getMinDuration(), trace2->getMaxDuration())
            * (double) weight1 * (double) weight2; // Add 3)
    
    if (ifAccumulate) {
      // Add 1) and 2) to exclusive diff score.
      exclusiveDiff += trace1->getDiffScore().getExclusive() + trace2->getDiffScore().getExclusive();
      
      // The above code has added 1) and 2.3) to inclusive diff score. Add 2.1) and 2.2) to inclusive diff score below.
      // Add 2.1)
      inclusiveDiff += trace1->getDiffScore().getInclusive();
      for (int i = 0; i < trace1->getNumChild(); i++)
        inclusiveDiff -= trace1->getChild(i)->getDiffScore().getInclusive();
      
      // Add 2.2)
      inclusiveDiff += trace2->getDiffScore().getInclusive();
      for (int i = 0; i < trace2->getNumChild(); i++)
        inclusiveDiff -= trace2->getChild(i)->getDiffScore().getInclusive();
    }
    
    /* Inclusive diff score should be no less than "exclusive diff score" plus minimum accumulation.
     * (As we hide noises from sampling, the resulting inclusive diff score could be smaller than 
     * the minimum threshold)
     */
    double minInclusiveDiff = (double)computeRangeDiff(trace1->getMinDuration(), trace1->getMaxDuration(), 
                                                    trace2->getMinDuration(), trace2->getMaxDuration())
            * (double) weight1 * (double) weight2;
    if (ifAccumulate)
      minInclusiveDiff += trace1->getDiffScore().getInclusive() + trace2->getDiffScore().getInclusive();
    
    if (inclusiveDiff < minInclusiveDiff) inclusiveDiff = minInclusiveDiff;
    
    mergedTrace->getDiffScore().setScores(inclusiveDiff, exclusiveDiff);
    
    return mergedTrace;
  }
  
  TCTANode* LocalDifferenceQuantifier::mergeLoopNode(const TCTLoopNode* loop1, long weight1, const TCTLoopNode* loop2, long weight2, 
          bool ifAccumulate, bool isScoreOnly) {
    TCTProfileNode* prof1 = TCTProfileNode::newProfileNode(loop1);
    TCTProfileNode* prof2 = TCTProfileNode::newProfileNode(loop2);
    TCTProfileNode* merged = mergeProfileNode(prof1, weight1, prof2, weight2, ifAccumulate, isScoreOnly);
    delete prof1;
    delete prof2;
    return merged;
  }
  
  LocalDifferenceQuantifier localDQ;
}