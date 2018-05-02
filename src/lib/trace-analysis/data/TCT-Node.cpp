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
 * File:   TCT-Node.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 5, 2018, 3:08 PM
 * 
 * Temporal Context Tree nodes.
 */

#include "TCT-Node.hpp"

namespace TraceAnalysis {
  string TCTANode::toString(int maxDepth, Time minDuration, Time samplingInterval) const {
    string ret;
    /*
    ret += "CFG-0x";
    if (cfgGraph == NULL) ret += vmaToHexString(0);
    else ret += vmaToHexString(cfgGraph->vma);
    ret += " RA-0x" + vmaToHexString(ra) + " ";
    */
    for (int i = 0; i < depth; i++) ret += "  ";
    ret += name + id.toString();
    ret += " " + time.toString();
    if (samplingInterval != 0)
      ret += ", " + std::to_string(getDuration()/samplingInterval) + " samples";
    if (diffScore.getInclusive() > 0)
      ret += ", DiffScore = " + std::to_string((long)diffScore.getInclusive())
              + "/" + std::to_string((long)diffScore.getExclusive());
    ret += "\n";
    
    return ret;
  }
  
  string TCTATraceNode::toString(int maxDepth, Time minDuration, Time samplingInterval) const {
    string ret = TCTANode::toString(maxDepth, minDuration, samplingInterval);
    
    if (depth >= maxDepth) return ret;
    if (minDuration > 0 && time.getDuration() < minDuration) return ret;
    if (name.find("<unknown procedure>") != string::npos) return ret;
    
    for (auto it = children.begin(); it != children.end(); it++)
      ret += (*it)->toString(maxDepth, minDuration, samplingInterval);
    return ret;
  }
  
  string TCTLoopNode::toString(int maxDepth, Time minDuration, Time samplingInterval) const {
    string ret = TCTANode::toString(maxDepth, minDuration, samplingInterval);
    ret.pop_back();
    ret += ", # iterations = " + std::to_string(numIteration) + "\n";
    
    if (depth >= maxDepth) return ret;
    if (minDuration > 0 && time.getDuration() < minDuration) return ret;
    
    for (auto it = acceptedIterations.begin(); it != acceptedIterations.end(); it++)
      ret += (*it)->toString(maxDepth, minDuration, samplingInterval);
    
    if (rejectedIterations != NULL)
      ret += rejectedIterations->toString(maxDepth, minDuration, samplingInterval);
    
    return ret;
  }
  
  string TCTProfileNode::toString(int maxDepth, Time minDuration, Time samplingInterval) const {
    string ret = TCTANode::toString(maxDepth, minDuration, samplingInterval);
    
    if (depth >= maxDepth) return ret;
    if (minDuration > 0 && time.getDuration() < minDuration) return ret;
    if (name.find("<unknown procedure>") != string::npos) return ret;
    
    for (auto it = childMap.begin(); it != childMap.end(); it++)
      ret += it->second->toString(maxDepth, minDuration, samplingInterval);
    return ret;
  }
  
  void TCTPerfLossMetric::initDurationMetric() {
    maxDuration = node->getDuration();
    minDuration = node->getDuration();
    totalDuration = (double)node->getDuration() * (double)node->getWeight();
  }
  
  void TCTPerfLossMetric::setDuratonMetric(const TCTPerfLossMetric& rep1, const TCTPerfLossMetric& rep2) {
    maxDuration = std::max(rep1.maxDuration, rep2.maxDuration);
    minDuration = std::min(rep1.minDuration, rep2.minDuration);
    totalDuration = rep1.totalDuration + rep2.totalDuration;
  }
  
  TCTLoopNode::TCTLoopNode(const TCTLoopNode& orig) : TCTANode(orig) {
    numIteration = orig.numIteration;
    
    if (orig.pendingIteration != NULL) {
      pendingIteration = (TCTIterationTraceNode*)(orig.pendingIteration->duplicate());
      pendingIterationAccepted = orig.pendingIterationAccepted;
    }
    if (orig.rejectedIterations != NULL)
      rejectedIterations = (TCTProfileNode*)(orig.rejectedIterations->duplicate());

    for (auto it = orig.acceptedIterations.begin(); it != orig.acceptedIterations.end(); it++)
      acceptedIterations.push_back((TCTIterationTraceNode*)((*it)->duplicate()));
  }
  
  TCTLoopNode::~TCTLoopNode()  {
    if (pendingIteration != NULL) delete pendingIteration;
    if (rejectedIterations != NULL) delete rejectedIterations;

    for (auto it = acceptedIterations.begin(); it != acceptedIterations.end(); it++)
      delete (*it);
  }
  
  void TCTLoopNode::finalizePendingIteration() {
    if (pendingIteration == NULL) return;
    
    numIteration++;
    
    if (pendingIterationAccepted) 
      acceptedIterations.push_back(pendingIteration);
    else {
      TCTProfileNode* prof = TCTProfileNode::newProfileNode(pendingIteration);
      if (rejectedIterations != NULL) {
        rejectedIterations->merge(prof);
        delete prof;
      } else {
        rejectedIterations = prof;
        rejectedIterations->name = "Rejected Iterations";
      }
      delete pendingIteration;
    }
    pendingIteration = NULL;
  }
  
  bool TCTLoopNode::acceptLoop() {
    if (rejectedIterations == NULL) return true;
    
    if (rejectedIterations->getDuration() >= getDuration() * LOOP_REJ_THRESHOLD)
      return false;
    return true;
  }
  
  TCTProfileNode::TCTProfileNode(const TCTATraceNode& trace) : TCTANode (trace, Prof) {
    for (auto it = trace.children.begin(); it != trace.children.end(); it++) {
      TCTProfileNode* child = TCTProfileNode::newProfileNode(*it);
      if (childMap.find(child->id) == childMap.end())
        childMap[child->id] = child;
      else {
        childMap[child->id]->merge(child);
        delete child;
      }
    }
  }
  
  TCTProfileNode::TCTProfileNode(const TCTLoopNode& loop) : TCTANode (loop, Prof) {
    for (auto it = loop.acceptedIterations.begin(); it != loop.acceptedIterations.end(); it++) {
      TCTProfileNode* child = TCTProfileNode::newProfileNode(*it);
      child->setDepth(this->depth);
      for (auto iit = child->childMap.begin(); iit != child->childMap.end(); iit++) {
        if (childMap.find(iit->second->id) == childMap.end())
          childMap[iit->second->id] = (TCTProfileNode*) iit->second->duplicate();
        else
          childMap[iit->second->id]->merge(iit->second);
      }
      delete child;
    }

    if (loop.rejectedIterations != NULL) {
      TCTProfileNode* child = TCTProfileNode::newProfileNode(loop.rejectedIterations);
      child->setDepth(this->depth);
      for (auto iit = child->childMap.begin(); iit != child->childMap.end(); iit++) {
        if (childMap.find(iit->second->id) == childMap.end())
          childMap[iit->second->id] = (TCTProfileNode*) iit->second->duplicate();
        else
          childMap[iit->second->id]->merge(iit->second);
      }
      delete child;
    }
  }
  
  TCTProfileNode::TCTProfileNode(const TCTProfileNode& prof, bool copyChildMap) : TCTANode (prof) {
    if (copyChildMap)
      for (auto it = prof.childMap.begin(); it != prof.childMap.end(); it++)
        childMap[it->second->id] = (TCTProfileNode*) it->second->duplicate();
  }
  
  void TCTProfileNode::addChild(TCTANode* child) {
    TCTProfileNode* profChild = NULL; 
    if (child->type == TCTANode::Prof)
      profChild = (TCTProfileNode*) child;
    else {
      profChild = TCTProfileNode::newProfileNode(child);
      delete child;
    }

    if (childMap.find(profChild->id) == childMap.end())
      childMap[profChild->id] = profChild;
    else {
      childMap[profChild->id]->merge(profChild);
      delete profChild;
    }
  }
  
  void TCTProfileNode::merge(const TCTProfileNode* other) {
    time.addTime(other->time);
    for (auto it = other->childMap.begin(); it != other->childMap.end(); it++) {
      if (childMap.find(it->second->id) == childMap.end())
        childMap[it->second->id] = (TCTProfileNode*) it->second->duplicate();
      else
        childMap[it->second->id]->merge(it->second);
    }
  }
  
  void TCTProfileNode::getExclusiveDuration(Time& minExclusive, Time& maxExclusive) const {
    minExclusive = getMinDuration();
    maxExclusive = getMaxDuration();
    for (auto it = childMap.begin(); it != childMap.end(); it++) {
      TCTProfileNode* child = it->second;
      minExclusive -= child->getMaxDuration();
      maxExclusive -= child->getMinDuration();
    }
  }
  
  void TCTATraceNode::getLastChildEndTime(int idx, Time& inclusive, Time& exclusive) const {
    if (idx < 0 || idx > getNumChild()) {
      print_msg(MSG_PRIO_MAX, "ERROR: wrong idx %d for getLastChildEndTime(). 0 <= idx <= %d should hold.\n", 
              idx, getNumChild());
      return;
    }

    if (idx == 0) {
      inclusive = time.getStartTimeExclusive();
      exclusive = time.getStartTimeInclusive();
    }
    else {
      inclusive = getChild(idx-1)->getTime().getEndTimeInclusive();
      exclusive = getChild(idx-1)->getTime().getEndTimeExclusive();
    }
  }
  
  void TCTATraceNode::getCurrChildStartTime(int idx, Time& exclusive, Time& inclusive) const {
    if (idx < 0 || idx > getNumChild()) {
      print_msg(MSG_PRIO_MAX, "ERROR: wrong idx %d for getCurrChildStartTime(). 0 <= idx <= %d should hold.\n", 
              idx, getNumChild());
      return;
    }
    
    if (idx == getNumChild()) {
      exclusive = time.getEndTimeInclusive();
      inclusive = time.getEndTimeExclusive();
    }
    else {
      exclusive = getChild(idx)->getTime().getStartTimeExclusive();
      inclusive = getChild(idx)->getTime().getStartTimeInclusive();
    }
  }
  
  void TCTATraceNode::getGapBeforeChild(int idx, Time& minGap, Time& maxGap) const {
    if (idx < 0 || idx > getNumChild()) {
      print_msg(MSG_PRIO_MAX, "ERROR: wrong idx %d for getGapBeforeChild(). 0 <= idx <= %d should hold.\n", 
              idx, getNumChild());
      return;
    }

    Time lastEndInclusive, lastEndExclusive, currStartExclusive, currStartInclusive;
    getLastChildEndTime(idx, lastEndInclusive, lastEndExclusive);
    getCurrChildStartTime(idx, currStartExclusive, currStartInclusive);
    
    minGap = currStartExclusive - lastEndExclusive + 1;
    maxGap = currStartInclusive - lastEndInclusive - 1;
  }
}