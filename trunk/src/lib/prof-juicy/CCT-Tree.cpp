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
  CSProfPgmNode* x_root = dynamic_cast<CSProfPgmNode*>(root());
  CSProfPgmNode* y_root = dynamic_cast<CSProfPgmNode*>(y->root());

  DIAG_Assert(x_root && y_root && x_root->GetName() == y_root->GetName(),
	      "Unexpected root!");

  x_root->merge_prepare(y_numMetrics);
  x_root->merge(y_root, new_mdesc, x_numMetrics, y_numMetrics);
}


void 
Tree::writeXML(std::ostream& os, int dmpFlag) const
{
  if (m_root) {
    m_root->writeXML(os, dmpFlag);
  }
}


void 
Tree::dump(std::ostream& os, int dmpFlag) const
{
  if (m_root) {
    m_root->writeXML(os, dmpFlag);
  }
}


void 
Tree::ddump() const
{
  dump(std::cerr, XML_FALSE);
}


int 
Tree::AddXMLEscapeChars(int dmpFlag)
{
  if ((dmpFlag & Tree::XML_TRUE) &&
      !(dmpFlag & Tree::XML_NO_ESC_CHARS)) {
    return xml::ESC_TRUE;
  } 
  else {
    return xml::ESC_FALSE;
  }
}



} // namespace CCT

} // namespace Prof



namespace Prof {

//***************************************************************************
// NodeType `methods' (could completely replace with dynamic typing)
//***************************************************************************

const string CSProfNode::NodeNames[CSProfNode::NUMBER_OF_TYPES] = {
  "PGM", "G", "C", "L", "S", "P", "S", "ANY"
};

const string&
CSProfNode::NodeTypeToName(NodeType tp)
{
  return NodeNames[tp]; 
}

CSProfNode::NodeType
CSProfNode::IntToNodeType(long i) 
{
  DIAG_Assert((i >= 0) && (i < NUMBER_OF_TYPES), "");
  return (NodeType)i;
}

//***************************************************************************
// CSProfNode, etc: constructors/destructors
//***************************************************************************

string CSProfCodeNode::BOGUS;

CSProfNode::CSProfNode(NodeType t, CSProfNode* _parent) 
  : NonUniformDegreeTreeNode(_parent), type(t)
{ 
  DIAG_Assert((type == PGM) || (AncestorPgm() == NULL) || 
	      !AncestorPgm()->IsFrozen(), "");
  static uint uniqueId = 1;
  uid = uniqueId++; 
}


static bool
OkToDelete(CSProfNode* x) 
{
  CSProfPgmNode* pgm = x->AncestorPgm(); 
  return ((pgm == NULL) || !(pgm->IsFrozen())); 
} 


CSProfNode::~CSProfNode() 
{
  DIAG_Assert(OkToDelete(this), ""); 
}


CSProfCodeNode::CSProfCodeNode(NodeType t, CSProfNode* _parent, 
			       SrcFile::ln begLn, SrcFile::ln endLn,
			       Struct::ACodeNode* strct) 
  : CSProfNode(t, _parent), 
    begLine(ln_NULL), endLine(ln_NULL), m_strct(strct) 
{ 
  SetLineRange(begLn, endLn); 
}


CSProfCodeNode::~CSProfCodeNode() 
{
}


CSProfPgmNode::CSProfPgmNode(const char* nm) 
  : CSProfNode(PGM, NULL) 
{ 
  DIAG_Assert(nm, "");
  frozen = false;
  name = nm; 
}


CSProfPgmNode::~CSProfPgmNode() 
{
  frozen = false;
}


CSProfGroupNode::CSProfGroupNode(CSProfNode* _parent, const char* nm) 
  : CSProfNode(GROUP, _parent)
{
  DIAG_Assert(nm, "");
  DIAG_Assert((_parent == NULL) || (_parent->GetType() == PGM)
	      || (_parent->GetType() == GROUP) 
	      || (_parent->GetType() == CALLSITE) 
	      || (_parent->GetType() == LOOP), "");
  name = nm;
}


CSProfGroupNode::~CSProfGroupNode()
{
}


static void
CSProfCallSiteNode_Check(CSProfCallSiteNode* n, CSProfNode* _parent) 
{
  DIAG_Assert((_parent == NULL) 
	      || (_parent->GetType() == CSProfNode::PGM)
	      || (_parent->GetType() == CSProfNode::GROUP) 
	      || (_parent->GetType() == CSProfNode::LOOP) 
	      || (_parent->GetType() == CSProfNode::PROCEDURE_FRAME) 
	      || (_parent->GetType() == CSProfNode::CALLSITE), "");
}


CSProfCallSiteNode::CSProfCallSiteNode(CSProfNode* _parent,
				       uint32_t cpid,
				       const SampledMetricDescVec* metricdesc)
  : CSProfCodeNode(CALLSITE, _parent, ln_NULL, ln_NULL),
    IDynNode(this, cpid, metricdesc)
{
  CSProfCallSiteNode_Check(this, _parent);
}


CSProfCallSiteNode::CSProfCallSiteNode(CSProfNode* _parent, 
				       lush_assoc_info_t as_info,
				       VMA ip, ushort opIndex, 
				       lush_lip_t* lip,
				       uint32_t cpid,
				       const SampledMetricDescVec* metricdesc,
				       std::vector<hpcfile_metric_data_t>& metrics)
  : CSProfCodeNode(CALLSITE, _parent, ln_NULL, ln_NULL), 
    IDynNode(this, as_info, ip, opIndex, lip, cpid, metricdesc, metrics)
{
  CSProfCallSiteNode_Check(this, _parent);
}

CSProfCallSiteNode::~CSProfCallSiteNode()
{
}


CSProfStatementNode::CSProfStatementNode(CSProfNode* _parent, 
					 uint32_t cpid,
					 const SampledMetricDescVec* metricdesc)
  :  CSProfCodeNode(STATEMENT, _parent, ln_NULL, ln_NULL),
     IDynNode(this, cpid, metricdesc)
{
}

CSProfStatementNode::~CSProfStatementNode()
{
}


void 
CSProfStatementNode::operator=(const CSProfStatementNode& x)
{
  if (this != &x) {
    IDynNode::operator=(x);

    file = x.GetFile();
    proc = x.GetProc();
    fileistext = x.FileIsText();
  }
}


void 
CSProfStatementNode::operator=(const CSProfCallSiteNode& x)
{
  IDynNode::operator=(x);
  
  file = x.GetFile();
  proc = x.GetProc();
  
  fileistext = x.FileIsText();
  donewithsrcinfproc = x.GotSrcInfo();
}


CSProfProcedureFrameNode::CSProfProcedureFrameNode(CSProfNode* _parent)
  : CSProfCodeNode(PROCEDURE_FRAME, _parent, ln_NULL, ln_NULL),
    fileistext(false), m_alien(false)
{
  CSProfCallSiteNode_Check(NULL, _parent);
}

CSProfProcedureFrameNode::~CSProfProcedureFrameNode()
{
}


CSProfLoopNode::CSProfLoopNode(CSProfNode* _parent, 
			       SrcFile::ln begLn, SrcFile::ln endLn, 
			       Struct::ACodeNode* strct)
  : CSProfCodeNode(LOOP, _parent, begLn, endLn, strct)
{
  DIAG_Assert((_parent == NULL) || (_parent->GetType() == GROUP) 
	      || (_parent->GetType() == CALLSITE) 
	      || (_parent->GetType() == PROCEDURE_FRAME) 
	      || (_parent->GetType() == LOOP), "");
}

CSProfLoopNode::~CSProfLoopNode()
{
}

CSProfStmtRangeNode::CSProfStmtRangeNode(CSProfNode* _parent, 
					 SrcFile::ln begLn, SrcFile::ln endLn, 
					 Struct::ACodeNode* strct)
  : CSProfCodeNode(STMT_RANGE, _parent, begLn, endLn, strct)
{
  DIAG_Assert((_parent == NULL) || (_parent->GetType() == GROUP)
	      || (_parent->GetType() == CALLSITE)
	      || (_parent->GetType() == LOOP), "");
}

CSProfStmtRangeNode::~CSProfStmtRangeNode()
{
}

//***************************************************************************
// CSProfNode, etc: Tree Navigation 
//***************************************************************************

CSProfNode* 
CSProfNode::NextSibling() const
{ 
  // siblings are linked in a circular list
  if ((Parent()->LastChild() != this)) {
    return dynamic_cast<CSProfNode*>(NextSibling()); 
  } 
  return NULL;  
}


CSProfNode* 
CSProfNode::PrevSibling() const
{ 
  // siblings are linked in a circular list
  if ((Parent()->FirstChild() != this)) {
    return dynamic_cast<CSProfNode*>(PrevSibling()); 
  } 
  return NULL;
}


#define dyn_cast_return(base, derived, expr) \
    { base* ptr = expr;  \
      if (ptr == NULL) {  \
        return NULL;  \
      } else {  \
	return dynamic_cast<derived*>(ptr);  \
      } \
    }


CSProfNode* 
CSProfNode::Ancestor(NodeType tp) const
{
  CSProfNode* s = const_cast<CSProfNode*>(this); 
  while (s && s->GetType() != tp) {
    s = s->Parent(); 
  } 
  return s;
} 


#if 0

int IsAncestorOf(CSProfNode* _parent, CSProfNode *son, int difference)
{
  CSProfNode *iter = son;
  while (iter && difference > 0 && iter != _parent) {
    iter = iter->Parent();
    difference--;
  }
  if (iter && iter == _parent)
     return 1;
  return 0;
}

#endif


CSProfPgmNode*
CSProfNode::AncestorPgm() const 
{
  if (Parent() == NULL) {
    return NULL;
  }  else { 
    dyn_cast_return(CSProfNode, CSProfPgmNode, Ancestor(PGM));
  }
}


CSProfGroupNode*
CSProfNode::AncestorGroup() const 
{
  dyn_cast_return(CSProfNode, CSProfGroupNode, Ancestor(GROUP)); 
}


CSProfCallSiteNode*
CSProfNode::AncestorCallSite() const
{
  dyn_cast_return(CSProfNode, CSProfCallSiteNode, Ancestor(CALLSITE)); 
}


CSProfProcedureFrameNode*
CSProfNode::AncestorProcedureFrame() const
{
  dyn_cast_return(CSProfNode, 
		  CSProfProcedureFrameNode,
		  Ancestor(PROCEDURE_FRAME)); 
}


CSProfLoopNode*
CSProfNode::AncestorLoop() const 
{
  dyn_cast_return(CSProfNode, CSProfLoopNode, Ancestor(LOOP));
}


CSProfStmtRangeNode*
CSProfNode::AncestorStmtRange() const 
{
  dyn_cast_return(CSProfNode, CSProfStmtRangeNode, Ancestor(STMT_RANGE));
}

//**********************************************************************
// 
//**********************************************************************


void 
IDynNode::mergeMetrics(const IDynNode& y, uint beg_idx)
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
IDynNode::appendMetrics(const IDynNode& y)
{
  for (int i = 0; i < y.numMetrics(); ++i) {
    m_metrics.push_back(y.metric(i));
  }
}


void 
IDynNode::expandMetrics_before(uint offset)
{
  for (int i = 0; i < offset; ++i) {
    m_metrics.insert(m_metrics.begin(), hpcfile_metric_data_ZERO);
  }
}


void 
IDynNode::expandMetrics_after(uint offset)
{
  for (int i = 0; i < offset; ++i) {
    m_metrics.push_back(hpcfile_metric_data_ZERO);
  }
}


void
CSProfNode::merge_prepare(uint numMetrics)
{
  IDynNode* dyn = dynamic_cast<IDynNode*>(this);
  if (dyn) {
    dyn->expandMetrics_after(numMetrics);
    DIAG_MsgIf(0, "Expanding " << hex << dyn << dec << " +" << numMetrics << " -> " << dyn->numMetrics());
  }

  // Recur on children
  for (CSProfNodeChildIterator it(this); it.Current(); ++it) {
    CSProfNode* child = it.CurNode();
    child->merge_prepare(numMetrics);
  }
}


// Let y be a node corresponding to 'this' = x and assume x is already
// merged.  Given y, merge y's children into x.
// NOTE: assume we can destroy y...
// NOTE: assume x already has space to store merged metrics
void
CSProfNode::merge(CSProfNode* y, 
		  const SampledMetricDescVec* new_mdesc,
		  uint x_numMetrics, uint y_numMetrics)
{
  CSProfNode* x = this;
  
  // ------------------------------------------------------------
  // 0. If y is childless, return.
  // ------------------------------------------------------------
  if (y->IsLeaf()) {
    return;
  }

  // ------------------------------------------------------------  
  // 1. If a child d of y _does not_ appear as a child of x, then copy
  //    (subtree) d [fixing d's metrics], make it a child of x and
  //    return.
  // 2. If a child d of y _does_ have a corresponding child c of x,
  //    merge [the metrics of] d into c and recur.
  // ------------------------------------------------------------  
  for (CSProfNodeChildIterator it(y); it.Current(); /* */) {
    CSProfNode* y_child = it.CurNode();
    IDynNode* y_child_dyn = dynamic_cast<IDynNode*>(y_child);
    DIAG_Assert(y_child_dyn, "");
    it++; // advance iterator -- it is pointing at 'child'

    CSProfNode* x_child = findDynChild(y_child_dyn->assocInfo(),
				       y_child_dyn->lm_id(),
				       y_child_dyn->ip_real(),
				       y_child_dyn->lip());
    if (!x_child) {
      y_child->Unlink();
      y_child->merge_fixup(new_mdesc, x_numMetrics);
      y_child->Link(x);
    }
    else {
      IDynNode* x_child_dyn = dynamic_cast<IDynNode*>(x_child);
      DIAG_MsgIf(0, "CSProfNode::merge: Merging y into x:\n"
		 << "  y: " << y_child_dyn->toString()
		 << "  x: " << x_child_dyn->toString());
      x_child_dyn->mergeMetrics(*y_child_dyn, x_numMetrics);
      x_child->merge(y_child, new_mdesc, x_numMetrics, y_numMetrics);
    }
  }
}


CSProfNode* 
CSProfNode::findDynChild(lush_assoc_info_t as_info, 
			 Epoch::LM_id_t lm_id, VMA ip, lush_lip_t* lip)
{
  for (CSProfNodeChildIterator it(this); it.Current(); ++it) {
    CSProfNode* child = it.CurNode();
    IDynNode* child_dyn = dynamic_cast<IDynNode*>(child);
    
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
CSProfNode::merge_fixup(const SampledMetricDescVec* new_mdesc, 
			int metric_offset)
{
  IDynNode* x_dyn = dynamic_cast<IDynNode*>(this);
  if (x_dyn) {
    x_dyn->expandMetrics_before(metric_offset);
    x_dyn->metricdesc(new_mdesc);
  }

  for (CSProfNodeChildIterator it(this); it.Current(); ++it) {
    CSProfNode* child = it.CurNode();
    IDynNode* child_dyn = dynamic_cast<IDynNode*>(child);
    if (child_dyn) {
      child->merge_fixup(new_mdesc, metric_offset);
    }
  }
}


// merge y into 'this' = x
void
CSProfNode::merge_node(CSProfNode* y)
{
  CSProfNode* x = this;
  
  // 1. copy y's metrics into x
  Prof::IDynNode* x_dyn = dynamic_cast<Prof::IDynNode*>(x);
  if (x_dyn) {
    Prof::IDynNode* y_dyn = dynamic_cast<Prof::IDynNode*>(y);
    DIAG_Assert(y_dyn, "");
    x_dyn->mergeMetrics(*y_dyn);
  }
  
  // 2. copy y's children into x
  for (Prof::CSProfNodeChildIterator it(y); it.Current(); /* */) {
    Prof::CSProfNode* y_child = it.CurNode();
    it++; // advance iterator -- it is pointing at 'y_child'
    y_child->Unlink();
    y_child->Link(x);
  }
  
  y->Unlink();
  delete y;
}

//**********************************************************************
// CSProfNode, etc: CodeName methods
//**********************************************************************

// NOTE: tallent: used for lush_cilkNormalize

string
CSProfCodeNode::codeName() const
{ 
  string self = NodeTypeToName(GetType()) + " "
    + GetFile() + ":" 
    + StrUtil::toStr(begLine) + "-" + StrUtil::toStr(endLine);
  return self;
}

string
CSProfProcedureFrameNode::codeName() const
{ 
  string self = NodeTypeToName(GetType()) + " "
    + GetProc() + " @ "
    + GetFile() + ":" 
    + StrUtil::toStr(begLine) + "-" + StrUtil::toStr(endLine);
  return self;
}


//**********************************************************************
// CSProfNode, etc: Dump contents for inspection
//**********************************************************************

string 
CSProfNode::toString_me(int dmpFlag) const
{ 
  string self;
  self = NodeTypeToName(GetType());

  // FIXME: tallent: temporary override
  if (GetType() == CSProfNode::PROCEDURE_FRAME) {
    const CSProfProcedureFrameNode* fr = 
      dynamic_cast<const CSProfProcedureFrameNode*>(this);
    self = fr->isAlien() ? "Pr" : "PF";
  }


  if ((dmpFlag & CCT::Tree::XML_TRUE) == CCT::Tree::XML_FALSE) {
    self = self + " uid" + xml::MakeAttrNum(id());
  }
  return self;
}


string
IDynNode::toString(const char* pre) const
{
  std::ostringstream os;
  dump(os, pre);
  return os.str();
}


std::string 
IDynNode::assocInfo_str() const
{
  char str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  lush_assoc_info_sprintf(str, m_as_info);
  return string(str);
}


std::string 
IDynNode::lip_str() const
{
  char str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(str, m_lip);
  return string(str);
}


void 
IDynNode::writeMetricsXML(std::ostream& os, 
			  int dmpFlag, const char* prefix) const
{
  bool wasMetricWritten = false;

  for (uint i = 0; i < numMetrics(); i++) {
    hpcfile_metric_data_t m = metric(i);
    if (!hpcfile_metric_data_iszero(m)) {
      os << ((!wasMetricWritten) ? prefix : "");
      os << "<M " << "n" << xml::MakeAttrNum(i) 
	 << " v" << writeMetric((*m_metricdesc)[i], m) << "/>";
      wasMetricWritten = true;
    }
  }
}


void
IDynNode::dump(std::ostream& o, const char* pre) const
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


void
IDynNode::ddump() const
{
  dump(std::cerr);
}


string
CSProfCodeNode::toString_me(int dmpFlag) const
{ 
  SrcFile::ln lnBeg = GetBegLine();
  string line = StrUtil::toStr(lnBeg);
  //SrcFile::ln lnEnd = GetEndLine();
  //if (lnBeg != lnEnd) {
  //  line += "-" + StrUtil::toStr(lnEnd);
  //}

  uint sId = (m_strct) ? m_strct->id() : 0;
  string self = CSProfNode::toString_me(dmpFlag)
    + " s" + xml::MakeAttrNum(sId)
    + " l" + xml::MakeAttrStr(line);
  return self;
}


string
CSProfPgmNode::toString_me(int dmpFlag) const
{ 
  string self = CSProfNode::toString_me(dmpFlag) + " n" +
    xml::MakeAttrStr(name, CCT::Tree::AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
CSProfGroupNode::toString_me(int dmpFlag) const
{
  string self = CSProfNode::toString_me(dmpFlag) + " n" +
    xml::MakeAttrStr(name, CCT::Tree::AddXMLEscapeChars(dmpFlag));
  return self;
}


string
CSProfProcedureFrameNode::toString_me(int dmpFlag) const
{
  string self = CSProfCodeNode::toString_me(dmpFlag);

  bool flg = CCT::Tree::AddXMLEscapeChars(dmpFlag);
  
  if (m_strct)  {
    const string& lm_nm = lmname();
    const string& fnm   = GetFile();
    const string& pnm   = GetProc();

    self += " lm" + xml::MakeAttrStr(lm_nm, flg)
      + " f" + xml::MakeAttrStr(fnm, flg)
      + " n" + xml::MakeAttrStr(pnm, flg);
    
    if (isAlien()) {
      self = self + " a=\"1\"";
    }
  }

  return self; 
}


string
CSProfCallSiteNode::toString_me(int dmpFlag) const
{
  string self = CSProfCodeNode::toString_me(dmpFlag);
  
  if (!(dmpFlag & CCT::Tree::XML_TRUE)) {
    self += " assoc" + xml::MakeAttrStr(assocInfo_str()) 
      + " ip_real" + xml::MakeAttrNum(ip_real(), 16)
      + " op" + xml::MakeAttrNum(opIndex())
      + " lip" + xml::MakeAttrStr(lip_str());
  }

  return self; 
} 


string
CSProfStatementNode::toString_me(int dmpFlag) const
{
  string self = CSProfCodeNode::toString_me(dmpFlag);

  if (!(dmpFlag & CCT::Tree::XML_TRUE)) {
    self += " ip" + xml::MakeAttrNum(ip(), 16) 
      + " op" + xml::MakeAttrNum(opIndex());
  }

  return self; 
} 


string 
CSProfLoopNode::toString_me(int dmpFlag) const
{
  string self = CSProfCodeNode::toString_me(dmpFlag); //+ " i" + MakeAttr(id);
  return self;
}


string
CSProfStmtRangeNode::toString_me(int dmpFlag) const
{
  string self = CSProfCodeNode::toString_me(dmpFlag); //+ " i" + MakeAttr(id);
  return self;
}


void
CSProfNode::writeXML(ostream &os, int dmpFlag, const char *pre) const 
{
  string indent = "  ";
  if (dmpFlag & CCT::Tree::COMPRESSED_OUTPUT) { 
    pre = ""; 
    indent = ""; 
  }

  if ( /*(dmpFlag & CCT::Tree::XML_TRUE) &&*/ IsLeaf()) { 
    dmpFlag |= CCT::Tree::XML_EMPTY_TAG; 
  }
  
  writeXML_pre(os, dmpFlag, pre); 
  string prefix = pre + indent;
  for (CSProfNodeSortedChildIterator it(this, CSProfNodeSortedIterator::cmpByStructureId); 
       it.Current(); it++) {
    CSProfNode* n = it.Current();
    n->writeXML(os, dmpFlag, prefix.c_str());
  }   
  writeXML_post(os, dmpFlag, pre);
}


void
CSProfNode::dump(ostream &os, int dmpFlag, const char *pre) const 
{
  writeXML(os, dmpFlag, pre); 
}


void
CSProfNode::ddump() const
{
  writeXML(std::cerr, CCT::Tree::XML_TRUE, ""); 
} 


void
CSProfNode::writeXML_pre(ostream& os, int dmpFlag, const char *prefix) const
{
  // tallent: Pgm has no representation
  if (GetType() == CSProfNode::PGM) {
    return;
  }

  os << prefix << "<" << toString_me(dmpFlag) << ">";

  const IDynNode* this_dyn = dynamic_cast<const IDynNode*>(this);
  if (this_dyn) {
    if (this_dyn->hasMetrics()) {
      os << endl;
      this_dyn->writeMetricsXML(os, dmpFlag, prefix);
    }
  }

  os << endl; 
}


void
CSProfNode::writeXML_post(ostream &os, int dmpFlag, const char *prefix) const
{
  // tallent: Pgm has no representation
  if (GetType() == CSProfNode::PGM) {
    return; 
  }

  // FIXME: tallent: temporary override
  if (GetType() == CSProfNode::PROCEDURE_FRAME) {
    const CSProfProcedureFrameNode* fr = 
      dynamic_cast<const CSProfProcedureFrameNode*>(this);
    string tag = fr->isAlien() ? "Pr" : "PF";
    os << prefix << "</" << tag << ">";
  }
  else {
    os << prefix << "</" << NodeTypeToName(GetType()) << ">";
  }

  os << endl; 
}


string 
CSProfNode::Types() const
{
  string types; 
  if (dynamic_cast<const CSProfNode*>(this)) {
    types += "CSProfNode "; 
  } 
  if (dynamic_cast<const CSProfCodeNode*>(this)) {
    types += "CSProfCodeNode "; 
  } 
  if (dynamic_cast<const CSProfPgmNode*>(this)) {
    types += "CSProfPgmNode "; 
  } 
  if (dynamic_cast<const CSProfGroupNode*>(this)) {
    types += "CSProfGroupNode "; 
  } 
  if (dynamic_cast<const CSProfCallSiteNode*>(this)) {
    types += "CSProfCallSiteNode "; 
  } 
  if (dynamic_cast<const CSProfLoopNode*>(this)) {
    types += "CSProfLoopNode "; 
  } 
  if (dynamic_cast<const CSProfStmtRangeNode*>(this)) {
    types += "CSProfStmtRangeNode "; 
  } 
  return types; 
} 


//**********************************************************************
// CSProfCodeNode specific methods 
//**********************************************************************

void 
CSProfCodeNode::SetLineRange(SrcFile::ln start, SrcFile::ln end) 
{
  // Sanity Checking
  DIAG_Assert(start <= end, "");   // start <= end

  if (start == ln_NULL) {
    DIAG_Assert(end == ln_NULL, "");
    // simply relocate at beginning of sibling list 
    // no range update in parents is necessary
    DIAG_Assert((begLine == ln_NULL) && (endLine == ln_NULL), ""); 
    //if (Parent() != NULL) Relocate(); 
  } 
  else {
    bool changed = false; 
    if (begLine == ln_NULL) {
      DIAG_Assert(endLine == ln_NULL, ""); 
      // initialize range
      begLine = start; 
      endLine = end; 
      changed = true;
    } 
    else {
      DIAG_Assert((begLine != ln_NULL) && (endLine != ln_NULL), "");
      // expand range ?
      if (start < begLine) { begLine = start; changed = true; }
      if (end   > endLine) { endLine = end; changed = true; }
    }
    CSProfCodeNode* _parent = dynamic_cast<CSProfCodeNode*>(Parent()); 
    if (changed && (_parent != NULL)) {
      //Relocate(); 
      //_parent->SetLineRange(begLine, endLine); 
    }
  }
}
  
void
CSProfCodeNode::Relocate() 
{
  CSProfCodeNode* prev = dynamic_cast<CSProfCodeNode*>(PrevSibling());
  CSProfCodeNode* next = dynamic_cast<CSProfCodeNode*>(NextSibling());
  if (((!prev) || (prev->endLine <= begLine)) && 
      ((!next) || (next->begLine >= endLine))) {
     return; 
  } 
  CSProfNode* _parent = Parent(); 
  Unlink(); 
  if (_parent->FirstChild() == NULL) {
    Link(_parent); 
  }
  else if (begLine == ln_NULL) {
    // insert as first child
    LinkBefore(_parent->FirstChild()); 
  } else {
    // insert after sibling with sibling->endLine < begLine 
    // or iff that does not exist insert as first in sibling list
    CSProfCodeNode *sibling = NULL;
    for (sibling = dynamic_cast<CSProfCodeNode*>(_parent->LastChild());
	 sibling; 
	 sibling = dynamic_cast<CSProfCodeNode*>(sibling->PrevSibling())) {
      if (sibling->endLine < begLine)  
	break; 
    } 
    if (sibling != NULL) {
      LinkAfter(sibling); 
    } else {
      LinkBefore(_parent->FirstChild()); 
    } 
  }
}

bool
CSProfCodeNode::ContainsLine(SrcFile::ln ln) const
{
  DIAG_Assert(ln != ln_NULL, ""); 
  return ((begLine >= 1) && (begLine <= ln) && (ln <= endLine)); 
} 

CSProfCodeNode* 
CSProfCodeNode::CSProfCodeNodeWithLine(SrcFile::ln ln) const
{
  DIAG_Assert(ln != ln_NULL, ""); 
  CSProfCodeNode *ci; 
  // ln > endLine means there is no child that contains ln
  if (ln <= endLine) {
    for (CSProfNodeChildIterator it(this); it.Current(); it++) {
      ci = dynamic_cast<CSProfCodeNode*>(it.Current()); 
      DIAG_Assert(ci, ""); 
      if  (ci->ContainsLine(ln)) {
	return ci->CSProfCodeNodeWithLine(ln); 
      } 
    }
  }
  return (CSProfCodeNode*) this; // base case
}

int CSProfCodeNodeLineComp(CSProfCodeNode* x, CSProfCodeNode* y)
{
  if (x->GetBegLine() == y->GetBegLine()) {
    // Given two CSProfCodeNode's with identical endpoints consider two
    // special cases:
    bool endLinesEqual = (x->GetEndLine() == y->GetEndLine());
    
    // 1. Otherwise: rank a leaf node before a non-leaf node
    if (endLinesEqual && !(x->IsLeaf() && y->IsLeaf())) {
      if      (x->IsLeaf()) { return -1; } // x < y 
      else if (y->IsLeaf()) { return 1; }  // x > y  
    }
    
    // 2. General case
    return SrcFile::compare(x->GetEndLine(), y->GetEndLine());
  }
  else {
    return SrcFile::compare(x->GetBegLine(), y->GetBegLine());
  }
}


} // namespace Prof

