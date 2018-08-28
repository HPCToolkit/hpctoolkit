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

#include "../TraceAnalysisCommon.hpp"
#include "TCT-CFG.hpp"
#include "TCT-Time.hpp"
#include "TCT-Metrics.hpp"
#include "TCT-Cluster.hpp"

#include <boost/serialization/split_member.hpp>

namespace TraceAnalysis {
  // Forward declarations
  class TCTANode;
  class TCTATraceNode;
  class TCTFunctionTraceNode;
  class TCTIterationTraceNode;
  class TCTLoopNode;
  class TCTClusterNode;
  class TCTProfileNode;
  class TCTRootNode;

  class TCTID {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
  public:
    // Constructor for serialization only.
    TCTID() : id(0), procID(0) {}
    
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
      Prof,
      Cluster
    };
    
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void save(Archive & ar, const unsigned int version) const;
    template<class Archive>
    void load(Archive & ar, const unsigned int version);
    BOOST_SERIALIZATION_SPLIT_MEMBER();
  protected:
    // Constructor for serialization only.
    TCTANode(NodeType type) : type(type), id(), cfgGraph(NULL), ra(0), name(), depth(0), weight(0), retCount(0), time(),
        diffScore(), plm() {}
    
  public:
    TCTANode(NodeType type, int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra) :
        type(type), id(id, procID), cfgGraph(cfgGraph), ra(ra), name(name), depth(depth), weight(1), retCount(0), time(),
        diffScore(), plm() {}
    TCTANode(const TCTANode& orig) : type(orig.type), id(orig.id), cfgGraph(orig.cfgGraph), 
        ra(orig.ra), name(orig.name), depth(orig.depth), weight(orig.weight), retCount(orig.retCount), time(orig.time),
        diffScore(orig.diffScore), plm(orig.plm) {}
    TCTANode(const TCTANode& orig, NodeType type) : type(type), id(orig.id), cfgGraph(orig.cfgGraph), 
        ra(orig.ra), name(orig.name), depth(orig.depth), weight(orig.weight), retCount(orig.retCount), time(orig.time),
        diffScore(orig.diffScore), plm(orig.plm) {}
    
    virtual ~TCTANode() {
    }
    
    virtual TCTTime& getTime() {
      return time;
    }
    
    virtual const TCTTime& getTime() const {
      return time;
    }
    
    virtual void shiftTime(Time offset) {
      time.shiftTime(offset);
    }
    
    virtual string getName() const {
      return name;
    }
    
    virtual void setName(string name) {
      this->name = name;
    }
    
    virtual int getDepth() const {
      return depth;
    }
    
    virtual double getNumSamples() const {
      return time.getNumSamples();
    }
    
    virtual Time getSamplingPeriod() const {
      return (Time)(time.getDuration() / time.getNumSamples());
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
    
    virtual long getWeight() const {
      return weight;
    }
    
    virtual void setWeight(long weight) {
      this->weight = weight;
    }
    
    virtual long getRetCount() const {
      return retCount;
    }
    
    virtual void setRetCount(long retCount) {
      this->retCount = retCount;
    }
    
    virtual void clearDiffScore() {
      diffScore.clear();
    }
    
    virtual TCTDiffScore& getDiffScore() {
      return diffScore;
    }
    
    virtual const TCTDiffScore& getDiffScore() const {
      return diffScore;
    }
    
    virtual void initPerfLossMetric() {
      plm.initDurationMetric(time, weight);
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
    
    // finalize loops in the subtree.
    virtual void finalizeEnclosingLoops() = 0;
    
    virtual void adjustIterationNumbers(long inc) = 0;
    
    // Print contents of an object to a string for debugging purpose.
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
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
    long weight;
    long retCount;
    TCTTime time;
    TCTDiffScore diffScore;
    TCTPerfLossMetric plm;
  };
  
  // Temporal Context Tree Abstract Trace Node
  class TCTATraceNode : public TCTANode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
  protected:
    // Constructor for serialization only.
    TCTATraceNode(NodeType type) : TCTANode(type) {}
 
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
    
    virtual void clearChildren() {
      for (auto it = children.begin(); it != children.end(); it++)
        delete (*it);
      children.clear();
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
    
    virtual void shiftTime(Time offset) {
      TCTANode::shiftTime(offset);
      for (int i = 0; i < getNumChild(); i++)
        getChild(i)->shiftTime(offset);
    }
    
    virtual void finalizeEnclosingLoops() {
      for (int i = 0; i < getNumChild(); i++)
        getChild(i)->finalizeEnclosingLoops();
    }
    
    virtual void adjustIterationNumbers(long inc) {
      for (int i = 0; i < getNumChild(); i++)
        getChild(i)->adjustIterationNumbers(inc);
    }
    
    virtual void clearDiffScore() {
      TCTANode::clearDiffScore();
      for (int i = 0; i < getNumChild(); i++)
        getChild(i)->clearDiffScore();
    }
    
    virtual void initPerfLossMetric() {
      TCTANode::initPerfLossMetric();
      for (int i = 0; i < getNumChild(); i++)
        getChild(i)->initPerfLossMetric();
    }
    
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
  protected:
    vector<TCTANode*> children;
  };
  
  // Temporal Context Tree Function Trace Node
  class TCTFunctionTraceNode : public TCTATraceNode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTFunctionTraceNode() : TCTATraceNode(Func) {}
    
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
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTRootNode() : TCTATraceNode(Root) {}
    
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
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTIterationTraceNode() : TCTATraceNode(Iter) {}
    
  public:
    TCTIterationTraceNode(int id, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Iter, id, 0, "", depth, cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
    TCTIterationTraceNode(int id, string name, int depth, CFGAGraph* cfgGraph) :
      TCTATraceNode(Iter, id, 0, name, depth, cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma) {}
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
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTLoopNode() : TCTANode(Loop), numIteration(-1), numAcceptedIteration(-1), 
      pendingIteration(NULL), clusterNode(NULL), rejectedIterations(NULL), profileNode(NULL) {}
    
  public:
    TCTLoopNode(int id, string name, int depth, CFGAGraph* cfgGraph);
    TCTLoopNode(const TCTLoopNode& orig);
    TCTLoopNode(const TCTLoopNode& loop1, long weight1, const TCTLoopNode& loop2, long weight2, bool accumulate);
    virtual ~TCTLoopNode();
    
    virtual TCTANode* duplicate() const {
      return new TCTLoopNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      TCTLoopNode* ret = new TCTLoopNode(id.id, name, depth, cfgGraph);
      return ret;
    }
    
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
    virtual void addChild(TCTANode* child) {
      if (child->type != TCTANode::Iter) {
        print_msg(MSG_PRIO_MAX, "ERROR: argument (name = %s) of TCTLoopNode::addChild() should be an iteration.\n", child->getName().c_str());
        delete child;
        return;
      }
      finalizePendingIteration();
      pendingIteration = (TCTIterationTraceNode*) child;
    }
    
    TCTIterationTraceNode* popLastChild() {
      TCTIterationTraceNode* ret = pendingIteration;
      pendingIteration = NULL;
      return ret;
    }
    
    const TCTIterationTraceNode* getPendingIteration() const {
      return pendingIteration;
    }
    
    int getNumIteration() const {
      return numIteration + (pendingIteration != NULL);
    }
    
    int getNumAcceptedIteration() const {
      return numAcceptedIteration;
    }

#ifdef KEEP_ACCEPTED_ITERATION    
    int getNumAcceptedIteration() const {
      if (acceptedIterations.size() != (unsigned long)numAcceptedIteration)
        print_msg(MSG_PRIO_MAX, "ERROR: numAcceptedIteration is wrong for loop node %s.\n", name.c_str());
      return numAcceptedIteration;
    }
    
    TCTIterationTraceNode* getAcceptedIteration(uint idx) {
      return acceptedIterations[idx];
    }
#endif    
    
    bool accept() const;
    
    virtual void shiftTime(Time offset);
    
    virtual void finalizeEnclosingLoops();
    
    virtual void adjustIterationNumbers(long inc);
  
    virtual void clearDiffScore();
    
    virtual void initPerfLossMetric();
    
    TCTProfileNode* getProfileNode();
    
    const TCTProfileNode* getProfileNode() const {
      return const_cast<TCTLoopNode*>(this)->getProfileNode();
    }
    
    const TCTClusterNode* getClusterNode() const {
      return clusterNode;
    }
    
    const TCTProfileNode* getRejectedIterations() const {
      return rejectedIterations;
    }
    
    bool hasPendingIteration() const {
      return (pendingIteration != NULL);
    }
    
  private:
    int numIteration;
    int numAcceptedIteration;
    // stores the last iteration that hasn't been finalized yet.
    TCTIterationTraceNode* pendingIteration;
    
#ifdef KEEP_ACCEPTED_ITERATION
    // stores all accepted iterations
    vector<TCTIterationTraceNode*> acceptedIterations;
#endif
    
    // Stores clusters of accepted loop iterations
    TCTClusterNode* clusterNode;
    // Stores all rejected iterations (merged into a Profile node).
    TCTProfileNode* rejectedIterations;
    
    // Stores a profile of the entire loop.
    // diffScore of this node quantifies the difference across multiple instances of this loop (when this loop is nested in another loop).
    TCTProfileNode* profileNode;
    
    // Return true if we are confident that children of the pending iteration
    // belong to one iteration in the execution.
    bool acceptPendingIteration();
    void finalizePendingIteration();
  };
  
  class TCTClusterNode : public TCTANode {
    friend class TCTLoopNode;
  public:
    typedef struct {
      TCTANode* representative;
      TCTClusterMembers* members;
    } TCTCluster;
    
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTClusterNode() : TCTANode(Cluster), numClusters(-1), avgRep(NULL) {}  
    
  public:
    TCTClusterNode(const TCTANode& node) : TCTANode(node, Cluster), numClusters(0), avgRep(NULL) {}
    TCTClusterNode(const TCTClusterNode& other, bool isVoid);
    TCTClusterNode(const TCTClusterNode& cluster1, const TCTClusterNode& cluster2);
    virtual ~TCTClusterNode() {
      if (avgRep != NULL) delete avgRep;
      for (int i = 0; i < numClusters; i++) {
        delete clusters[i].representative;
        delete clusters[i].members;
      }
    }
    
    virtual TCTANode* duplicate() const {
      return new TCTClusterNode(*this, false);
    }
    
    virtual TCTANode* voidDuplicate() const {
      return new TCTClusterNode(*this, true);
    }
    
    virtual void addChild(TCTANode* child) {
      print_msg(MSG_PRIO_MAX, "ERROR: TCTClusterNode::addChild(TCTANode*) not implemented. Use TCTClusterNode::addChild(TCTANode*, long) instead.\n");
    }
      
    void addChild(TCTANode* child, long idx);
    
    int getNumClusters() const {
      return numClusters;
    }
    
    const TCTANode* getClusterRepAt(int idx) const {
      return clusters[idx].representative;
    }
    
    void computeAvgRep();
    
    const TCTANode* getAvgRep() const {
      return avgRep;
    }
    
    virtual void finalizeEnclosingLoops() {}
    
    virtual void adjustIterationNumbers(long inc);
    
    void clearDiffScore() {
      TCTANode::clearDiffScore();
      if (avgRep != NULL) avgRep->clearDiffScore();
    }
    
    virtual void initPerfLossMetric() {
      TCTANode::initPerfLossMetric();
      if (avgRep != NULL) avgRep->initPerfLossMetric();
      for (int i = 0; i < numClusters; i++)
        clusters[i].representative->initPerfLossMetric();
    }
    
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
  private:
    bool mergeClusters();
    
    int numClusters;
    TCTCluster clusters[MAX_NUM_CLUSTER*2];
    double diffRatio[MAX_NUM_CLUSTER*2][MAX_NUM_CLUSTER*2] = {};
    
    TCTANode* avgRep;
  };
  
  class TCTProfileNode : public TCTANode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTProfileNode() : TCTANode(Prof) {}  
    
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
      ret->retCount = 0;
      ret->time.clear();
      ret->diffScore.clear();
      ret->plm.clearDurationMetric();
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
    
    virtual void finalizeEnclosingLoops() {}
    
    virtual void adjustIterationNumbers(long inc) {}
    
    virtual void clearDiffScore() {
      TCTANode::clearDiffScore();
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->clearDiffScore();        
    }
    
    virtual void initPerfLossMetric() {
      TCTANode::initPerfLossMetric();
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->initPerfLossMetric();  
    }
    
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
  protected:
    // Map id to child profile node.
    map<TCTID, TCTProfileNode*> childMap;
    
    TCTProfileNode(const TCTATraceNode& trace);
    TCTProfileNode(const TCTLoopNode& loop);
    TCTProfileNode(const TCTProfileNode& prof, bool copyChildMap);
    
    void setDepth(int depth) {
      this->depth = depth;
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->setDepth(depth+1);
    }
        
    void amplify(long amplifier, long divider, const TCTTime& parent_time);
  };
}

#endif /* TCT_NODE_HPP */

