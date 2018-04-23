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
        type(type), id(id, procID), name(name), cfgGraph(cfgGraph), ra(ra), 
        depth(depth), weight(1) {
      time = new TCTTraceTime();
    }
    TCTANode(const TCTANode& orig) : type(orig.type), id(orig.id), name(orig.name), 
        cfgGraph(orig.cfgGraph), ra(orig.ra), depth(orig.depth), weight(orig.weight) {
      time = orig.time->duplicate();
    }
    TCTANode(const TCTANode& orig, NodeType type) : type(type), id(orig.id), name(orig.name), 
        cfgGraph(orig.cfgGraph), ra(orig.ra), depth(orig.depth), weight(orig.weight) {
      time = orig.time->duplicate();
    }
    virtual ~TCTANode() {
      delete time;
    }
    
    virtual TCTTraceTime* getTraceTime() {
      if (time->type == TCTATime::Trace) return (TCTTraceTime*)time;
      else return NULL;
    }
    
    virtual int getDepth() {
      return depth;
    }
    
    virtual Time getDuration() {
      return time->getDuration();
    }
        
    // returns a pointer to a duplicate of this object. 
    // Caller responsible for deallocating the duplicate.
    virtual TCTANode* duplicate() const = 0;
    
    // returns a pointer to a void duplicate of this object. 
    // Caller responsible for deallocating the void duplicate.
    virtual TCTANode* voidDuplicate() const = 0;
    
    // Print contents of an object to a string for debugging purpose.
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval);
    
    // Add child to this node. Deallocation responsibility is transfered to this node.
    // Child could be deallocated inside the call.
    virtual void addChild(TCTANode* child) = 0;
    
    const NodeType type;
    const TCTID id;
    const string name;
    // CFG Abstract Graph for this node.
    CFGAGraph* const cfgGraph; 
    // RA for call sites or VMA for loops.
    const VMA ra; 
    
  protected:
    int depth;
    TCTATime* time;
    int weight;
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
    
    virtual int getNumChild() {
      return children.size();
    }
    
    virtual TCTANode* getChild(int idx) {
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
    
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval);
    
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
              cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma), pendingIteration(NULL) {}
    TCTLoopNode(const TCTLoopNode& orig) : TCTANode(orig) {
      if (orig.pendingIteration != NULL) {
        pendingIteration = (TCTIterationTraceNode*)(orig.pendingIteration->duplicate());
        pendingIterationAccepted = orig.pendingIterationAccepted;
      }
      for (auto it = orig.iterations.begin(); it != orig.iterations.end(); it++)
        iterations.push_back((TCTIterationTraceNode*)((*it)->duplicate()));
    }
    virtual ~TCTLoopNode() {
      if (pendingIteration != NULL) delete pendingIteration;
      for (auto it = iterations.begin(); it != iterations.end(); it++)
        delete (*it);
    }
    
    virtual TCTANode* duplicate() const {
      return new TCTLoopNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTLoopNode(id.id, name, depth, cfgGraph);
    }
    
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval);
    
    virtual void addChild(TCTANode* child) {
      printf("ERROR: addChild() for loop node %s is not implemented.\n", name.c_str());
      delete child;
    }
    
    int getNumIteration() {
      return iterations.size() + hasPendingIteration();
    }
    
    TCTIterationTraceNode* getIteration(uint idx) {
      if (idx < iterations.size()) return iterations[idx];
      if (idx == iterations.size()) return pendingIteration;
      return NULL;
    }
    
    bool hasPendingIteration() {
      return (pendingIteration != NULL);
    }
    
    bool isPendingIterationAccepted() {
      return pendingIterationAccepted;
    }
    
    TCTIterationTraceNode* popPendingIteration() {
      pendingIteration = NULL;
      return pendingIteration;
    }
    
    bool pushPendingIteration(TCTIterationTraceNode* pendingIteration, bool accepted) {
      if (hasPendingIteration()) return false;
      this->pendingIteration = pendingIteration;
      this->pendingIterationAccepted = accepted;
      return true;
    }
    
    bool finalizePendingIteration() {
      if (!hasPendingIteration()) return false;
      iterations.push_back(pendingIteration);
      pendingIteration = NULL;
      return true;
    }
  
  private:
    // stores the last iteration, when may hasn't been finished yet.
    TCTIterationTraceNode* pendingIteration;
    // if the pending iteration passed the acceptIteration test.
    bool pendingIterationAccepted;
    
    vector<TCTIterationTraceNode*> iterations;
  };
  
  class TCTProfileNode : public TCTANode {
  public:
    static TCTProfileNode* newProfileNode(TCTANode* node) {
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
      ret->time->clear();
      return ret;
    }
    
    virtual void addChild(TCTANode* child) {
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
    
    virtual string toString(int maxDepth, Time minDuration, Time samplingInterval);
    
    virtual void setDepth(int depth) {
      this->depth = depth;
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->setDepth(depth+1);
    }
    
  protected:
    // Map id to child profile node.
    map<TCTID, TCTProfileNode*> childMap;
    
    TCTProfileNode(const TCTATraceNode& trace) : TCTANode (trace, Prof) {
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
    
    TCTProfileNode(const TCTLoopNode& loop) : TCTANode (loop, Prof) {
      for (auto it = loop.iterations.begin(); it != loop.iterations.end(); it++) {
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
    }
    
    TCTProfileNode(const TCTProfileNode& prof, bool copyChildMap) : TCTANode (prof) {
      if (copyChildMap)
        for (auto it = prof.childMap.begin(); it != prof.childMap.end(); it++)
          childMap[it->second->id] = (TCTProfileNode*) it->second->duplicate();
    }
    
    virtual void merge(TCTProfileNode* other) {
      TCTProfileTime* profileTime = NULL; 
      if (time->type == TCTATime::Profile)
        profileTime = (TCTProfileTime*) time;
      else {
        profileTime = new TCTProfileTime(*time);
        delete time;
        time = profileTime;
      }
      
      profileTime->merge(other->time);
      for (auto it = other->childMap.begin(); it != other->childMap.end(); it++) {
        if (childMap.find(it->second->id) == childMap.end())
          childMap[it->second->id] = (TCTProfileNode*) it->second->duplicate();
        else
          childMap[it->second->id]->merge(it->second);
      }
    }
  };
}

#endif /* TCT_NODE_HPP */

