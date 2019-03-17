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
 * File:   ClockSynchronizer.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 14, 2019, 11:32 AM
 */
#include <algorithm>
#include <unordered_map>
using std::unordered_map;

#include <vector>
using std::vector;

#include <queue>
using std::priority_queue;

#include <functional> 

#include "TraceAnalysisCommon.hpp"
#include "ClockSynchronizer.hpp"
#include "data/TCT-Node.hpp"

namespace TraceAnalysis {
  bool compareIterationTrace(const TCTIterationTraceNode* iter1, const TCTIterationTraceNode* iter2) {
    return iter1->getIterNum() < iter2->getIterNum();
  }
  
  class TimestampRecord {
  public:
    int numInstances;
    vector<TCTTime> times1;
    vector<TCTTime> times2;
    
    TimestampRecord() : numInstances(0) {}
    
    void insertRecord(const TCTTime& t1, const TCTTime& t2) {
      numInstances++;
      times1.push_back(t1);
      times2.push_back(t2);
    }
  };
  
  class ClockAdjRecord {
  public:
    Time minAdj;
    Time maxAdj;
    
    ClockAdjRecord(Time minAdj, Time maxAdj) : minAdj(minAdj), maxAdj(maxAdj) {};
    
    bool operator<(const ClockAdjRecord& other) const {
      return minAdj < other.minAdj;
    }
  };
  
  class ClockSynchronizerImpl {
    static const int MSG_PRIO = MSG_PRIO_LOW;
  public:
    ClockSynchronizerImpl(const TCTRootNode* repRoot) : repRoot(repRoot) {
      checkGlobalSync(repRoot);
    }
    
    bool checkGlobalSync(const TCTANode* node) {
      if (node->type == TCTANode::NonCFGProf)
        return false;
      else if (node->type == TCTANode::Loop) 
        return checkGlobalSync(((const TCTLoopNode*)node)->getProfileNode());
      else {
        const TCTACFGNode* cfgNode = (const TCTACFGNode*) node;
        if ((cfgNode->getOriginalSemanticLabel() & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) {
          if (cfgNode->getDuration() >= repRoot->getDuration() * 0.1 * MIN_SYNC_COMM_IR) {
            callpathsWithSync[cfgNode->id] = cfgNode;
            print_msg(MSG_PRIO, "Sync call path found: %s", cfgNode->toString(0, 0, 0).c_str());
            return true;
          }
          else
            return false;
        }
        
        if ((cfgNode->getOriginalSemanticLabel() & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION)
          return false;
        
        bool hasSync = false;
        for (int idx = 0; idx < cfgNode->getNumChild(); idx++) {
          double sumWeight = 0;
          for (auto edge : cfgNode->getOutEdges(idx))
            sumWeight += edge->getWeight();
          if (sumWeight * (1-FP_ERROR) > node->getWeight())
            print_msg(MSG_PRIO_MAX, "ERROR: sum of out edges exceeds the weight of root node.\n%s\n", cfgNode->toString(cfgNode->getDepth()+1, 0, 0).c_str());
          else if (sumWeight * (1+FP_ERROR) >= node->getWeight())
            hasSync |= checkGlobalSync(cfgNode->getChild(idx));
        }
        
        if (hasSync)
          callpathsWithSync[cfgNode->id] = cfgNode;
        
        return hasSync;
      }
    }
    
    Time getClockDifference(const TCTRootNode* root1, const TCTRootNode* root2) {
      unordered_map<TCTID, TimestampRecord> records;
      collectTimestampRecords(records, root1, root2);
      
      if (records.empty()) {
        print_msg(MSG_PRIO_MAX, "WARNING: no sync call paths for clock synchronization.\n");
        return 0;
      }
      
      // For all records, calculate the clock adjustment needed to match the endTime.
      vector<ClockAdjRecord> clockAdjRecords;
      for (auto rit = records.begin(); rit != records.end(); rit++) {
        TimestampRecord& record = rit->second;
        print_msg(MSG_PRIO, "\nnode %s\n", rit->first.toString().c_str());
        for (int idx = 0; idx < record.numInstances; idx++) {
          print_msg(MSG_PRIO, "  time1 = %s, time2 = %s.\n", record.times1[idx].toString().c_str(), record.times2[idx].toString().c_str());
          Time adj1 = record.times1[idx].getEndTimeInclusive() - record.times2[idx].getEndTimeExclusive();
          Time adj2 = record.times1[idx].getEndTimeExclusive() - record.times2[idx].getEndTimeInclusive();
          clockAdjRecords.push_back(ClockAdjRecord(std::min(adj1, adj2), std::max(adj1, adj2)));
          print_msg(MSG_PRIO, "  adjMin = %ld, adjMax = %ld.\n", clockAdjRecords.back().minAdj, clockAdjRecords.back().maxAdj);
        }
      }
      records.clear();
      std::sort(clockAdjRecords.begin(), clockAdjRecords.end());
      
      if (clockAdjRecords.back().minAdj - clockAdjRecords.front().maxAdj > repRoot->getSamplingPeriod() * MAX_CLOCK_SYNC_NOISE)
        return CLOCK_SYNC_FAILED;
      
      // Find an adjustment segment [totalMinAdj, totalMaxAdj] that minimize the overall clock misalignment.
      Time totalMisalignment = 0;
      Time currentAdj = clockAdjRecords[0].minAdj;
      for (uint idx = 1; idx < clockAdjRecords.size(); idx++)
        totalMisalignment += clockAdjRecords[idx].minAdj - currentAdj;
      
      Time totalMinAdj = currentAdj;
      Time totalMaxAdj = currentAdj;
      
      priority_queue<Time, vector<Time>, std::greater<Time>> activeMaxAdjs;
      activeMaxAdjs.push(clockAdjRecords[0].maxAdj);
      
      uint idx = 1;
      while (true) {
        int numLeftRecords = idx - activeMaxAdjs.size(); // number of records whose maxAdj is less than currentAdj;
        int numRightRecords = clockAdjRecords.size() - idx; // number of records whose minAdj is greater than currentAdj;
        if (numLeftRecords > numRightRecords) break; // When there is more records on the left, further scan would only lead to bigger totalMisalignment.
        
        Time prevAdj = currentAdj;
        
        // Move currectAdj
        if (activeMaxAdjs.empty() || (idx < clockAdjRecords.size() && clockAdjRecords[idx].minAdj <= activeMaxAdjs.top())) {
          currentAdj = clockAdjRecords[idx].minAdj;
          activeMaxAdjs.push(clockAdjRecords[idx].maxAdj);
          idx++;
        }
        else {
          currentAdj = activeMaxAdjs.top();
          activeMaxAdjs.pop();
        }
        
        // Update values
        if (numLeftRecords == numRightRecords) {
          totalMaxAdj = currentAdj;
        }
        else { // numLeftRecords < numRightRecords
          totalMisalignment -= (numRightRecords - numLeftRecords) * (currentAdj - prevAdj);
          totalMinAdj = currentAdj;
          totalMaxAdj = currentAdj;
        }
      }
      
      print_msg(MSG_PRIO, "totalAdjMin = %ld, totalAdjMax = %ld, misAlignment = %ld.\n", totalMinAdj, totalMaxAdj, totalMisalignment);
      // return the mid point of [totalMinAdj, totalMaxAdj]
      return (totalMinAdj + totalMaxAdj) / 2;
    }
    
  private:
    const TCTRootNode* repRoot;
    unordered_map<TCTID, const TCTANode*> callpathsWithSync;
    
    void collectTimestampRecords(unordered_map<TCTID, TimestampRecord>& records, const TCTANode* node1, const TCTANode* node2) {
      // Check if this call path contains synchronization.
      if (callpathsWithSync.find(node1->id) == callpathsWithSync.end())
        return;
      
      if (node1->type != node2->type) {
        print_msg(MSG_PRIO_MAX, "ERROR: collecting timestamps on nodes of different type.\n");
        return;
      }
      
      if ((node1->getOriginalSemanticLabel() & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) {
        records[node1->id].insertRecord(node1->getTime(), node2->getTime());
        return;
      }

      if (node1->type == TCTANode::Loop) {
        const TCTLoopNode* loop1 = (const TCTLoopNode*) node1;
        const TCTLoopNode* loop2 = (const TCTLoopNode*) node2;
        
        if ((!loop1->accept()) || (!loop2->accept()))
          return;
        
        vector<const TCTIterationTraceNode*> iterSamples1(loop1->getNumSampledIterations());
        for (int idx = 0; idx < loop1->getNumSampledIterations(); idx++)
          iterSamples1[idx] = loop1->getSampledIteration(idx);
        std::sort(iterSamples1.begin(), iterSamples1.end(), compareIterationTrace);
        
        vector<const TCTIterationTraceNode*> iterSamples2(loop2->getNumSampledIterations());
        for (int idx = 0; idx < loop2->getNumSampledIterations(); idx++)
          iterSamples2[idx] = loop2->getSampledIteration(idx);
        std::sort(iterSamples2.begin(), iterSamples2.end(), compareIterationTrace);
        
        uint k1 = 0, k2 = 0;
        while (k1 < iterSamples1.size() && k2 < iterSamples2.size()) {
          if (iterSamples1[k1]->getIterNum() == iterSamples2[k2]->getIterNum()) {
            collectTimestampRecords(records, iterSamples1[k1], iterSamples2[k2]);
            k1++;
            k2++;
          }
          else if (iterSamples1[k1]->getIterNum() < iterSamples2[k2]->getIterNum())
            k1++;
          else 
            k2++;
        }
      }
      else if (node1->type != TCTANode::NonCFGProf) {
        // is a CFG Node
        const TCTACFGNode* mergedNode = (const TCTACFGNode*) callpathsWithSync[node1->id];
        const TCTACFGNode* cfgNode1 = (const TCTACFGNode*) node1;
        const TCTACFGNode* cfgNode2 = (const TCTACFGNode*) node2;
        
        if (cfgNode1->isCFGProfile() || cfgNode2->isCFGProfile()) {
          print_msg(MSG_PRIO_MAX, "ERROR: collecting timestamps on CFG profile nodes.\n");
          return;
        }
        
        int k1 = 0;
        int k2 = 0;
        for (int idx = 0; idx < mergedNode->getNumChild(); idx++) {
          const TCTANode* mergedChild = mergedNode->getChild(idx);
          const TCTANode* child1 = NULL;
          const TCTANode* child2 = NULL;
          
          if (k1 < cfgNode1->getNumChild() && cfgNode1->getChild(k1)->id == mergedChild->id)
            child1 = cfgNode1->getChild(k1);
          if (k2 < cfgNode2->getNumChild() && cfgNode2->getChild(k2)->id == mergedChild->id)
            child2 = cfgNode2->getChild(k2);
          
          if (child1 == NULL && child2 == NULL) continue;
          else if (child1 != NULL && child2 != NULL) {
            collectTimestampRecords(records, child1, child2);
            k1++;
            k2++;
          }
          else if (child1 != NULL) {
            TCTANode* child2 = child1->voidDuplicate();
            Time endInclusive, endExclusive;
            if (k2 == 0)
              endInclusive = cfgNode2->getTime().getStartTimeExclusive();
            else
              endInclusive = cfgNode2->getChild(k2-1)->getTime().getEndTimeInclusive();
            
            if (k2 == cfgNode2->getNumChild())
              endExclusive = cfgNode2->getTime().getEndTimeExclusive();
            else
              endExclusive = cfgNode2->getChild(k2)->getTime().getStartTimeInclusive();
            
            child2->getTime().setStartTime(endInclusive, endExclusive);
            child2->getTime().setEndTime(endInclusive, endExclusive);
            
            collectTimestampRecords(records, child1, child2);
            
            delete child2;
            k1++;
          }
          else {
            TCTANode* child1 = child2->voidDuplicate();
            Time endInclusive, endExclusive;
            if (k1 == 0)
              endInclusive = cfgNode1->getTime().getStartTimeExclusive();
            else
              endInclusive = cfgNode1->getChild(k1-1)->getTime().getEndTimeInclusive();
            
            if (k1 == cfgNode1->getNumChild())
              endExclusive = cfgNode1->getTime().getEndTimeExclusive();
            else
              endExclusive = cfgNode1->getChild(k1)->getTime().getStartTimeInclusive();
            
            child1->getTime().setStartTime(endInclusive, endExclusive);
            child1->getTime().setEndTime(endInclusive, endExclusive);
            
            collectTimestampRecords(records, child1, child2);
            
            delete child1;
            k2++;
          }
        }
      }
    }
  };
  
  ClockSynchronizer::ClockSynchronizer(const TCTRootNode* repRoot) {
    ptr = new ClockSynchronizerImpl(repRoot);
  }
  
  ClockSynchronizer::~ClockSynchronizer() {
    delete (ClockSynchronizerImpl*)ptr;
  }
  
  Time ClockSynchronizer::getClockDifference(const TCTRootNode* root1, const TCTRootNode* root2) {
    return ((ClockSynchronizerImpl*)ptr)->getClockDifference(root1, root2);
  }
}
