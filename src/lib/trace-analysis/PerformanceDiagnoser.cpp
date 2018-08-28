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

#include <algorithm>
using std::max;

namespace TraceAnalysis {
  struct CallPathMetrics {
    uint semanticLabel;
    double inclusiveImbIR;
    double exclusiveImbIR;
    unordered_map<uint, double> inclusiveCommIR;
    unordered_map<uint, double> exclusiveCommIR;
    
    double getInclusiveIR() {
      double ir = inclusiveImbIR;
      if (inclusiveCommIR.find(SEMANTIC_LABEL_COMMUNICATION) != inclusiveCommIR.end())
        ir += inclusiveCommIR[SEMANTIC_LABEL_COMMUNICATION];
      return ir;
    }
    
    double getExclusiveIR() {
      double ir = exclusiveImbIR;
      if (exclusiveCommIR.find(SEMANTIC_LABEL_COMMUNICATION) != exclusiveCommIR.end())
        ir += exclusiveCommIR[SEMANTIC_LABEL_COMMUNICATION];
      return ir;
    }
    
    void addChildMetrics(const CallPathMetrics* childMetrics) {
      inclusiveImbIR += childMetrics->inclusiveImbIR;
      for (auto it = childMetrics->inclusiveCommIR.begin(); it != childMetrics->inclusiveCommIR.end(); it++)
        if (inclusiveCommIR.find(it->first) != inclusiveCommIR.end())
          inclusiveCommIR[it->first] += it->second;
        else
          inclusiveCommIR[it->first] = it->second;
    }
  };
  
  struct Callpath {
    vector<const TCTANode*> callpath;

    Callpath(const vector<const TCTANode*>& cp) {
      callpath = cp;
    }
  };
  
  struct ExecutionSegment {
    Callpath* priorSyncCallpath;
    vector<Callpath*> callpaths;
    
    double totalImbIR;
    double totalCommIR;
    
    double syncImbIR;
    double syncCommIR;
    
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
    }
  };
  
  class PerformanceDiagnoserImpl {
  public:
    PerformanceDiagnoserImpl(TCTRootNode* mergedRoot, string dbDir) : root(mergedRoot), dbDir(dbDir) {
      numProc = root->getWeight();
      totalDuration = root->getDuration();
      current_segment = new ExecutionSegment();
      
      computeMetrics(root, SEMANTIC_LABEL_COMPUTATION);
      
      vector<const TCTANode*> callpath;
      callpath.push_back(root);
      generateDiagnosisReport(callpath);
      if (current_segment->totalCommIR + current_segment->totalImbIR >= MIN_EXECUTION_SEGMENT_IR)
        all_segments.push_back(current_segment);
      else
        delete current_segment;
      current_segment = NULL;
      
      printExecutionSegments();
    }
    virtual ~PerformanceDiagnoserImpl() {
      for (auto it = metricsMap.begin(); it != metricsMap.end(); it++)
        delete it->second;
      for (auto it = all_segments.begin(); it != all_segments.end(); it++)
        delete *it;
    }
    
  private:
    void computeMetrics(const TCTANode* node, uint parentSemanticLabel) {
      CallPathMetrics* metrics = new CallPathMetrics();
      metricsMap[node] = metrics;
      
      metrics->semanticLabel = getFuncSemanticInfo(node->getName()).semantic_label | parentSemanticLabel;
      double imbalance, comm;
      if ((metrics->semanticLabel & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) {
        // imb(S) = avg(S) - min(S);
        imbalance = node->getPerfLossMetric().getAvgDuration(node->getWeight()) - node->getPerfLossMetric().getMinDuration();
        // comm(S) = min(S);
        comm = node->getPerfLossMetric().getMinDuration();
      }
      else if ((metrics->semanticLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        // imb(W) = max(W) - avg(W);
        imbalance = node->getPerfLossMetric().getMaxDuration() - node->getPerfLossMetric().getAvgDuration(node->getWeight());
        // comm(W) = avg(W);
        comm = node->getPerfLossMetric().getAvgDuration(node->getWeight());
      }
      else {
        // imb(C) = max(C) - avg(C);
        imbalance = node->getPerfLossMetric().getMaxDuration() - node->getPerfLossMetric().getAvgDuration(node->getWeight());
        comm = 0;
      }
      
      imbalance = max(imbalance, 0.0);
      comm = max(comm, 0.0);
      
      // Set exclusive imbalance improvement ratio in metrics
      metrics->exclusiveImbIR = imbalance * node->getWeight() / numProc / totalDuration;
      
      // Set exclusive communication improvement ratio in metrics
      double commIR = comm * node->getWeight() / numProc / totalDuration;
      // If commIR is larger than zero, enumerate all semantic labels
      if (commIR > 0) {
        for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
          // if the label is a communication label
          if ((SEMANTIC_LABEL_ARRAY[i].label & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION
                  // and the node's semantic label is in accordance with the enumerated label
                  && (metrics->semanticLabel & SEMANTIC_LABEL_ARRAY[i].label) == SEMANTIC_LABEL_ARRAY[i].label)
            metrics->exclusiveCommIR[SEMANTIC_LABEL_ARRAY[i].label] = commIR;
      }
      
      metrics->inclusiveImbIR = 0;
      if (node->type == TCTANode::Prof) {
        const TCTProfileNode* prof = (const TCTProfileNode*) node;
        for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++) {
          computeMetrics(it->second, metrics->semanticLabel);
          metrics->addChildMetrics(metricsMap[it->second]);
        }
      }
      else if (node->type == TCTANode::Loop) {
        const TCTANode* avgRep = ((TCTLoopNode*)node)->getClusterNode()->getAvgRep();
        computeMetrics(avgRep, metrics->semanticLabel);
        //TODO metrics from rejected iterations are ignored.
        metrics->addChildMetrics(metricsMap[avgRep]);
      }
      else {
        const TCTATraceNode* trace = (const TCTATraceNode*) node;
        for (int i = 0; i < trace->getNumChild(); i++) {
          computeMetrics(trace->getChild(i), metrics->semanticLabel);
          metrics->addChildMetrics(metricsMap[trace->getChild(i)]);
        }
      }
      
      // Adjust inclusive IRs when they are less than exclusive IRs.
      metrics->inclusiveImbIR = max(metrics->exclusiveImbIR, metrics->inclusiveImbIR);
      if ((metrics->semanticLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        for (auto it = metrics->exclusiveCommIR.begin(); it != metrics->exclusiveCommIR.end(); it++)
          if (metrics->inclusiveCommIR.find(it->first) != metrics->inclusiveCommIR.end())
            metrics->inclusiveCommIR[it->first] = max(it->second, metrics->inclusiveCommIR[it->first]);
          else
            metrics->inclusiveCommIR[it->first] = it->second;
      }
    }

    void mergeExecutionSegmentsAfter(uint idx) {
      if (all_segments.size() > idx) {
        // Merge all segments after all_segments[idx].
        for (uint k = idx + 1; idx < all_segments.size(); k++) {
          all_segments[idx]->totalCommIR += all_segments[k]->totalCommIR;
          all_segments[idx]->totalImbIR  += all_segments[k]->totalImbIR;
          all_segments[idx]->syncCommIR  += all_segments[k]->syncCommIR;
          all_segments[idx]->syncImbIR   += all_segments[k]->syncImbIR;
          all_segments[idx]->callpaths.insert(all_segments[idx]->callpaths.end(), 
                  all_segments[k]->callpaths.begin(), all_segments[k]->callpaths.end());
          
          all_segments[k]->callpaths.clear();
          delete all_segments[k];
        }
        all_segments.resize(idx+1);
        
        // Also merge current_segment to all_segments[idx]
        all_segments[idx]->totalCommIR += current_segment->totalCommIR;
        all_segments[idx]->totalImbIR  += current_segment->totalImbIR;
        all_segments[idx]->callpaths.insert(all_segments[idx]->callpaths.end(), 
                current_segment->callpaths.begin(), current_segment->callpaths.end());
        
        current_segment->totalCommIR = 0;
        current_segment->totalImbIR = 0;
        current_segment->callpaths.clear();
      }
    }
    
    // Return true if one or more inefficient execution segment is generated.
    void generateDiagnosisReport(vector<const TCTANode*>& callpath) {
      const TCTANode* node = callpath.back();
      CallPathMetrics* metrics = metricsMap[node];
      
      // If the current node is synchronization.
      if ((metrics->semanticLabel & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) {
        // If IR passes the threshold, terminate the current execution segment.
        if (metrics->exclusiveImbIR >= MIN_SYNC_IMB_IR
                || metrics->exclusiveCommIR[SEMANTIC_LABEL_SYNC] >= MIN_SYNC_COMM_IR) {
          // Check if improvement ratio of this execution segment exceeds the threshold.
          if (current_segment->totalCommIR + current_segment->totalImbIR + metrics->exclusiveCommIR[SEMANTIC_LABEL_SYNC]
                  >= MIN_EXECUTION_SEGMENT_IR) {
            current_segment->callpaths.push_back(new Callpath(callpath));
            current_segment->syncImbIR += metrics->exclusiveImbIR;
            current_segment->syncCommIR += metrics->exclusiveCommIR[SEMANTIC_LABEL_SYNC];
            all_segments.push_back(current_segment);
          }
          else
            delete current_segment;
          
          current_segment = new ExecutionSegment();
          current_segment->priorSyncCallpath = new Callpath(callpath);
        }
        
        return;
      }
      
      // If the node has a sync successor in its subtree.
      bool hasSync = (metrics->inclusiveCommIR.find(SEMANTIC_LABEL_SYNC) != metrics->inclusiveCommIR.end());
      double incIR = metrics->getInclusiveIR();
      double excIR = metrics->getExclusiveIR();
      // If improvement ratio of this node is not significant and the node has no sync successor, return.
      if (incIR < MIN_INC_IR && (!hasSync)) return;
      
      // If the node is communication, add to significant callpaths and return. (Details in its subtree will be investigated later)
      if ((metrics->semanticLabel & SEMANTIC_LABEL_COMMUNICATION) == SEMANTIC_LABEL_COMMUNICATION) {
        current_segment->totalImbIR += metrics->inclusiveImbIR;
        current_segment->totalCommIR += (incIR - metrics->inclusiveImbIR);
        current_segment->callpaths.push_back(new Callpath(callpath));
        return;
      }
      
      // If the node is computation
      bool inspectChild = hasSync;

      if (node->type == TCTANode::Loop) {
        inspectChild = true;
        const TCTANode* avgRep = ((TCTLoopNode*)node)->getClusterNode()->getAvgRep();
        callpath.push_back(avgRep);
        generateDiagnosisReport(callpath);
        callpath.pop_back();
      }
      else if (node->type == TCTANode::Prof) {
        const TCTProfileNode* prof = (const TCTProfileNode*) node;
        // First, see if there is a "hot path" child.
        if (!inspectChild) {
          for (auto it = prof->getChildMap().begin(); it != prof->getChildMap().end(); it++) {
            double childIncIR = metricsMap[it->second]->getInclusiveIR();
            if (childIncIR >= excIR * HOT_PATH_RATIO) {
              inspectChild = true;
              break;
            }
          }
        }
        // If there is, inspect every child.
        if (inspectChild) {
          uint numSegments = all_segments.size();
          
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
            callpath.push_back(children[k]);
            generateDiagnosisReport(callpath);
            callpath.pop_back();
          }
          
          // All execution segments generated will be merged into one.
          //TODO mergeExecutionSegmentsAfter(numSegments);
        }
      }
      else {
        const TCTATraceNode* trace = (const TCTATraceNode*) node;
        if (!inspectChild) {
          for (int i = 0; i < trace->getNumChild(); i++) {
            double childIncIR = metricsMap[trace->getChild(i)]->getInclusiveIR();
            if (childIncIR >= excIR * HOT_PATH_RATIO) {
              inspectChild = true;
              break;
            }
          }
        }
        if (inspectChild) {
          uint numSegments = all_segments.size();
          
          for (int i = 0; i < trace->getNumChild(); i++) {
            callpath.push_back(trace->getChild(i));
            generateDiagnosisReport(callpath); 
            callpath.pop_back();
          }
          
          // All execution segments generated will be merged into one for loops
          //TODO mergeExecutionSegmentsAfter(numSegments);
        }
      }
      
      // When no child is significant, report the inefficiency at the current callpath depth.
      if (!inspectChild) {
        current_segment->totalImbIR += metrics->inclusiveImbIR;
        current_segment->callpaths.push_back(new Callpath(callpath));
      }
    }
    
    const int MSG_PRIO = MSG_PRIO_NORMAL;
    void printExecutionSegments() {
      print_msg(MSG_PRIO, "\nInefficient execution segments:\n");
      
      for (uint k = 0; k < all_segments.size(); k++) {
        ExecutionSegment* segment = all_segments[k];
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
      
      for (uint k = depth; k < callpath->callpath.size()-1; k++) {
        const TCTANode* node = callpath->callpath[k];
        CallPathMetrics* metrics = metricsMap[node];
        if (idx > 0)
          print_msg(MSG_PRIO, "%s%s%s: all = %.2f%%(%.2f%%), imb = %.2f%%(%.2f%%)\n", indent.c_str(), node->getName().c_str(), node->id.toString().c_str(),
                  metrics->getInclusiveIR() * 100, metrics->getExclusiveIR() * 100, metrics->inclusiveImbIR * 100, metrics->exclusiveImbIR * 100);
        else
          print_msg(MSG_PRIO, "%s%s%s\n", indent.c_str(), node->getName().c_str(), node->id.toString().c_str());
        indent += "  ";
      }
      
      const TCTANode* node = callpath->callpath.back();
      CallPathMetrics* metrics = metricsMap[node];
      if (idx > 0) {
        if ((metrics->semanticLabel & SEMANTIC_LABEL_SYNC) == SEMANTIC_LABEL_SYNC) 
          indent += "**Sync #" + std::to_string(idx) + " ";
        else
          indent += "**Cause #" + std::to_string(idx) + " ";
        print_msg(MSG_PRIO, "%s%s%s: all = %.2f%%(%.2f%%), imb = %.2f%%(%.2f%%)\n", indent.c_str(), node->getName().c_str(), node->id.toString().c_str(),
                metrics->getInclusiveIR() * 100, metrics->getExclusiveIR() * 100, metrics->inclusiveImbIR * 100, metrics->exclusiveImbIR * 100);
      }
      else
        print_msg(MSG_PRIO, "%s**PRIOR SYNC** %s%s\n", indent.c_str(), node->getName().c_str(), node->id.toString().c_str());
    }
    
  private:
    TCTRootNode* root;
    long numProc;
    string dbDir;
    Time totalDuration;
    
    unordered_map<const TCTANode*, CallPathMetrics*> metricsMap;
    
    ExecutionSegment* current_segment;
    vector<ExecutionSegment*> all_segments;
  };
  
  void PerformanceDiagnoser::generateDiagnosis(TCTRootNode* mergedRoot, string dbDir) {
    PerformanceDiagnoserImpl diagnoser(mergedRoot, dbDir);
  }
}