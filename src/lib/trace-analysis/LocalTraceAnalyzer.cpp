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
 * File:   LocalTraceAnalyzer.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 * 
 * Created on March 6, 2018, 11:27 PM
 * 
 * Analyzes traces for a rank/thread and generates a summary temporal context tree.
 */

#include <string>
using std::string;
#include <vector>
using std::vector;

#include <lib/prof-lean/hpcrun-fmt.h>

#include "LocalTraceAnalyzer.hpp"

namespace TraceAnalysis {

  LocalTraceAnalyzer::LocalTraceAnalyzer(BinaryAnalyzer& binaryAnalyzer, 
          CCTVisitor& cctVisitor, string traceFileName, Time minTime) : 
          binaryAnalyzer(binaryAnalyzer), reader(cctVisitor, traceFileName, minTime) {}

  LocalTraceAnalyzer::~LocalTraceAnalyzer() {}
  
  void pushActiveStack(vector<TCTATraceNode*>& activeStack, TCTATraceNode* node, 
          Time startTimeExclusive, Time startTimeInclusive) {
    // Iteration Detection
    if (activeStack.back()->type == TCTANode::Iter) {
      CFGAGraph* cfg = activeStack.back()->cfgGraph;
      int prevRA = 0;
      for (int i = activeStack.back()->getNumChild()-1; i >= 0 && !cfg->hasChild(prevRA); i--)
        prevRA = activeStack.back()->getChild(i)->ra;
      
      // when both prevRA and thisRA is child of cfg, and thisRA is not a successor of prevRA
      if (cfg->hasChild(prevRA) && cfg->hasChild(node->ra) && cfg->compareChild(prevRA, node->ra) != -1) {
        // pop the old iteration
        activeStack.back()->getTraceTime()->setEndTime(startTimeExclusive, startTimeInclusive);
        activeStack.pop_back();
        // push a new iteration
        TCTATraceNode* node = new TCTIterationTraceNode(activeStack.back()->id, activeStack.back()->getNumChild(), 
                activeStack.size(), cfg);
        node->getTraceTime()->setStartTime(startTimeExclusive, startTimeInclusive);
        activeStack.back()->addChild(node);
        activeStack.push_back(node);
      }
    }
    
    if (node->type == TCTANode::Loop) {
      //TODO: for loop nodes with NULL cfgGraph, convert to Profile Node.
      
      // for loops with valid cfgGraph
      if (node->cfgGraph != NULL) {
        // add to stack
        node->getTraceTime()->setStartTime(startTimeExclusive, startTimeInclusive);
        activeStack.back()->addChild(node);
        activeStack.push_back(node);
        // create a iteration
        node = new TCTIterationTraceNode(activeStack.back()->id, activeStack.back()->getNumChild(), 
                activeStack.size(), activeStack.back()->cfgGraph);
        node->getTraceTime()->setStartTime(startTimeExclusive, startTimeInclusive);
        activeStack.back()->addChild(node);
        activeStack.push_back(node);
        return;
      }
    }
    
    //TODO: for func and iter nodes, check if a sub-loop is "splitted".
    //TODO: for func and iter nodes, check if there is undetected loop.
    
    node->getTraceTime()->setStartTime(startTimeExclusive, startTimeInclusive);
    activeStack.back()->addChild(node);
    activeStack.push_back(node);
  }
  
  void popActiveStack(vector<TCTATraceNode*>& activeStack, 
          Time endTimeInclusive, Time endTimeExclusive) {
    if (activeStack.back()->type == TCTANode::Iter) {
      // When need to pop loops, first pop iteration and then pop loop.
      activeStack.back()->getTraceTime()->setEndTime(endTimeInclusive, endTimeExclusive);
      activeStack.pop_back();
    }
    activeStack.back()->getTraceTime()->setEndTime(endTimeInclusive, endTimeExclusive);
    activeStack.pop_back();
  }
  
  int computeLCADepth(CallPathSample* prev, CallPathSample* current) {
    int depth = current->getDepth();
    int dLCA = current->dLCA;
    if (dLCA != HPCRUN_FMT_DLCA_NULL) {
      while (depth > 0 && dLCA > 0) {
        if (current->getFrameAtDepth(depth).type == CallPathFrame::Func)
          dLCA--;
        depth--;
      }
    }
    
    if (depth > prev->getDepth()) depth = prev->getDepth();
    while (depth > 0 && prev->getFrameAtDepth(depth).id != current->getFrameAtDepth(depth).id)
      depth--;
    
    return depth;
  }
  
  void LocalTraceAnalyzer::analyze() {
    // Init and process first sample
    vector<TCTATraceNode*> activeStack;
    CallPathSample* current = reader.readNextSample();
    uint64_t numSamples = 0;
    if (current != NULL) {
      numSamples++;
      for (int i = 0; i <= current->getDepth(); i++) {
        CallPathFrame& frame = current->getFrameAtDepth(i);
        if (frame.type == CallPathFrame::Root) {
          TCTATraceNode* node = new TCTRootNode(frame.id, frame.name, activeStack.size());
          node->getTraceTime()->setStartTime(0, current->timestamp);
          activeStack.push_back(node);
        } 
        else if (frame.type == CallPathFrame::Loop) {
          TCTATraceNode* node = new TCTLoopTraceNode(frame.id, frame.name, activeStack.size(), 
                  binaryAnalyzer.findLoop(frame.vma));
          pushActiveStack(activeStack, node, 0, current->timestamp);
        }
        else {
          // frame.type == CallPathFrame::Func
          TCTATraceNode* node = new TCTFunctionTraceNode(frame.id, frame.name, activeStack.size(),
                  binaryAnalyzer.findFunc(frame.vma), frame.ra);
          pushActiveStack(activeStack, node, 0, current->timestamp);
        }
      }
    }
    
    CallPathSample* prev = current;
    current = reader.readNextSample();
    
    while (current != NULL) {
      numSamples++;
      int lcaDepth = computeLCADepth(prev, current);
      int prevDepth = prev->getDepth();
      
      while (prevDepth > lcaDepth) {
        prevDepth--;
        popActiveStack(activeStack, prev->timestamp, current->timestamp);
      }
      
      for (int i = lcaDepth + 1; i <= current->getDepth(); i++) {
        CallPathFrame& frame = current->getFrameAtDepth(i);
        if (frame.type == CallPathFrame::Loop) {
          TCTATraceNode* node = new TCTLoopTraceNode(frame.id, frame.name, activeStack.size(), 
                  binaryAnalyzer.findLoop(frame.vma));
          pushActiveStack(activeStack, node, prev->timestamp, current->timestamp);
        } else {
          // frame.type == CallPathFrame::Func
          TCTATraceNode* node = new TCTFunctionTraceNode(frame.id, frame.name, activeStack.size(),
                  binaryAnalyzer.findFunc(frame.vma), frame.ra);
          pushActiveStack(activeStack, node, prev->timestamp, current->timestamp);
        }
      }
      
      delete prev;
      prev = current;
      current = reader.readNextSample();
    }
    
    TCTATraceNode* root = activeStack[0];
    while (activeStack.size() > 0)
      popActiveStack(activeStack, prev->timestamp, prev->timestamp + 1);
    
    delete prev;
    
    printf("%s\n", root->toString(10, 0).c_str());
    delete root;
  }
}

