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

  class LocalTraceAnalyzerImpl {
  public:
    BinaryAnalyzer& binaryAnalyzer;
    TraceFileReader reader;
    
    vector<TCTANode*> activeStack;
    TCTRootNode* root;
    
    uint64_t numSamples;
    Time samplingInterval;
    
    LocalTraceAnalyzerImpl(BinaryAnalyzer& binaryAnalyzer, 
          CCTVisitor& cctVisitor, string traceFileName, Time minTime) : 
          binaryAnalyzer(binaryAnalyzer), reader(cctVisitor, traceFileName, minTime) {
      numSamples = 0;
      samplingInterval = 0;
    }

    ~LocalTraceAnalyzerImpl() {}
    
      
    void pushOneNode(TCTANode* node, Time startTimeExclusive, Time startTimeInclusive) {
      node->getTraceTime()->setStartTime(startTimeExclusive, startTimeInclusive);
      activeStack.push_back(node);
    }

    void popOneNode(Time endTimeInclusive, Time endTimeExclusive) {
      TCTANode* node = activeStack.back();
      node->getTraceTime()->setEndTime(endTimeInclusive, endTimeExclusive);
      activeStack.pop_back();

      if (activeStack.size() > 0) {
        if (node->type == TCTANode::Iter) {
          TCTLoopTraceNode* loop = (TCTLoopTraceNode*) activeStack.back();
          if (loop->hasPendingIteration() && !loop->isPendingIterationAccepted()) {
            loop->finalizePendingIteration();
            loop->pushPendingIteration((TCTIterationTraceNode*)node, false);
            loop->finalizePendingIteration();
            activeStack.pop_back();
            activeStack.push_back(TCTProfileNode::newProfileNode(loop));
            //printf("Loop rejected:\n%s", loop->toString(loop->getDepth()+2, 0, samplingInterval).c_str());
            //printf("Converted to:\n%s\n", activeStack.back()->toString(loop->getDepth()+2, 0, samplingInterval).c_str());
            delete loop;
            return;
          }
          if (loop->hasPendingIteration())
            loop->finalizePendingIteration();
          loop->pushPendingIteration((TCTIterationTraceNode*)node, acceptIteration((TCTIterationTraceNode*)node));
        }
        else
          activeStack.back()->addChild(node);
      }
    }

    void popActiveStack(Time endTimeInclusive, Time endTimeExclusive) {
      if (activeStack.back()->type == TCTANode::Iter) {
        // When need to pop an iteration, first pop iteration and then pop the loop containing this iteration.
        popOneNode(endTimeInclusive, endTimeExclusive);
      }
      popOneNode(endTimeInclusive, endTimeExclusive);
    }

    void pushActiveStack(TCTANode* node, Time startTimeExclusive, Time startTimeInclusive) {
      detectIteration(node, startTimeExclusive, startTimeInclusive);
      if (detectConflict(node)) return;

      if (node->type == TCTANode::Loop) {
        // for loops with valid cfgGraph
        if (node->cfgGraph != NULL) {
          // if the loop is not "split", add to stack
          pushOneNode(node, startTimeExclusive, startTimeInclusive);
          // create a iteration
          pushOneNode(new TCTIterationTraceNode(activeStack.back()->id.id, 0, activeStack.size(), activeStack.back()->cfgGraph),
                  startTimeExclusive, startTimeInclusive);
          return;
        } else {
          // for loops without valid cfgGraph, turn them into profile nodes.
          TCTProfileNode* profile = TCTProfileNode::newProfileNode(node);
          delete node;
          pushOneNode(profile, startTimeExclusive, startTimeInclusive);
          return;
        }
      }

      pushOneNode(node, startTimeExclusive, startTimeInclusive);
    }
    
    void detectIteration(TCTANode* node, Time startTimeExclusive, Time startTimeInclusive) {
      // Iteration Detection
      if (activeStack.back()->type == TCTANode::Iter) {
        TCTIterationTraceNode* iter = (TCTIterationTraceNode*) activeStack.back();
        CFGAGraph* cfg = iter->cfgGraph;
        int prevRA = 0;
        for (int i = iter->getNumChild()-1; i >= 0 && !cfg->hasChild(prevRA); i--)
          prevRA = iter->getChild(i)->ra;

        // when both prevRA and thisRA is child of cfg, and thisRA is not a successor of prevRA
        if (cfg->hasChild(prevRA) && cfg->hasChild(node->ra) && cfg->compareChild(prevRA, node->ra) != -1) {
          // pop the old iteration
          popOneNode(startTimeExclusive, startTimeInclusive);
          
          // push a new iteration when parent loop is still valid
          if (activeStack.back()->type == TCTANode::Loop) {
            int iterNum = ((TCTLoopTraceNode*)activeStack.back())->getNumIteration();
            pushOneNode(new TCTIterationTraceNode(activeStack.back()->id.id, iterNum, activeStack.size(), activeStack.back()->cfgGraph),
                  startTimeExclusive, startTimeInclusive);
          }
          // if the parent loop is converted to profile, do nothing
        }
      }
    }

    // Return true if conflict detected. False otherwise.
    // Conflict = a non-loop trace node where two children have the same id.
    bool detectConflict(TCTANode* node) {
      //if the parent is a non-loop trace node, detect conflict
      if (activeStack.back()->type != TCTANode::Loop && activeStack.back()->type != TCTANode::Prof) {
        TCTATraceNode* parent = (TCTATraceNode*)activeStack.back();
        for (int i = parent->getNumChild()-1; i >= 0; i--)
          if (parent->getChild(i)->id == node->id) {
            bool printError = (i != parent->getNumChild() - 1) && parent->name.find("<unknown procedure>") == string::npos;
            if (printError) printf("Conflict detected. Node %s has two occurrence of %s:\n", parent->id.toString().c_str(), node->id.toString().c_str());
            if (printError) printf("%s", parent->toString(parent->getDepth()+1, -LONG_MAX, 0).c_str());
            // if conflict is detected, replace node with the prior one, re-push it onto stack, 
            // and remove it from the parent (as it will be re-added later when popped from stack).
            delete node;
            node = parent->removeChild(i);
            pushOneNode(node, node->getTraceTime()->getStartTimeExclusive(), node->getTraceTime()->getStartTimeInclusive());

            // if node is a loop, push the latest iteration onto stack.
            if (node->type == TCTANode::Loop) {
              TCTLoopTraceNode* loop = (TCTLoopTraceNode*) node;
              TCTANode* iter = loop->removeChild(loop->getNumChild()-1);
              pushOneNode(iter, iter->getTraceTime()->getStartTimeExclusive(), iter->getTraceTime()->getStartTimeInclusive());
            }

            // remove parent's children starting from i
            while (parent->getNumChild() > i) {
              TCTANode* child = parent->removeChild(i);
              if (printError) printf("Deleting child %s:\n", child->id.toString().c_str());
              if (printError) printf("%s", child->toString(child->getDepth()+1, -LONG_MAX, 0).c_str());
              delete child;
            }

            if (printError) printf("\n");
            return true;
          }
      }
      return false;
    }

    bool acceptIteration(TCTIterationTraceNode* iter) {
      int countFunc = 0;
      int countLoop = 0;
      
      for (int k = 0; k < iter->getNumChild(); k++)
        if (iter->getChild(k)->getDuration() / samplingInterval 
                >= IterationChildDurationThreshold) {
          if (iter->getChild(k)->type == TCTANode::Func) countFunc++;
          else countLoop++; // TCTANode::Loop and TCTANode::Prof are all loops.
        }

      // When an iteration has no func child passing the IterationChildDurationThreshold,
      // and has less than two loop children passing the IterationChildDurationThreshold,
      // and the number of children doesn't pass the IterationNumChildThreshold,
      // children of this iteration may belong to distinct iterations in the execution.
      if (countFunc < IterationNumAccFuncThreshold 
              && countLoop < IterationNumAccLoopThreshold 
              && iter->getNumChild() < IterationNumChildThreshold)
        return false;
      else
        return true;
    }
    
    
    /*
    uint32_t loopACCcnt;
    uint32_t loopDECcnt;
    vector<TCTLoopTraceNode*> topDECloops;
    const static int maxNumDECloops = 10;
    
    bool acceptLoop(TCTLoopTraceNode* loop) {
      for (int i = 0; i < loop->getNumChild(); i++) {
        TCTIterationTraceNode* iter = (TCTIterationTraceNode*) loop->getChild(i);
        
        int countFunc = 0;
        int countLoop = 0;
        
        for (int k = 0; k < iter->getNumChild(); k++)
          if (iter->getChild(k)->getDuration() / samplingInterval 
                  >= IterationChildDurationThreshold) {
            if (iter->getChild(k)->type == TCTANode::Func) countFunc++;
            else countLoop++; // TCTANode::Loop and TCTANode::Prof are all loops.
          }
        
        // When an iteration has no func child passing the IterationChildDurationThreshold,
        // and has less than two loop children passing the IterationChildDurationThreshold,
        // and the number of children doesn't pass the IterationNumChildThreshold,
        // children of this iteration may belong to distinct iterations in the execution.
        if (countFunc < IterationNumAccFuncThreshold 
                && countLoop < IterationNumAccLoopThreshold 
                && iter->getNumChild() < IterationNumChildThreshold)
          return false;
      }
      return true;
    }
    
    void verifyIterationDetection(TCTANode* node) {
      if (node->type == TCTANode::Prof) {
        return;
      }
      else if (node->type == TCTANode::Loop) {
        TCTLoopTraceNode* loop = (TCTLoopTraceNode*) node;
        if (loop->getNumChild() == 1) {
          //printf("%s", loop->toString(loop->getDepth(), 0).c_str());
          verifyIterationDetection(loop->getChild(0));
          return;
        }

        if (acceptLoop(loop)) {
          loopACCcnt++;
          printf("\n%s", loop->toString(loop->getDepth()+2, 0, samplingInterval).c_str());

          for (int i = 0; i < loop->getNumChild(); i++)
            verifyIterationDetection(loop->getChild(i));
        }
        else {
          loopDECcnt++;
          
          size_t i = 0;
          for (; i < topDECloops.size(); i++) 
            if (loop->getDuration() > topDECloops[i]->getDuration()) {
              topDECloops.insert(topDECloops.begin() + i, loop);
              break;
            }
          
          if (i == topDECloops.size() && i < maxNumDECloops)
            topDECloops.push_back(loop);
          
          if (topDECloops.size() > maxNumDECloops)
            topDECloops.pop_back();
        }
      } 
      else {
        TCTATraceNode* trace = (TCTATraceNode*) node;
        //printf("%s", trace->toString(trace->getDepth(), 0).c_str());
        for (int i = 0; i < trace->getNumChild(); i++)
          verifyIterationDetection(trace->getChild(i));
      }
    }
    
    void finalizeIterationDetection() {
      loopACCcnt = 0;
      loopDECcnt = 0;
      
      printf("\nList of accepted loops:\n");
      verifyIterationDetection(root);
      
      printf("\nList of top %lu rejected loops:\n", topDECloops.size());
      for (size_t i = 0; i < topDECloops.size(); i++) {
          printf("\n%s", topDECloops[i]->toString(topDECloops[i]->getDepth()+2, 0, samplingInterval).c_str());
      }
      
      printf("\n Num of ACC loop = %u, Num of REJ loop = %u.\n", loopACCcnt, loopDECcnt);
    }
    */

    TCTANode* finalizeLoops(TCTANode* node) {
      if (node->type == TCTANode::Prof) {
        return NULL;
      }
      else if (node->type == TCTANode::Loop) {
        TCTLoopTraceNode* loop = (TCTLoopTraceNode*) node;
        if (loop->hasPendingIteration() && !loop->isPendingIterationAccepted()) {
          loop->finalizePendingIteration();
          TCTProfileNode* prof = TCTProfileNode::newProfileNode(loop);
          //printf("Loop rejected at last:\n%s", loop->toString(loop->getDepth()+2, 0, samplingInterval).c_str());
          //printf("Should be converted to:\n%s\n", prof->toString(loop->getDepth()+2, 0, samplingInterval).c_str());
          return prof;
        }
        if (loop->hasPendingIteration())
          loop->finalizePendingIteration();
        printf("Loop accepted:\n%s", loop->toString(loop->getDepth()+2, 0, samplingInterval).c_str());
        
        for (int i = 0; i < loop->getNumChild(); i++) {
          TCTANode* child = finalizeLoops(loop->getChild(i));
          if (child != NULL)
            loop->replaceChild(i, child);
        }
        return NULL;
      }
      else {
        TCTATraceNode* trace = (TCTATraceNode*) node;
        for (int i = 0; i < trace->getNumChild(); i++) {
          TCTANode* child = finalizeLoops(trace->getChild(i));
          if (child != NULL)
            trace->replaceChild(i, child);
        }
        return NULL;
      }
    }
    
    int computeLCADepth(CallPathSample* prev, CallPathSample* current) {
      int depth = prev->getDepth();
      int dLCA = current->getDLCA();
      if (dLCA != HPCRUN_FMT_DLCA_NULL) {
        while (depth > 0 && dLCA > 0) {
          if (prev->getFrameAtDepth(depth).type == CallPathFrame::Func)
            dLCA--;
          depth--;
        }
      }
      // when dLCA is no less than the number of call frames on call path,
      // dLCA must be wrong. Reset depth.
      string error = "";
      if (depth == 0) {
        depth = prev->getDepth();
        error = "dLCA larger than number of call frames.";
      }

      if (depth > current->getDepth()) depth = current->getDepth();
      while (depth > 0 && (prev->getFrameAtDepth(depth).id != current->getFrameAtDepth(depth).id ||
              prev->getFrameAtDepth(depth).procID != current->getFrameAtDepth(depth).procID))
        depth--;

      if (depth == 0) {
        error += "LCA Depth = 0";
      }
      
      if (error != "") {
        printf("%s\n", error.c_str());
        printf("Prev = ");
        for (int i = 0; i < prev->getDepth(); i++)
          printf(" %s(%u,%u), ", prev->getFrameAtDepth(i).name.c_str(), 
                  prev->getFrameAtDepth(i).id, prev->getFrameAtDepth(i).procID);
        printf(".\nCurr = ");
        for (int i = 0; i < current->getDepth(); i++)
          printf(" %s(%u,%u), ", current->getFrameAtDepth(i).name.c_str(),
                  current->getFrameAtDepth(i).id, current->getFrameAtDepth(i).procID);
        printf(" dLCA = %u.\n", current->getDLCA());
      }

      return depth;
    }
    
    bool filterFrame(const CallPathFrame& frame) {
      if (frame.name.length() >= 5 && frame.name.substr(0, 5) == "PMPI_")
        return true;
      if (frame.name.length() >= 4 && frame.name.substr(0, 4) == "MPI_")
        return true;
      return false;
    }
    
    void analyze() {
      // Init and process first sample
      CallPathSample* current = reader.readNextSample();
      Time startTime = current->timestamp;
      int currentDepth = 0;
      if (current != NULL) {
        numSamples++;
        for (currentDepth = 0; currentDepth <= current->getDepth(); currentDepth++) {
          CallPathFrame& frame = current->getFrameAtDepth(currentDepth);
          if (frame.type == CallPathFrame::Root) {
            root = new TCTRootNode(frame.id, frame.procID, frame.name, activeStack.size());
            pushOneNode(root, 0, current->timestamp);
          } 
          else if (frame.type == CallPathFrame::Loop) {
            TCTANode* node = new TCTLoopTraceNode(frame.id, frame.name, activeStack.size(), 
                    binaryAnalyzer.findLoop(frame.vma));
            pushActiveStack(node, 0, current->timestamp);
          }
          else {
            // frame.type == CallPathFrame::Func
            TCTANode* node = new TCTFunctionTraceNode(frame.id, frame.procID, frame.name, activeStack.size(),
                    binaryAnalyzer.findFunc(frame.vma), frame.ra);
            pushActiveStack(node, 0, current->timestamp);
          }
          
          if (filterFrame(frame)) break;
        }
      }

      // Process later samples
      CallPathSample* prev = current;
      int prevDepth = currentDepth;
      current = reader.readNextSample();
      while (current != NULL) {
        numSamples++;
        samplingInterval = (current->timestamp - startTime) / (numSamples - 1);
        int lcaDepth = computeLCADepth(prev, current);
        if (prevDepth > prev->getDepth()) prevDepth = prev->getDepth();
        
        while (prevDepth > lcaDepth) {
          prevDepth--;
          popActiveStack(prev->timestamp, current->timestamp);
        }
        
        if (prevDepth < lcaDepth)
          lcaDepth = prevDepth;
        
        currentDepth = lcaDepth;
        if (!filterFrame(current->getFrameAtDepth(lcaDepth))) {
          for (currentDepth = lcaDepth + 1; currentDepth <= current->getDepth(); currentDepth++) {
            CallPathFrame& frame = current->getFrameAtDepth(currentDepth);
            if (frame.type == CallPathFrame::Loop) {
              TCTANode* node = new TCTLoopTraceNode(frame.id, frame.name, activeStack.size(), 
                      binaryAnalyzer.findLoop(frame.vma));
              pushActiveStack(node, prev->timestamp, current->timestamp);
            } else {
              // frame.type == CallPathFrame::Func
              TCTANode* node = new TCTFunctionTraceNode(frame.id, frame.procID, frame.name, activeStack.size(),
                      binaryAnalyzer.findFunc(frame.vma), frame.ra);
              pushActiveStack(node, prev->timestamp, current->timestamp);
            }
            
            if (filterFrame(frame)) break;
          }
        }

        delete prev;
        prev = current;
        prevDepth = currentDepth;
        current = reader.readNextSample();
      }

      // Pop every thing in active stack.
      while (activeStack.size() > 0)
        popActiveStack(prev->timestamp, prev->timestamp + 1);
      
      delete prev;

      printf("\nNum Samples = %lu, Sampling interval = %ld\n\n", numSamples, samplingInterval);

      finalizeLoops(root);

      printf("\n\n\n\n%s", root->toString(10, samplingInterval * IterationChildDurationThreshold, samplingInterval).c_str());

      delete root;
    }
  };

  
  LocalTraceAnalyzer::LocalTraceAnalyzer(BinaryAnalyzer& binaryAnalyzer, 
          CCTVisitor& cctVisitor, string traceFileName, Time minTime) {
    ptr = new LocalTraceAnalyzerImpl(binaryAnalyzer, cctVisitor,
            traceFileName, minTime);
  }

  LocalTraceAnalyzer::~LocalTraceAnalyzer() {
    delete (LocalTraceAnalyzerImpl*) ptr;
  }
  
  void LocalTraceAnalyzer::analyze() {
    ((LocalTraceAnalyzerImpl*)ptr)->analyze();
  }
}

