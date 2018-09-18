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
 * File:   PerformanceDiagnoser.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on August 20, 2018, 1:27 PM
 */

#include "PerformanceDiagnoser.hpp"

#include "TraceAnalysisCommon.hpp"
#include "data/TCT-Semantic-Label.hpp"

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include <set>
using std::set;

#include <algorithm>
using std::max;

namespace TraceAnalysis {
  struct CallpathMetrics {
    uint originalLabel;
    uint derivedLabel;
    double subtreeImbIR; // sum of imbalance in inclusive time of the entire sub-tree
    double nodeImbIR; // imbalance in inclusive time 
    double nodeExcImbIR; // imbalance in exclusive time
    unordered_map<uint, double> subTreeCommIR;
    unordered_map<uint, double> nodeCommIR;
    
    double getSubtreeIR() {
      double ir = subtreeImbIR;
      if (subTreeCommIR.find(SEMANTIC_LABEL_COMMUNICATION) != subTreeCommIR.end())
        ir += subTreeCommIR[SEMANTIC_LABEL_COMMUNICATION];
      return ir;
    }
    
    double getNodeIR() {
      double ir = nodeImbIR;
      if (nodeCommIR.find(SEMANTIC_LABEL_COMMUNICATION) != nodeCommIR.end())
        ir += nodeCommIR[SEMANTIC_LABEL_COMMUNICATION];
      return ir;
    }
    
    void addChildMetrics(const CallpathMetrics* childMetrics) {
      subtreeImbIR += childMetrics->subtreeImbIR;
      for (auto it = childMetrics->subTreeCommIR.begin(); it != childMetrics->subTreeCommIR.end(); it++)
        if (subTreeCommIR.find(it->first) != subTreeCommIR.end())
          subTreeCommIR[it->first] += it->second;
        else
          subTreeCommIR[it->first] = it->second;
    }
  };
  
  struct Callpath {
    vector<const TCTANode*> callpath;
    bool isInclusive;

    Callpath(const vector<const TCTANode*>& cp, bool inclusive) {
      callpath = cp;
      isInclusive = inclusive;
    }
    
    Callpath(const Callpath& orig) {
      callpath = orig.callpath;
      isInclusive = orig.isInclusive;
    }
  };
  
  struct LoadImbalance {
    vector<int> symptomIdxes;
    vector<int> causeIdxes;
    int lcaDepth;
    bool resolved;
    double minIR;
    double maxIR;
  };
  
  struct CommunicationInstance {
    int callpathIdx;
    
    uint semanticLabel;
    double IR;

    long numInstances;
    long avgNumSamples;
  };
  
  struct ExecutionSegment {
    Callpath* priorSyncCallpath;
    vector<Callpath*> callpaths;
    
    double totalImbIR;
    double totalCommIR;
    
    double syncImbIR;
    double syncCommIR;
    
    vector<LoadImbalance*> imbs;
    vector<CommunicationInstance*> comms;
    
    ExecutionSegment() {
      priorSyncCallpath = NULL;
      totalImbIR = 0;
      totalCommIR = 0;
      syncImbIR = 0;
      syncCommIR = 0;
    }
    
    ~ExecutionSegment() {
      if (priorSyncCallpath != NULL) delete priorSyncCallpath;
      for (auto it = callpaths.begin(); it != callpaths.end(); it++)
        delete (*it);
      for (auto it = imbs.begin(); it != imbs.end(); it++)
        delete (*it);
      for (auto it = comms.begin(); it != comms.end(); it++)
        delete (*it);
    }
  };
  
  class PerformanceDiagnoserImpl {
  public:
    PerformanceDiagnoserImpl(TCTRootNode* mergedRoot, string dbDir) : root(mergedRoot), dbDir(dbDir) {
      numProc = root->getWeight();
      totalDuration = root->getDuration();
      currentSegment = new ExecutionSegment();
      
      assignSemanticLabel(root, SEMANTIC_LABEL_COMPUTATION);
      computeMetrics(root);
      
      vector<const TCTANode*> callpath;
      callpath.push_back(root);
      generateInefficientSegments(callpath, false);
      if (currentSegment->totalCommIR + currentSegment->totalImbIR >= MIN_EXECUTION_SEGMENT_IR)
        allSegments.push_back(currentSegment);
      else
        delete currentSegment;
      currentSegment = NULL;
      
      generateDiagnosis();
      
      printExecutionSegments();
    }
    virtual ~PerformanceDiagnoserImpl() {
      for (auto it = metricsMap.begin(); it != metricsMap.end(); it++)
        delete it->second;
      for (auto it = allSegments.begin(); it != allSegments.end(); it++)
        delete *it;
    }
    
  private:
    // Determine the semantic label of all nodes.
    // CallpathMetrics::subTreeCommIR is used to accumulate the time spent in communication.
    void assignSemanticLabel(const TCTANode* node, uint parentSemanticLabel) {
      CallpathMetrics* metrics = new CallpathMetrics();
      metricsMap[node] = metrics;
      metrics->originalLabel = getFuncSemanticInfo(node->getName()).semantic_label | parentSemanticLabel;
      
      if (node->type == TCTANode::Prof) {
        const TCTProfileNode* prof = (const TCTProfileNode*) node;
        for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++) {
          assignSemanticLabel(it->second, metrics->originalLabel);
          metrics->addChildMetrics(metricsMap[it->second]);
        }
      }
      else if (node->type == TCTANode::Loop) {
        const TCTANode* avgRep = ((TCTLoopNode*)node)->getClusterNode()->getAvgRep();
        assignSemanticLabel(avgRep, metrics->originalLabel);
        metrics->addChildMetrics(metricsMap[avgRep]);
        
        const TCTANode* rejected = ((TCTLoopNode*)node)->getRejectedIterations();
        if (rejected != NULL) {
          assignSemanticLabel(rejected, metrics->originalLabel);
          metrics->addChildMetrics(metricsMap[rejected]);
        }
      }
      else {
        const TCTATraceNode* trace = (const TCTATraceNode*) node;
        for (int i = 0; i < trace->getNumChild(); i++) {
          assignSemanticLabel(trace->getChild(i), metrics->originalLabel);
          metrics->addChildMetrics(metricsMap[trace->getChild(i)]);
        }
      }

      double totalDuration = (double)node->getDuration() * (double)node->getWeight();
      
      metrics->derivedLabel = metrics->originalLabel;
      // Check if communication is the major source of the time spent in this node.
      for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
        // metrics->derivedLabel is computation, or SEMANTIC_LABEL_ARRAY[i].label is a child of metrics->semanticLabel
        if (((SEMANTIC_LABEL_ARRAY[i].label & metrics->derivedLabel) == metrics->derivedLabel) 
            && (SEMANTIC_LABEL_ARRAY[i].label != metrics->derivedLabel)
            && (metrics->subTreeCommIR.find(SEMANTIC_LABEL_ARRAY[i].label) != metrics->subTreeCommIR.end())
            // SEMANTIC_LABEL_ARRAY[i].label is the major source
            && (metrics->subTreeCommIR[SEMANTIC_LABEL_ARRAY[i].label] >= totalDuration * HOT_PATH_RATIO) ) {
          // If so, change the semantic label of this node.
          metrics->derivedLabel = SEMANTIC_LABEL_ARRAY[i].label;
        }
      
      // If is communication, enumerate all semantic labels
      if ((metrics->originalLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
          // if the label is a communication label
          if ((SEMANTIC_LABEL_ARRAY[i].label & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION
                  // and the node's semantic label is in accordance with the enumerated label
                  && (metrics->originalLabel & SEMANTIC_LABEL_ARRAY[i].label) == SEMANTIC_LABEL_ARRAY[i].label)
            metrics->subTreeCommIR[SEMANTIC_LABEL_ARRAY[i].label] = totalDuration;
      }
      
      return;
    }
    
    void computeMetrics(const TCTANode* node) {
      CallpathMetrics* metrics = metricsMap[node];
      metrics->subTreeCommIR.clear();

      double imb, comm, excImb;
      double samplePeriod = node->getDuration() / node->getNumSamples();
      if ((metrics->derivedLabel & SEMANTIC_LABEL_DATA_TRANSFER) == SEMANTIC_LABEL_DATA_TRANSFER) {
        // imb(W) = max(W) - avg(W);
        imb = node->getPerfLossMetric().getMaxDurationInc() - node->getPerfLossMetric().getAvgDurationInc(node->getWeight()) - samplePeriod;
        excImb = 0; //TODO
        // comm(W) = avg(W);
        comm = node->getPerfLossMetric().getAvgDurationInc(node->getWeight());
      }
      else if ((metrics->derivedLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        // imb(S) = avg(S) - min(S);
        imb = node->getPerfLossMetric().getAvgDurationInc(node->getWeight()) - node->getPerfLossMetric().getMinDurationInc() - samplePeriod;
        excImb = 0; //TODO
        // comm(S) = min(S);
        comm = node->getPerfLossMetric().getMinDurationInc();
      }
      else {
        // imb(C) = max(C) - avg(C);
        imb = node->getPerfLossMetric().getMaxDurationInc() - node->getPerfLossMetric().getAvgDurationInc(node->getWeight()) - samplePeriod;
        excImb = node->getPerfLossMetric().getMaxDurationExc() - node->getPerfLossMetric().getAvgDurationExc(node->getWeight()) - samplePeriod;
        comm = 0;
      }
      
      imb = max(imb, 0.0);
      excImb = max(excImb, 0.0);
      comm = max(comm, 0.0);
      
      // exclusive imbalance in loops is not considered.
      if (node->isLoop()) excImb = 0;
      
      // Set node imbalance improvement ratio in metrics
      metrics->nodeImbIR = imb * node->getWeight() / numProc / totalDuration;
      metrics->nodeExcImbIR = excImb * node->getWeight() / numProc / totalDuration;
      
      // Set node communication improvement ratio in metrics
      double commIR = comm * node->getWeight() / numProc / totalDuration;
      // If is communication, enumerate all semantic labels
      if ((metrics->originalLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
          // if the label is a communication label
          if ((SEMANTIC_LABEL_ARRAY[i].label & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION
                  // and the node's semantic label is in accordance with the enumerated label
                  && (metrics->originalLabel & SEMANTIC_LABEL_ARRAY[i].label) == SEMANTIC_LABEL_ARRAY[i].label)
            metrics->nodeCommIR[SEMANTIC_LABEL_ARRAY[i].label] = commIR;
      }
      
      metrics->subtreeImbIR = metrics->nodeExcImbIR;
      if (node->type == TCTANode::Prof) {
        const TCTProfileNode* prof = (const TCTProfileNode*) node;
        for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++) {
          computeMetrics(it->second);
          metrics->addChildMetrics(metricsMap[it->second]);
        }
      }
      else if (node->type == TCTANode::Loop) {
        const TCTANode* avgRep = ((TCTLoopNode*)node)->getClusterNode()->getAvgRep();
        computeMetrics(avgRep);
        //TODO metrics from rejected iterations are ignored.
        metrics->addChildMetrics(metricsMap[avgRep]);
      }
      else {
        const TCTATraceNode* trace = (const TCTATraceNode*) node;
        for (int i = 0; i < trace->getNumChild(); i++) {
          computeMetrics(trace->getChild(i));
          metrics->addChildMetrics(metricsMap[trace->getChild(i)]);
        }
      }
      
      // Adjust subtree IRs when they are less than node IRs.
      metrics->subtreeImbIR = max(metrics->nodeImbIR, metrics->subtreeImbIR);
      if ((metrics->originalLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        for (auto it = metrics->nodeCommIR.begin(); it != metrics->nodeCommIR.end(); it++)
          if (metrics->subTreeCommIR.find(it->first) != metrics->subTreeCommIR.end())
            metrics->subTreeCommIR[it->first] = max(it->second, metrics->subTreeCommIR[it->first]);
          else
            metrics->subTreeCommIR[it->first] = it->second;
      }
    }
    
    void mergeExecutionSegmentsAfter(uint idx) {
      if (allSegments.size() > idx) {
        // Merge all segments after all_segments[idx].
        for (uint k = idx + 1; idx < allSegments.size(); k++) {
          allSegments[idx]->totalCommIR += allSegments[k]->totalCommIR;
          allSegments[idx]->totalImbIR  += allSegments[k]->totalImbIR;
          allSegments[idx]->syncCommIR  += allSegments[k]->syncCommIR;
          allSegments[idx]->syncImbIR   += allSegments[k]->syncImbIR;
          allSegments[idx]->callpaths.insert(allSegments[idx]->callpaths.end(), 
                  allSegments[k]->callpaths.begin(), allSegments[k]->callpaths.end());
          
          allSegments[k]->callpaths.clear();
          delete allSegments[k];
        }
        allSegments.resize(idx+1);
        
        // Also merge current_segment to all_segments[idx]
        allSegments[idx]->totalCommIR += currentSegment->totalCommIR;
        allSegments[idx]->totalImbIR  += currentSegment->totalImbIR;
        allSegments[idx]->callpaths.insert(allSegments[idx]->callpaths.end(), 
                currentSegment->callpaths.begin(), currentSegment->callpaths.end());
        
        currentSegment->totalCommIR = 0;
        currentSegment->totalImbIR = 0;
        currentSegment->callpaths.clear();
      }
    }
    
    void generateInefficientSegments(vector<const TCTANode*>& callpath, bool isParentAdded) {
      const TCTANode* node = callpath.back();
      CallpathMetrics* metrics = metricsMap[node];
      
      // If the current node is synchronization.
      if ((metrics->originalLabel & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) {
        // If IR passes the threshold, terminate the current execution segment.
        if (metrics->nodeImbIR >= MIN_SYNC_IMB_IR
                || metrics->nodeCommIR[SEMANTIC_LABEL_SYNC] >= MIN_SYNC_COMM_IR) {
          // Check if improvement ratio of this execution segment exceeds the threshold.
          if (currentSegment->totalCommIR + currentSegment->totalImbIR + metrics->nodeCommIR[SEMANTIC_LABEL_SYNC]
                  >= MIN_EXECUTION_SEGMENT_IR) {
            currentSegment->callpaths.push_back(new Callpath(callpath, true));
            currentSegment->syncImbIR += metrics->nodeImbIR;
            currentSegment->syncCommIR += metrics->nodeCommIR[SEMANTIC_LABEL_SYNC];
            allSegments.push_back(currentSegment);
          }
          else
            delete currentSegment;
          
          currentSegment = new ExecutionSegment();
          currentSegment->priorSyncCallpath = new Callpath(callpath, true);
        }
        
        return;
      }
      
      // If the node has a sync successor in its subtree.
      bool hasSync = (metrics->subTreeCommIR.find(SEMANTIC_LABEL_SYNC) != metrics->subTreeCommIR.end());
      // If improvement ratio of this node is not significant and the node has no sync successor, return.
      if (metrics->getSubtreeIR() < MIN_SUBTREE_IR && (!hasSync)) return;

      // If the node is communication, add to significant callpaths and return. 
      // (Details in its subtree will be investigated later)
      if ((metrics->originalLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        if (!isParentAdded) {
          currentSegment->totalImbIR += metrics->subtreeImbIR;
          currentSegment->totalCommIR += (metrics->getSubtreeIR() - metrics->subtreeImbIR);
        }
        currentSegment->callpaths.push_back(new Callpath(callpath, true));
        return;
      }
      
      // If the node's imbalance in the subtree can be mostly explained by itself
      bool explainImb = false;
      // A set of semantic labels where time spent in the subtree can be mostly explained by the node itself.
      set<uint> explainComm;
      
      // loops are excluded as hpctraceviewer won't support loops.
      if ((!hasSync) && (!node->isLoop())) {
        explainImb = (metrics->nodeImbIR >= metrics->subtreeImbIR * HOT_PATH_RATIO && metrics->subtreeImbIR >= MIN_SUBTREE_IR);
        if (metrics->nodeExcImbIR >= metrics->subtreeImbIR * HOT_PATH_RATIO)
          explainImb = false;
        
        for (auto it = metrics->nodeCommIR.begin(); it != metrics->nodeCommIR.end(); it++)
          if (it->second >= metrics->subTreeCommIR[it->first] * HOT_PATH_RATIO && metrics->subTreeCommIR[it->first] >= MIN_SUBTREE_IR)
            explainComm.insert(it->first); 
        
        if (node->type == TCTANode::Loop) {
          explainImb = false;
          explainComm.clear();
        }
        else if (node->type == TCTANode::Prof) {
          const TCTProfileNode* prof = (const TCTProfileNode*) node;
          for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++) {
            const TCTANode* child = it->second;
            CallpathMetrics* childMetrics = metricsMap[child];
            
            // Check if the child's imbalance dominates.
            if (childMetrics->subtreeImbIR >= metrics->subtreeImbIR * HOT_PATH_RATIO)
              explainImb = false;
            // Check if some communication in the children dominates.
            for (auto it = childMetrics->subTreeCommIR.begin(); it != childMetrics->subTreeCommIR.end(); it++)
              if (it->second >= metrics->subTreeCommIR[it->first] * HOT_PATH_RATIO)
                explainComm.erase(it->first);
          }
        }
        else {
          const TCTATraceNode* trace = (const TCTATraceNode*) node;
          for (int i = 0; i < trace->getNumChild(); i++) {
            const TCTANode* child = trace->getChild(i);
            CallpathMetrics* childMetrics = metricsMap[child];
            
            // Check if the child's imbalance dominates.
            if (childMetrics->subtreeImbIR >= metrics->subtreeImbIR * HOT_PATH_RATIO)
              explainImb = false;
            // Check if some communication in the children dominates.
            for (auto it = childMetrics->subTreeCommIR.begin(); it != childMetrics->subTreeCommIR.end(); it++)
              if (it->second >= metrics->subTreeCommIR[it->first] * HOT_PATH_RATIO)
                explainComm.erase(it->first);
          }
        }
      }
      
      if (explainImb || explainComm.size() > 0) {
        if (!isParentAdded) {
          currentSegment->totalImbIR += metrics->subtreeImbIR;
          if (metrics->subTreeCommIR.find(SEMANTIC_LABEL_COMMUNICATION) != metrics->subTreeCommIR.end())
            currentSegment->totalCommIR += metrics->subTreeCommIR[SEMANTIC_LABEL_COMMUNICATION];
        }
        currentSegment->callpaths.push_back(new Callpath(callpath, true));
        isParentAdded = true;
      }
      
      // When imbalance in exclusive time is an important part of the imbalance in the subtree
      if ((!explainImb) && (!node->isLoop()) && (metrics->nodeExcImbIR >= metrics->subtreeImbIR * SIDE_PATH_RATIO) 
          && (metrics->nodeExcImbIR >= MIN_SUBTREE_IR)) {
        if (!isParentAdded) currentSegment->totalImbIR += metrics->nodeExcImbIR;
        currentSegment->callpaths.push_back(new Callpath(callpath, false));
      }
      
      if (node->type == TCTANode::Loop) {
        const TCTANode* avgRep = ((TCTLoopNode*)node)->getClusterNode()->getAvgRep();
        callpath.push_back(avgRep);
        generateInefficientSegments(callpath, isParentAdded);
        callpath.pop_back();
      }
      else if (node->type == TCTANode::Prof) {
        const TCTProfileNode* prof = (const TCTProfileNode*) node;

        // Copy all children into an array
        uint numChildren = prof->getChildMap().size();
        const TCTProfileNode** children = new const TCTProfileNode*[numChildren];
        uint k = 0;
        for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++)
          children[k++] = it->second;

        // Arrange children according to their order in CFG
        if (prof->cfgGraph != NULL) {
          for (uint i = 0; i < numChildren; i++)
            for (uint j = i+1; j < numChildren; j++) {
              // If one of the RA is not a located in the CFG, the corresponding node is considered as "predecessor".
              int compare = 0;
              bool hasChildi = prof->cfgGraph->hasChild(children[i]->ra);
              bool hasChildj = prof->cfgGraph->hasChild(children[j]->ra);
              if ((!hasChildi) && hasChildj) compare = -1;
              if (hasChildi && (!hasChildj)) compare = 1;

              if (compare == 0)
                compare = prof->cfgGraph->compareChild(children[i]->ra, children[j]->ra);

              // Use their id as tie breaker
              if ( (compare == 1) || (compare == 0 && children[j]->id < children[i]->id) ) {
                const TCTProfileNode* swap = children[i];
                children[i] = children[j];
                children[j] = swap;
              }
            }
        }

        for (k = 0; k < numChildren; k++) {
          const TCTANode* child = children[k];
          CallpathMetrics* childMetrics = metricsMap[child];
          
          bool inspect = false;
          // Inspect the child if it contains sync.
          if (childMetrics->subTreeCommIR.find(SEMANTIC_LABEL_SYNC) != childMetrics->subTreeCommIR.end())
            inspect = true;
          // Inspect the child if its imbalance in the subtree needs to be inspected.
          if (childMetrics->subtreeImbIR >= metrics->subtreeImbIR * HOT_PATH_RATIO)
            inspect = true;
          if ((!explainImb) && childMetrics->subtreeImbIR >= metrics->subtreeImbIR * SIDE_PATH_RATIO)
            inspect = true;
          // Inspect the child if some communication in the subtree needs to be inspected.
          for (auto it = childMetrics->subTreeCommIR.begin(); it != childMetrics->subTreeCommIR.end(); it++) {
            if (inspect) break;
            if (it->second >= metrics->subTreeCommIR[it->first] * HOT_PATH_RATIO)
              inspect = true;
            if ((explainComm.find(it->first) == explainComm.end()) &&
                    (it->second >= metrics->subTreeCommIR[it->first] * SIDE_PATH_RATIO))
              inspect = true;
          }
          
          if (inspect) {
            callpath.push_back(child);
            generateInefficientSegments(callpath, isParentAdded);
            callpath.pop_back();
          }
        }
        
        delete children;
      }
      else {
        const TCTATraceNode* trace = (const TCTATraceNode*) node;

        for (int k = 0; k < trace->getNumChild(); k++) {
          const TCTANode* child = trace->getChild(k);
          CallpathMetrics* childMetrics = metricsMap[child];
          
          bool inspect = false;
          // Inspect the child if it contains sync.
          if (childMetrics->subTreeCommIR.find(SEMANTIC_LABEL_SYNC) != childMetrics->subTreeCommIR.end())
            inspect = true;
          // Inspect the child if its imbalance in the subtree needs to be inspected.
          if (childMetrics->subtreeImbIR >= metrics->subtreeImbIR * HOT_PATH_RATIO)
            inspect = true;
          if ((!explainImb) && childMetrics->subtreeImbIR >= metrics->subtreeImbIR * SIDE_PATH_RATIO)
            inspect = true;
          // Inspect the child if some communication in the subtree needs to be inspected.
          for (auto it = childMetrics->subTreeCommIR.begin(); it != childMetrics->subTreeCommIR.end(); it++) {
            if (inspect) break;
            if (it->second >= metrics->subTreeCommIR[it->first] * HOT_PATH_RATIO)
              inspect = true;
            if ((explainComm.find(it->first) == explainComm.end()) &&
                    (it->second >= metrics->subTreeCommIR[it->first] * SIDE_PATH_RATIO))
              inspect = true;
          }
          
          if (inspect) {
            callpath.push_back(child);
            generateInefficientSegments(callpath, isParentAdded);
            callpath.pop_back();
          }
        }
      }
      
    }
    
    void generateDiagnosis() {
      for (uint k = 0; k < allSegments.size(); k++) {
        diagnoseLoadImbalance(allSegments[k]);
        diagnoseCommunication(allSegments[k]);
      }
    }
    
    void inspectCallpath(Callpath* callpath, vector<Callpath*>& subCallpaths) {
      const TCTANode* node = callpath->callpath.back();
      CallpathMetrics* metrics = metricsMap[node];
      
      if (metrics->getSubtreeIR() < MIN_SUBTREE_IR) return;
      
      if ((metrics->originalLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        if (node->type == TCTANode::Loop) {
          const TCTANode* avgRep = ((TCTLoopNode*)node)->getClusterNode()->getAvgRep();
          callpath->callpath.push_back(avgRep);
          inspectCallpath(callpath, subCallpaths);
          callpath->callpath.pop_back();
        }
        else if (node->type == TCTANode::Prof) {
          const TCTProfileNode* prof = (const TCTProfileNode*) node;

          // Copy all children into an array
          uint numChildren = prof->getChildMap().size();
          const TCTProfileNode** children = new const TCTProfileNode*[numChildren];
          uint k = 0;
          for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++)
            children[k++] = it->second;
        }
        else {
          const TCTATraceNode* trace = (const TCTATraceNode*) node;
          for (int i = 0; i < trace->getNumChild(); i++) {
            //callpath.push_back(trace->getChild(i));
            //generateDiagnosisReport(callpath); 
            //callpath.pop_back();
          }
        }
      }
    }
    
    int computeLCADepth(Callpath* callpath1, Callpath* callpath2) {
      int depth = 0;
      while (depth < (int)callpath1->callpath.size() && depth < (int)callpath2->callpath.size() &&
          callpath1->callpath[depth] == callpath2->callpath[depth])
        depth++;
      return depth - 1;
    }
    
    void diagnoseLoadImbalance(ExecutionSegment* segment) {
      const int END = -3;
      const int RESOLVED = -2;
      const int UNSOLVED = -1;
      
      // Extract the imbalance of each call path.
      double* imb = new double[segment->callpaths.size()];
      bool* isComm = new bool[segment->callpaths.size()];
      for (uint k = 0; k < segment->callpaths.size(); k++) {
        const TCTANode* node = segment->callpaths[k]->callpath.back();
        CallpathMetrics* metrics = metricsMap[node];
        if (segment->callpaths[k]->isInclusive)
          imb[k] = metrics->subtreeImbIR;
        else
          imb[k] = metrics->nodeExcImbIR;
        isComm[k] = ((metrics->derivedLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION);
      }
      
      // Compute LCA depth for neighboring call paths.
      int* lcaDepth = new int[segment->callpaths.size()+1];
      for (uint k = 1; k < segment->callpaths.size(); k++)
        lcaDepth[k] = computeLCADepth(segment->callpaths[k-1], segment->callpaths[k]);
      lcaDepth[0] = END;
      lcaDepth[segment->callpaths.size()] = END;
      
      while (true) {
        // find idx such that lcaDepth[idx] is largest -- corresponding two call paths are "closest".
        int idx = 0;
        for (uint k = 1; k < segment->callpaths.size(); k++)
          if (lcaDepth[k] > lcaDepth[idx])
            idx = k;
        
        if (lcaDepth[idx] < 0) break;
        
        int startIdx = idx-1;
        int endIdx = idx;
        // Locate all call paths who have the same LCA.
        while (lcaDepth[startIdx] == lcaDepth[idx] || lcaDepth[startIdx] == UNSOLVED || lcaDepth[startIdx] == RESOLVED)
          startIdx--;
        while (lcaDepth[endIdx+1] == lcaDepth[idx] || lcaDepth[endIdx+1] == UNSOLVED || lcaDepth[endIdx+1] == RESOLVED)
          endIdx++;
        
        // Compute totalImb for unsolved call paths.
        double commImb = 0;
        double compImb = 0;
        vector<int> commIdxes;
        vector<int> compIdxes;
        for (int k = startIdx; k <= endIdx; k++)
          if (lcaDepth[k] != RESOLVED && lcaDepth[k+1] != RESOLVED && imb[k] >= MIN_SUBTREE_IR) {
            if (isComm[k]) {
              commImb += imb[k];
              commIdxes.push_back(k);
            }
            else {
              compImb += imb[k];
              compIdxes.push_back(k);
            }
          }
        
        // When all significant call paths are resolved.
        if (commIdxes.size() + compIdxes.size() == 0) {
          for (int k = startIdx+1; k <= endIdx; k++)
            lcaDepth[k] = RESOLVED;
          continue;
        }
        
        // Only one call path hasn't been resolved
        if (commIdxes.size() + compIdxes.size() == 1) {
          LoadImbalance* imbalance = new LoadImbalance();
          imbalance->symptomIdxes = commIdxes;
          imbalance->symptomIdxes.insert(imbalance->symptomIdxes.end(), compIdxes.begin(), compIdxes.end());
          imbalance->lcaDepth = lcaDepth[idx];
          imbalance->resolved = false;
          imbalance->minIR = commImb + compImb;
          imbalance->maxIR = imbalance->minIR;
          segment->imbs.push_back(imbalance);
          
          for (int k = startIdx+1; k <= endIdx; k++)
            lcaDepth[k] = RESOLVED;
          continue;
        }
        
        // Multiple call path hasn't been resolved
        const TCTANode* lcaNode = segment->callpaths[idx]->callpath[lcaDepth[idx]];
        CallpathMetrics* lcaMetrics = metricsMap[lcaNode];
        // Imbalance of all unsolved call paths from startIdx to endIdx is resolved at depth lcaDepth[idx].
        if (lcaMetrics->nodeImbIR <= (commImb + compImb) * IMB_RESOLVE_RATIO) {
          LoadImbalance* imbalance = new LoadImbalance();
          imbalance->lcaDepth = lcaDepth[idx];
          imbalance->resolved = true;
          
          // See if imbalance from computation is offset by imbalance in communication
          double diff = compImb - commImb;
          if (diff < 0) diff = -diff;
          imbalance->minIR = max(compImb, commImb);
          imbalance->maxIR = imbalance->minIR;
          if (diff <= imbalance->minIR * IMB_RESOLVE_RATIO) {
            // If so, computation call paths are causes while communication ones are symptoms.
            imbalance->causeIdxes = compIdxes;
            imbalance->symptomIdxes = commIdxes;
          }
          else {
            // If not, cause-symptom relationship is unclear.
            imbalance->symptomIdxes = compIdxes;
            imbalance->symptomIdxes.insert(imbalance->symptomIdxes.end(), commIdxes.begin(), commIdxes.end());
            
            imbalance->minIR = imb[imbalance->symptomIdxes[0]];
            for (uint i = 1; i < imbalance->symptomIdxes.size(); i++)
              if (imb[imbalance->symptomIdxes[i]] > imbalance->minIR)
                imbalance->minIR = imb[imbalance->symptomIdxes[i]];
            imbalance->maxIR = max(imbalance->minIR, (commImb+compImb)/2);
          }
          
          segment->imbs.push_back(imbalance);
          
          for (int k = startIdx+1; k <= endIdx; k++)
            lcaDepth[k] = RESOLVED;
        }
        else {
          for (int k = startIdx+1; k <= endIdx; k++)
            lcaDepth[k] = UNSOLVED;
        }
      }
      
      delete lcaDepth;
      delete isComm;
      delete imb;
    }
    
    void diagnoseCommunication(ExecutionSegment* segment) {
      for (uint k = 0; k < segment->callpaths.size(); k++) {
        const TCTANode* node = segment->callpaths[k]->callpath.back();
        CallpathMetrics* metrics = metricsMap[node];
        if (((metrics->derivedLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION)
            && (metrics->subTreeCommIR[SEMANTIC_LABEL_COMMUNICATION] >= MIN_SUBTREE_IR)) {
          uint label = SEMANTIC_LABEL_COMMUNICATION;
          double ir = metrics->subTreeCommIR[label];
          // Find the label which is the major source of the time spent in communication
          for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
            if (((SEMANTIC_LABEL_ARRAY[i].label & label) == label) // SEMANTIC_LABEL_ARRAY[i].label is a child of label
                && (SEMANTIC_LABEL_ARRAY[i].label != label)
                && (metrics->subTreeCommIR.find(SEMANTIC_LABEL_ARRAY[i].label) != metrics->subTreeCommIR.end())
                && (metrics->subTreeCommIR[SEMANTIC_LABEL_ARRAY[i].label] >= ir * HOT_PATH_RATIO) ) { // SEMANTIC_LABEL_ARRAY[i].label is the major source
              label = SEMANTIC_LABEL_ARRAY[i].label;
              ir = metrics->subTreeCommIR[label];
            }
          
          CommunicationInstance* comm = new CommunicationInstance();
          comm->callpathIdx = k;
          comm->semanticLabel = label;
          comm->IR = metrics->subTreeCommIR[SEMANTIC_LABEL_COMMUNICATION];
          comm->numInstances = node->getRetCount() / numProc;
          comm->avgNumSamples = (long)(node->getNumSamples() * node->getWeight() / node->getRetCount());

          segment->comms.push_back(comm);
        }
      }
    }
    
    const int MSG_PRIO = MSG_PRIO_NORMAL;
    
    void printLoadImbalance(ExecutionSegment* segment) {
      for (uint k = 0; k < segment->imbs.size(); k++) {
        LoadImbalance* imbalance = segment->imbs[k];
        
        string causeStr = "";
        for (uint i = 0; i < imbalance->causeIdxes.size(); i++)
          causeStr += "#" + std::to_string(imbalance->causeIdxes[i]+1) + ",";
        if (causeStr.size() > 0) causeStr = causeStr.substr(0, causeStr.size()-1);
        
        string symptomStr = "";
        for (uint i = 0; i < imbalance->symptomIdxes.size(); i++)
          symptomStr += "#" + std::to_string(imbalance->symptomIdxes[i]+1) + ",";
        symptomStr = symptomStr.substr(0, symptomStr.size()-1);
        
        if (causeStr.size() > 0)
          print_msg(MSG_PRIO, "Imbalance in [%s (cause)] caused the imbalance in [%s (symptom)] at LCA depth %d: IR = %.2f%%\n",  
                causeStr.c_str(), symptomStr.c_str(), imbalance->lcaDepth, imbalance->minIR * 100);
        else if (imbalance->resolved) 
          print_msg(MSG_PRIO, "Imbalance in [%s] is resolved at LCA depth %d but the cause-symptom relationship is unclear: minIR = %.2f%%, maxIR = %.2f%%\n",  
                symptomStr.c_str(), imbalance->lcaDepth, imbalance->minIR * 100, imbalance->maxIR * 100);
        else
          print_msg(MSG_PRIO, "Imbalance in [%s] is not resolved: IR = %.2f%%\n",  
                symptomStr.c_str(), imbalance->minIR * 100);
      }
    }
    
    void printCommunication(ExecutionSegment* segment) {
      for (uint k = 0; k < segment->comms.size(); k++) {
        CommunicationInstance* comm = segment->comms[k];
        
        string prefix = "#" + std::to_string(comm->callpathIdx+1) ;
        if (comm->avgNumSamples >= COMM_LONG_INSTANCE_SAMPLES)
          prefix = "Long call instances to [" + prefix + "]";
        else if (comm->avgNumSamples <= COMM_SHORT_INSTANCE_SAMPLES)
          prefix = "Frequent but short call instances to [" + prefix + "]";
        else
          prefix = "Call instances to [" + prefix + "]";
        
        print_msg(MSG_PRIO, "%s leads to significant amount of time spent in [%s]: IR = %.2f%%\n",  
              prefix.c_str(), semanticLabelToString(comm->semanticLabel).c_str(), comm->IR * 100);
      }
    }
    
    void printExecutionSegments() {
      print_msg(MSG_PRIO, "\nInefficient execution segments:\n");
      
      for (uint k = 0; k < allSegments.size(); k++) {
        ExecutionSegment* segment = allSegments[k];
        
        print_msg(MSG_PRIO, "\nSegments #%d: imbalance = %.2f%%, comm = %.2f%%, sync_imb = %.2f%%, sync = %.2f%%\n", k, 
                segment->totalImbIR * 100, segment->totalCommIR * 100, segment->syncImbIR * 100, segment->syncCommIR * 100);
        
        Callpath* lastCallpath = NULL;
        if (segment->priorSyncCallpath != NULL) {
          printCallpath(segment->priorSyncCallpath, lastCallpath, 0);
          lastCallpath = segment->priorSyncCallpath;
        } 
        for (uint i = 0; i < segment->callpaths.size(); i++) {
          Callpath* callpath = segment->callpaths[i];
          printCallpath(callpath, lastCallpath, i+1);
          lastCallpath = callpath;
        }
        
        printLoadImbalance(segment);
        printCommunication(segment);
        print_msg(MSG_PRIO, "\n");
      }
    }
    
    void printCallpath(Callpath* callpath, Callpath* lastCallpath, int idx) {
      uint depth = 0;
      string indent = "";
      while ((lastCallpath != NULL) && (depth < lastCallpath->callpath.size())
              && (depth < callpath->callpath.size()) && (callpath->callpath[depth] == lastCallpath->callpath[depth])) {
        depth ++;
        indent += "  ";
      }
      
      for (uint k = depth; k < callpath->callpath.size(); k++) {
        const TCTANode* node = callpath->callpath[k];
        CallpathMetrics* metrics = metricsMap[node];
        if (idx > 0) {
          if (k == callpath->callpath.size() - 1) {
            if ((metrics->derivedLabel & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) 
              indent += "**Sync #" + std::to_string(idx);
            else
              indent += "**Cause #" + std::to_string(idx);
            
            if (callpath->isInclusive)
                indent += " (inclusive) ";
            else
                indent += " (exclusive) ";
          }

          print_msg(MSG_PRIO, "%s%s%s [%s]: all = %.2f%%(%.2f%%)", indent.c_str(), node->getName().c_str(), node->id.toString().c_str(),
                  semanticLabelToString(metrics->derivedLabel).c_str(), metrics->getSubtreeIR() * 100, metrics->getNodeIR() * 100);
          print_msg(MSG_PRIO, ", imbalance = %.2f%%(%.2f%%)", metrics->subtreeImbIR * 100, metrics->nodeImbIR * 100); 
          if ((metrics->derivedLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION)
            for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
              if (metrics->subTreeCommIR.find(SEMANTIC_LABEL_ARRAY[i].label) != metrics->subTreeCommIR.end()) {
                double nodeIR = 0;
                if (metrics->nodeCommIR.find(SEMANTIC_LABEL_ARRAY[i].label) != metrics->nodeCommIR.end())
                  nodeIR = metrics->nodeCommIR[SEMANTIC_LABEL_ARRAY[i].label];
                print_msg(MSG_PRIO, ", %s = %.2f%%(%.2f%%)", semanticLabelToString(SEMANTIC_LABEL_ARRAY[i].label).c_str(), 
                    metrics->subTreeCommIR[SEMANTIC_LABEL_ARRAY[i].label] * 100, nodeIR * 100);
              }
          
          if ((k == callpath->callpath.size() - 1) && (!callpath->isInclusive))
            print_msg(MSG_PRIO, ", excImb = %.2f%%", metrics->nodeExcImbIR * 100);
          
          print_msg(MSG_PRIO, "\n");
        }
        // idx == 0. which stands for prior sync callpath
        else if (k < callpath->callpath.size() - 1)
          print_msg(MSG_PRIO, "%s%s%s\n", indent.c_str(), node->getName().c_str(), node->id.toString().c_str());
        else
          print_msg(MSG_PRIO, "%s**PRIOR SYNC** %s%s\n", indent.c_str(), node->getName().c_str(), node->id.toString().c_str());
          
        indent += "  ";
      }
    }
    
  private:
    TCTRootNode* root;
    long numProc;
    string dbDir;
    Time totalDuration;
    
    unordered_map<const TCTANode*, CallpathMetrics*> metricsMap;
    
    ExecutionSegment* currentSegment;
    vector<ExecutionSegment*> allSegments;
  };
  
  void PerformanceDiagnoser::generateDiagnosis(TCTRootNode* mergedRoot, string dbDir) {
    PerformanceDiagnoserImpl diagnoser(mergedRoot, dbDir);
  }
}
