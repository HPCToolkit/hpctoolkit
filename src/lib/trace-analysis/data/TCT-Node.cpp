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
#include "../DifferenceQuantifier.hpp"

namespace TraceAnalysis {
  string TCTANode::toString(int maxDepth, Time minDuration, double minDiffScore) const {
    string ret;
    /*
    ret += "CFG-0x";
    if (cfgGraph == NULL) ret += vmaToHexString(0);
    else ret += vmaToHexString(cfgGraph->vma);
    ret += " RA-0x" + vmaToHexString(ra) + " ";
    */
    for (int i = 0; i < depth; i++) ret += "  ";
    switch (type) {
      case TCTANode::Root : ret += "(R) "; break;
      case TCTANode::Func : ret += "(F) "; break;
      case TCTANode::Iter : ret += "(I) "; break;
      case TCTANode::Loop : ret += "(L) "; break;
      case TCTANode::NonCFGProf : ret += "(P) "; break;
      default: ret += "    ";
    }
    ret += name + id.toString();
    ret += " " + time.toString();
    if (weight > 1) ret += ", w = " + std::to_string(weight);
    ret += ", retCount = " + std::to_string(retCount);
    
    if (diffScore.getInclusive() > 0)
      ret += ", DiffScore = " + std::to_string((long)diffScore.getInclusive())
              + "/" + std::to_string((long)diffScore.getExclusive());
    
    if (plm.getMinDurationInc() < plm.getMaxDurationInc()) {
      ret += ", PLM = " + std::to_string(plm.getMinDurationInc()) 
              + "/" + std::to_string(plm.getMaxDurationInc())
              + "/" + std::to_string(plm.getAvgDurationInc(weight));
    }
    
    if (type == TCTANode::Root) {
      ret += ", Sampling Period = " + std::to_string((Time)(this->getDuration() / this->getNumSamples()));
    }
    
    ret += "\n";
    
    return ret;
  }
  
  string TCTACFGNode::Edge::toString() const {
    return src->id.toString() + " -> " + dst->id.toString() + ", w = " + std::to_string(weight) + "\n";
  }
  
  string TCTACFGNode::toString(int maxDepth, Time minDuration, double minDiffScore) const {
    string ret = TCTANode::toString(maxDepth, minDuration, minDiffScore);
    
    if (depth >= maxDepth) return ret;
    if (minDuration > 0 && time.getDuration() < minDuration) return ret;
    if (diffScore.getInclusive() < minDiffScore) return ret;
    if (name.find("<unknown procedure>") != string::npos) return ret;
    
    for (auto eit = outEdges.at(dummyBeginNode.id).begin(); eit != outEdges.at(dummyBeginNode.id).end(); eit++)
      ret += (*eit)->toString();
    
    for (TCTANode* child : children)
      for (auto eit = outEdges.at(child->id).begin(); eit != outEdges.at(child->id).end(); eit++)
        ret += (*eit)->toString();
    
    for (TCTANode* child : children)
      ret += child->toString(maxDepth, minDuration, minDiffScore);
    return ret;
  }
  
  string TCTLoopNode::toString(int maxDepth, Time minDuration, double minDiffScore) const {
    if (accept()) {
      string ret = TCTANode::toString(maxDepth, minDuration, minDiffScore);
     
      if (profileNode != NULL) { 
        string temp = profileNode->toString(maxDepth, minDuration, minDiffScore);
        ret += temp.substr(temp.find_first_of('\n') + 1);
      }
      
      if (clusterNode != NULL)
        ret += clusterNode->toString(maxDepth, minDuration, minDiffScore);
      
      if (rejectedIterations != NULL)
        ret += rejectedIterations->toString(maxDepth, minDuration, minDiffScore);
                
      return ret;
    }
    else if (profileNode != NULL)
      return profileNode->toString(maxDepth, minDuration, minDiffScore);
    else
      return TCTANode::toString(maxDepth, minDuration, minDiffScore);
  }
  
  string TCTClusterNode::toString(int maxDepth, Time minDuration, double minDiffScore) const {
    string prefix = "";
    for (int i = 0; i <= depth; i++) prefix += "  ";
    
    string ret = prefix + "# clusters = " + std::to_string(numClusters) + "\n";
    for (int i = 0; i < numClusters; i++) {
      ret += prefix + "CLUSTER #" + std::to_string(i) + " members: " + clusters[i].members->toString() + "\n";
      ret += clusters[i].representative->toString(maxDepth, minDuration, minDiffScore);
    }
    
    ret += prefix + "Diff ratio among clusters:\n";
    for (int i = 0; i < numClusters; i++) {
      ret += prefix;
      for (int j = 0; j < numClusters; j++)
        ret += std::to_string(diffRatio[i][j] * 100) + "%\t";
      ret += "\n";
    }
    return ret;
  }
  
  string TCTNonCFGProfileNode::toString(int maxDepth, Time minDuration, double minDiffScore) const {
    string ret = TCTANode::toString(maxDepth, minDuration, minDiffScore);
    
    if (depth >= maxDepth) return ret;
    if (minDuration > 0 && time.getDuration() < minDuration) return ret;
    if (diffScore.getInclusive() < minDiffScore) return ret;
    if (name.find("<unknown procedure>") != string::npos) return ret;
    
    for (auto it = childMap.begin(); it != childMap.end(); it++)
      ret += it->second->toString(maxDepth, minDuration, minDiffScore);
    return ret;
  }
  
  void TCTANode::assignDerivedSemanticLabel(Time* durations) {
    // Accumulate durations into an array
    Time subTreeDurations[SEMANTIC_LABEL_ARRAY_SIZE];
    for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++) subTreeDurations[i] = 0;
    accumulateSemanticDurations(subTreeDurations);

    long totalDuration = getDuration() * getWeight();
    uint tempDerivedLabel;
    // Assign derived semantic label
    if (getNumSamples() * getWeight() < DERIVED_SEMANTIC_LABEL_MIN)
      // If we don't have enough samples to get accurate result for this node, use SEMANTIC_LABEL_ANY.
      tempDerivedLabel = SEMANTIC_LABEL_ANY;
    else {
      // If so, assign to the finest possible label such as time spent in the label is no less than half of the node's total duration.
      tempDerivedLabel = semanticLabel;
      for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
        // SEMANTIC_LABEL_ARRAY[i].label is a child of derivedLabel
        if (((SEMANTIC_LABEL_ARRAY[i].label & tempDerivedLabel) == tempDerivedLabel) 
            && (SEMANTIC_LABEL_ARRAY[i].label != tempDerivedLabel)
            // SEMANTIC_LABEL_ARRAY[i].label is the major source
            && (subTreeDurations[i] >= totalDuration / 2) )
          // If so, change the semantic label of this node.
          tempDerivedLabel = SEMANTIC_LABEL_ARRAY[i].label;
    }
    derivedLabel &= tempDerivedLabel;

    // Accumulate durations from this sub-tree to the input array.
    if (durations != NULL) {
      for (uint i = 0; i < SEMANTIC_LABEL_ARRAY_SIZE; i++)
        if ((semanticLabel & SEMANTIC_LABEL_ARRAY[i].label) == SEMANTIC_LABEL_ARRAY[i].label)
          durations[i] += totalDuration;
        else
          durations[i] += subTreeDurations[i];
    }
  }
  
  TCTLoopNode::TCTLoopNode(int id, string name, int depth, CFGAGraph* cfgGraph, uint semanticLabel) :
    TCTANode(Loop, id, 0, name, depth, cfgGraph, cfgGraph == NULL ? 0 : cfgGraph->vma, semanticLabel) {
      numIteration = 0;
      numAcceptedIteration = 0;
      pendingIteration = NULL;
      rejectedIterations = NULL;
      clusterNode = new TCTClusterNode(*this);
      profileNode = new TCTIterationTraceNode(id, name, depth, cfgGraph, semanticLabel);
      ((TCTACFGNode*)profileNode)->toCFGProfile();
    }
  
  TCTLoopNode::TCTLoopNode(const TCTLoopNode& orig) : TCTANode(orig) {
    numIteration = orig.numIteration;
    numAcceptedIteration = orig.numAcceptedIteration;
    
    if (orig.pendingIteration != NULL)
      pendingIteration = (TCTIterationTraceNode*)(orig.pendingIteration->duplicate());
    else
      pendingIteration = NULL;
    
    if (orig.rejectedIterations != NULL)
      rejectedIterations = (TCTNonCFGProfileNode*)(orig.rejectedIterations->duplicate());
    else
      rejectedIterations = NULL;

#ifdef KEEP_ACCEPTED_ITERATION
    for (auto it = orig.acceptedIterations.begin(); it != orig.acceptedIterations.end(); it++)
      acceptedIterations.push_back((TCTIterationTraceNode*)((*it)->duplicate()));
#endif
    
    if (orig.clusterNode != NULL) 
      clusterNode = (TCTClusterNode*)(orig.clusterNode->duplicate());
    else
      clusterNode = NULL;
    
    if (orig.profileNode != NULL)
      profileNode = (TCTNonCFGProfileNode*)(orig.profileNode->duplicate());
    else
      profileNode = NULL;
  }
  
  TCTLoopNode::TCTLoopNode(const TCTLoopNode& loop1, long weight1, const TCTLoopNode& loop2, long weight2, bool accumulate) : TCTANode(loop1) {
    time.setAsAverageTime(loop1.getTime(), weight1, loop2.getTime(), weight2);
    setWeight(weight1 + weight2);
    setRetCount(loop1.retCount + loop2.retCount);
    setDerivedSemanticLabel(loop1.derivedLabel & loop2.derivedLabel);
    getPerfLossMetric().setDuratonMetric(loop1.plm, loop2.plm);
    
    if (loop1.numIteration != loop2.numIteration)
      print_msg(MSG_PRIO_MAX, "ERROR: merging two loops with different number of iterations %d vs %d.\n", loop1.numIteration, loop2.numIteration);
    if (loop1.numAcceptedIteration != loop2.numAcceptedIteration)
      print_msg(MSG_PRIO_MAX, "ERROR: merging two loops with different number of accepted iterations %d vs %d.\n", loop1.numAcceptedIteration, loop2.numAcceptedIteration);
    numIteration = loop1.numIteration;
    numAcceptedIteration = loop1.numAcceptedIteration;
    pendingIteration = NULL;
    
    profileNode = diffQ.mergeNode(loop1.getProfileNode(), weight1, loop2.getProfileNode(), weight2, accumulate, false, false);
    
    TCTANode* rej1 = loop1.rejectedIterations;
    TCTANode* rej2 = loop2.rejectedIterations;
    if (rej1 == NULL && rej2 == NULL)
      rejectedIterations = NULL;
    else {
      if (rej1 == NULL)  {
        rej1 = rej2->voidDuplicate();
        rej1->setWeight(weight1);
      }
      if (rej2 == NULL) {
        rej2 = rej1->voidDuplicate();
        rej2->setWeight(weight2);
      }
      rejectedIterations = diffQ.mergeNode(rej1, weight1, rej2, weight2, accumulate, false, false);
      
      if (loop1.rejectedIterations == NULL) delete rej1;
      if (loop2.rejectedIterations == NULL) delete rej2;
    }
    
    if (loop1.accept() && loop2.accept())
      clusterNode = new TCTClusterNode(*loop1.getClusterNode(), *loop2.getClusterNode());
    else
      clusterNode = NULL;
  }
  
  TCTLoopNode::~TCTLoopNode()  {
    if (pendingIteration != NULL) delete pendingIteration;
    if (rejectedIterations != NULL) delete rejectedIterations;
#ifdef KEEP_ACCEPTED_ITERATION
    for (auto it = acceptedIterations.begin(); it != acceptedIterations.end(); it++)
      delete (*it);
#endif
    if (clusterNode != NULL) delete clusterNode;
    if (profileNode != NULL) delete profileNode;
  }
  
  Time TCTLoopNode::getExclusiveDuration() const {
    if (profileNode != NULL)
      return profileNode->getExclusiveDuration();
    
    TCTNonCFGProfileNode* tmp = TCTNonCFGProfileNode::newNonCFGProfileNode(this);
    Time exclusive = tmp->getExclusiveDuration();
    delete tmp;
    return exclusive;
  }
  
  bool TCTLoopNode::acceptPendingIteration() {
    if (pendingIteration == NULL) return false;
    
    int countFunc = 0;
    int countLoop = 0;

    for (int k = 0; k < pendingIteration->getNumChild(); k++)
      if (pendingIteration->getChild(k)->getNumSamples() >= ITER_CHILD_DUR_ACC) {
        if (pendingIteration->getChild(k)->type == TCTANode::Func) countFunc++;
        else countLoop++; // TCTANode::Loop and TCTANode::Prof are all loops.
      }

    // When an iteration has no func child passing the IterationChildDurationThreshold,
    // and has less than two loop children passing the IterationChildDurationThreshold,
    // and the number of children doesn't pass the IterationNumChildThreshold,
    // children of this iteration may belong to distinct iterations in the execution.
    if (countFunc < ITER_NUM_FUNC_ACC 
            && countLoop < ITER_NUM_LOOP_ACC 
            && pendingIteration->getNumChild() < ITER_NUM_CHILD_ACC)
      return false;
    else
      return true;
  }
  
  void TCTLoopNode::finalizePendingIteration() {
    if (pendingIteration == NULL) return;
    
    pendingIteration->finalizeEnclosingLoops();
    pendingIteration->shiftTime(-pendingIteration->getTime().getStartTimeExclusive());
    
    TCTACFGNode* iterProfile = (TCTACFGNode*)pendingIteration->duplicate();
    iterProfile->toCFGProfile();
    iterProfile->setDepth(profileNode->getDepth());
    
    TCTANode* mergedProf = diffQ.mergeNode(profileNode, profileNode->getWeight(), iterProfile, iterProfile->getWeight(), 
        false, false, true);
    delete profileNode;
    profileNode = mergedProf;

    if (acceptPendingIteration()) {
      numAcceptedIteration++;
#ifdef KEEP_ACCEPTED_ITERATION      
      acceptedIterations.push_back(pendingIteration->duplicate());
#endif     
      clusterNode->addChild(pendingIteration, numIteration);
      delete iterProfile;
    }
    else {
      delete pendingIteration;
      
      if (rejectedIterations != NULL) {
        TCTANode* mergedRejected = diffQ.mergeNode(rejectedIterations, rejectedIterations->getWeight(), iterProfile, iterProfile->getWeight(),
            false, false, true);
        delete rejectedIterations;
        delete iterProfile;
        rejectedIterations = mergedRejected;
      } else {
        iterProfile->setName("Rejected Iterations");
        rejectedIterations = iterProfile;
      }
    }
    
    numIteration++;
    pendingIteration = NULL;
  }
  
  bool TCTLoopNode::accept() const {
    if (clusterNode == NULL) return false;
    if (rejectedIterations == NULL) return true;
    
    if (rejectedIterations->getDuration() > getDuration() * LOOP_REJ_THRESHOLD)
      return false;
    return true;
  }

  void TCTLoopNode::finalizeEnclosingLoops() {
    finalizePendingIteration();
    
    //if (profileNode != NULL)
      //print_msg(MSG_PRIO_MAX, "ERROR: TCTLoopNode::finalizeEnclosingLoops() called when profileNode is not NULL for loop %s.\n", getName().c_str());
    //profileNode = TCTNonCFGProfileNode::newNonCFGProfileNode(this);
    profileNode->clearDiffScore();
    profileNode->initPerfLossMetric();
    
    if (clusterNode->getNumClusters() == 0) {
      delete clusterNode;
      clusterNode = NULL;
    }
    
    if (clusterNode != NULL) {
      clusterNode->getTime().setNumSamples(getNumSamples());
      clusterNode->getTime().setStartTime(getTime().getStartTimeExclusive(), getTime().getStartTimeInclusive());
      clusterNode->getTime().setEndTime(getTime().getEndTimeInclusive(), getTime().getEndTimeExclusive());
      clusterNode->computeAvgRep();
    }
    
    if (accept() && getNumIteration() > 1) {
      print_msg(MSG_PRIO_LOW, "\nLoop accepted: \n%s", toString(getDepth()+5, 0, 0).c_str());

      if (getClusterNode()->getNumClusters() > 1) {
        TCTANode* highlight = diffQ.mergeNode(getClusterNode()->getClusterRepAt(0), getClusterNode()->getClusterRepAt(0)->getWeight(), 
                                                getClusterNode()->getClusterRepAt(1), getClusterNode()->getClusterRepAt(1)->getWeight(), false, false, false);
        print_msg(MSG_PRIO_LOW, "Compare first two clusters:\n");
        print_msg(MSG_PRIO_LOW, "%s\n", highlight->toString(highlight->getDepth()+5, 0, 0).c_str());
        delete highlight;
        print_msg(MSG_PRIO_LOW, "\n");
      }
    }
  }
  
  void TCTLoopNode::clearDiffScore() {
    TCTANode::clearDiffScore();
    if (clusterNode != NULL) clusterNode->clearDiffScore();
    if (rejectedIterations != NULL) rejectedIterations->clearDiffScore();
    if (profileNode != NULL) profileNode->clearDiffScore();
  }
  
  void TCTLoopNode::initPerfLossMetric() {
    TCTANode::initPerfLossMetric();
    if (clusterNode != NULL) clusterNode->initPerfLossMetric();
    if (rejectedIterations != NULL) rejectedIterations->initPerfLossMetric();
    if (profileNode != NULL) profileNode->initPerfLossMetric();
  }
  
  void TCTLoopNode::adjustIterationNumbers(long inc) {
    if (clusterNode != NULL)
      clusterNode->adjustIterationNumbers(inc * numIteration);
  }
  
  void TCTLoopNode::shiftTime(Time offset) {
    TCTANode::shiftTime(offset);
    if (clusterNode != NULL) clusterNode->shiftTime(offset);
    if (profileNode != NULL) profileNode->shiftTime(offset);
  }
  
  TCTClusterNode::TCTClusterNode(const TCTClusterNode& other, bool isVoid) : TCTANode(other) {
    if (isVoid) {
      numClusters = 0;
      avgRep = NULL;
    }
    else {
      numClusters = other.numClusters;
      if (other.avgRep != NULL) avgRep = other.avgRep->duplicate();
      else avgRep = NULL;
      for (int i = 0; i < numClusters; i++) {
        clusters[i].representative = other.clusters[i].representative->duplicate();
        clusters[i].members = other.clusters[i].members->duplicate();
      }
      for (int i = 0; i < numClusters; i++)
        for (int j = i+1; j < numClusters; j++)
          diffRatio[i][j] = other.diffRatio[i][j];
    }
  }

  TCTClusterNode::TCTClusterNode(const TCTClusterNode& cluster1, const TCTClusterNode& cluster2) : TCTANode(cluster1) {
    getTime().setAsAverageTime(cluster1.getTime(), cluster1.getWeight(), cluster2.getTime(), cluster1.getWeight());
    setWeight(cluster1.weight + cluster2.weight);
    setDerivedSemanticLabel(cluster1.derivedLabel & cluster2.derivedLabel);
    getPerfLossMetric().setDuratonMetric(cluster1.plm, cluster2.plm);
    
    numClusters = cluster1.numClusters + cluster2.numClusters;
    
    if (cluster1.avgRep != NULL && cluster2.avgRep != NULL) 
      avgRep = diffQ.mergeNode(cluster1.avgRep, cluster1.avgRep->getWeight(), cluster2.avgRep, cluster2.avgRep->getWeight(), false, false, false);
    else if (cluster1.avgRep != NULL)
      avgRep = cluster1.avgRep->duplicate();
    else if (cluster2.avgRep != NULL)
      avgRep = cluster2.avgRep->duplicate();
    else
      avgRep = NULL;
    if (avgRep != NULL) avgRep->clearDiffScore();
    
    for (int i = 0; i < cluster1.numClusters; i++) {
      clusters[i].representative = cluster1.clusters[i].representative->duplicate();
      clusters[i].members = cluster1.clusters[i].members->duplicate();
    }
    for (int i = 0; i < cluster2.numClusters; i++) {
      clusters[i+cluster1.numClusters].representative = cluster2.clusters[i].representative->duplicate();
      clusters[i+cluster1.numClusters].members = cluster2.clusters[i].members->duplicate();
    }
    
    for (int i = 0; i < cluster1.numClusters; i++)
      for (int j = i+1; j < cluster1.numClusters; j++)
        diffRatio[i][j] = cluster1.diffRatio[i][j];
    for (int i = 0; i < cluster2.numClusters; i++)
      for (int j = i+1; j < cluster2.numClusters; j++)
        diffRatio[i+cluster1.numClusters][j+cluster1.numClusters] = cluster2.diffRatio[i][j];
    for (int i = 0; i < cluster1.numClusters; i++)
      for (int j = 0; j < cluster2.numClusters; j++) {
        TCTANode* tempNode = diffQ.mergeNode(clusters[i].representative, 1, clusters[j+cluster1.numClusters].representative, 1, false, true, false);
        diffRatio[i][j+cluster1.numClusters] = tempNode->getDiffScore().getInclusive() / 
                (double)(clusters[i].representative->getDuration() + clusters[j+cluster1.numClusters].representative->getDuration());
        delete tempNode;
      }
    
    while (numClusters > 1 && mergeClusters());
  }
  
  void TCTClusterNode::computeAvgRep() {
    if (avgRep != NULL) 
      print_msg(MSG_PRIO_MAX, "ERROR: TCTClusterNode::computeAvgRep() called when avgRep isn't NULL.\n");
    avgRep = diffQ.computeAvgRep(this);
  }
  
  void TCTClusterNode::adjustIterationNumbers(long inc) {
    if (avgRep != NULL) avgRep->adjustIterationNumbers(inc);
    for (int i = 0; i < numClusters; i++) {
      clusters[i].members->shiftID(inc);
      clusters[i].representative->adjustIterationNumbers(inc);
    }
  }
  
  void TCTClusterNode::addChild(TCTANode* child, long idx) {
    child->adjustIterationNumbers(idx);
    child->initPerfLossMetric();
    
    clusters[numClusters].representative = child;
    child->setName("CLUSTER_#" + std::to_string(numClusters) + " REP");
    clusters[numClusters].members = new TCTClusterMembers();
    clusters[numClusters].members->addMember(idx);
    
    for (int k = 0; k < numClusters; k++) {
      TCTANode* tempNode = diffQ.mergeNode(child, 1, clusters[k].representative, 1, false, true, false);
      diffRatio[k][numClusters] = tempNode->getDiffScore().getInclusive() / (double)(child->getDuration() + clusters[k].representative->getDuration());
      delete tempNode;
    }
    
    numClusters++;
    while (numClusters > 1 && mergeClusters());
  }
  
  bool TCTClusterNode::mergeClusters() {
    int min_x = 0;
    int min_y = 1;
    double maxDR = diffRatio[min_x][min_y];
    
    for (int x = 0; x < numClusters; x++)
      for (int y = x+1; y < numClusters; y++)
        if (diffRatio[x][y] < diffRatio[min_x][min_y]) {
          min_x = x;
          min_y = y;
        }
        else if (diffRatio[x][y] > maxDR)
          maxDR = diffRatio[x][y];
    
    if (diffRatio[min_x][min_y] <= MIN_DIFF_RATIO ||
          diffRatio[min_x][min_y] <= maxDR * REL_DIFF_RATIO ||
          numClusters > MAX_NUM_CLUSTER) {
      TCTANode* tempNode = diffQ.mergeNode(clusters[min_x].representative, clusters[min_x].representative->getWeight(),
              clusters[min_y].representative, clusters[min_y].representative->getWeight(), true, false, false);
      delete clusters[min_x].representative;
      delete clusters[min_y].representative;
      clusters[min_x].representative = tempNode;
      tempNode->setName("CLUSTER_#" + std::to_string(min_x) + " REP");
      
      TCTClusterMembers* tempMembers = new TCTClusterMembers(*clusters[min_x].members, *clusters[min_y].members);
      delete clusters[min_x].members;
      delete clusters[min_y].members;
      clusters[min_x].members = tempMembers;
      
      // Remove the cluster at min_y. Update clusters[], diffRatio[][], and numClusters.
      for (int y = min_y+1; y < numClusters; y++) {
        clusters[y-1] = clusters[y];
        clusters[y-1].representative->setName("CLUSTER_#" + std::to_string(y-1) + " REP");
        for (int x = 0; x < min_y; x++)
          diffRatio[x][y-1] = diffRatio[x][y];
        for (int x = min_y + 1; x < y; x++)
          diffRatio[x-1][y-1] = diffRatio[x][y];
      }
      numClusters--;
      clusters[numClusters].members = NULL;
      clusters[numClusters].representative = NULL;
      
      // Recompute diffRatio values for the merged cluster.
      for (int k = 0; k < min_x; k++) {
        TCTANode* tempNode = diffQ.mergeNode(clusters[min_x].representative, 1, clusters[k].representative, 1, false, true, false);
        diffRatio[k][min_x] = tempNode->getDiffScore().getInclusive() / (double)(clusters[min_x].representative->getDuration() + clusters[k].representative->getDuration());
        delete tempNode;
      }
      
      for (int k = min_x+1; k < numClusters; k++) {
        TCTANode* tempNode = diffQ.mergeNode(clusters[min_x].representative, 1, clusters[k].representative, 1, false, true, false);
        diffRatio[min_x][k] = tempNode->getDiffScore().getInclusive() / (double)(clusters[min_x].representative->getDuration() + clusters[k].representative->getDuration());
        delete tempNode;
      }
      
      return true;
    }
    
    return false;
  }
  
  TCTNonCFGProfileNode::TCTNonCFGProfileNode(const TCTACFGNode& node) : TCTANode (node, NonCFGProf) {
    for (int k = 0; k < node.getNumChild(); k++) {
      TCTNonCFGProfileNode* child = TCTNonCFGProfileNode::newNonCFGProfileNode(node.getChild(k));
      child->getTime().toProfileTime();
      if (childMap.find(child->id) == childMap.end())
        childMap[child->id] = child;
      else {
        childMap[child->id]->merge(child);
        delete child;
      }
    }
  }
  
  TCTNonCFGProfileNode::TCTNonCFGProfileNode(const TCTLoopNode& loop) : TCTANode (loop, NonCFGProf) {
    print_msg(MSG_PRIO_MAX, "ERROR: conversion from TCTLoopNode to TCTNonCFGProfileNode shouldn't been called.\n");
    
    if (loop.getClusterNode() != NULL) {
      for (int k = 0; k < loop.getClusterNode()->getNumClusters(); k++) {
        TCTNonCFGProfileNode* child = TCTNonCFGProfileNode::newNonCFGProfileNode(loop.getClusterNode()->getClusterRepAt(k));
        child->setDepth(this->depth);
        child->amplify(child->weight, weight, getTime());
        for (auto iit = child->childMap.begin(); iit != child->childMap.end(); iit++) {
          if (childMap.find(iit->second->id) == childMap.end())
            childMap[iit->second->id] = (TCTNonCFGProfileNode*) iit->second->duplicate();
          else
            childMap[iit->second->id]->merge(iit->second);
        }
        delete child;
      }
    }
    
    if (loop.getRejectedIterations() != NULL) {
      TCTNonCFGProfileNode* child = TCTNonCFGProfileNode::newNonCFGProfileNode(loop.getRejectedIterations());
      child->setDepth(this->depth);
      for (auto iit = child->childMap.begin(); iit != child->childMap.end(); iit++) {
        if (childMap.find(iit->second->id) == childMap.end())
          childMap[iit->second->id] = (TCTNonCFGProfileNode*) iit->second->duplicate();
        else
          childMap[iit->second->id]->merge(iit->second);
      }
      delete child;
    }
    
    if (loop.hasPendingIteration()) {
      TCTNonCFGProfileNode* child = TCTNonCFGProfileNode::newNonCFGProfileNode(loop.getPendingIteration());
      child->setDepth(this->depth);
      for (auto iit = child->childMap.begin(); iit != child->childMap.end(); iit++) {
        if (childMap.find(iit->second->id) == childMap.end())
          childMap[iit->second->id] = (TCTNonCFGProfileNode*) iit->second->duplicate();
        else
          childMap[iit->second->id]->merge(iit->second);
      }
      delete child;
    }
  }
  
  TCTNonCFGProfileNode::TCTNonCFGProfileNode(const TCTNonCFGProfileNode& prof, bool copyChildMap) : TCTANode (prof) {
    if (copyChildMap)
      for (auto it = prof.childMap.begin(); it != prof.childMap.end(); it++)
        childMap[it->second->id] = (TCTNonCFGProfileNode*) it->second->duplicate();
  }
  
  void TCTNonCFGProfileNode::addChild(TCTANode* child) {
    TCTNonCFGProfileNode* profChild = NULL; 
    if (child->type == TCTANode::NonCFGProf)
      profChild = (TCTNonCFGProfileNode*) child;
    else {
      profChild = TCTNonCFGProfileNode::newNonCFGProfileNode(child);
      delete child;
    }

    if (childMap.find(profChild->id) == childMap.end())
      childMap[profChild->id] = profChild;
    else {
      childMap[profChild->id]->merge(profChild);
      delete profChild;
    }
  }
  
  void TCTNonCFGProfileNode::merge(const TCTNonCFGProfileNode* other) {
    retCount += other->retCount;
    time.addTime(other->time);
    
    /*
    if (getMaxDuration() - getMinDuration() > getSamplingPeriod() * MAX_SAMPLE_NOISE) {
      Time mid = getDuration();
      Time samplingPeriod = getSamplingPeriod();
      getTime().setDuration(mid - MAX_SAMPLE_NOISE * samplingPeriod / 2, mid + MAX_SAMPLE_NOISE * samplingPeriod / 2);
    }*/
    
    for (auto it = other->childMap.begin(); it != other->childMap.end(); it++) {
      if (childMap.find(it->second->id) == childMap.end())
        childMap[it->second->id] = (TCTNonCFGProfileNode*) it->second->duplicate();
      else
        childMap[it->second->id]->merge(it->second);
    }
  }
  
  Time TCTNonCFGProfileNode::getExclusiveDuration() const {
    Time exclusive = getDuration();
    for (auto it = childMap.begin(); it != childMap.end(); it++)
      if (it->second->isLoop())
        exclusive -= (it->second->getDuration() - it->second->getExclusiveDuration());
      else
        exclusive -= it->second->getDuration();
    return exclusive;
  }
  
  void TCTNonCFGProfileNode::getExclusiveDuration(Time& minExclusive, Time& maxExclusive) const {
    minExclusive = getMinDuration();
    maxExclusive = getMaxDuration();
    for (auto it = childMap.begin(); it != childMap.end(); it++) {
      TCTNonCFGProfileNode* child = it->second;
      minExclusive -= child->getMaxDuration();
      maxExclusive -= child->getMinDuration();
    }
  }
  
  void TCTNonCFGProfileNode::amplify(long amplifier, long divider, const TCTTime& parent_time) {  
    getTime().setNumSamples(getNumSamples() * amplifier / divider);
    getTime().setDuration(getMinDuration() * amplifier / divider, getMaxDuration() * amplifier / divider);
    if (getTime().getMaxDuration() > parent_time.getMaxDuration()) {
      long diff = getTime().getMaxDuration() - parent_time.getMaxDuration();
      getTime().setDuration(getMinDuration() + diff, getMaxDuration() - diff);
    }
    
    for (auto it = childMap.begin(); it != childMap.end(); it++)
      it->second->amplify(amplifier, divider, getTime());
    
    setWeight(weight * divider / amplifier);
  }
  
  /*
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
  */
  
  TCTACFGNode::TCTACFGNode(const TCTACFGNode& orig) : TCTANode(orig), isProfile(orig.isProfile) {
    unordered_map<TCTID, TCTANode*> childTable;
    for (auto it = orig.children.begin(); it != orig.children.end(); it++) {
      children.push_back((*it)->duplicate());
      childTable[children.back()->id] = children.back();
    }
    childTable[dummyBeginNode.id] = &dummyBeginNode;
    childTable[dummyEndNode.id] = &dummyEndNode;

    for (auto it = orig.outEdges.begin(); it != orig.outEdges.end(); it++) {
      outEdges[it->first] = unordered_set<Edge*>();
      for (auto eit = it->second.begin(); eit != it->second.end(); eit++) {
        Edge* newEdge = (*eit)->duplicate();
        newEdge->setSrc(childTable[newEdge->getSrc()->id]);
        newEdge->setDst(childTable[newEdge->getDst()->id]);
        outEdges[it->first].insert(newEdge);
      }
    }
  }
  
  Time TCTACFGNode::getExclusiveDuration() const {
    Time exclusive = getDuration();
    for (auto it = children.begin(); it != children.end(); it++)
      if ((*it)->isLoop()) 
        exclusive -= ((*it)->getDuration() - (*it)->getExclusiveDuration());
      else
        exclusive -= (*it)->getDuration();
    return exclusive;
  }
  
  void TCTACFGNode::getExclusiveDuration(Time& minExclusive, Time& maxExclusive) const {
    minExclusive = getMinDuration();
    maxExclusive = getMaxDuration();
    for (auto it = children.begin(); it != children.end(); it++) {
      TCTANode* child = *it;
      minExclusive -= child->getMaxDuration();
      maxExclusive -= child->getMinDuration();
    }
  }
  
  void TCTACFGNode::finishInit() {
    TCTANode::finishInit();
    
    outEdges[dummyBeginNode.id] = unordered_set<Edge*>();
    if (getNumChild() > 0) {
      Edge* edge = new Edge(&dummyBeginNode, children[0], 1);
      outEdges[dummyBeginNode.id].insert(edge);
    }
    
    for (int i = 0; i < getNumChild(); i++) {
      outEdges[children[i]->id] = unordered_set<Edge*>();
      if (i+1 < getNumChild()) {
        Edge* edge = new Edge(children[i], children[i+1], 1);
        outEdges[children[i]->id].insert(edge);
      }
      else {
        Edge* edge = new Edge(children[i], &dummyEndNode, 1);
        outEdges[children[i]->id].insert(edge);
      }
    }
  }
  
  const unordered_set<TCTACFGNode::Edge*>& TCTACFGNode::getEntryEdges() const {
    return outEdges.at(dummyBeginNode.id);
  }
  
  void TCTACFGNode::setEdges(vector<Edge*>& edges) {
    outEdges[dummyBeginNode.id] = unordered_set<Edge*>();
    for (int i = 0; i < getNumChild(); i++)
      outEdges[children[i]->id] = unordered_set<Edge*>();
    
    while (!edges.empty()) {
      Edge* edge = edges.back();
      outEdges[edge->getSrc()->id].insert(edge);
      edges.pop_back();
    }
  }
  
  void TCTACFGNode::toCFGProfile() {
    if (isProfile)
      return;
    
    isProfile = true;
    for (uint i = 0; i < children.size(); i++) {
      children[i]->getTime().toProfileTime();
      if (children[i]->type == TCTANode::Loop) {
        TCTLoopNode* loop = (TCTLoopNode*)children[i];
        children[i] = loop->getProfileNode()->duplicate();

        for (auto it = outEdges.begin(); it != outEdges.end(); it++)
          for (auto eit = it->second.begin(); eit != it->second.end(); eit++) {
            if ((*eit)->getSrc() == loop)
              (*eit)->setSrc(children[i]);
            if ((*eit)->getDst() == loop)
              (*eit)->setDst(children[i]);
          }
        
        delete loop;
      }
      else if (children[i]->type != TCTANode::NonCFGProf)
        ((TCTACFGNode*)children[i])->toCFGProfile();
    }
  }
  
  TCTFunctionTraceNode TCTACFGNode::dummyBeginNode(-1, -1, "dummy begin node", -1, NULL, 0, SEMANTIC_LABEL_COMPUTATION);
  TCTFunctionTraceNode TCTACFGNode::dummyEndNode(-2, -2, "dummy end node", -2, NULL, 0, SEMANTIC_LABEL_COMPUTATION);
  
  /*
  TCTCFGProfileNode* TCTCFGProfileNode::newCFGProfileNode(const TCTANode* node) {
    if (node->type == TCTANode::NonCFGProf)
      return NULL;
    if (node->type == TCTANode::Loop) {
      if (((TCTLoopNode*)node)->profileNode != NULL)
        return newCFGProfileNode(((TCTLoopNode*)node)->profileNode);
      else 
        return new TCTCFGProfileNode(*((TCTLoopNode*)node));
    }
    return new TCTCFGProfileNode(*((TCTACFGNode*)node));
  }

  TCTCFGProfileNode::TCTCFGProfileNode(const TCTACFGNode& node) : TCTACFGNode (node, CFGProf) {
    unordered_map<TCTID, TCTANode*> childTable;
    for (int idx = 0; idx < node.getNumChild(); idx++) {
      children.push_back(node.getChild(idx)->duplicate());
      childTable[children.back()->id] = children.back();
    }
    childTable[dummyBeginNode.id] = &dummyBeginNode;
    childTable[dummyEndNode.id] = &dummyEndNode;

    for (auto it = node.outEdges.begin(); it != node.outEdges.end(); it++) {
      outEdges[it->first] = unordered_set<Edge*>();
      for (auto eit = it->second.begin(); eit != it->second.end(); eit++) {
        Edge* newEdge = (*eit)->duplicate();
        newEdge->setSrc(childTable[newEdge->getSrc()->id]);
        newEdge->setDst(childTable[newEdge->getDst()->id]);
        outEdges[it->first].insert(newEdge);
      }
    }
  }
  
  TCTCFGProfileNode::TCTCFGProfileNode(const TCTLoopNode& node) : TCTACFGNode (node, CFGProf) {
    
  }
  */
}
