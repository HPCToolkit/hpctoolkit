// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <typeinfo>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "CCT-Tree.hpp"

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

uint Tree::raToCallsiteOfst = 1;


Tree::Tree(const CallPath::Profile* metadata)
  : m_root(NULL), m_metadata(metadata), m_maxDenseId(0), m_nodeidMap(NULL)
{
}


Tree::~Tree()
{
  delete m_root; 
  m_metadata = NULL;
  delete m_nodeidMap; 
}


void
Tree::merge(const Tree* y, uint x_newMetricBegIdx, uint y_newMetrics,
	    int dbgFlg)
{
  CCT::ANode* x_root = root();
  CCT::ANode* y_root = y->root();

  // Merge pre-condition: both x and y should be "locally merged",
  // i.e. they should be equal except for metrics and children.
  bool isPrecondition = false;
  if (typeid(*x_root) == typeid(CCT::Root)
      && typeid(*y_root) == typeid(CCT::Root)) {
    isPrecondition = true;
  }
  else {
    ADynNode* x_dyn = dynamic_cast<ADynNode*>(x_root);
    ADynNode* y_dyn = dynamic_cast<ADynNode*>(y_root);
    if (x_dyn && y_dyn && ADynNode::isMergable(*x_dyn, *y_dyn)) {
      isPrecondition = true;
    }
  }
  DIAG_Assert(isPrecondition, "Prof::CCT::Tree::merge: Merge precondition fails!");

  x_root->mergeDeep(y_root, x_newMetricBegIdx, y_newMetrics, dbgFlg);
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


std::ostream&
Tree::writeXML(std::ostream& os, uint metricBeg, uint metricEnd,
	       int oFlags) const
{
  if (m_root) {
    m_root->writeXML(os, metricBeg, metricEnd, oFlags);
  }
  return os;
}


std::ostream& 
Tree::dump(std::ostream& os, int oFlags) const
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
  "Root", "P", "Pr", "L", "C", "S", "Any"
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

string ProcFrm::BOGUS;


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
      
      for (VMAIntervalSet::const_iterator it = ivalset.begin();
	   it != ivalset.end(); ++it) {
	const VMAInterval& ival = *it;
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
  bool isFrame = (typeid(*n) == typeid(ProcFrm)
		  && !(static_cast<ProcFrm*>(n)->isAlien()));
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
  uint numMetrics = mMgr.size();

  for (uint mId = mBegId; mId < mEndId; ++mId) {
    const Metric::ADesc* m = mMgr.metric(mId);
    const Metric::DerivedDesc* mm = dynamic_cast<const Metric::DerivedDesc*>(m);
    if (mm && mm->expr()) {
      const Metric::AExpr* expr = mm->expr();
      double val = expr->eval(*this);
      demandMetric(mId, numMetrics/*size*/) = val;
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


//**********************************************************************
// Merging
//**********************************************************************


void
ANode::mergeDeep(ANode* y, uint x_newMetricBegIdx, uint y_newMetrics,
		 int dbgFlg)
{
  ANode* x = this;
  
  // ------------------------------------------------------------
  // 0. If y is childless, return.
  // ------------------------------------------------------------
  if (y->isLeaf()) {
    return;
  }

  // ------------------------------------------------------------  
  // 1. If a child y_child of y _does not_ appear as a child of x,
  //    then copy (subtree) y_child [fixing its metrics], make it a
  //    child of x and return.
  // 2. If a child y_child of y _does_ have a corresponding child
  //    x_child of x, merge [the metrics of] y_child into x_child and
  //    recur.
  // ------------------------------------------------------------  
  for (ANodeChildIterator it(y); it.Current(); /* */) {
    ANode* y_child = it.current();
    ADynNode* y_child_dyn = dynamic_cast<ADynNode*>(y_child);
    DIAG_Assert(y_child_dyn, "ANode::mergeDeep");
    it++; // advance iterator -- it is pointing at 'child'

    ADynNode* x_child_dyn = x->findDynChild(*y_child_dyn);

    if (!x_child_dyn) {
      // case 1
      DIAG_Assert( !(dbgFlg & Tree::OFlg_MergeOnly),
		   "CCT::ANode::mergeDeep: adding not permitted");
      DIAG_MsgIf(0 /*(dbgFlg & Tree::OFlg_Debug)*/,
		 "CCT::ANode::mergeDeep: Adding:\n     "
		 << y_child->toStringMe(Tree::OFlg_Debug));
      y_child->unlink();
      y_child->mergeDeep_fixup(x_newMetricBegIdx);
      y_child->link(x);
    }
    else {
      // case 2
      DIAG_MsgIf(0 /*(dbgFlg & Tree::OFlg_Debug)*/,
		 "CCT::ANode::mergeDeep: Merging x <= y:\n"
		 << "  x: " << x_child_dyn->toStringMe(Tree::OFlg_Debug)
		 << "\n  y: " << y_child_dyn->toStringMe(Tree::OFlg_Debug));
      x_child_dyn->mergeMe(*y_child_dyn, x_newMetricBegIdx);
      x_child_dyn->mergeDeep(y_child, x_newMetricBegIdx, y_newMetrics, dbgFlg);
    }
  }
}


void
ANode::merge(ANode* y)
{
  ANode* x = this;
  
  // 1. copy y's metrics into x
  x->mergeMe(*y);
  
  // 2. copy y's children into x
  for (ANodeChildIterator it(y); it.Current(); /* */) {
    ANode* y_child = it.current();
    it++; // advance iterator -- it is pointing at 'y_child'
    y_child->unlink();
    y_child->link(x);
  }
  
  y->unlink();
  delete y;
}


void 
ANode::mergeMe(const ANode& y, uint metricBegIdx)
{
  ANode* x = this;
  
  uint x_end = metricBegIdx + y.numMetrics(); // open upper bound
  if ( !(x_end <= x->numMetrics()) ) {
    ensureMetricsSize(x_end);
  }

  for (uint x_i = metricBegIdx, y_i = 0; x_i < x_end; ++x_i, ++y_i) {
    x->metric(x_i) += y.metric(y_i);
  }
}

//add by Xu Liu
int
ANode::mergeMe_IBS(ANode& y, uint metricBegIdx)
{
  int min, max;
  ANode* x = this;

  /*if we find y touches static data, we set malloc num to 0*/  
  if(y.getStaticId()!=0)
    y.setMallocNodeNum(0);

  uint x_end = metricBegIdx + y.numMetrics();
  if ( !(x_end <= x->numMetrics()) ) {
    ensureMetricsSize(x_end);
  }
  
  min = -1;
  for (uint i=metricBegIdx; i<x_end; i++)
  {
    if(x -> fmt(i) == 1){
      if(min < 0)
        min = i;
      else
        max = i;
    }
  }
  uint umin = (uint)min;
  uint umax = (uint)max;
  if((min < 0) || (max < 0))//no merge
    return 0;
 
  int merge_flag; //0 means not merge, else merge
  merge_flag = 0;

  /*if node x touches static data (not heap data),
 *  * set mallocnodenum of this node to 0. If two nodes
 *  have the same static data id, merge them*/
  if(x->getStaticId() != 0)
  {
    if(x->getStaticId() == y.getStaticId())
    {
      merge_flag = 1;
    }
    x->setMallocNodeNum(0);//this may be useless
  }

  for(uint i = 0; i<x->getMallocNodeNum(); i++)
  {
    for(uint j = 0; j<y.getMallocNodeNum(); j++)
    {
      if(static_cast<ANode*>(x->getMallocId(i))->id() == static_cast<ANode*>(y.getMallocId(j))->id())
      {
        merge_flag = 1;
        break;
      }
    }
  }
  /*we merge the two nodes which are related to the same malloc node or have overlapping address ranges.*/
  if(merge_flag || (!((x->metric(umin) > y.metric(umax)) || (x->metric(umax) < y.metric(umin))))) //have overlap
  {
    for (uint x_i = metricBegIdx, y_i = 0; x_i < x_end; ++x_i, ++y_i) {
      if(x_i == umin)
      {
        if(x->metric(umin) < y.metric(umin))
          continue;
        else{
          x->metric(umin) = y.metric(umin);
          continue;
        }
      }
      if(x_i == umax)
      {
        if(x->metric(umax) > y.metric(umax))
          continue;
        else
        {
          x->metric(umax) = y.metric(umax);
          continue;
        }
      }
      x->metric(x_i) += y.metric(y_i);
    }
    return 1;
  }
  else //no merge
    return 0;
}

void 
ADynNode::mergeMe(const ANode& y, uint metricBegIdx)
{
  // N.B.: Assumes ADynNode::isMergable() holds

  const ADynNode* y_dyn = dynamic_cast<const ADynNode*>(&y);
  DIAG_Assert(y_dyn, "ADynNode::mergeMe: " << DIAG_UnexpectedInput);

  ANode::mergeMe(y, metricBegIdx);
  
  // FIXME: Temporary 'assert' and 'if'. In reality, the assertion
  // below may not hold because we will have to support merging two
  // nodes with non-NULL cpIds.  However, if it does hold, we have
  // temporarily dodged a bullet.
  DIAG_Assert(m_cpId == 0 || y_dyn->m_cpId == 0, "ADynNode::mergeMe: conflicting cpIds!");
  if (m_cpId == 0) { m_cpId = y_dyn->m_cpId; }
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


void
ANode::mergeDeep_fixup(int newMetrics)
{
  for (ANodeIterator it(this); it.Current(); ++it) {
    ANode* n = it.current();
    n->insertMetricsBefore(newMetrics);
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
  
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByStructureId);
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
    nm += " <=" + caller->nameDyn();
  }

  return nm;
}


//**********************************************************************
// ANode, etc: Dump contents for inspection
//**********************************************************************

string 
ANode::toString(int oFlags, const char* pfx) const
{
  std::ostringstream os;
  writeXML(os, Metric::IData::npos, Metric::IData::npos, oFlags, pfx);
  return os.str();
}


string 
ANode::toStringMe(int oFlags) const
{ 
  string self;
  self = ANodeTyToName(type());

  // FIXME: tallent: temporary override
  if (type() == ANode::TyProcFrm) {
    const ProcFrm* fr = dynamic_cast<const ProcFrm*>(this);
    self = fr->isAlien() ? "Pr" : "PF";
  }

  SrcFile::ln lnBeg = begLine();
  string line = StrUtil::toStr(lnBeg);
  //SrcFile::ln lnEnd = endLine();
  //if (lnBeg != lnEnd) {
  //  line += "-" + StrUtil::toStr(lnEnd);
  //}

  uint sId = (m_strct) ? m_strct->id() : 0;

  if ((oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll)) {
    self += " uid" + xml::MakeAttrNum(m_id);
    self += " strct" + xml::MakeAttrNum((uintptr_t)m_strct, 16);
  }

  self += " s" + xml::MakeAttrNum(sId) + " l" + xml::MakeAttrStr(line);

  return self;
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
  string nm = "[" + assocInfo_str() + ": ("
    + StrUtil::toStr(m_lmId) + ", " + StrUtil::toStr(m_ip, 16) + ") ("
    + lip_str() + ")]";
  return nm;
}


void
ADynNode::writeDyn(std::ostream& o, int oFlags, const char* pfx) const
{
  string p(pfx);

  o << std::showbase;

  o << p << assocInfo_str() 
    << hex << " [ip " << m_ip << ", " << dec << m_opIdx << "] "
    << hex << m_lip << " [lip " << lip_str() << "]" << dec;

  o << p << " [metrics";
  for (uint i = 0; i < numMetrics(); ++i) {
    o << " " << metric(i);
  }
  o << "]" << endl;
}


string
Root::toStringMe(int oFlags) const
{ 
  string self = ANode::toStringMe(oFlags) + " n" + xml::MakeAttrStr(m_name);
  return self;
}


string
ProcFrm::toStringMe(int oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  
  if (m_strct)  {
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
    if (isAlien()) {
      self = self + " a=\"1\"";
    }
  }

  return self; 
}


string
Proc::toStringMe(int oFlags) const
{
  string self = ANode::toStringMe(oFlags); //+ " i" + MakeAttr(id);
  return self;
}


string 
Loop::toStringMe(int oFlags) const
{
  string self = ANode::toStringMe(oFlags); //+ " i" + MakeAttr(id);
  return self;
}


string
Call::toStringMe(int oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  if ((oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll)) {
    self += " " + nameDyn();
  }
  return self;
}


string
Stmt::toStringMe(int oFlags) const
{
  string self = ANode::toStringMe(oFlags);
  if ((oFlags & Tree::OFlg_Debug) || (oFlags & Tree::OFlg_DebugAll)) {
    self += " " + nameDyn();
  }
  if (hpcrun_fmt_doRetainId(cpId())) {
    self += " i" + xml::MakeAttrNum(cpId());
  }
  return self;
}


std::ostream&
ANode::writeXML(ostream& os, uint metricBeg, uint metricEnd,
		int oFlags, const char* pfx) const
{
  string indent = "  ";
  if (oFlags & CCT::Tree::OFlg_Compressed) {
    pfx = "";
    indent = "";
  }
  
  bool doPost = writeXML_pre(os, metricBeg, metricEnd, oFlags, pfx);
  string prefix = pfx + indent;
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByDynInfo);
       it.current(); it++) {
    ANode* n = it.current();
    n->writeXML(os, metricBeg, metricEnd, oFlags, prefix.c_str());
  }
  if (doPost) {
    writeXML_post(os, oFlags, pfx);
  }
  return os;
}

//(Xu)
bool
ANode::Predecessor(std::deque<Prof::CCT::ANode*> XMLdeq)
{
  bool ret = false;
  std::deque<Prof::CCT::ANode*>::iterator itt;
  itt = XMLdeq.begin();
  while(itt!=XMLdeq.end())
  {
    if (*itt == this)
      return true;
    itt++;
  }
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByDynInfo);
       it.current(); it++)
  {
    ANode* n = it.current();
    ret = n->Predecessor(XMLdeq);
    if(ret == true)
      return true;
  }
  return ret;
}

std::ostream&
ANode::useReuseWriteXML(ostream& os, std::deque<Prof::CCT::ANode*> XMLdeq, uint metricBeg, uint metricEnd,
                int oFlags, Prof::CCT::ANode* mallocnode, const char* pfx) const
{
  ANode *parMalloc = NULL;
  std::deque<Prof::CCT::ANode*> tmpdeq;
  string indent = "  ";
  if (oFlags & CCT::Tree::OFlg_Compressed) {
    pfx = "";
    indent = "";
  }

  if(mallocnode != NULL)
  {
    tmpdeq.push_back(mallocnode);
//    mallocnode->parent()->writeXML_pre(os, 0, 0, oFlags, pfx);
    parMalloc = mallocnode->parent();
/*    bool level = false;//The first PF is malloc in IBS SYNC. We want the second PF
    while (parMalloc->parent()->parent() != NULL)//we do not want to write malloc node from main()
    {
      if (parMalloc->type() == Prof::CCT::ANode::TyProcFrm)
      {
        const Prof::CCT::ProcFrm* fr = dynamic_cast<const Prof::CCT::ProcFrm*>(parMalloc);
        if(!fr->isAlien())
        {
          if(level)
            break;
          else
          {
            level = true;
            parMalloc=parMalloc->parent();
          }
        }
        else
          parMalloc=parMalloc->parent();
      }
      else
        parMalloc=parMalloc->parent();
    }
*/
    //find the root but one. Begin writing XML from this point. 
    while (parMalloc->parent()->parent() != NULL)
      parMalloc = parMalloc->parent();

    parMalloc->writeXML_pre(os, 0, 0, oFlags, pfx);
    for (ANodeSortedChildIterator it(parMalloc, ANodeSortedIterator::cmpByDynInfo);
       it.current(); it++) 
    {
      ANode* n = it.current();
      if(n->Predecessor(tmpdeq))
      {
        n->useReuseWriteXML(os, tmpdeq, 0, 0, oFlags, NULL);
        break;
      }
    }
  }

  bool doPost = false;
  if(this != parMalloc)//only write when it is not the same as parMollcoc
  {
    if(parMalloc != NULL)
    {
      if(m_strct != NULL)
      {
        if(parMalloc->structure()->id() != m_strct->id())//different ANode ids may have same structure id (same function in different call paths)
        {
          doPost = writeXML_pre(os, metricBeg, metricEnd, oFlags, pfx);
        }
      }
      else
        doPost = writeXML_pre(os, metricBeg, metricEnd, oFlags, pfx);
    }
    else
      doPost = writeXML_pre(os, metricBeg, metricEnd, oFlags, pfx);
  }
  string prefix = pfx + indent;
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByDynInfo);
       it.current(); it++) {
    ANode* n = it.current();
    if(!n->Predecessor(XMLdeq))
      continue;
    n->useReuseWriteXML(os, XMLdeq, metricBeg, metricEnd, oFlags, NULL, prefix.c_str());
  }
  if (doPost) {
    writeXML_post(os, oFlags, pfx);
  }

  if(mallocnode != NULL)
  {
//   mallocnode->parent()->writeXML_post(os, oFlags, pfx);
     parMalloc->writeXML_post(os, oFlags, pfx);
  }
  return os;
}


std::ostream&
ANode::dump(ostream& os, int oFlags, const char* pfx) const 
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
		    int oFlags, const char* pfx) const
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
ANode::writeXML_post(ostream &os, int oFlags, const char* pfx) const
{
  bool doTag = (type() != ANode::TyRoot);
  if (!doTag) {
    return;
  }
  
  if (type() == ANode::TyProcFrm) {
    // FIXME: tallent: temporary override
    const ProcFrm* fr = dynamic_cast<const ProcFrm*>(this);
    string tag = fr->isAlien() ? "Pr" : "PF";
    os << pfx << "</" << tag << ">" << endl;
  }
  else {
    os << pfx << "</" << ANodeTyToName(type()) << ">" << endl;
  }
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

