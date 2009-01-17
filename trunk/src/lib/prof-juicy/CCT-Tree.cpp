// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
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

//*************************** User Include Files ****************************

#include <include/general.h>

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


Tree::Tree(const CallPath::Profile* metadata)
  : m_root(NULL), m_metadata(metadata)
{
}


Tree::~Tree()
{
  delete m_root; 
  m_metadata = NULL;
}


void 
Tree::merge(const Tree* y, 
	    const SampledMetricDescVec* new_mdesc,		  
	    uint x_numMetrics, uint y_numMetrics)
{
  Root* x_root = dynamic_cast<Root*>(root());
  Root* y_root = dynamic_cast<Root*>(y->root());

  DIAG_Assert(x_root && y_root && x_root->name() == y_root->name(),
	      "Unexpected root!");

  x_root->merge_prepare(y_numMetrics);
  x_root->merge(y_root, new_mdesc, x_numMetrics, y_numMetrics);
}


std::ostream&
Tree::writeXML(std::ostream& os, int oFlags) const
{
  if (m_root) {
    m_root->writeXML(os, oFlags);
  }
  return os;
}


std::ostream& 
Tree::dump(std::ostream& os, int oFlags) const
{
  if (m_root) {
    m_root->writeXML(os, oFlags);
  }
  return os;
}


void 
Tree::ddump() const
{
  dump(std::cerr, 0/*oFlags*/);
}


} // namespace CCT

} // namespace Prof



namespace Prof {

namespace CCT {

//***************************************************************************
// NodeType `methods' (could completely replace with dynamic typing)
//***************************************************************************

const string ANode::NodeNames[ANode::TyNUMBER] = {
  "Root", "P", "Pr", "L", "S", "C", "Any"
};


const string&
ANode::NodeTypeToName(NodeType tp)
{
  return NodeNames[tp]; 
}


ANode::NodeType
ANode::IntToNodeType(long i) 
{
  DIAG_Assert((i >= 0) && (i < TyNUMBER), "");
  return (NodeType)i;
}


//***************************************************************************
// ANode, etc: constructors/destructors
//***************************************************************************


Root::Root(const char* nm) 
  : ANode(TyRoot, NULL) 
{ 
  DIAG_Assert(nm, "");
  m_name = nm; 
}


Root::~Root() 
{
}


string ProcFrm::BOGUS;

static void
Call_Check(Call* n, ANode* _parent) 
{
  DIAG_Assert((_parent == NULL) 
	      || (_parent->type() == ANode::TyRoot)
	      || (_parent->type() == ANode::TyLoop) 
	      || (_parent->type() == ANode::TyProcFrm) 
	      || (_parent->type() == ANode::TyCall), "");
}


ProcFrm::ProcFrm(ANode* _parent, Struct::ACodeNode* strct)
  : ANode(TyProcFrm, _parent, strct)
{
  Call_Check(NULL, _parent);
}


ProcFrm::~ProcFrm()
{
}


Proc::Proc(ANode* _parent, Struct::ACodeNode* strct)
  : ANode(TyProc, _parent, strct)
{
  DIAG_Assert((_parent == NULL)
	      || (_parent->type() == TyCall)
	      || (_parent->type() == TyLoop), "");
}

Proc::~Proc()
{
}


Loop::Loop(ANode* _parent, Struct::ACodeNode* strct)
  : ANode(TyLoop, _parent, strct)
{
  DIAG_Assert((_parent == NULL)
	      || (_parent->type() == TyCall) 
	      || (_parent->type() == TyProcFrm) 
	      || (_parent->type() == TyLoop), "");
}


Loop::~Loop()
{
}


Stmt& 
Stmt::operator=(const Call& x) 
{
  ADynNode::operator=(x);

  // NOTE: the above copies the type field -- reset it
  m_type = TyStmt;

  return *this;
}


Call::Call(ANode* _parent, uint32_t cpid,
	   const SampledMetricDescVec* metricdesc)
  : ADynNode(TyCall, _parent, NULL, cpid, metricdesc)
{
  Call_Check(this, _parent);
}


Call::Call(ANode* _parent, 
	   lush_assoc_info_t as_info,
	   VMA ip, ushort opIndex, 
	   lush_lip_t* lip,
	   uint32_t cpid,
	   const SampledMetricDescVec* metricdesc,
	   std::vector<hpcfile_metric_data_t>& metrics)
  : ADynNode(TyCall, _parent, NULL, 
	     as_info, ip, opIndex, lip, cpid, metricdesc, metrics)
{
  Call_Check(this, _parent);
}


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
ANode::Ancestor(NodeType tp) const
{
  ANode* s = const_cast<ANode*>(this); 
  while (s && s->type() != tp) {
    s = s->parent(); 
  } 
  return s;
} 


#if 0

int IsAncestorOf(ANode* _parent, ANode *son, int difference)
{
  ANode *iter = son;
  while (iter && difference > 0 && iter != _parent) {
    iter = iter->Parent();
    difference--;
  }
  if (iter && iter == _parent)
     return 1;
  return 0;
}

#endif


Root*
ANode::ancestorRoot() const 
{
  if (Parent() == NULL) {
    return NULL;
  }  else { 
    dyn_cast_return(ANode, Root, Ancestor(TyRoot));
  }
}


ProcFrm*
ANode::ancestorProcFrm() const
{
  dyn_cast_return(ANode, ProcFrm, Ancestor(TyProcFrm)); 
}


Loop*
ANode::ancestorLoop() const 
{
  dyn_cast_return(ANode, Loop, Ancestor(TyLoop));
}


Stmt*
ANode::ancestorStmt() const 
{
  dyn_cast_return(ANode, Stmt, Ancestor(TyStmt));
}


Call*
ANode::ancestorCall() const
{
  dyn_cast_return(ANode, Call, Ancestor(TyCall)); 
}


//**********************************************************************
// 
//**********************************************************************


void 
ADynNode::mergeMetrics(const ADynNode& y, uint beg_idx)
{
#if 0
  if (numMetrics() != y.numMetrics()) {
    m_metrics.resize(y.numMetrics());
  }
#endif

  DIAG_Assert(m_cpid == 0 || y.m_cpid == 0, "multiple node ids for a call path"); 

  // if either node has an id, make sure the node after the merge has a node id
  if (m_cpid == 0) m_cpid = y.m_cpid;

  uint x_end = y.numMetrics() + beg_idx;
  DIAG_Assert(x_end <= numMetrics(), "Insufficient space for merging.");

  for (uint x_i = beg_idx, y_i = 0; x_i < x_end; ++x_i, ++y_i) {
    metricIncr((*m_metricdesc)[x_i], &m_metrics[x_i], y.metric(y_i));
  }
}


void 
ADynNode::appendMetrics(const ADynNode& y)
{
  for (int i = 0; i < y.numMetrics(); ++i) {
    m_metrics.push_back(y.metric(i));
  }
}


void 
ADynNode::expandMetrics_before(uint offset)
{
  for (int i = 0; i < offset; ++i) {
    m_metrics.insert(m_metrics.begin(), hpcfile_metric_data_ZERO);
  }
}


void 
ADynNode::expandMetrics_after(uint offset)
{
  for (int i = 0; i < offset; ++i) {
    m_metrics.push_back(hpcfile_metric_data_ZERO);
  }
}


void
ANode::merge_prepare(uint numMetrics)
{
  ADynNode* dyn = dynamic_cast<ADynNode*>(this);
  if (dyn) {
    dyn->expandMetrics_after(numMetrics);
    DIAG_MsgIf(0, "Expanding " << hex << dyn << dec << " +" << numMetrics << " -> " << dyn->numMetrics());
  }

  // Recur on children
  for (ANodeChildIterator it(this); it.Current(); ++it) {
    ANode* child = it.CurNode();
    child->merge_prepare(numMetrics);
  }
}


// Let y be a node corresponding to 'this' = x and assume x is already
// merged.  Given y, merge y's children into x.
// NOTE: assume we can destroy y...
// NOTE: assume x already has space to store merged metrics
void
ANode::merge(ANode* y, const SampledMetricDescVec* new_mdesc,
	     uint x_numMetrics, uint y_numMetrics)
{
  ANode* x = this;
  
  // ------------------------------------------------------------
  // 0. If y is childless, return.
  // ------------------------------------------------------------
  if (y->isLeaf()) {
    return;
  }

  // ------------------------------------------------------------  
  // 1. If a child d of y _does not_ appear as a child of x, then copy
  //    (subtree) d [fixing d's metrics], make it a child of x and
  //    return.
  // 2. If a child d of y _does_ have a corresponding child c of x,
  //    merge [the metrics of] d into c and recur.
  // ------------------------------------------------------------  
  for (ANodeChildIterator it(y); it.Current(); /* */) {
    ANode* y_child = it.CurNode();
    ADynNode* y_child_dyn = dynamic_cast<ADynNode*>(y_child);
    DIAG_Assert(y_child_dyn, "");
    it++; // advance iterator -- it is pointing at 'child'

    ANode* x_child = findDynChild(y_child_dyn->assocInfo(),
				  y_child_dyn->lm_id(),
				  y_child_dyn->ip_real(),
				  y_child_dyn->lip());
    if (!x_child) {
      y_child->Unlink();
      y_child->merge_fixup(new_mdesc, x_numMetrics);
      y_child->Link(x);
    }
    else {
      ADynNode* x_child_dyn = dynamic_cast<ADynNode*>(x_child);
      DIAG_MsgIf(0, "ANode::merge: Merging y into x:\n"
		 << "  y: " << y_child_dyn->toString()
		 << "  x: " << x_child_dyn->toString());
      x_child_dyn->mergeMetrics(*y_child_dyn, x_numMetrics);
      x_child->merge(y_child, new_mdesc, x_numMetrics, y_numMetrics);
    }
  }
}


ANode* 
ANode::findDynChild(lush_assoc_info_t as_info, 
		    Epoch::LM_id_t lm_id, VMA ip, lush_lip_t* lip)
{
  for (ANodeChildIterator it(this); it.Current(); ++it) {
    ANode* child = it.CurNode();
    ADynNode* child_dyn = dynamic_cast<ADynNode*>(child);
    
    lush_assoc_t as = lush_assoc_info__get_assoc(as_info);

    if (child_dyn 
	&& child_dyn->lm_id() == lm_id
	&& child_dyn->ip_real() == ip
	&& lush_lip_eq(child_dyn->lip(), lip)
	&& lush_assoc_class_eq(child_dyn->assoc(), as) 
	&& lush_assoc_info__path_len_eq(child_dyn->assocInfo(), as_info)) {
      return child;
    }
  }
  return NULL;
}


void
ANode::merge_fixup(const SampledMetricDescVec* new_mdesc, int metric_offset)
{
  ADynNode* x_dyn = dynamic_cast<ADynNode*>(this);
  if (x_dyn) {
    x_dyn->expandMetrics_before(metric_offset);
    x_dyn->metricdesc(new_mdesc);
  }

  for (ANodeChildIterator it(this); it.Current(); ++it) {
    ANode* child = it.CurNode();
    ADynNode* child_dyn = dynamic_cast<ADynNode*>(child);
    if (child_dyn) {
      child->merge_fixup(new_mdesc, metric_offset);
    }
  }
}


// merge y into 'this' = x
void
ANode::merge_node(ANode* y)
{
  ANode* x = this;
  
  // 1. copy y's metrics into x
  ADynNode* x_dyn = dynamic_cast<ADynNode*>(x);
  if (x_dyn) {
    ADynNode* y_dyn = dynamic_cast<ADynNode*>(y);
    DIAG_Assert(y_dyn, "");
    x_dyn->mergeMetrics(*y_dyn);
  }
  
  // 2. copy y's children into x
  for (ANodeChildIterator it(y); it.Current(); /* */) {
    ANode* y_child = it.CurNode();
    it++; // advance iterator -- it is pointing at 'y_child'
    y_child->Unlink();
    y_child->Link(x);
  }
  
  y->Unlink();
  delete y;
}

//**********************************************************************
// ANode, etc: CodeName methods
//**********************************************************************

// NOTE: tallent: used for lush_cilkNormalize

string
ANode::codeName() const
{ 
  string self = NodeTypeToName(type()) + " "
    //+ GetFile() + ":" 
    + StrUtil::toStr(begLine()) + "-" + StrUtil::toStr(endLine());
  return self;
}

string
ProcFrm::codeName() const
{ 
  string self = NodeTypeToName(type()) + " "
    + procName() + " @ "
    + fileName() + ":" 
    + StrUtil::toStr(begLine()) + "-" + StrUtil::toStr(endLine());
  return self;
}


//**********************************************************************
// ANode, etc: Dump contents for inspection
//**********************************************************************

string 
ANode::toString(int oFlags, const char* pre) const
{
  std::ostringstream os;
  writeXML(os, oFlags, pre);
  return os.str();
}


string 
ANode::toString_me(int oFlags) const
{ 
  string self;
  self = NodeTypeToName(type());

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
  self += " s" + xml::MakeAttrNum(sId)
    + " l" + xml::MakeAttrStr(line);

  if (oFlags & Tree::OFlg_Debug) {
    self = self + " uid" + xml::MakeAttrNum(id());
  }
  return self;
}


std::string 
ADynNode::assocInfo_str() const
{
  char str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  lush_assoc_info_sprintf(str, m_as_info);
  return string(str);
}


std::string 
ADynNode::lip_str() const
{
  char str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(str, m_lip);
  return string(str);
}


void
ADynNode::writeDyn(std::ostream& o, int oFlags, const char* pre) const
{
  string p(pre);

  o << std::showbase;
  o << p << assocInfo_str() 
    << hex << " [ip " << m_ip << ", " << dec << m_opIdx << "] ";
  o << hex << m_lip << " [lip " << lip_str() << "]" << dec;
  o << p << " [metrics";
  for (uint i = 0; i < m_metrics.size(); ++i) {
    o << " " << m_metrics[i];
  }
  o << "]" << endl;
}


std::ostream&
ADynNode::writeMetricsXML(std::ostream& os, int oFlags, const char* pfx) const
{
  bool wasMetricWritten = false;

  for (uint i = 0; i < numMetrics(); i++) {
    hpcfile_metric_data_t m = metric(i);
    if (!hpcfile_metric_data_iszero(m)) {
      os << ((!wasMetricWritten) ? pfx : "");
      os << "<M " << "n" << xml::MakeAttrNum(i) 
	 << " v" << writeMetric((*m_metricdesc)[i], m) << "/>";
      wasMetricWritten = true;
    }
  }

  return os;
}


string
Root::toString_me(int oFlags) const
{ 
  string self = ANode::toString_me(oFlags) + " n" + xml::MakeAttrStr(m_name);
  return self;
}


string
ProcFrm::toString_me(int oFlags) const
{
  string self = ANode::toString_me(oFlags);

  if (m_strct)  {
#if (FIXME_WRITE_CCT_DICTIONARIES)
    self += " lm" + xml::MakeAttrNum(lmId())
      + " f" + xml::MakeAttrNum(fileId())
      + " n" + xml::MakeAttrNum(procId());
#else
    const string& lm_nm = lmName();
    const string& fnm   = fileName();
    const string& pnm   = procName();

    self += " lm" + xml::MakeAttrStr(lm_nm)
      + " f" + xml::MakeAttrStr(fnm)
      + " n" + xml::MakeAttrStr(pnm);
#endif

    if (isAlien()) {
      self = self + " a=\"1\"";
    }
  }

  return self; 
}


string
Call::toString_me(int oFlags) const
{
  string self = ANode::toString_me(oFlags);
  
  if (oFlags & CCT::Tree::OFlg_Debug) {
    // writeDyn();
    self += " assoc" + xml::MakeAttrStr(assocInfo_str()) 
      + " ip_real" + xml::MakeAttrNum(ip_real(), 16)
      + " op" + xml::MakeAttrNum(opIndex())
      + " lip" + xml::MakeAttrStr(lip_str());
  }

  return self; 
}


string
Stmt::toString_me(int oFlags) const
{
  string self = ANode::toString_me(oFlags);

  if (oFlags & CCT::Tree::OFlg_Debug) {
    self += " ip" + xml::MakeAttrNum(ip(), 16) 
      + " op" + xml::MakeAttrNum(opIndex());
  }

  return self; 
} 


string 
Loop::toString_me(int oFlags) const
{
  string self = ANode::toString_me(oFlags); //+ " i" + MakeAttr(id);
  return self;
}


string
Proc::toString_me(int oFlags) const
{
  string self = ANode::toString_me(oFlags); //+ " i" + MakeAttr(id);
  return self;
}


std::ostream&
ANode::writeXML(ostream &os, int oFlags, const char *pre) const 
{
  string indent = "  ";
  if (oFlags & CCT::Tree::OFlg_Compressed) { 
    pre = ""; 
    indent = ""; 
  }
  
  bool doPost = writeXML_pre(os, oFlags, pre);
  string prefix = pre + indent;
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByStructureId);
       it.Current(); it++) {
    ANode* n = it.Current();
    n->writeXML(os, oFlags, prefix.c_str());
  }
  if (doPost) {
    writeXML_post(os, oFlags, pre);
  }
  return os;
}


std::ostream&
ANode::dump(ostream &os, int oFlags, const char *pre) const 
{
  writeXML(os, oFlags, pre); 
  return os;
}


void
ANode::ddump() const
{
  writeXML(std::cerr, 0/*oFlags*/, "");
} 


void
ANode::ddump_me() const
{
  string str = toString_me(0/*oFlags*/);
  std::cerr << str;
}


bool
ANode::writeXML_pre(ostream& os, int oFlags, const char *prefix) const
{
  const ADynNode* this_dyn = dynamic_cast<const ADynNode*>(this);

  bool doTag = (type() != TyRoot);
  bool hasMetrics = (this_dyn && this_dyn->hasMetrics());
  bool isXMLLeaf = isLeaf() && !hasMetrics;

  // 1. Write element name
  if (doTag) {
    if (isXMLLeaf) {
      os << prefix << "<" << toString_me(oFlags) << "/>" << endl;
    }
    else {
      os << prefix << "<" << toString_me(oFlags) << ">" << endl;
    }
  }

  // 2. Write associated metrics
  if (hasMetrics) {
    this_dyn->writeMetricsXML(os, oFlags, prefix);
    os << endl;
  }

  return !isXMLLeaf; // whether to execute writeXML_post()
}


void
ANode::writeXML_post(ostream &os, int oFlags, const char *prefix) const
{
  bool doTag = (type() != ANode::TyRoot);
  if (!doTag) {
    return;
  }
  
  if (type() == ANode::TyProcFrm) {
    // FIXME: tallent: temporary override
    const ProcFrm* fr = dynamic_cast<const ProcFrm*>(this);
    string tag = fr->isAlien() ? "Pr" : "PF";
    os << prefix << "</" << tag << ">" << endl;
  }
  else {
    os << prefix << "</" << NodeTypeToName(type()) << ">" << endl;
  }
}


string 
ANode::Types() const
{
  string types; 
  if (dynamic_cast<const ANode*>(this)) {
    types += "ANode "; 
  } 
  if (dynamic_cast<const Root*>(this)) {
    types += "Root "; 
  } 
  if (dynamic_cast<const ProcFrm*>(this)) {
    types += "ProcFrm "; 
  } 
  if (dynamic_cast<const Proc*>(this)) {
    types += "Proc "; 
  } 
  if (dynamic_cast<const Loop*>(this)) {
    types += "Loop "; 
  } 
  if (dynamic_cast<const Stmt*>(this)) {
    types += "Stmt "; 
  } 
  if (dynamic_cast<const Call*>(this)) {
    types += "Call "; 
  } 
  return types; 
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

