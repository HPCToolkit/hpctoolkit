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
 * File:   TCT-Node.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 4, 2018, 11:22 PM
 * 
 * Temporal Context Tree nodes.
 */

#ifndef TCT_NODE_HPP
#define TCT_NODE_HPP

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <set>
using std::set;

#include "../TraceAnalysisCommon.hpp"
#include "TCT-CFG.hpp"
#include "TCT-Time.hpp"

namespace TraceAnalysis {
  // Forward declarations
  class TCTANode;
  class TCTATraceNode;
  class TCTFunctionTraceNode;
  class TCTIterationTraceNode;
  class TCTLoopNode;
  class TCTProfileNode;
  class TCTRootNode;
  
  class TCTID {
  public:
    const int id; // Calling Context Tree ID for call sites and loops.
    const int procID; // Proc ID for call sites.
    
    TCTID(int id, int procID) : id(id), procID(procID) {}
    TCTID(const TCTID& other) : id(other.id), procID(other.procID) {}
    virtual ~TCTID() {}
    
    bool operator==(const TCTID& other) const {
      return (id == other.id) && (procID == other.procID);
    }
    
    bool operator< (const TCTID& other) const {
      if (id < other.id) return true;
      if (id == other.id && procID < other.procID) return true;
      return false;
    }
    
    string toString() const {
      return "(" + std::to_string(id) + "," + std::to_string(procID) + ")";
    }
  };
  
  class TCTDiffScore {
    friend class TCTANode;
  public:
    double getInclusive() const {
      return inclusive;
    }
    
    double getExclusive() const {
      return exclusive;
    }
    
    void setScores(double inclusive, double exclusive) {
      this->inclusive = inclusive;
      this->exclusive = exclusive;
    }
    
    void clear() {
      inclusive = 0;
      exclusive = 0;
    }
    
  private:
    double inclusive;
    double exclusive;
    
    TCTDiffScore() : inclusive(0), exclusive(0) {}
    TCTDiffScore(const TCTDiffScore& other): inclusive(other.inclusive), exclusive(other.exclusive) {}
    virtual ~TCTDiffScore() {}
  };
  
  // Records performance loss metrics
  class TCTPerfLossMetric {
    friend class TCTANode;
  public:
    void initDurationMetric();
    void setDuratonMetric(const TCTPerfLossMetric& rep1, const TCTPerfLossMetric& rep2);
    
  private:
    TCTPerfLossMetric(TCTANode* node) : node(node) {}
    virtual ~TCTPerfLossMetric() {}
    
    TCTANode* const node; // the TCTANode
    
    // Metrics that records durations of all instances represented by this TCTANode
    Time minDuration;
    Time maxDuration;
    double totalDuration;
  };
  
  // Temporal Context Tree Abstract Node
  class TCTANode {
  public:
    enum NodeType {
      Root,
      Func,
      Iter,
      Loop,
      Prof
    };
    
    TCTANode(NodeType type, int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
        type(type), id(id, procID), cfgGraph(cfgGraph), ra(ra), name(name), 
        depth(depth), weight(1), time(), diffScore(), plm(this) {}
    TCTANode(const TCTANode& orig) : type(orig.type), id(orig.id), cfgGraph(orig.cfgGraph), 
        ra(orig.ra), name(orig.name), depth(orig.depth), weight(orig.weight), 
        time(orig.time), diffScore(orig.diffScore), plm(this) {}
    TCTANode(const TCTANode& orig, NodeType type) : type(type), id(orig.id), cfgGraph(orig.cfgGraph), 
        ra(orig.ra), name(orig.name), depth(orig.depth), weight(orig.weight),
        time(orig.time), diffScore(orig.diffScore), plm(this) {}
    
    virtual ~TCTANode() {}
    
    virtual TCTTime& getTime() {
      return time;
    }
    
    virtual const TCTTime& getTime() const {
      return time;
    }
    
    virtual string getName() const {
      return name;
    }
    
    virtual int getDepth() const {
      return depth;
    }
    
    virtual Time getDuration() const {
      return time.getDuration();
    }
    
    virtual Time getMinDuration() const {
      return time.getMinDuration();
    }
        
    virtual Time getMaxDuration() const {
      return time.getMaxDuration();
    }
    
    virtual int getWeight() const {
      return weight;
    }
    
    virtual void setWeight(int weight) {
      this->weight = weight;
    }
    
    virtual TCTDiffScore& getDiffScore() {
      return diffScore;
    }
    
    virtual const TCTDiffScore& getDiffScore() const {
      return diffScore;
    }
    
    virtual TCTPerfLossMetric& getPerfLossMetric() {
      return plm;
    }
    
    virtual const TCTPerfLossMetric& getPerfLossMetric() const {
      return plm;
    }
    
    // returns a pointer to a duplicate of this object. 
    // Caller responsible for deallocating the duplicate.
    virtual TCTANode* duplicate() const = 0;
    
    // returns a pointer to a void duplicate of this object. 
    // Caller responsible for deallocating the void duplicate.
    virtual TCTANode* voidDuplicate() const = 0;
    
    // Print contents of an object to a string for debugging purpose.
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval) const;
    
    // Add child to this node. Deallocation responsibility is transfered to this node.
    // Child could be deallocated inside the call.
    virtual void addChild(TCTANode* child) = 0;
    
    const NodeType type;
    const TCTID id;
    // CFG Abstract Graph for this node.
    CFGAGraph* const cfgGraph; 
    // RA for call sites or VMA for loops.
    const VMA ra; 
    
  protected:
    string name;
    int depth;
    int weight;
    TCTTime time;
    TCTDiffScore diffScore;
    TCTPerfLossMetric plm;
  };
  
  // Temporal Context Tree Abstract Trace Node
  class TCTATraceNode : public TCTANode {
    friend class TCTProfileNode;
  public:
    TCTATraceNode(NodeType type, int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
      TCTANode(type, id, procID, name, depth, cfgGraph, ra) {}
    TCTATraceNode(const TCTATraceNode& orig) : TCTANode(orig) {
      for (auto it = orig.children.begin(); it != orig.children.end(); it++)
        children.push_back((*it)->duplicate());
    }
    virtual ~TCTATraceNode() {
      for (auto it = children.begin(); it != children.end(); it++)
        delete (*it);
    }
    
    virtual int getNumChild() const {
      return children.size();
    }
    
    virtual TCTANode* getChild(int idx) {
      return children[idx];
    }
    
    virtual const TCTANode* getChild(int idx) const {
      return children[idx];
    }
    
    virtual void addChild(TCTANode* child) {
      children.push_back(child);
    }
    
    // Replace a child. The old child will be deallocated.
    virtual void replaceChild(int idx, TCTANode* child) {
      delete children[idx];
      children[idx] = child;
    }
    
    virtual TCTANode* removeChild(int idx) {
      TCTANode* ret = getChild(idx);
      children.erase(children.begin() + idx);
      return ret;
    }
    
    // Return the end time of child #(idx-1). 
    // When idx = 0, return the start time of the node itself.
    virtual void getLastChildEndTime(int idx, Time& inclusive, Time& exclusive) const;
    
    // Return the start time of child #(idx). 
    // When idx = getNumChild(), return the end time of the node itself.
    virtual void getCurrChildStartTime(int idx, Time& exclusive, Time& inclusive) const;
    
    // Return the gap between child #(idx-1) and #(idx).
    // When idx = 0, return the gap before child #0.
    // When idx = getNumChild(), return the gap after child #(getNumChild()-1).
    virtual void getGapBeforeChild(int idx, Time& minGap, Time& maxGap) const;
    
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval) const;
    
  protected:
    vector<TCTANode*> children;
  };
  
  // Temporal Context Tree Function Trace Node
  class TCTFunctionTraceNode : public TCTATraceNode {
  public:
    TCTFunctionTraceNode(int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
      TCTATraceNode(Func, id, procID, name, depth, cfgGraph, ra) {}
    TCTFunctionTraceNode(const TCTFunctionTraceNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTFunctionTraceNode() {}
    
    virtual TCTANode* duplicate() const {
      return new TCTFunctionTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTFunctionTraceNode(id.id, id.procID, name, depth, cfgGraph, ra);
    }
  };
  
  // Temporal Context Tree Root Node
  class TCTRootNode : public TCTATraceNode {
  public:
    TCTRootNode(int id, int procID, string name, int depth) : 
            TCTATraceNode(Root, id, procID, name, depth, NULL, 0) {}
    TCTRootNode(const TCTRootNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTRootNode() {}
    
    virtual TCTANode* duplicate() const {
      return new TCTRootNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTRootNode(id.id, id.procID, name, depth);
    }
  };
  
  // Temporal Context Tree Iteration Trace Node
  class TCTIterationTraceNode : public TCTATraceNode {
  public:
    TCTIterationTraceNode(int id, uint64_t iterNum, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Iter, id, 0, "ITER_#" + std::to_string(iterNum), depth, 
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
    TCTIterationTraceNode(int id, string name, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Iter, id, 0, name, depth, 
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
    TCTIterationTraceNode(const TCTIterationTraceNode& orig) : TCTATraceNode(orig) {}
    virtual ~TCTIterationTraceNode() {}
    
    virtual TCTANode* duplicate() const {
      return new TCTIterationTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTIterationTraceNode(id.id, name, depth, cfgGraph);
    }
  };
  
  class TCTLoopNode : public TCTANode {
    friend class TCTProfileNode;
  public:
    TCTLoopNode(int id, string name, int depth, CFGAGraph* cfgGraph) :
      TCTANode(Loop, id, 0, name, depth, 
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {
        numIteration = 0;
        pendingIteration = NULL;
        rejectedIterations = NULL;
      }
    TCTLoopNode(const TCTLoopNode& orig);
    virtual ~TCTLoopNode();
    
    virtual TCTANode* duplicate() const {
      return new TCTLoopNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTLoopNode(id.id, name, depth, cfgGraph);
    }
    
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval) const;
    
    virtual void addChild(TCTANode* child) {
      print_msg(MSG_PRIO_MAX, "ERROR: addChild() for loop node %s should never been called and is not implemented.\n", name.c_str());
      delete child;
    }
    
    int getNumIteration() {
      return numIteration + (pendingIteration != NULL);
    }
    
    int getNumAcceptedIteration() {
      return acceptedIterations.size(); 
    }
    
    TCTIterationTraceNode* getAcceptedIteration(uint idx) {
      return acceptedIterations[idx];
    }
    
    void pushPendingIteration(TCTIterationTraceNode* pendingIteration, bool accepted) {
      finalizePendingIteration();
      
      this->pendingIteration = pendingIteration;
      this->pendingIterationAccepted = accepted;
    }
    
    TCTIterationTraceNode* popPendingIteration() {
      TCTIterationTraceNode* ret = pendingIteration;
      pendingIteration = NULL;
      return ret;
    }
    
    void finalizePendingIteration();
    
    bool acceptLoop();
  
  private:
    int numIteration;
    
    // stores the last iteration, which may hasn't been finished yet.
    TCTIterationTraceNode* pendingIteration;
    // if the pending iteration passed the acceptIteration test.
    bool pendingIterationAccepted;
    
    // stores all accepted iterations
    vector<TCTIterationTraceNode*> acceptedIterations;
    // all rejected iterations are merged into a Profile node.
    TCTProfileNode* rejectedIterations;
  };
  
  class TCTProfileNode : public TCTANode {
    friend class TCTLoopNode;
  public:
    static TCTProfileNode* newProfileNode(const TCTANode* node) {
      if (node->type == TCTANode::Prof)
        return new TCTProfileNode(*((TCTProfileNode*)node), true);
      if (node->type == TCTANode::Loop)
        return new TCTProfileNode(*((TCTLoopNode*)node));
      return new TCTProfileNode(*((TCTATraceNode*)node));
    }
    
    virtual ~TCTProfileNode() {
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        delete it->second;
    }
    
    virtual TCTANode* duplicate() const {
      return new TCTProfileNode(*this, true);
    };
    
    virtual TCTANode* voidDuplicate() const {
      TCTProfileNode* ret = new TCTProfileNode(*this, false);
      ret->time.clear();
      ret->diffScore.clear();
      return ret;
    }
    
    virtual void addChild(TCTANode* child);
    
    virtual const map<TCTID, TCTProfileNode*>& getChildMap() const {
      return childMap;
    }
    
    virtual void getExclusiveDuration(Time& minExclusive, Time& maxExclusive) const;
    
    // Merge with the input profile node. Deallocation responsibility is NOT transfered.
    // This node won't hold any reference to the input node or its children.
    virtual void merge(const TCTProfileNode* other);
    
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval) const;
    
    virtual void setDepth(int depth) {
      this->depth = depth;
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->setDepth(depth+1);
    }
    
  protected:
    // Map id to child profile node.
    map<TCTID, TCTProfileNode*> childMap;
    
    TCTProfileNode(const TCTATraceNode& trace);
    TCTProfileNode(const TCTLoopNode& loop);
    TCTProfileNode(const TCTProfileNode& prof, bool copyChildMap);
  };
}

#endif /* TCT_NODE_HPP */

