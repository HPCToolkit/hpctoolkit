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

#include "LocalTraceAnalyzer.hpp"

#include <sys/time.h>
#include <dirent.h>
#include <stdio.h>

#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/support/FileUtil.hpp>

#include "BinaryAnalyzer.hpp"
#include "CCTVisitor.hpp"
#include "TraceFileReader.hpp"

#ifdef KEEP_ACCEPTED_ITERATION 
#include "DifferenceQuantifier.hpp"
#endif

#include "data/TCT-CFG.hpp"
#include "data/TCT-Node.hpp"
#include "data/TCT-Semantic-Label.hpp"

namespace TraceAnalysis {

  class TraceFileAnalyzerImpl {
  public:
    const BinaryAnalyzer& binaryAnalyzer;
    
    TraceFileReader reader;
    
    vector<TCTANode*> activeStack;
    TCTRootNode* root;
    
    uint64_t numSamples;
    
    unordered_map<int, uint64_t> nodeStartSample;
    unordered_map<int, uint64_t> iterStartSample;
    
    TraceFileAnalyzerImpl(const BinaryAnalyzer& binaryAnalyzer, CCTVisitor& cctVisitor, string traceFileName, Time minTime) : 
          binaryAnalyzer(binaryAnalyzer),
          reader(cctVisitor, traceFileName, minTime),
          numSamples(0) {}
    ~TraceFileAnalyzerImpl() {}
    
      
    void pushOneNode(TCTANode* node, Time startTimeExclusive, Time startTimeInclusive, uint64_t startSample) {
      if (node->type == TCTANode::Iter) iterStartSample[node->id.id] = startSample;
      else nodeStartSample[node->id.id] = startSample;
      node->getTime().setStartTime(startTimeExclusive, startTimeInclusive);
      activeStack.push_back(node);
    }

    void popOneNode(Time endTimeInclusive, Time endTimeExclusive, uint64_t endSample) {
      TCTANode* node = activeStack.back();
      uint64_t startSample = 0;
      if (node->type == TCTANode::Iter) startSample = iterStartSample[node->id.id];
      else startSample = nodeStartSample[node->id.id];
      node->getTime().setNumSamples(endSample - startSample);
      node->getTime().setEndTime(endTimeInclusive, endTimeExclusive);
      
      node->completeNodeInit();
      
      activeStack.pop_back();
      if (activeStack.size() > 0) activeStack.back()->addChild(node);
    }

    void popActiveStack(Time endTimeInclusive, Time endTimeExclusive) {
      if (activeStack.back()->type == TCTANode::Iter) {
        // When need to pop an iteration, first pop iteration and then pop the loop containing this iteration.
        popOneNode(endTimeInclusive, endTimeExclusive, numSamples);
      }
      popOneNode(endTimeInclusive, endTimeExclusive, numSamples);
    }

    void pushActiveStack(TCTANode* node, Time startTimeExclusive, Time startTimeInclusive) {
      if (detectConflict(node)) return;
      detectIteration(node, startTimeExclusive, startTimeInclusive);

      if (node->type == TCTANode::Loop) {
        // for loops with valid cfgGraph
        if (node->cfgGraph != NULL) {
          // if the loop is not "split", add to stack
          pushOneNode(node, startTimeExclusive, startTimeInclusive, numSamples);
          // create a iteration
          pushOneNode(new TCTIterationTraceNode(activeStack.back()->id.id, activeStack.size(), activeStack.back()->cfgGraph, activeStack.back()->getOriginalSemanticLabel()),
                  startTimeExclusive, startTimeInclusive, numSamples);
          return;
        } else {
          // for loops without valid cfgGraph, turn them into profile nodes.
          TCTNonCFGProfileNode* profile = TCTNonCFGProfileNode::newNonCFGProfileNode(node);
          delete node;
          pushOneNode(profile, startTimeExclusive, startTimeInclusive, numSamples);
          return;
        }
      }

      pushOneNode(node, startTimeExclusive, startTimeInclusive, numSamples);
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
          popOneNode(startTimeExclusive, startTimeInclusive, numSamples);
          
          // push the new iteration
          pushOneNode(new TCTIterationTraceNode(activeStack.back()->id.id, activeStack.size(), activeStack.back()->cfgGraph, activeStack.back()->getOriginalSemanticLabel()),
                startTimeExclusive, startTimeInclusive, numSamples);
        }
      }
    }

    // This function handles noises from sampling and loop augmentation. 
    // Conflict for a non-loop node = when the node has multiple occurrence of the same child.
    // Conflict for a loop node = when loop augmentation is wrong and a nested loop is split.
    // Return true if conflict detected. False otherwise.
    bool detectConflict(TCTANode* node) {
      if (activeStack.back()->type == TCTANode::Loop) 
        print_msg(MSG_PRIO_HIGH, "ERROR: top of activeStack is a loop node when detectConflict() is called.\n");
      // if the parent is an iteration node, detect conflict on nested loop
      else if (activeStack.back()->type == TCTANode::Iter) {
        if (node->type == TCTANode::Loop) {
          TCTIterationTraceNode* parent = (TCTIterationTraceNode*)activeStack.back();
          // when a nested loop is split
          if (parent->getNumChild() > 0 && parent->getChild(parent->getNumChild()-1)->id.id == node->id.id) {
            // Push the nested loop on to active stack.
            TCTANode* loop = parent->removeChild(parent->getNumChild()-1);
            pushOneNode(loop, loop->getTime().getStartTimeExclusive(), loop->getTime().getStartTimeInclusive(), nodeStartSample[loop->id.id]);
            // If the loop wasn't turned into a profile node, also push the pending iteration on to active stack.
            if (loop->type == TCTANode::Loop) {
              TCTANode* iter = ((TCTLoopNode*)loop)->popLastChild();
              pushOneNode(iter, iter->getTime().getStartTimeExclusive(), iter->getTime().getStartTimeInclusive(), iterStartSample[iter->id.id]);
            }
            
            delete node;
            return true;
          }
        }
      }
      // if the parent is a non-loop trace node, detect conflict on all child nodes
      else if (activeStack.back()->type != TCTANode::NonCFGProf) {
        TCTACFGNode* parent = (TCTACFGNode*)activeStack.back();
        for (int i = parent->getNumChild()-1; i >= 0; i--)
          if (parent->getChild(i)->id.id == node->id.id) {
            if (parent->getChild(i)->id.procID == node->id.procID // duplicate occurrence
                || parent->getChild(i)->getNumSamples() > ITER_CHILD_DUR_ACC // tail call but duration of the function who makes the tail call is significant
                ) { 
              bool printError = false; //(i != parent->getNumChild() - 1) && parent->getName().find("<unknown procedure>") == string::npos;
              if (printError) print_msg(MSG_PRIO_NORMAL, "ERROR: Conflict detected. Node %s has two occurrence of %s:\n", parent->id.toString().c_str(), node->id.toString().c_str());
              if (printError) print_msg(MSG_PRIO_NORMAL, "%s", parent->toString(parent->getDepth()+1, -LONG_MAX, 0).c_str());

              // if conflict is detected, remove parent's children starting after i
              while (parent->getNumChild() > i+1) {
                TCTANode* child = parent->removeChild(parent->getNumChild()-1);
                if (printError) print_msg(MSG_PRIO_NORMAL, "Deleting child %s:\n", child->id.toString().c_str());
                if (printError) print_msg(MSG_PRIO_NORMAL, "%s", child->toString(child->getDepth()+1, -LONG_MAX, 0).c_str());
                delete child;
              }

              // replace node with the i-th child, re-push it onto stack, 
              // and remove it from the parent (as it will be re-added later when popped from stack).
              delete node;
              node = parent->removeChild(i);
              pushOneNode(node, node->getTime().getStartTimeExclusive(), node->getTime().getStartTimeInclusive(), nodeStartSample[node->id.id]);

              // if node is a loop, push the latest iteration onto stack.
              if (node->type == TCTANode::Loop) {
                TCTLoopNode* loop = (TCTLoopNode*) node;
                TCTANode* iter = loop->popLastChild();
                pushOneNode(iter, iter->getTime().getStartTimeExclusive(), iter->getTime().getStartTimeInclusive(), iterStartSample[iter->id.id]);
              }

              if (printError) print_msg(MSG_PRIO_NORMAL, "\n");
              return true;
            }
            else { // tail call but duration of the function who makes the tail call is NOT significant
              print_msg(MSG_PRIO_NORMAL, "WARNING: tail call detected. Under parent %s%s, %s%s is a tail call of %s%s:\n", 
                  parent->getName().c_str(), parent->id.toString().c_str(), 
                  node->getName().c_str(), node->id.toString().c_str(), 
                  parent->getChild(i)->getName().c_str(), parent->getChild(i)->id.toString().c_str()
                  );
              
              // In case of a tail call, we don't expect to delete any extra child of the parent node.
              while (parent->getNumChild() > i+1) {
                TCTANode* child = parent->removeChild(parent->getNumChild()-1);
                print_msg(MSG_PRIO_MAX, "ERROR: deleting unrelated child %s for tail call:\n", child->id.toString().c_str());
                print_msg(MSG_PRIO_MAX, "%s", child->toString(child->getDepth()+1, -LONG_MAX, 0).c_str());
                delete child;
              }
              
              TCTANode* child = parent->removeChild(i);
              print_msg(MSG_PRIO_NORMAL, "WARNING: deleting child %s for tail call:\n", child->id.toString().c_str());
              print_msg(MSG_PRIO_NORMAL, "%s", child->toString(child->getDepth()+1, -LONG_MAX, 0).c_str());
              
              pushOneNode(node, child->getTime().getStartTimeExclusive(), child->getTime().getStartTimeInclusive(), nodeStartSample[node->id.id]);
              delete child;
              
              return true;
            }
          }
      }
      return false;
    }

#ifdef KEEP_ACCEPTED_ITERATION      
    void printLoops(TCTANode* node) {
      if (node->type == TCTANode::NonCFGProf) return;
      else if (node->type == TCTANode::Loop) {
        TCTLoopNode* loop = (TCTLoopNode*) node;
        
        for (int i = 0; i < loop->getNumAcceptedIteration(); i++)
          printLoops(loop->getAcceptedIteration(i));
        
        double ratio = (double)loop->getDuration() * 100.0 / (double) root->getDuration();
        if (loop->accept()) {
          if (ratio >= 1 && loop->getNumIteration() > 1) {
            print_msg(MSG_PRIO_NORMAL, "\nLoop accepted: duration = %lf%%\n%s", ratio, loop->toString(loop->getDepth()+5, 0).c_str());
            print_msg(MSG_PRIO_LOW, "Diff among iterations:\n");
            TCTANode* highlight = NULL;
            for (int i = 0; i < loop->getNumAcceptedIteration(); i++) {
              for (int j = 0; j < loop->getNumAcceptedIteration(); j++) {
                TCTANode* diffNode = diffQ.mergeNode(loop->getAcceptedIteration(i), 1, loop->getAcceptedIteration(j), 1, false, true);
                double diffRatio = 100.0 * diffNode->getDiffScore().getInclusive() / 
                          (loop->getAcceptedIteration(i)->getDuration() + loop->getAcceptedIteration(j)->getDuration());
                delete diffNode;
                print_msg(MSG_PRIO_LOW, "%.2lf%%\t", diffRatio);
              }
              print_msg(MSG_PRIO_LOW, "\n");
            }
            highlight = diffQ.mergeNode(loop->getAcceptedIteration(0), 1, loop->getAcceptedIteration(loop->getNumAcceptedIteration()-1), 1, false, false);
            print_msg(MSG_PRIO_LOW, "Compare:\n%s%s", loop->getAcceptedIteration(0)->toString(highlight->getDepth()+5, 0).c_str()
                    , loop->getAcceptedIteration(loop->getNumAcceptedIteration()-1)->toString(highlight->getDepth()+5, 0).c_str());
            print_msg(MSG_PRIO_LOW, "Highlighted diff node:\n%s\n", highlight->toString(highlight->getDepth()+5, 0).c_str());
            delete highlight;
            print_msg(MSG_PRIO_LOW, "\n");
          }
        } else
          print_msg(MSG_PRIO_HIGH, "ERROR: unaccepted loop encountered in printLoops()\n");
      }
      else {
        TCTATraceNode* trace = (TCTATraceNode*) node;
        for (int i = 0; i < trace->getNumChild(); i++)
          printLoops(trace->getChild(i));
      }
    }
#endif    
    
    // Compute LCA Depth of the previous and current sample.
    int computeLCADepth(CallPathSample* prev, CallPathSample* current) {
      int depth = prev->getDepth();
      int dLCA = current->getDLCA(); // distance to LCA
      if (dLCA != HPCTRACE_FMT_DLCA_NULL) {
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
        print_msg(MSG_PRIO_HIGH, "ERROR: %s\n", error.c_str());
        print_msg(MSG_PRIO_HIGH, "Prev = ");
        for (int i = 0; i < prev->getDepth(); i++)
          print_msg(MSG_PRIO_HIGH, " %s(%u,%u), ", prev->getFrameAtDepth(i).name.c_str(), 
                  prev->getFrameAtDepth(i).id, prev->getFrameAtDepth(i).procID);
        print_msg(MSG_PRIO_HIGH, ".\nCurr = ");
        for (int i = 0; i < current->getDepth(); i++)
          print_msg(MSG_PRIO_HIGH, " %s(%u,%u), ", current->getFrameAtDepth(i).name.c_str(),
                  current->getFrameAtDepth(i).id, current->getFrameAtDepth(i).procID);
        print_msg(MSG_PRIO_HIGH, " dLCA = %u.\n", current->getDLCA());
      }

      return depth;
    }
    
    TCTRootNode* analyze() {
      // Init and process first sample
      CallPathSample* current = reader.readNextSample();
      int currentDepth = 0;
      if (current != NULL) {
        uint sementicLabel = SEMANTIC_LABEL_COMPUTATION;
        for (currentDepth = 0; currentDepth <= current->getDepth(); currentDepth++) {
          CallPathFrame& frame = current->getFrameAtDepth(currentDepth);
          if (frame.type == CallPathFrame::Root) {
            root = new TCTRootNode(frame.id, frame.procID, frame.name, activeStack.size());
            pushOneNode(root, current->timestamp-1, current->timestamp, numSamples);
          } 
          else if (frame.type == CallPathFrame::Loop) {
            TCTANode* node = new TCTLoopNode(frame.id, frame.name, activeStack.size(), 
                    binaryAnalyzer.findLoop(frame.vma), sementicLabel);
            pushActiveStack(node, current->timestamp-1, current->timestamp);
          }
          else {
            // frame.type == CallPathFrame::Func
            FUNC_SEMANTIC_INFO info = getFuncSemanticInfo(frame.name);
            sementicLabel |= info.semantic_label;
            TCTANode* node = new TCTFunctionTraceNode(frame.id, frame.procID, frame.name, activeStack.size(),
                    binaryAnalyzer.findFunc(frame.vma), frame.ra, sementicLabel);
            pushActiveStack(node, current->timestamp-1, current->timestamp);
            // if semantic label of a function indicates that its children should be ignored in trace analysis, break.
            if (info.ignore_child) break;
          }
        }
      }

      // Process later samples
      CallPathSample* prev = current;
      int prevDepth = currentDepth;
      current = reader.readNextSample();
      while (current != NULL) {
        numSamples++;
        int lcaDepth = computeLCADepth(prev, current);
        if (prevDepth > prev->getDepth()) prevDepth = prev->getDepth();
        
        // pop all frames below LCA in the previous sample
        while (prevDepth > lcaDepth) {
          prevDepth--;
          popActiveStack(prev->timestamp, current->timestamp);
        }
        
        if (prevDepth < lcaDepth) // when prevDepth is smaller than lcaDepth due to ignored children frames
          lcaDepth = prevDepth; // reset lcaDepth
        
        currentDepth = lcaDepth;
        if (!getFuncSemanticInfo(current->getFrameAtDepth(lcaDepth).name).ignore_child) {
          uint sementicLabel = activeStack.back()->getOriginalSemanticLabel();
          for (currentDepth = lcaDepth + 1; currentDepth <= current->getDepth(); currentDepth++) {
            CallPathFrame& frame = current->getFrameAtDepth(currentDepth);
            if (frame.type == CallPathFrame::Loop) {
              TCTANode* node = new TCTLoopNode(frame.id, frame.name, activeStack.size(), 
                      binaryAnalyzer.findLoop(frame.vma), sementicLabel);
              pushActiveStack(node, prev->timestamp, current->timestamp);
            } else {
              // frame.type == CallPathFrame::Func
              FUNC_SEMANTIC_INFO info = getFuncSemanticInfo(frame.name);
              sementicLabel |= info.semantic_label;
              TCTANode* node = new TCTFunctionTraceNode(frame.id, frame.procID, frame.name, activeStack.size(),
                      binaryAnalyzer.findFunc(frame.vma), frame.ra, sementicLabel);
              pushActiveStack(node, prev->timestamp, current->timestamp);
              // if semantic label of a function indicates that its children should be ignored in trace analysis, break.
              if (info.ignore_child) break;
            }
          }
        }

        delete prev;
        prev = current;
        prevDepth = currentDepth;
        current = reader.readNextSample();
      }

      Time samplePeriod = (prev->timestamp - root->getTime().getStartTimeExclusive()) / numSamples;
      
      // Pop every thing in active stack.
      numSamples++;
      while (activeStack.size() > 0)
        popActiveStack(prev->timestamp, prev->timestamp + samplePeriod);    
      delete prev;

      root->finalizeEnclosingLoops();
      root->completeThreadTCT();
      //printLoops(root);

      return root;
    }
  };

  int hpctraceFileFilter(const struct dirent* entry)
  {
    static const string ext = string(".") + HPCRUN_TraceFnmSfx;
    static const uint extLen = ext.length();

    uint nmLen = strlen(entry->d_name);
    if (nmLen > extLen) {
      int cmpbeg = (nmLen - extLen);
      return (strncmp(&entry->d_name[cmpbeg], ext.c_str(), extLen) == 0);
    }
    return 0;
  }
  
  // Return true if it is a worker thread.
  bool workerThreadFilter(string filename) {
    if (filename.find("-000-") == string::npos)
      return true;
    else
      return false;
  }
  
  class LocalTraceAnalyzerImpl {
  public:
    LocalTraceAnalyzerImpl(Prof::CallPath::Profile* prof, string dbDir, const BinaryAnalyzer& binaryAnalyzer, 
        int myRank, int numRanks) : binaryAnalyzer(binaryAnalyzer), cctVisitor(prof->cct()) {
      // Get a list of trace files.
      string path = dbDir;
      if (FileUtil::isDir(path.c_str())) {
        // ensure 'path' ends in '/'
        if (path[path.length() - 1] != '/') {
          path += "/";
        }

        struct dirent** dirEntries = NULL;
        int dirEntriesSz = scandir(path.c_str(), &dirEntries,
                                   hpctraceFileFilter, alphasort);
        if (dirEntriesSz > 0) {
          for (int i = 0; i < dirEntriesSz; ++i) {
            if (!workerThreadFilter(dirEntries[i]->d_name))
              traceFiles.push_back(path + dirEntries[i]->d_name);
            free(dirEntries[i]);
          }
          free(dirEntries);
        }
      }
      
      traceMinTime = prof->traceMinTime();
      
      // Calculate begIdx and endIdx for this rank.
      if (numRanks > (int)traceFiles.size())
        numRanks = traceFiles.size();
      if (myRank >= numRanks) {
        begIdx = -1;
        endIdx = -1;
      } else {
        begIdx = traceFiles.size() * myRank / numRanks;
        endIdx = traceFiles.size() * (myRank + 1) / numRanks;
      }
    }
    virtual ~LocalTraceAnalyzerImpl() {}
    
    TCTClusterNode* analyze(vector<TCTRootNode*>& rootNodes) {
      TCTClusterNode* rootCluster = NULL;
      for (int i = begIdx; i < endIdx; i++) {
        print_msg(MSG_PRIO_MAX, "Analyzing file #%d = %s.\n", i, traceFiles[i].c_str());
        TraceFileAnalyzerImpl analyzer(binaryAnalyzer, cctVisitor, traceFiles[i], traceMinTime);
        TCTRootNode* root = analyzer.analyze();
        root->setRootID(i);
        rootNodes.push_back((TCTRootNode*)root->duplicate());
        print_msg(MSG_PRIO_LOW, "\n\n\n\n%s", root->toString(10, 0, 0).c_str());

        if (rootCluster == NULL) {
          rootCluster = new TCTClusterNode(*root);
          rootCluster->setName("All Roots");
        }
        rootCluster->addChild(root, i);
      }

      print_msg(MSG_PRIO_LOW, "\n\n\nRoot Cluster:\n%s", rootCluster->toString(10, 4000, 0).c_str());

      return rootCluster;
    }
    
    bool adjustClockDiff(const vector<Time>& clockDiff) {
      if ((int)clockDiff.size() != (endIdx - begIdx)) {
        print_msg(MSG_PRIO_MAX, "ERROR: size of clockDiff[] doesn't match the number of trace files.\n");
        return false;
      }
        
      for (int i = begIdx; i < endIdx; i++) 
        if (clockDiff[i-begIdx] == CLOCK_SYNC_FAILED)
          print_msg(MSG_PRIO_MAX, "Clock sync for trace file #%d failed.\n", i, clockDiff[i-begIdx]);
        else if (clockDiff[i-begIdx] != 0) {
          TraceFileRewriter rewriter(traceFiles[i]);
          if (!rewriter.rewriteTimestamps(clockDiff[i-begIdx])) {
            print_msg(MSG_PRIO_MAX, "ERROR: rewrite trace file #%d failed.\n", i);
            return false;
          }
          print_msg(MSG_PRIO_LOW, "Clock sync for trace file #%d successful, offset = %ld.\n", i, clockDiff[i-begIdx]);
        }
      
      return true;
    }
    
  private:
    const BinaryAnalyzer& binaryAnalyzer;
    CCTVisitor cctVisitor;
    Time traceMinTime;
    vector<string> traceFiles;
    int begIdx;
    int endIdx;
  };
  
  LocalTraceAnalyzer::LocalTraceAnalyzer(Prof::CallPath::Profile* prof, string dbDir, const BinaryAnalyzer& binaryAnalyzer, 
      int myRank, int numRanks) {
    ptr = new LocalTraceAnalyzerImpl(prof, dbDir, binaryAnalyzer, myRank, numRanks);
  }

  LocalTraceAnalyzer::~LocalTraceAnalyzer() {
    delete (LocalTraceAnalyzerImpl*)ptr;
  }
  
  TCTClusterNode* LocalTraceAnalyzer::analyze(vector<TCTRootNode*>& rootNodes) {
    return ((LocalTraceAnalyzerImpl*)ptr)->analyze(rootNodes);
  }
  
  bool LocalTraceAnalyzer::adjustClockDiff(const vector<Time>& clockDiff) {
    return ((LocalTraceAnalyzerImpl*)ptr)->adjustClockDiff(clockDiff);
  }
}

