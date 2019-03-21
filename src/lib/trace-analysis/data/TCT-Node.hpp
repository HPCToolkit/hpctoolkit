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

#include <unordered_map>
using std::unordered_map;

#include <unordered_set>
using std::unordered_set;

#include <map>
using std::map;

#include "../TraceAnalysisCommon.hpp"
#include "TCT-CFG.hpp"
#include "TCT-Time.hpp"
#include "TCT-Metrics.hpp"
#include "TCT-Cluster.hpp"
#include "TCT-Semantic-Label.hpp"

#include <boost/serialization/split_member.hpp>

namespace TraceAnalysis {
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
}

namespace std {
  template <> struct hash<TraceAnalysis::TCTID>
  {
    std::size_t operator()(const TraceAnalysis::TCTID& id) const noexcept
    {
      std::size_t h1 = hash<int>()(id.id);
      std::size_t h2 = hash<int>()(id.procID);
      return h1 ^ (h2 << 8);
    }
  };
}

namespace TraceAnalysis {
  // Forward declarations
  class TCTANode;
  class TCTACFGNode;
  class TCTFunctionTraceNode;
  class TCTIterationTraceNode;
  class TCTLoopNode;
  class TCTClusterNode;
  class TCTNonCFGProfileNode;
  class TCTRootNode;

  // Temporal Context Tree Abstract Node
  class TCTANode {
  public:
    enum NodeType {
      Root,
      Func,
      Iter,
      Loop,
      NonCFGProf,
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
    TCTANode(NodeType type) : type(type), id(), cfgGraph(NULL), ra(0), name(), depth(0), weight(0), retCount(0), 
        semanticLabel(SEMANTIC_LABEL_ANY), derivedLabel(SEMANTIC_LABEL_ANY), time(), diffScore(), plm() {}
    
  public:
    TCTANode(NodeType type, int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra, uint semanticLabel) :
        type(type), id(id, procID), cfgGraph(cfgGraph), ra(ra), name(name), depth(depth), weight(1), retCount(0), 
        semanticLabel(semanticLabel), derivedLabel(SEMANTIC_LABEL_ANY), time(), diffScore(), plm() {}
    TCTANode(const TCTANode& orig) : type(orig.type), id(orig.id), cfgGraph(orig.cfgGraph), ra(orig.ra), name(orig.name), 
        depth(orig.depth), weight(orig.weight), retCount(orig.retCount), semanticLabel(orig.semanticLabel), derivedLabel(orig.derivedLabel),
        time(orig.time), diffScore(orig.diffScore), plm(orig.plm) {}
    TCTANode(const TCTANode& orig, NodeType type) : type(type), id(orig.id), cfgGraph(orig.cfgGraph), ra(orig.ra), name(orig.name), 
        depth(orig.depth), weight(orig.weight), retCount(orig.retCount), semanticLabel(orig.semanticLabel), derivedLabel(orig.derivedLabel),
        time(orig.time), diffScore(orig.diffScore), plm(orig.plm) {}
    
    virtual ~TCTANode() {
    }
    
    virtual bool isLoop() const {
      if (id.procID == 0 && id.id != 0)
        return true;
      return false;
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
    
    virtual uint getOriginalSemanticLabel() const {
      return semanticLabel;
    }
    
    virtual uint getDerivedSemanticLabel() const {
      return derivedLabel;
    }
    
    virtual void setDerivedSemanticLabel(uint label) {
      derivedLabel = label;
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
    
    // Return the exclusive time in this node, which excludes time spent in
    // functions called by this node.
    virtual Time getExclusiveDuration() const = 0;
    
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
      plm.initDurationMetric(this, weight);
    }
    
    virtual TCTPerfLossMetric& getPerfLossMetric() {
      return plm;
    }
    
    virtual const TCTPerfLossMetric& getPerfLossMetric() const {
      return plm;
    }
    
    // complete a node after all its children are added.
    virtual void completeNodeInit() {
      setRetCount(1);
      plm.initDurationMetric(this, weight);
    }
    
    virtual void setDepth(int depth) = 0;
    
    // returns a pointer to a duplicate of this object. 
    // Caller responsible for deallocating the duplicate.
    virtual TCTANode* duplicate() const = 0;
    
    // returns a pointer to a void duplicate of this object. 
    // Caller responsible for deallocating the void duplicate.
    virtual TCTANode* voidDuplicate() const = 0;
    
    // finalize loops in the subtree.
    virtual void finalizeEnclosingLoops() = 0;
    
    // Assign derived semantic labels to all nodes in the subtree.
    // Input/output: durations -- an array that accumulates time spent in all semantic categories.
    void assignDerivedSemanticLabel(Time* durations);
    
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
    uint semanticLabel;
    uint derivedLabel;
    TCTTime time;
    TCTDiffScore diffScore;
    TCTPerfLossMetric plm;
    
    virtual void accumulateSemanticDurations(Time* durations) = 0;
  };
  
  // Temporal Context tree Abstract CFG Node
  class TCTACFGNode : public TCTANode {
    friend class boost::serialization::access;
    
  public:
    static TCTFunctionTraceNode dummyBeginNode;
    static TCTFunctionTraceNode dummyEndNode;
  
    class Edge {
      friend class boost::serialization::access;
      
    private:
      template<class Archive>
      void serialize(Archive & ar, const unsigned int version);
      // Constructor for serialization only.
      Edge() {}
      
    public:
      Edge(TCTANode* src, TCTANode* dst, double weight) : src(src), dst(dst), weight(weight) {}
      virtual ~Edge() {}
      
    private:
      Edge(const Edge& orig) : src(orig.src), dst(orig.dst), weight(orig.weight) {}
      
    public:
      TCTANode* getSrc() {return src;}
      TCTANode* getDst() {return dst;}
      double getWeight() {return weight;}
      
      Edge* duplicate() {
        return new Edge(*this);
      }
      
      void addWeight(double inc) {
        weight += inc;
      }
      
      void setWeight(double w) {
        weight = w;
      }
      
      void setSrc(TCTANode* src) {
        this->src = src;
      }
      
      void setDst(TCTANode* dst) {
        this->dst = dst;
      }
      
      string toString() const;
      
    private:
      TCTANode* src;
      TCTANode* dst;
      double weight;
    };
    
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
  protected:
    // Constructor for serialization only.
    TCTACFGNode(NodeType type) : TCTANode(type) {}

  public:
    TCTACFGNode(NodeType type, int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra, uint semanticLabel) :
      TCTANode(type, id, procID, name, depth, cfgGraph, ra, semanticLabel), isProfile(false) {}
    TCTACFGNode(const TCTACFGNode& orig);

    virtual ~TCTACFGNode() {
      for (auto it = children.begin(); it != children.end(); it++)
        delete (*it);
      for (auto it = outEdges.begin(); it != outEdges.end(); it++) {
        for (auto eit = it->second.begin(); eit != it->second.end(); eit++)
          delete (*eit);
      }
    }
    
    virtual int getNumChild() const {
      return children.size();
    }
    
    virtual const TCTANode* getChild(int idx) const {
      return children[idx];
    }
    
    virtual TCTANode* getChild(int idx) {
      return children[idx];
    }
    
    virtual void addChild(TCTANode* child) {
      children.push_back(child);
    }
    
    virtual TCTANode* removeChild(int idx) {
      TCTANode* ret = getChild(idx);
      children.erase(children.begin() + idx);
      return ret;
    }
    
    virtual const unordered_set<TCTACFGNode::Edge*>& getEntryEdges() const;
    
    virtual const unordered_set<TCTACFGNode::Edge*>& getOutEdges(int idx) const {
      if (outEdges.find(children[idx]->id) == outEdges.end())
        print_msg(MSG_PRIO_MAX, "ERROR: no edge set located in TCTACFGNode::getOutEdges().");
      return outEdges.at(children[idx]->id);
    }
    
    virtual void setEdges(vector<Edge*>& edges);
    
    virtual void adjustEdgeWeight(int w);
    
    virtual Time getExclusiveDuration() const;
    
    virtual void getExclusiveDuration(Time& minExclusive, Time& maxExclusive) const;
    
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
    
    virtual void completeNodeInit();
    
    // Convert a trace node to CFG profile node
    virtual void toCFGProfile();
    
    virtual bool isCFGProfile() const {
      return isProfile;
    }

    virtual void setDepth(int depth) {
      this->depth = depth;
      for (auto child : children)
        child->setDepth(depth+1);
    }
      
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
  protected:
    vector<TCTANode*> children;
    //          Source ID
    unordered_map<TCTID, unordered_set<Edge*>> outEdges;
    
    bool isProfile;
    
    virtual void accumulateSemanticDurations(Time* durations) {
      for (int i = 0; i < getNumChild(); i++)
        getChild(i)->assignDerivedSemanticLabel(durations);
    }
  };
  
  class TCTFunctionTraceNode : public TCTACFGNode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTFunctionTraceNode() : TCTACFGNode(Func) {}
    
  public:
    TCTFunctionTraceNode(int id, int procID, string name, int depth, CFGAGraph* cfgGraph, VMA ra, uint semanticLabel) :
      TCTACFGNode(Func, id, procID, name, depth, cfgGraph, ra, semanticLabel) {}
    TCTFunctionTraceNode(const TCTFunctionTraceNode& orig) : TCTACFGNode(orig) {}
    virtual ~TCTFunctionTraceNode() {}
    
    virtual TCTANode* duplicate() const {
      return new TCTFunctionTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTFunctionTraceNode(id.id, id.procID, name, depth, cfgGraph, ra, semanticLabel);
    }
  };

  // Temporal Context Tree Root Node
  class TCTRootNode : public TCTACFGNode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTRootNode() : TCTACFGNode(Root) {}
    
  public:
    TCTRootNode(int id, int procID, string name, int depth) : 
            TCTACFGNode(Root, id, procID, name, depth, NULL, 0, SEMANTIC_LABEL_COMPUTATION), pid(-1) {}
    TCTRootNode(const TCTRootNode& orig) : TCTACFGNode(orig), pid(orig.pid) {}
    virtual ~TCTRootNode() {}
    
    virtual TCTANode* duplicate() const {
      return new TCTRootNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTRootNode(id.id, id.procID, name, depth);
    }
    
    virtual void completeThreadTCT() {
      assignDerivedSemanticLabel(NULL);
      adjustEdgeWeight(1);
    }
    
    virtual void setRootID(int id) {
      pid = id;
    }
    
    virtual int getRootID() const {
      return pid;
    }
    
  private:
    int pid;
  };
  
  // Temporal Context Tree Iteration Trace Node
  class TCTIterationTraceNode : public TCTACFGNode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTIterationTraceNode() : TCTACFGNode(Iter) {}
    
  public:
    TCTIterationTraceNode(int id, int depth, CFGAGraph* cfgGraph, uint semanticLabel) :
      TCTACFGNode(Iter, id, 0, "", depth, cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma, semanticLabel), iterNum(-1) {}
    TCTIterationTraceNode(int id, string name, int depth, CFGAGraph* cfgGraph, uint semanticLabel) :
      TCTACFGNode(Iter, id, 0, name, depth, cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma, semanticLabel), iterNum(-1) {}
    TCTIterationTraceNode(const TCTIterationTraceNode& orig) : TCTACFGNode(orig), iterNum(orig.iterNum) {}
    virtual ~TCTIterationTraceNode() {}
    
    virtual TCTANode* duplicate() const {
      return new TCTIterationTraceNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTIterationTraceNode(id.id, name, depth, cfgGraph, semanticLabel);
    }
    
    int getIterNum() const {
      return iterNum;
    }
    
    void setIterNum(int num) {
      iterNum = num;
      setName("Iteration #" + std::to_string(iterNum));
    }
    
  private:
    int iterNum;
  };
  
  class TCTLoopNode : public TCTANode {
    friend class TCTNonCFGProfileNode;
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTLoopNode() : TCTANode(Loop), numIteration(-1), numAcceptedIteration(-1), 
      pendingIteration(NULL), clusterNode(NULL), rejectedIterations(NULL), profileNode(NULL) {}
    
    // A simple implementation of random number generator.
    class RandGenerator {
    public:
      RandGenerator() : seed(ITER_SAMPLE_SEED) {};
      RandGenerator(const RandGenerator& orig) : seed(orig.seed) {}
      
      int rand(int max) {
        seed = seed * 48271 % 2147483647;
        return seed % max;
      }
      
    private:
      long seed;
    };
    
  public:
    TCTLoopNode(int id, string name, int depth, CFGAGraph* cfgGraph, uint semanticLabel);
    TCTLoopNode(const TCTLoopNode& orig);
    TCTLoopNode(const TCTLoopNode& loop1, long weight1, const TCTLoopNode& loop2, long weight2, bool accumulate);
    virtual ~TCTLoopNode();
    
    virtual TCTANode* duplicate() const {
      return new TCTLoopNode(*this);
    }
    virtual TCTANode* voidDuplicate() const {
      return new TCTLoopNode(id.id, name, depth, cfgGraph, semanticLabel);
    }
    
    virtual Time getExclusiveDuration() const;
    
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
    
    virtual void setDepth(int depth) {
      print_msg(MSG_PRIO_MAX, "ERROR: TCTLoopNode::setDepth() is not implemented.\n");
    }
    
    TCTANode* getProfileNode() {
      return profileNode;
    }
    
    const TCTANode* getProfileNode() const {
      return profileNode;
    }
    
    const TCTClusterNode* getClusterNode() const {
      return clusterNode;
    }
    
    const TCTANode* getRejectedIterations() const {
      return rejectedIterations;
    }
    
    bool hasPendingIteration() const {
      return (pendingIteration != NULL);
    }
    
    int getNumSampledIterations() const {
      return std::min(ITER_SAMPLE_SIZE, numAcceptedIteration);
    }
    
    const TCTIterationTraceNode* getSampledIteration(int idx) const {
      return sampledIterations[idx];
    }
    
    void adjustCFGEdgeWeight(int w);
    
  protected:
    virtual void accumulateSemanticDurations(Time* durations) {
      ((TCTANode*)profileNode)->assignDerivedSemanticLabel(durations);
      if (clusterNode != NULL) ((TCTANode*)clusterNode)->assignDerivedSemanticLabel(NULL);
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
    
    // Random number generator
    RandGenerator generator;
    // Stores sampled iterations
    TCTIterationTraceNode* sampledIterations[ITER_SAMPLE_SIZE];
    
    // Stores clusters of accepted loop iterations
    TCTClusterNode* clusterNode;
    // Stores all rejected iterations (merged into a Profile node).
    TCTANode* rejectedIterations;
    
    // Stores a profile of the entire loop.
    // diffScore of this node quantifies the difference across multiple instances of this loop (when this loop is nested in another loop).
    TCTANode* profileNode;
    
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
    
    virtual Time getExclusiveDuration() const {
      return 0;
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
    
    virtual void setDepth(int depth) {
      print_msg(MSG_PRIO_MAX, "ERROR: TCTClusterNode::setDepth() is not implemented.\n");
    }
    
    virtual string toString(int maxDepth, Time minDuration, double minDiffScore) const;
    
  protected:
    virtual void accumulateSemanticDurations(Time* durations) {
      avgRep->assignDerivedSemanticLabel(durations);
      //TODO calls to representative of each cluster.
    }
    
  private:
    bool mergeClusters();
    
    int numClusters;
    TCTCluster clusters[MAX_NUM_CLUSTER*2];
    double diffRatio[MAX_NUM_CLUSTER*2][MAX_NUM_CLUSTER*2] = {};
    
    TCTANode* avgRep;
  };
  
  class TCTNonCFGProfileNode : public TCTANode {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    // Constructor for serialization only.
    TCTNonCFGProfileNode() : TCTANode(NonCFGProf) {}  
    
  public:
    static TCTNonCFGProfileNode* newNonCFGProfileNode(const TCTANode* node) {
      if (node->type == TCTANode::NonCFGProf)
        return new TCTNonCFGProfileNode(*((TCTNonCFGProfileNode*)node), true);
      if (node->type == TCTANode::Loop)
          return newNonCFGProfileNode(((TCTLoopNode*)node)->profileNode);
      return new TCTNonCFGProfileNode(*((TCTACFGNode*)node));
    }
    
    virtual ~TCTNonCFGProfileNode() {
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        delete it->second;
    }
    
    virtual TCTANode* duplicate() const {
      return new TCTNonCFGProfileNode(*this, true);
    };
    
    virtual TCTANode* voidDuplicate() const {
      TCTNonCFGProfileNode* ret = new TCTNonCFGProfileNode(*this, false);
      ret->retCount = 0;
      ret->time.clear();
      ret->diffScore.clear();
      ret->plm.clearDurationMetric();
      return ret;
    }
    
    virtual void addChild(TCTANode* child);
    
    virtual const map<TCTID, TCTNonCFGProfileNode*>& getChildMap() const {
      return childMap;
    }
    
    virtual Time getExclusiveDuration() const;
    
    virtual void getExclusiveDuration(Time& minExclusive, Time& maxExclusive) const;
    
    // Merge with the input profile node. Deallocation responsibility is NOT transfered.
    // This node won't hold any reference to the input node or its children.
    virtual void merge(const TCTNonCFGProfileNode* other);
    
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
    map<TCTID, TCTNonCFGProfileNode*> childMap;
    
    TCTNonCFGProfileNode(const TCTACFGNode& node);
    TCTNonCFGProfileNode(const TCTLoopNode& loop);
    TCTNonCFGProfileNode(const TCTNonCFGProfileNode& prof, bool copyChildMap);
    
    void setDepth(int depth) {
      this->depth = depth;
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->setDepth(depth+1);
    }
        
    void amplify(long amplifier, long divider, const TCTTime& parent_time);
    
    virtual void accumulateSemanticDurations(Time* durations) {
      for (auto it = childMap.begin(); it != childMap.end(); it++)
        it->second->assignDerivedSemanticLabel(durations);
    }
  };
}

#endif /* TCT_NODE_HPP */

