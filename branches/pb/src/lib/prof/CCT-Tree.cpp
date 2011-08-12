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
// Copyright ((c)) 2002-2011, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;
using std::endl;
using std::hex;
using std::dec;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <set>
using std::set;

#include <typeinfo>

#include <sstream>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "CCT-Tree.hpp"
#include "CallPath-Profile.hpp" // for CCT::Tree::metadata()

#include <lib/xml/xml.hpp> 

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/StrUtil.hpp>
#include <lib/support/Trace.hpp>

//*************************** Forward Declarations ***************************


//***************************************************************************

//***************************************************************************
// Tree
//***************************************************************************

namespace Prof {

namespace CCT {
  
Tree::Tree(const CallPath::Profile* metadata)
  : m_root(NULL), m_metadata(metadata),
    m_maxDenseId(0), m_nodeidMap(NULL),
    m_mergeCtxt(NULL)
{
}


Tree::~Tree()
{
  delete m_root;
  m_metadata = NULL;
  delete m_nodeidMap;
  delete m_mergeCtxt;
}


MergeEffectList*
Tree::merge(const Tree* y, uint x_newMetricBegIdx, uint mrgFlag, uint oFlag)
{
  Tree* x = this;
  ANode* x_root = root();
  ANode* y_root = y->root();
  
  // -------------------------------------------------------
  // Merge pre-condition: both x and y should be "locally merged",
  // i.e. they should be equal except for metrics and children.
  // -------------------------------------------------------
  bool isPrecondition = false;
  if (typeid(*x_root) == typeid(CCT::Root)
      && typeid(*y_root) == typeid(CCT::Root)) {
    // Case 1
    isPrecondition = true;
  }
  else {
    ADynNode* x_dyn = dynamic_cast<ADynNode*>(x_root);
    ADynNode* y_dyn = dynamic_cast<ADynNode*>(y_root);
    if (x_dyn && y_dyn && ADynNode::isMergable(*x_dyn, *y_dyn)) {
      // Case 2a
      isPrecondition = true;
    }
    else if ((x_dyn->childCount() == 0 || y_dyn->childCount() == 0)
	     && x_dyn->isPrimarySynthRoot() && y_dyn->isPrimarySynthRoot()) {
      // Case 2b (A special sub-case of Case 1): (a) Neither tree x
      // nor y have been canonicalized (and therefore do not have a
      // CCT::Root node); and (b) one of x or y is a single-node tree.
      isPrecondition = true;
    }
  }
  DIAG_Assert(isPrecondition, "Prof::CCT::Tree::merge: Merge precondition fails!");

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  if (!m_mergeCtxt) {
    bool doTrackCPIds = !metadata()->traceFileNameSet().empty();
    m_mergeCtxt = new MergeContext(x, doTrackCPIds);
  }
  m_mergeCtxt->flags(mrgFlag);
  
  MergeEffectList* mrgEffects =
    x_root->mergeDeep(y_root, x_newMetricBegIdx, *m_mergeCtxt, oFlag);

  DIAG_If(0) {
    verifyUniqueCPIds();
  }

  return mrgEffects;
}


void
Tree::pruneCCTByNodeId(const uint8_t* prunedNodes)
{
  m_root->pruneChildrenByNodeId(prunedNodes);
  DIAG_Assert(!prunedNodes[m_root->id()], "Prof::CCT::Tree::pruneCCTByNodeId(): cannot delete root!");
  
  //Prof::CCT::ANode::pruneByNodeId(m_root, prunedNodes);
  //DIAG_Assert(m_root, "Prof::CCT::Tree::pruneCCTByNodeId(): cannot delete root!");
}


uint
Tree::makeDensePreorderIds()
{
  uint nextId = 1; // cf. s_nextUniqueId
  nextId = m_root->makeDensePreorderIds(nextId);

  m_maxDenseId = (nextId - 1);
  return m_maxDenseId;
}


ANode*
Tree::findNode(uint nodeId) const
{
  if (!m_nodeidMap) {
    m_nodeidMap = new NodeIdToANodeMap;
    for (ANodeIterator it(m_root); it.Current(); ++it) {
      ANode* n = it.current();
      m_nodeidMap->insert(std::make_pair(n->id(), n));
    }
  }
  NodeIdToANodeMap::iterator it = m_nodeidMap->find(nodeId);
  return (it != m_nodeidMap->end()) ? it->second : NULL;
}


bool
Tree::verifyUniqueCPIds()
{
  bool areAllUnique = true;
  MergeContext::CPIdSet cpIdSet;

  for (ANodeIterator it(root()); it.Current(); ++it) {
    ANode* n = it.current();

    ADynNode* n_dyn = dynamic_cast<ADynNode*>(n);
    if (n_dyn && n_dyn->cpId() != HPCRUN_FMT_CCTNodeId_NULL) {
      std::pair<MergeContext::CPIdSet::iterator, bool> ret =
	cpIdSet.insert(n_dyn->cpId());
      bool isInserted = ret.second;
      areAllUnique = areAllUnique && isInserted;
      DIAG_MsgIf(!isInserted,  "CCT::Tree::verifyUniqueCPIds: Duplicate CPId: " << n_dyn->cpId());
    }
  }

  return areAllUnique;
}


std::ostream&
Tree::writeXML(std::ostream& os, uint metricBeg, uint metricEnd,
	       uint oFlags) const
{
  if (m_root) {
    m_root->writeXML(os, metricBeg, metricEnd, oFlags);
  }
  return os;
}
void
Tree::writePB(google::protobuf::io::CodedOutputStream* cos,uint beg,uint end,
	      int prettyPrint)
{
  if (m_root){
    m_root->writePB(cos, 0,beg,end, prettyPrint,0);
  }
}

std::ostream& 
Tree::dump(std::ostream& os, uint oFlags) const
{
  writeXML(os, Metric::IData::npos, Metric::IData::npos, oFlags);
  return os;
}


void 
Tree::ddump() const
{
  dump(std::cerr, Tree::OFlg_DebugAll);
}


} // namespace CCT

} // namespace Prof



namespace Prof {

namespace CCT {

//***************************************************************************
// ANodeTy `methods' (could completely replace with dynamic typing)
//***************************************************************************

const string ANode::NodeNames[ANode::TyNUMBER] = {
  "Root", "PF", "Pr", "L", "C", "S", "Any"
};


const string&
ANode::ANodeTyToName(ANodeTy tp)
{
  return NodeNames[tp];
}


ANode::ANodeTy
ANode::IntToANodeType(long i)
{
  DIAG_Assert((i >= 0) && (i < TyNUMBER), "");
  return (ANodeTy)i;
}

uint ANode::s_nextUniqueId = 2;


//***************************************************************************
// ANode, etc: constructors/destructors
//***************************************************************************

uint ANode::s_raToCallsiteOfst = 1;

string AProcNode::BOGUS;


//***************************************************************************
// ANode, etc: Tree Navigation 
//***************************************************************************

#define dyn_cast_return(base, derived, expr) \
    { base* ptr = expr;  \
      if (ptr == NULL) {  \
        return NULL;  \
      } else {  \
	return dynamic_cast<derived*>(ptr);  \
      } \
    }


ANode* 
ANode::ancestor(ANodeTy tp) const
{
  ANode* s = const_cast<ANode*>(this); 
  while (s && s->type() != tp) {
    s = s->parent(); 
  } 
  return s;
} 


#if 0 // is this obsolete?
int
isAncestorOf(ANode* parent, ANode* son, int difference)
{
  ANode *iter = son;
  while (iter && difference > 0 && iter != parent) {
    iter = iter->Parent();
    difference--;
  }
  if (iter && iter == parent) {
    return 1;
  }
  return 0;
}
#endif


Root*
ANode::ancestorRoot() const 
{
  if (Parent() == NULL) {
    return NULL;
  }
  else { 
    dyn_cast_return(ANode, Root, ancestor(TyRoot));
  }
}


ProcFrm*
ANode::ancestorProcFrm() const
{
  dyn_cast_return(ANode, ProcFrm, ancestor(TyProcFrm)); 
}


Proc*
ANode::ancestorProc() const
{
  dyn_cast_return(ANode, Proc, ancestor(TyProc)); 
}


Loop*
ANode::ancestorLoop() const 
{
  dyn_cast_return(ANode, Loop, ancestor(TyLoop));
}


Call*
ANode::ancestorCall() const
{
  dyn_cast_return(ANode, Call, ancestor(TyCall)); 
}


Stmt*
ANode::ancestorStmt() const 
{
  dyn_cast_return(ANode, Stmt, ancestor(TyStmt));
}


//***************************************************************************
// Metrics
//***************************************************************************

void
ANode::zeroMetricsDeep(uint mBegId, uint mEndId)
{
  if ( !(mBegId < mEndId) ) {
    return; // short circuit
  }

  for (ANodeIterator it(this); it.Current(); ++it) {
    ANode* n = it.current();
    n->zeroMetrics(mBegId, mEndId);
  }
}


void
ANode::aggregateMetricsIncl(uint mBegId, uint mEndId)
{
  VMAIntervalSet ivalset; // TODO: cheat using a VMAInterval set
  for (uint mId = mBegId; mId < mEndId; ++mId) {
    ivalset.insert(VMAInterval(mId, mId + 1)); // [ )
  }
  aggregateMetricsIncl(ivalset);
}


void
ANode::aggregateMetricsIncl(const VMAIntervalSet& ivalset)
{
  if (ivalset.empty()) {
    return; // short circuit
  }

  const ANode* root = this;
  ANodeIterator it(root, NULL/*filter*/, false/*leavesOnly*/,
		   IteratorStack::PostOrder);
  for (ANode* n = NULL; (n = it.current()); ++it) {
    if (n != root) {
      ANode* n_parent = n->parent();
      
      for (VMAIntervalSet::const_iterator it1 = ivalset.begin();
	   it1 != ivalset.end(); ++it1) {
	const VMAInterval& ival = *it1;
	uint mBegId = (uint)ival.beg(), mEndId = (uint)ival.end();

	for (uint mId = mBegId; mId < mEndId; ++mId) {
	  double mVal = n->demandMetric(mId, mEndId/*size*/);
	  n_parent->demandMetric(mId, mEndId/*size*/) += mVal;
	}
      }
    }
  }
}


void
ANode::aggregateMetricsExcl(uint mBegId, uint mEndId)
{
  VMAIntervalSet ivalset; // TODO: cheat using a VMAInterval set
  for (uint mId = mBegId; mId < mEndId; ++mId) {
    ivalset.insert(VMAInterval(mId, mId + 1)); // [ )
  }
  aggregateMetricsExcl(ivalset);
}


void
ANode::aggregateMetricsExcl(const VMAIntervalSet& ivalset)
{
  if (ivalset.empty()) {
    return; // short circuit
  }

  ProcFrm* frame = NULL; // will be set during tree traversal
  aggregateMetricsExcl(frame, ivalset);
}


void
ANode::aggregateMetricsExcl(ProcFrm* frame, const VMAIntervalSet& ivalset)
{
  ANode* n = this;

  // -------------------------------------------------------
  // Pre-order visit
  // -------------------------------------------------------
  bool isFrame = (typeid(*n) == typeid(ProcFrm));
  ProcFrm* frameNxt = (isFrame) ? static_cast<ProcFrm*>(n) : frame;

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  for (ANodeChildIterator it(n); it.Current(); ++it) {
    ANode* x = it.current();
    x->aggregateMetricsExcl(frameNxt, ivalset);
  }

  // -------------------------------------------------------
  // Post-order visit
  // -------------------------------------------------------
  if (typeid(*n) == typeid(CCT::Stmt)) {
    ANode* n_parent = n->parent();

    for (VMAIntervalSet::const_iterator it = ivalset.begin();
	 it != ivalset.end(); ++it) {
      const VMAInterval& ival = *it;
      uint mBegId = (uint)ival.beg(), mEndId = (uint)ival.end();

      for (uint mId = mBegId; mId < mEndId; ++mId) {
	double mVal = n->demandMetric(mId, mEndId/*size*/);
	n_parent->demandMetric(mId, mEndId/*size*/) += mVal;
	if (frame && frame != n_parent) {
	  frame->demandMetric(mId, mEndId/*size*/) += mVal;
	}
      }
    }
  }
}


void
ANode::computeMetrics(const Metric::Mgr& mMgr, uint mBegId, uint mEndId)
{
  if ( !(mBegId < mEndId) ) {
    return;
  }
  
  // N.B. pre-order walk assumes point-wise metrics
  // Cf. Analysis::Flat::Driver::computeDerivedBatch().

  for (ANodeIterator it(this); it.Current(); ++it) {
    ANode* n = it.current();
    n->computeMetricsMe(mMgr, mBegId, mEndId);
  }
}


void
ANode::computeMetricsMe(const Metric::Mgr& mMgr, uint mBegId, uint mEndId)
{
  //uint numMetrics = mMgr.size();

  for (uint mId = mBegId; mId < mEndId; ++mId) {
    const Metric::ADesc* m = mMgr.metric(mId);
    const Metric::DerivedDesc* mm = dynamic_cast<const Metric::DerivedDesc*>(m);
    if (mm && mm->expr()) {
      const Metric::AExpr* expr = mm->expr();
      expr->evalNF(*this);
      // double val = eval(); demandMetric(mId, numMetrics/*size*/) = val;
    }
  }
}


void
ANode::computeMetricsIncr(const Metric::Mgr& mMgr, uint mBegId, uint mEndId,
			  Metric::AExprIncr::FnTy fn)
{
  if ( !(mBegId < mEndId) ) {
    return;
  }
  
  // N.B. pre-order walk assumes point-wise metrics
  // Cf. Analysis::Flat::Driver::computeDerivedBatch().

  for (ANodeIterator it(this); it.Current(); ++it) {
    ANode* n = it.current();
    n->computeMetricsIncrMe(mMgr, mBegId, mEndId, fn);
  }
}


void
ANode::computeMetricsIncrMe(const Metric::Mgr& mMgr, uint mBegId, uint mEndId,
			    Metric::AExprIncr::FnTy fn)
{
  for (uint mId = mBegId; mId < mEndId; ++mId) {
    const Metric::ADesc* m = mMgr.metric(mId);
    const Metric::DerivedIncrDesc* mm =
      dynamic_cast<const Metric::DerivedIncrDesc*>(m);
    if (mm && mm->expr()) {
      const Metric::AExprIncr* expr = mm->expr();
      switch (fn) {
        case Metric::AExprIncr::FnInit:
	  expr->initialize(*this); break;
        case Metric::AExprIncr::FnInitSrc:
	  expr->initializeSrc(*this); break;
        case Metric::AExprIncr::FnAccum:
	  expr->accumulate(*this); break;
        case Metric::AExprIncr::FnCombine:
	  expr->combine(*this); break;
        case Metric::AExprIncr::FnFini:
	  expr->finalize(*this); break;
        default:
	  DIAG_Die(DIAG_UnexpectedInput);
      }
    }
  }
}


void
ANode::pruneByMetrics(const Metric::Mgr& mMgr, const VMAIntervalSet& ivalset,
		      const ANode* root, double thresholdPct,
		      uint8_t* prunedNodes)
{
  for (ANodeChildIterator it(this); it.Current(); /* */) {
    ANode* x = it.current();
    it++; // advance iterator -- it is pointing at 'x'


    // ----------------------------------------------------------
    // Determine whether 'x' is important: any inclusive metric >= threshold
    // ----------------------------------------------------------
    uint numIncl = 0;
    bool isImportant = false;

    for (VMAIntervalSet::const_iterator it1 = ivalset.begin();
	 it1 != ivalset.end(); ++it1) {
      const VMAInterval& ival = *it1;
      uint mBegId = (uint)ival.beg(), mEndId = (uint)ival.end();

      for (uint mId = mBegId; mId < mEndId; ++mId) {
	const Prof::Metric::ADesc* m = mMgr.metric(mId);
	if (m->type() != Metric::ADesc::TyIncl) {
	  continue;
	}
	numIncl++;
	
	double total = root->metric(mId); // root->metric(m->partner()->id());
	
	double pct = x->metric(mId) * 100 / total;
	if (pct >= thresholdPct) {
	  isImportant = true;
	  break;
	}
      }
      if (isImportant) { break; }
    }

    // ----------------------------------------------------------
    // 
    // ----------------------------------------------------------
    if (isImportant || numIncl == 0) {
      x->pruneByMetrics(mMgr, ivalset, root, thresholdPct, prunedNodes);
    }
    else {
      deleteChaff(x, prunedNodes);
    }
  }
}


#if 0
void
ANode::pruneByNodeId(ANode*& x, const uint8_t* prunedNodes)
{
  // Visiting in preorder can save a little work since a whole subtree
  // can be deleted (factoring out the destructor's recursion)
  if (prunedNodes[x->id()]) {
    x->unlink(); // unlink 'x' from tree
    delete x;
    x = NULL;
  }
  else {
    for (ANodeChildIterator it(x); it.Current(); /* */) {
      ANode* x_child = it.current();
      it++; // advance iterator -- it is pointing at 'x_child'
      pruneByNodeId(x_child, prunedNodes);
    }
  }
}
#endif


void
ANode::pruneChildrenByNodeId(const uint8_t* prunedNodes)
{
  ANode* x = this;

  for (ANodeChildIterator it(x); it.Current(); /* */) {
    ANode* x_child = it.current();
    it++; // advance iterator -- it is pointing at 'x_child'

    // Visiting in preorder can save a little work since a whole subtree
    // can be deleted (factoring out the destructor's recursion)
    if (prunedNodes[x_child->id()]) {
      x_child->unlink(); // unlink 'x_child' from tree
      delete x_child;
    }
    else {
      x_child->pruneChildrenByNodeId(prunedNodes);
    }
  }
}

bool
ANode::deleteChaff(ANode* x, uint8_t* deletedNodes)
{
  bool x_isLeaf = x->isLeaf(); // N.B. must perform before below
  uint x_id = x->id();

  bool wereAllChildrenDeleted = true;
  for (ANodeChildIterator it(x); it.Current(); /* */) {
    ANode* x_child = it.current();
    it++; // advance iterator -- it is pointing at 'x_child'
    
    wereAllChildrenDeleted = (wereAllChildrenDeleted 
			      && deleteChaff(x_child, deletedNodes));
  }

  bool wasDeleted = false;
  if (wereAllChildrenDeleted) {
    Prof::CCT::ADynNode* x_dyn = dynamic_cast<Prof::CCT::ADynNode*>(x);
    bool mustRetain = (x_dyn && hpcrun_fmt_doRetainId(x_dyn->cpId()));
    if ((x_isLeaf && !mustRetain) || !x_isLeaf) {
      x->unlink(); // unlink 'x' from tree
      delete x;
      if (deletedNodes) {
	deletedNodes[x_id] = 1;
      }
      wasDeleted = true;
    }
  }

  return wasDeleted;
}


//**********************************************************************
// Merging
//**********************************************************************

MergeEffectList*
ANode::mergeDeep(ANode* y, uint x_newMetricBegIdx, MergeContext& mrgCtxt,
		 uint oFlag)
{
  ANode* x = this;

  // ------------------------------------------------------------
  // 0. If y is childless, return.
  // ------------------------------------------------------------
  if (y->isLeaf()) {
    return NULL;
  }

  // ------------------------------------------------------------
  // 1. If a child y_child of y _does not_ appear as a child of x,
  //    then copy (subtree) y_child [fixing its metrics], make it a
  //    child of x and return.
  // 2. If a child y_child of y _does_ have a corresponding child
  //    x_child of x, merge [the metrics of] y_child into x_child and
  //    recur.
  // ------------------------------------------------------------
  MergeEffectList* effctLst = new MergeEffectList;
  
  for (ANodeChildIterator it(y); it.Current(); /* */) {
    ANode* y_child = it.current();
    ADynNode* y_child_dyn = dynamic_cast<ADynNode*>(y_child);
    DIAG_Assert(y_child_dyn, "ANode::mergeDeep");
    it++; // advance iterator -- it is pointing at 'child'

    MergeEffectList* effctLst1 = NULL;

    ADynNode* x_child_dyn = x->findDynChild(*y_child_dyn);

    if (!x_child_dyn) {
      // case 1: insert nodes
      DIAG_Assert( !(mrgCtxt.flags() & MrgFlg_AssertCCTMergeOnly),
		   "CCT::ANode::mergeDeep: adding not permitted");
      if ( !(mrgCtxt.flags() & MrgFlg_CCTMergeOnly) ) {
	DIAG_MsgIf(0 /*(oFlag & Tree::OFlg_Debug)*/,
		   "CCT::ANode::mergeDeep: Adding:\n     "
		   << y_child->toStringMe(Tree::OFlg_Debug));
	y_child->unlink();
  
	effctLst1 = y_child->mergeDeep_fixInsert(x_newMetricBegIdx, mrgCtxt);

	y_child->link(x);
      }
    }
    else {
      // case 2: merge nodes
      DIAG_MsgIf(0 /*(oFlag & Tree::OFlg_Debug)*/,
		 "CCT::ANode::mergeDeep: Merging x <= y:\n"
		 << "  x: " << x_child_dyn->toStringMe(Tree::OFlg_Debug)
		 << "\n  y: " << y_child_dyn->toStringMe(Tree::OFlg_Debug));
      MergeEffect effct =
	x_child_dyn->mergeMe(*y_child_dyn, &mrgCtxt, x_newMetricBegIdx);
      if (mrgCtxt.doPropagateEffects() && !effct.isNoop()) {
	effctLst->push_back(effct);
      }
      
      effctLst1 = x_child_dyn->mergeDeep(y_child, x_newMetricBegIdx, mrgCtxt,
					 oFlag);
    }

    if (effctLst1 && !effctLst1->empty()) {
      effctLst->splice(effctLst->end(), *effctLst1);
      DIAG_MsgIf(0, MergeEffect::toString(*effctLst));
    }
    delete effctLst1;
  }

  return effctLst;
}


MergeEffect
ANode::merge(ANode* y)
{
  ANode* x = this;
  
  // 1. copy y's metrics into x
  MergeEffect effct = x->mergeMe(*y);
  
  // 2. copy y's children into x
  for (ANodeChildIterator it(y); it.Current(); /* */) {
    ANode* y_child = it.current();
    it++; // advance iterator -- it is pointing at 'y_child'
    y_child->unlink();
    y_child->link(x);
  }
  
  y->unlink();
  delete y;

  return effct;
}


MergeEffect
ANode::mergeMe(const ANode& y, MergeContext* GCC_ATTR_UNUSED mrgCtxt,
	       uint metricBegIdx)
{
  ANode* x = this;
  
  uint x_end = metricBegIdx + y.numMetrics(); // open upper bound
  if ( !(x_end <= x->numMetrics()) ) {
    ensureMetricsSize(x_end);
  }

  for (uint x_i = metricBegIdx, y_i = 0; x_i < x_end; ++x_i, ++y_i) {
    x->metric(x_i) += y.metric(y_i);
  }
  
  MergeEffect noopEffect;
  return noopEffect;
}


MergeEffect
ADynNode::mergeMe(const ANode& y, MergeContext* mrgCtxt, uint metricBegIdx)
{
  // N.B.: Assumes ADynNode::isMergable() holds
  ADynNode* x = this;

  const ADynNode* y_dyn = dynamic_cast<const ADynNode*>(&y);
  DIAG_Assert(y_dyn, "ADynNode::mergeMe: " << DIAG_UnexpectedInput);

  MergeEffect effct = ANode::mergeMe(y, mrgCtxt, metricBegIdx);

  // merge cp-ids
  if (hasMergeEffects(*x, *y_dyn)) {
    // 1. Conflicting ids:
    //    => keep x's cpId; within y, translate [y_dyn->m_cpId ==> m_cpId]
    effct.old_cpId = y_dyn->m_cpId;
    effct.new_cpId = m_cpId;
  }
  else if (y_dyn->cpId() == HPCRUN_FMT_CCTNodeId_NULL) {
    // 2. Trivial conflict: y's cpId is NULL; x's may or may not be NULL:
    //    => keep x's cpId.
  }
  else if (m_cpId == HPCRUN_FMT_CCTNodeId_NULL) {
    // 3. Semi-trivial conflict: x's cpId is NULL, but y's is not
    //     => use y's cpId *if* it does not conflict with one already
    //        in x's tree.
    DIAG_Assert(mrgCtxt, "ADynNode::mergeMe: potentially introducing cp-id conflicts; cannot verify without MergeContext!");

    MergeContext::pair ret = mrgCtxt->ensureUniqueCPId(y_dyn->cpId());
    m_cpId = ret.cpId;
    DIAG_Assert(effct.isNoop(), DIAG_UnexpectedInput);
    effct = ret.effect;
  }
  
  return effct;
}


ADynNode*
ANode::findDynChild(const ADynNode& y_dyn)
{
  for (ANodeChildIterator it(this); it.Current(); ++it) {
    ANode* x = it.current();

    ADynNode* x_dyn = dynamic_cast<ADynNode*>(x);
    if (x_dyn) {
      // Base case: an ADynNode descendent
      if (ADynNode::isMergable(*x_dyn, y_dyn)) {
	return x_dyn;
      }
    }
    else {
      // Inductive case: some other type; find the first ADynNode descendents.
      ADynNode* x_dyn_descendent = x->findDynChild(y_dyn);
      if (x_dyn_descendent) {
	return x_dyn_descendent;
      }
    }
  }
  return NULL;
}


MergeEffectList*
ANode::mergeDeep_fixInsert(int newMetrics, MergeContext& mrgCtxt)
{
  // Assumes: While merging CCT::Tree y into CCT::Tree x, subtree
  // 'this', which used to live in 'y', has just been inserted into
  // 'x'.

  MergeEffectList* effctLst = new MergeEffectList();

  for (ANodeIterator it(this); it.Current(); ++it) {
    ANode* n = it.current();

    // -----------------------------------------------------
    // 1. Ensure no cpId in subtree 'this' conflicts with an existing
    // cpId in CCT::Tree x.
    // -----------------------------------------------------
    ADynNode* n_dyn = dynamic_cast<ADynNode*>(n);
    if (n_dyn) {
      MergeContext::pair ret = mrgCtxt.ensureUniqueCPId(n_dyn->cpId());
      n_dyn->cpId(ret.cpId);
      if (!ret.effect.isNoop()) {
	effctLst->push_back(ret.effect);
      }
    }

    // -----------------------------------------------------
    // 2. Make space for the metrics of CCT::Tree x
    // -----------------------------------------------------
    n->insertMetricsBefore(newMetrics);
  }
  
  return effctLst;
}

//**********************************************************************
//  Constructors - Reading from input
//**********************************************************************
/*
Root::Root(Nodes::Root* root,ANode* parent,
	   Prof::Struct::ACodeNode* anode):ANode(TyRoot,parent)
{
  id(root->id());
  m_name = root->name();
  m_strct=anode;
  for(int i=0;i<root->metric_values().size();i++) 
  {
    demandMetric(root->metric_values(i).name())=root->metric_values(i).value();
  }
}
*/
/*
ProcFrm::ProcFrm(Nodes::ProcFrm* procfrm, ANode* parent,
	    Prof::Struct::ACodeNode* anode)
            :AProcNode(TyProcFrm,parent,anode)
{
  id(procfrm->id());
  for(int i=0;i<procfrm->metric_values().size();i++) 
  {
    demandMetric(procfrm->metric_values(i).name())=procfrm->metric_values(i).value();
  }
}
*/
/*
Proc::Proc(Nodes::Proc* proc, ANode* parent,
	   Prof::Struct::ACodeNode* anode)
           :AProcNode(TyProc,parent, anode)
{
  id(proc->id());
  for(int i=0;i<proc->metric_values().size();i++) 
  {
    demandMetric(proc->metric_values(i).name())=proc->metric_values(i).value();
  }
}
*/
/*
Loop::Loop(Nodes::Loop* loop, ANode* parent,
	   Prof::Struct::ACodeNode* anode):ANode(TyLoop,parent)
{
  id(loop->id());
  m_strct=anode;
  for(int i=0;i<loop->metric_values().size();i++) 
  {
    demandMetric(loop->metric_values(i).name())=loop->metric_values(i).value();
  }
}
*/
/*
Call::Call(Nodes::Call* call, ANode* parent,
	   Prof::Struct::ACodeNode* anode):ADynNode(TyCall,parent,anode,0)
{
  id(call->id());
  for(int i=0;i<call->metric_values().size();i++) 
  {
    demandMetric(call->metric_values(i).name())=call->metric_values(i).value();
  }
  
  //nameDyn
  /string nameD=call->name_debug();
  //int a=nameD.find(") ip(");
  //char assocI[a-7];
  //nameD.copy(assocI,a-7,7);//idk what to do with assocInfo_str()
  //int b=nameD.find(", ",a);
  //char lmId[b-a-5];
  //nameD.copy(lmId,b-a-5,a+5);
  //int c=nameD.find(") lip(",b);
  //char lmIP[c-b-2];
  //nameD.copy(lmIP,c-b-2,b+2);
  //char lip[nameD.length()-2-c-6];
  //nameD.copy(lip,nameD.length()-2-c-6,c+6);//idk what to do with lip_str()
  //std::stringstream ss(lmId);
  //uint lmId_uint;
  //ss>>lmId_uint;
  //lmId_real((LoadMap::LMId_t)lmId_uint);
  //int vma;
  //sscanf (lmIP,"%x",&vma);
  //ADynNode::lmIP((VMA)vma,0);
  
}
*/
/*
Stmt::Stmt(Nodes::Stmt* stmt, ANode* parent,
	   Prof::Struct::ACodeNode* anode):ADynNode(TyStmt,parent,anode,0)
{
  id(stmt->id());
  cpId(stmt->trace_id());
  for(int i=0;i<stmt->metric_values().size();i++) 
  {
    demandMetric(stmt->metric_values(i).name())=stmt->metric_values(i).value();
  }
  
  //nameDyn
  //string nameD=stmt->name_debug();
  //int a=nameD.find(") ip(");
  //char assocI[a-7];
  //nameD.copy(assocI,a-7,7);//idk what to do with assocInfo_str()
  //int b=nameD.find(", ",a);
  //char lmId[b-a-5];
  //nameD.copy(lmId,b-a-5,a+5);
  //int c=nameD.find(") lip(",b);
  //char lmIP[c-b-2];
  //nameD.copy(lmIP,c-b-2,b+2);
  //char lip[nameD.length()-2-c-6];
  //nameD.copy(lip,nameD.length()-2-c-6,c+6);//idk what to do with lip_str()
  //std::stringstream ss(lmId);
  //uint lmId_uint;
  //ss>>lmId_uint;
  //lmId_real((LoadMap::LMId_t)lmId_uint);
  //int vma;
  //sscanf (lmIP,"%x",&vma);
  //ADynNode::lmIP((VMA)vma,0);
  
}
*/

Root::Root(Nodes::GenNode* root,ANode* parent,
	   Prof::Struct::ACodeNode* anode):ANode(TyRoot,parent)
{
  id(root->id());
  m_name = root->name();
  m_strct=anode;
  for(int i=0;i<root->metric_values().size();i++) 
  {
    demandMetric(root->metric_values(i).name())=root->metric_values(i).value();
  }
}


ProcFrm::ProcFrm(Nodes::GenNode* procfrm, ANode* parent,
	    Prof::Struct::ACodeNode* anode)
            :AProcNode(TyProcFrm,parent,anode)
{
  id(procfrm->id());
  for(int i=0;i<procfrm->metric_values().size();i++) 
  {
    demandMetric(procfrm->metric_values(i).name())=procfrm->metric_values(i).value();
  }
}


Proc::Proc(Nodes::GenNode* proc, ANode* parent,
	   Prof::Struct::ACodeNode* anode)
           :AProcNode(TyProc,parent, anode)
{
  id(proc->id());
  for(int i=0;i<proc->metric_values().size();i++) 
  {
    demandMetric(proc->metric_values(i).name())=proc->metric_values(i).value();
  }
}


Loop::Loop(Nodes::GenNode* loop, ANode* parent,
	   Prof::Struct::ACodeNode* anode):ANode(TyLoop,parent)
{
  id(loop->id());
  m_strct=anode;
  for(int i=0;i<loop->metric_values().size();i++) 
  {
    demandMetric(loop->metric_values(i).name())=loop->metric_values(i).value();
  }
}


Call::Call(Nodes::GenNode* call, ANode* parent,
	   Prof::Struct::ACodeNode* anode):ADynNode(TyCall,parent,anode,0)
{
  id(call->id());
  for(int i=0;i<call->metric_values().size();i++) 
  {
    demandMetric(call->metric_values(i).name())=call->metric_values(i).value();
  }
}


Stmt::Stmt(Nodes::GenNode* stmt, ANode* parent,
	   Prof::Struct::ACodeNode* anode):ADynNode(TyStmt,parent,anode,0)
{
  id(stmt->id());
  cpId(stmt->trace_id());
  for(int i=0;i<stmt->metric_values().size();i++) 
  {
    demandMetric(stmt->metric_values(i).name())=stmt->metric_values(i).value();
  }
}


//**********************************************************************
// 
//**********************************************************************

uint
ANode::makeDensePreorderIds(uint nextId)
{
  // N.B.: use a sorted iterator to support hpcprof-mpi where we need
  // to ensure multiple processes obtain the same numbering.
  
  id(nextId);
  nextId++;
  
  for (ANodeSortedChildIterator it(this,ANodeSortedIterator::cmpByStructureInfo);
       it.current(); it++) {
    CCT::ANode* n = it.current();
    nextId = n->makeDensePreorderIds(nextId);
  }

  return nextId;
}


//**********************************************************************
// ANode, etc: CodeName methods
//**********************************************************************

// NOTE: tallent: used for lush_cilkNormalize

string
ANode::codeName() const
{ 
  string self = ANodeTyToName(type()) + " "
    //+ GetFile() + ":" 
    + StrUtil::toStr(begLine()) + "-" + StrUtil::toStr(endLine());
  return self;
}


string
ProcFrm::codeName() const
{ 
  string self = ANodeTyToName(type()) + " "
    + procName() + " @ "
    + fileName() + ":" 
    + StrUtil::toStr(begLine()) + "-" + StrUtil::toStr(endLine());
  return self;
}


string
ProcFrm::procNameDbg() const
{
  string nm = procName();

  CCT::Call* caller = ancestorCall();
  if (caller) {
    nm += " <= " + caller->nameDyn();
  }

  return nm;
}


//**********************************************************************
// ANode, etc: Dump contents for inspection
//**********************************************************************

string 
ANode::toString(uint oFlags, const char* pfx) const
{
  std::ostringstream os;
  writeXML(os, Metric::IData::npos, Metric::IData::npos, oFlags, pfx);
  return os.str();
}


string 
ANode::toStringMe(uint oFlags) const
{ 
  string self;
  self = ANodeTyToName(type());

  SrcFile::ln lnBeg = begLine();
  string line = StrUtil::toStr(lnBeg);
  //SrcFile::ln lnEnd = endLine();
  //if (lnBeg != lnEnd) {
  //  line += "-" + StrUtil::toStr(lnEnd);
  //}

  uint sId = (m_strct) ? m_strct->id() : 0;

  self += " i" + xml::MakeAttrNum(m_id);
  self += " s" + xml::MakeAttrNum(sId) + " l" + xml::MakeAttrStr(line);
  if ((oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll)) {
    self += " strct" + xml::MakeAttrNum((uintptr_t)m_strct, 16);
  }

  return self;
}

// Parent function that serializes data to pb coded output stream
void
ANode::toPBMe(google::protobuf::io::CodedOutputStream* cos, int parent_id, 
	      uint metricBeg, uint metricEnd, int prettyPrint,int depth)
{
  Nodes::GenNode gnode;
  gnode.set_static_scope_id((m_strct) ? m_strct->id() : 0);
  gnode.set_id(m_id);
  gnode.set_parent_id(parent_id);
  gnode.set_type(this->type());
  gnode.set_depth(depth);
  writeMetricsPB(&gnode, metricBeg, metricEnd, prettyPrint);
  if (prettyPrint == 0) {
    cos->WriteVarint32(gnode.ByteSize());
    gnode.SerializeToCodedStream(cos);
  }
  else {
    cos->WriteString(gnode.DebugString());
  }
}


string 
ADynNode::assocInfo_str() const
{
  char str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  lush_assoc_info_sprintf(str, m_as_info);
  return string(str);
}


string 
ADynNode::lip_str() const
{
  char str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(str, m_lip);
  return string(str);
}


string 
ADynNode::nameDyn() const
{
  string nm = "[assoc(" + assocInfo_str() + ") ip("
    + StrUtil::toStr(lmId_real()) + ", " 
    + StrUtil::toStr(lmIP_real(), 16) + ") lip(" + lip_str() + ")]";
  return nm;
}


void
ADynNode::writeDyn(std::ostream& o, uint GCC_ATTR_UNUSED oFlags,
		   const char* pfx) const
{
  string p(pfx);

  o << std::showbase;

  o << p << assocInfo_str() 
    << hex << " [ip " << m_lmIP << ", " << dec << m_opIdx << "] "
    << hex << m_lip << " [lip " << lip_str() << "]" << dec;

  o << p << " [metrics";
  for (uint i = 0; i < numMetrics(); ++i) {
    o << " " << metric(i);
  }
  o << "]" << endl;
}


string
Root::toStringMe(uint oFlags) const
{ 
  string self = ANode::toStringMe(oFlags) + " n" + xml::MakeAttrStr(m_name);
  return self;
}

// Function that serializes data to pb coded output stream
void
Root::toPBMe(google::protobuf::io::CodedOutputStream* cos,int parent_id, 
	     uint metricBeg, uint metricEnd, int prettyPrint,int depth)
{
  Nodes::GenNode rnode;
  rnode.set_static_scope_id((m_strct) ? m_strct->id() : 1);
  rnode.set_id(m_id);
  rnode.set_parent_id(parent_id);
  rnode.set_type(this->type());
  rnode.set_depth(depth);
  //rnode.set_name(m_name);
  writeMetricsPB(&rnode,metricBeg, metricEnd, prettyPrint);
  if (prettyPrint == 0){
    cos->WriteVarint32(rnode.ByteSize());
    rnode.SerializeToCodedStream(cos);
  }
  else{
    cos->WriteString(rnode.DebugString());
  }

}


string
ProcFrm::toStringMe(uint oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  
  if (m_strct) {
    string lm_nm = xml::MakeAttrNum(lmId());
    string fnm = xml::MakeAttrNum(fileId());
    string pnm = xml::MakeAttrNum(procId());

    if (oFlags & Tree::OFlg_DebugAll) {
      lm_nm = xml::MakeAttrStr(lmName());
      fnm = xml::MakeAttrStr(fileName());
    }
    if ( (oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll) ) {
      pnm = xml::MakeAttrStr(procNameDbg());
    }

    self += " lm" + lm_nm + " f" + fnm + " n" + pnm;
  }

  return self; 
}

// Function that serializes data to pb coded output stream
void
ProcFrm::toPBMe(google::protobuf::io::CodedOutputStream* cos,int parent_id, 
		uint metricBeg, uint metricEnd, int prettyPrint, int depth)
{
  Nodes::GenNode pfnode;
  pfnode.set_static_scope_id((m_strct) ? m_strct->id() : 0);
  pfnode.set_id(m_id);
  pfnode.set_parent_id(parent_id);
  pfnode.set_type(this->type());
  pfnode.set_depth(depth);
  pfnode.set_file(fileId());
  writeMetricsPB(&pfnode, metricBeg, metricEnd, prettyPrint);
  if (prettyPrint == 0){
    cos->WriteVarint32(pfnode.ByteSize());
    pfnode.SerializeToCodedStream(cos);
  }
  else{
    cos->WriteString(pfnode.DebugString());
  }
}


string
Proc::toStringMe(uint oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  
  if (m_strct) {
    string lm_nm = xml::MakeAttrNum(lmId());
    string fnm = xml::MakeAttrNum(fileId());
    string pnm = xml::MakeAttrNum(procId());

    if (oFlags & Tree::OFlg_DebugAll) {
      lm_nm = xml::MakeAttrStr(lmName());
      fnm = xml::MakeAttrStr(fileName());
      pnm = xml::MakeAttrStr(procName());
    }

    self += " lm" + lm_nm + " f" + fnm + " n" + pnm;
    if (isAlien()) {
      self = self + " a=\"1\"";
    }
  }

  return self; 
}

// Function that serializes data to pb coded output stream
void
Proc::toPBMe(google::protobuf::io::CodedOutputStream* cos,int parent_id,
	     uint metricBeg, uint metricEnd, int prettyPrint,int depth)
{
  Nodes::GenNode pnode;
  pnode.set_static_scope_id((m_strct) ? m_strct->id() : 0);
  pnode.set_id(m_id);
  pnode.set_parent_id(parent_id);
  pnode.set_type(this->type());
  pnode.set_depth(depth);
  pnode.set_file(fileId());
  writeMetricsPB(&pnode, metricBeg, metricEnd, prettyPrint);
  if (prettyPrint == 0){
    cos->WriteVarint32(pnode.ByteSize());
    pnode.SerializeToCodedStream(cos);
  }
  else{
    cos->WriteString(pnode.DebugString());
  }
}


string 
Loop::toStringMe(uint oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  return self;
}

// Function that serializes data to pb coded output stream
void
Loop::toPBMe(google::protobuf::io::CodedOutputStream* cos, int parent_id, 
	     uint metricBeg, uint metricEnd, int prettyPrint, int depth)
{
  Nodes::GenNode lnode;
  lnode.set_static_scope_id((m_strct) ? m_strct->id() : 0);
  lnode.set_id(m_id);
  lnode.set_parent_id(parent_id);
  lnode.set_type(this->type());
  lnode.set_depth(depth);
  lnode.set_line_range(m_strct->begLine());
  writeMetricsPB(&lnode, metricBeg, metricEnd, prettyPrint);
  if (prettyPrint == 0){
    cos->WriteVarint32(lnode.ByteSize());
    lnode.SerializeToCodedStream(cos);
  }
  else{
    cos->WriteString(lnode.DebugString());
  }
}

string
Call::toStringMe(uint oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  if ((oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll)) {
    self += " n=\"" + nameDyn() + "\"";
  }
  return self;
}

// Function that serializes data to pb coded output stream
void
Call::toPBMe(google::protobuf::io::CodedOutputStream* cos,int parent_id, 
	     uint metricBeg, uint metricEnd, int prettyPrint, int depth)
{
  Nodes::GenNode cnode;
  cnode.set_static_scope_id((m_strct) ? m_strct->id() : 0);
  cnode.set_id(m_id);
  cnode.set_parent_id(parent_id);
  cnode.set_type(this->type());
  cnode.set_depth(depth);
  //cnode.set_name_debug(nameDyn());
  writeMetricsPB(&cnode, metricBeg, metricEnd, prettyPrint);
  if (prettyPrint == 0){
    cos->WriteVarint32(cnode.ByteSize());
    cnode.SerializeToCodedStream(cos);
  }
  else{
    cos->WriteString(cnode.DebugString());
  }
}


string
Stmt::toStringMe(uint oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  if ((oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll)) {
    self += " n=\"" + nameDyn() + "\"";
  }
  if (hpcrun_fmt_doRetainId(cpId())) {
    self += " it" + xml::MakeAttrNum(cpId());
  }
  return self;
}

// Function that serializes data to pb coded output stream
void
Stmt::toPBMe(google::protobuf::io::CodedOutputStream* cos,int parent_id, 
	     uint metricBeg, uint metricEnd, int prettyPrint, int depth)
{
  Nodes::GenNode snode;
  snode.set_static_scope_id((m_strct) ? m_strct->id() : 0);
  snode.set_id(m_id);
  snode.set_parent_id(parent_id);
  snode.set_type(this->type());
  snode.set_depth(depth);
  snode.set_file(m_strct->ancestorFile()->id());
  snode.set_line_range(m_strct->begLine());
  //snode.set_name_debug(nameDyn());
  writeMetricsPB(&snode,metricBeg, metricEnd, prettyPrint);
  snode.set_trace_id(cpId());
  if (prettyPrint == 0){
    cos->WriteVarint32(snode.ByteSize());
    snode.SerializeToCodedStream(cos);
  }
  else{
    cos->WriteString(snode.DebugString());
  }
}

std::ostream&
ANode::writeXML(ostream& os, uint metricBeg, uint metricEnd,
		uint oFlags, const char* pfx) const
{
  string indent = "  ";
  if (oFlags & CCT::Tree::OFlg_Compressed) {
    pfx = "";
    indent = "";
  }
  
  bool doPost = writeXML_pre(os, metricBeg, metricEnd, oFlags, pfx);
  string prefix = pfx + indent;
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpById);//Changed for testing purposes
       it.current(); it++) {
    ANode* n = it.current();
    n->writeXML(os, metricBeg, metricEnd, oFlags, prefix.c_str());
  }
  if (doPost) {
    writeXML_post(os, oFlags, pfx);
  }
  return os;
}

// Iterates to coded output stream based of ANodetype
void
ANode::writePB(google::protobuf::io::CodedOutputStream* cos,int parent_id,
	       uint metricBeg, uint metricEnd, int prettyPrint, int depth)
{
  switch (type()){
  case ANode::TyRoot:
    dynamic_cast<Root*>(this)->toPBMe(cos, parent_id, metricBeg, metricEnd, 
				      prettyPrint,depth);
    break;
  case ANode::TyProcFrm:
    dynamic_cast<ProcFrm*>(this)->toPBMe(cos, parent_id, metricBeg, metricEnd, 
				      prettyPrint,depth);
    break;
  case ANode::TyProc:
    dynamic_cast<Proc*>(this)->toPBMe(cos, parent_id, metricBeg, metricEnd, 
				      prettyPrint,depth);
    break;
  case ANode::TyLoop:
    dynamic_cast<Loop*>(this)->toPBMe(cos, parent_id, metricBeg, metricEnd, 
				      prettyPrint,depth);
    break;
  case ANode::TyCall:
    dynamic_cast<Call*>(this)->toPBMe(cos, parent_id, metricBeg, metricEnd, 
				      prettyPrint,depth);
    break;
  case ANode::TyStmt:
    dynamic_cast<Stmt*>(this)->toPBMe(cos, parent_id, metricBeg, metricEnd, 
				      prettyPrint,depth);
    break;
  default:
    printf("\n ERROR: Unexpected Type of ANode in ANode::writePB \n");
  }
  for (ANodeSortedChildIterator it(this,ANodeSortedIterator::cmpByStructureInfo);it.current();it++){
    ANode* n = it.current();
    n->writePB(cos, this->id(), metricBeg, metricEnd, prettyPrint,depth+1);
  }
}

std::ostream&
ANode::dump(ostream& os, uint oFlags, const char* pfx) const 
{
  writeXML(os, Metric::IData::npos, Metric::IData::npos, oFlags, pfx); 
  return os;
}


void
ANode::ddump() const
{
  writeXML(std::cerr, Metric::IData::npos, Metric::IData::npos,
	   Tree::OFlg_DebugAll, "");
} 


void
ANode::ddumpMe() const
{
  string str = toStringMe(Tree::OFlg_DebugAll);
  std::cerr << str;
}


bool
ANode::writeXML_pre(ostream& os, uint metricBeg, uint metricEnd,
		    uint oFlags, const char* pfx) const
{
  bool doTag = (type() != TyRoot);
  bool doMetrics = ((oFlags & Tree::OFlg_LeafMetricsOnly)
		    ? isLeaf() && hasMetrics(metricBeg, metricEnd)
		    : hasMetrics(metricBeg, metricEnd));
  bool isXMLLeaf = isLeaf() && !doMetrics;

  // 1. Write element name
  if (doTag) {
    if (isXMLLeaf) {
      os << pfx << "<" << toStringMe(oFlags) << "/>" << endl;
    }
    else {
      os << pfx << "<" << toStringMe(oFlags) << ">" << endl;
    }
  }

  // 2. Write associated metrics
  if (doMetrics) {
    writeMetricsXML(os, metricBeg, metricEnd, oFlags, pfx);
    os << endl;
  }

  return !isXMLLeaf; // whether to execute writeXML_post()
}


void
ANode::writeXML_post(ostream& os, uint GCC_ATTR_UNUSED oFlags,
		     const char* pfx) const
{
  bool doTag = (type() != ANode::TyRoot);
  if (!doTag) {
    return;
  }
  
  os << pfx << "</" << ANodeTyToName(type()) << ">" << endl;
}


//**********************************************************************
// 
//**********************************************************************


int ANodeLineComp(ANode* x, ANode* y)
{
  if (x->begLine() == y->begLine()) {
    // Given two ANode's with identical endpoints consider two
    // special cases:
    bool endLinesEqual = (x->endLine() == y->endLine());
    
    // 1. Otherwise: rank a leaf node before a non-leaf node
    if (endLinesEqual && !(x->isLeaf() && y->isLeaf())) {
      if      (x->isLeaf()) { return -1; } // x < y 
      else if (y->isLeaf()) { return 1; }  // x > y  
    }
    
    // 2. General case
    return SrcFile::compare(x->endLine(), y->endLine());
  }
  else {
    return SrcFile::compare(x->begLine(), y->begLine());
  }
}


} // namespace CCT

} // namespace Prof

