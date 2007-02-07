// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    CSProfTree.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;
using std::endl;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include <include/general.h>

#include "CSProfTree.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>

#include <lib/xml/xml.hpp> 
using namespace xml;

//*************************** Forward Declarations ***************************

int SimpleLineCmp(suint x, suint y);

//***************************************************************************

//***************************************************************************
// CSProfTree
//***************************************************************************

CSProfTree::CSProfTree()
  : root(NULL)
{
}

CSProfTree::~CSProfTree()
{
  delete root; 
}

void 
CSProfTree::Dump(std::ostream& os, int dmpFlag) const
{
  os << "<CSPROFILETREE>\n";
  if (root) {
    root->DumpLineSorted(os, dmpFlag);
  }
  os << "</CSPROFILETREE>\n";
}

void 
CSProfTree::DDump() const
{
  Dump();
}

//***************************************************************************
// NodeType `methods' (could completely replace with dynamic typing)
//***************************************************************************

const string CSProfNode::NodeNames[CSProfNode::NUMBER_OF_TYPES] = {
  "PGM", "G", "CALLSITE", "L", "S", "PROCEDURE_FRAME", "STATEMENT", "ANY"
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
  static unsigned int uniqueId = 0; 
  uid = uniqueId++; 
  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 if (type==STATEMENT) {
	   fprintf(stderr, " CSProfNode constructor: copying callsite into statement node\n");
	   )
	 }
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
  IFTRACE << "~CSProfNode " << this << " " << ToString() << endl; 
}

CSProfCodeNode::CSProfCodeNode(NodeType t, CSProfNode* _parent, 
			       suint begLn, suint endLn) 
  : CSProfNode(t, _parent), begLine(UNDEF_LINE), endLine(UNDEF_LINE)
{ 
  SetLineRange(begLn, endLn); 
  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 if (type==STATEMENT) {
	   fprintf(stderr, " CSProfCodeNode constructor: copying callsite into statement node\n");
	   )
	 }}

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
	      || (_parent->GetType() == CSProfNode::PROCEDURE_FRAME) 
	      || (_parent->GetType() == CSProfNode::CALLSITE), "");
}

CSProfCallSiteNode::CSProfCallSiteNode(CSProfNode* _parent)
  : CSProfCodeNode(CALLSITE, _parent, UNDEF_LINE, UNDEF_LINE)
{
  CSProfCallSiteNode_Check(this, _parent);
}


CSProfCallSiteNode::CSProfCallSiteNode(CSProfNode* _parent, VMA _ip, 
			      ushort _opIndex, vector<suint> _metrics)
  : CSProfCodeNode(CALLSITE, _parent, UNDEF_LINE, UNDEF_LINE), 
    ip(_ip), opIndex(_opIndex), metrics(_metrics) 
{
  CSProfCallSiteNode_Check(this, _parent);
}

CSProfCallSiteNode::~CSProfCallSiteNode()
{
}


CSProfStatementNode::CSProfStatementNode(CSProfNode* _parent)
  :  CSProfCodeNode(STATEMENT, _parent, UNDEF_LINE, UNDEF_LINE)
{
}

CSProfStatementNode::~CSProfStatementNode()
{
}


void 
CSProfStatementNode::operator=(const CSProfStatementNode& x)
{
  file = x.GetFile();
  proc = x.GetProc();
  SetLine(x.GetLine());
  SetIP(x.GetIP(), x.GetOpIndex());

  metrics.resize(x.GetMetricCount());
  addMetrics(&x);

  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 fprintf(stderr, " copied metrics\n"));
  fileistext = x.FileIsText();
}


void 
CSProfStatementNode::copyCallSiteNode(CSProfCallSiteNode* _node) 
{
  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 fprintf(stderr, " explicit copying callsite into statement node\n"));
  file = _node->GetFile();
  proc = _node->GetProc();
  SetLine(_node->GetLine());
  SetIP(_node->GetIP(), _node->GetOpIndex());
  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 fprintf(stderr, " copied file, proc, line, ip, opindex\n"));
  int i;
  for (i=0; i<_node->GetMetrics().size(); i++) 
    metrics.push_back(_node->GetMetric(i));
  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 fprintf(stderr, " copied metrics\n"));
  fileistext = _node->FileIsText();
  donewithsrcinfproc = _node->GotSrcInfo();
}


CSProfProcedureFrameNode::CSProfProcedureFrameNode(CSProfNode* _parent)
  : CSProfCodeNode(PROCEDURE_FRAME, _parent, UNDEF_LINE, UNDEF_LINE)
{
  CSProfCallSiteNode_Check(NULL, _parent);
}

CSProfProcedureFrameNode::~CSProfProcedureFrameNode()
{
}


CSProfLoopNode::CSProfLoopNode(CSProfNode* _parent, 
			       suint begLn, suint endLn, int _id)
  : CSProfCodeNode(LOOP, _parent, begLn, endLn), id(_id)
{
  DIAG_Assert((_parent == NULL) || (_parent->GetType() == GROUP) 
	      || (_parent->GetType() == CALLSITE) 
	      || (_parent->GetType() == LOOP), "");
}

CSProfLoopNode::~CSProfLoopNode()
{
}

CSProfStmtRangeNode::CSProfStmtRangeNode(CSProfNode* _parent, 
					 suint begLn, suint endLn, int _id)
  : CSProfCodeNode(STMT_RANGE, _parent, begLn, endLn), id(_id)
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
// CSProfNode, etc: CodeName methods
//**********************************************************************

string
CSProfLoopNode::CodeName() const
{
  string self = NodeTypeToName(GetType());
  self += " " + CSProfCodeNode::ToDumpString();
  return self;
}

string
CSProfStmtRangeNode::CodeName() const
{
  string self = NodeTypeToName(GetType());
  self += " " + CSProfCodeNode::ToDumpString();
  return self;
}

//**********************************************************************
// CSProfNode, etc: Dump contents for inspection
//**********************************************************************

string 
CSProfNode::ToDumpString(int dmpFlag) const
{ 
  string self;
  self = NodeTypeToName(GetType());
  if ((dmpFlag & CSProfTree::XML_TRUE) == CSProfTree::XML_FALSE) {
    self = self + " uid" + MakeAttrNum(GetUniqueId());
  }
  return self;
} 

string 
CSProfNode::ToDumpMetricsString(int dmpFlag) const {
  return "";
}


string
CSProfCodeNode::ToDumpString(int dmpFlag) const
{ 
  string self = CSProfNode::ToDumpString(dmpFlag) + 
    " b" + MakeAttrNum(begLine) + " e" + MakeAttrNum(endLine);
  return self;
}

string
CSProfPgmNode::ToDumpString(int dmpFlag) const
{ 
  string self = CSProfNode::ToDumpString(dmpFlag) + " n" +
    MakeAttrStr(name, AddXMLEscapeChars(dmpFlag));
  return self;
}

string 
CSProfGroupNode::ToDumpString(int dmpFlag) const
{
  string self = CSProfNode::ToDumpString(dmpFlag) + " n" +
    MakeAttrStr(name, AddXMLEscapeChars(dmpFlag));
  return self;
}

string
CSProfCallSiteNode::ToDumpString(int dmpFlag) const
{
  string self = CSProfNode::ToDumpString(dmpFlag);
  
  if (!(dmpFlag & CSProfTree::XML_TRUE)) {
    self = self + " ip" + MakeAttrNum(ip, 16) 
      + " op" + MakeAttrNum(opIndex);
  } 

  if (!file.empty()) { 
     if (fileistext)
        self = self + " f" + MakeAttrStr(file, AddXMLEscapeChars(dmpFlag)); 
     else 
        self = self + " lm" + MakeAttrStr(file, AddXMLEscapeChars(dmpFlag)); 
   } 

  if (!proc.empty()) {
    self = self + " p" + MakeAttrStr(proc, AddXMLEscapeChars(dmpFlag));
  } 
  else {
    self = self + " ip" + MakeAttrNum(ip, 16);
  }
  

  if (GetBegLine() != UNDEF_LINE) {
    self = self + " l" + MakeAttrNum(GetBegLine());
  }  

  return self; 
} 


string 
CSProfCallSiteNode::ToDumpMetricsString(int dmpFlag) const 
{
  int i;
  string metricsString;

  xDEBUG(DEB_READ_MMETRICS,
	 fprintf(stderr, "dumping metrics for node %lx \n", ip); 
	 fflush(stderr);
	 for (i=0; i<metrics.size(); i++) {
	   fprintf(stderr, "Metric %d: %ld\n", i, metrics[i]);
	   fflush(stderr);
	 }
	 );

  metricsString ="";
  for (i=0; i<metrics.size(); i++) {
    suint crtMetric = metrics[i];
    if (crtMetric!= 0) {
      metricsString  +=  " <M ";
      metricsString  +=  "n"+MakeAttrNum(i)+" v" + MakeAttrNum(crtMetric);
      metricsString  +=  " />";
    }
  }
  return metricsString;
}

/** Add metrics from call site node c to current node.
 * @param c call site node to be "added" to this node
 */
void 
CSProfCallSiteNode::addMetrics(CSProfCallSiteNode* c) 
{
  DIAG_Assert(metrics.size() == c->metrics.size(), "");
  int numMetrics = metrics.size();
  int i;
  for (i=0; i<numMetrics; i++) {
    metrics[i] += c->metrics[i];
  }
}

string
CSProfStatementNode::ToDumpString(int dmpFlag) const
{
  string self = CSProfNode::ToDumpString(dmpFlag);
  
  if (!(dmpFlag & CSProfTree::XML_TRUE)) {
    self = self + " ip" + MakeAttrNum(ip, 16) 
      + " op" + MakeAttrNum(opIndex);
  } 

  if (!file.empty()) { 
     if (fileistext)
        self = self + " f" + MakeAttrStr(file, AddXMLEscapeChars(dmpFlag)); 
     else 
        self = self + " lm" + MakeAttrStr(file, AddXMLEscapeChars(dmpFlag)); 
   } 

  if (!proc.empty()) {
    self = self + " p" + MakeAttrStr(proc, AddXMLEscapeChars(dmpFlag));
  } 
  else {
    self = self + " ip" + MakeAttrNum(ip, 16);
  }
  

  if (GetBegLine() != UNDEF_LINE) {
    self = self + " l" + MakeAttrNum(GetBegLine());
  }  

  return self; 
} 


string 
CSProfStatementNode::ToDumpMetricsString(int dmpFlag) const {
  int i;
  string metricsString;

  xDEBUG(DEB_READ_MMETRICS,
	 fprintf(stderr, "dumping metrics for node %lx \n", ip); 
	 fflush(stderr);
	 for (i=0; i<metrics.size(); i++) {
	   fprintf(stderr, "Metric %d: %ld\n", i, metrics[i]);
	   fflush(stderr);
	 }
	 );

  metricsString ="";
  for (i=0; i<metrics.size(); i++) {
    suint crtMetric = metrics[i];
    if (crtMetric!= 0) {
      metricsString += " <M ";
      metricsString += "n" + MakeAttrNum(i) + " v" + MakeAttrNum(crtMetric);
      metricsString += " />";
    }
  }
  return metricsString;
}


/** Add metrics from call site node c to current node.
 * @param c call site node to be "added" to this node
 */
void 
CSProfStatementNode::addMetrics(const CSProfStatementNode* c) 
{
  DIAG_Assert(metrics.size() == c->metrics.size(), "");
  int numMetrics = metrics.size();
  int i;
  for (i=0; i<numMetrics; i++) {
    metrics[i] += c->metrics[i];
  }
}


string
CSProfProcedureFrameNode::ToDumpString(int dmpFlag) const
{
  string self = CSProfNode::ToDumpString(dmpFlag);
  
  if (!file.empty()) { 
     if (fileistext)
        self = self + " f" + MakeAttrStr(file, AddXMLEscapeChars(dmpFlag)); 
     else 
        self = self + " lm" + MakeAttrStr(file, AddXMLEscapeChars(dmpFlag)); 
   } 

  if (!proc.empty()) {
    self = self + " p" + MakeAttrStr(proc, AddXMLEscapeChars(dmpFlag));
  } else {
    self = self + " p" + MakeAttrStr("unknown", AddXMLEscapeChars(dmpFlag)) ; 
  }

  if (GetBegLine() != UNDEF_LINE) {
    self = self + " l" + MakeAttrNum(GetBegLine());
  }  

  return self; 
} 


string 
CSProfLoopNode::ToDumpString(int dmpFlag) const
{
  string self = CSProfCodeNode::ToDumpString(dmpFlag); // + " i" + MakeAttr(id);
  return self;
}

string
CSProfStmtRangeNode::ToDumpString(int dmpFlag) const
{
  string self = CSProfCodeNode::ToDumpString(dmpFlag); // + " i" + MakeAttr(id);
  return self;
}


void
CSProfNode::DumpSelfBefore(ostream &os, int dmpFlag, const char *prefix) const
{
  os << prefix << "<" << ToDumpString(dmpFlag);
  os << ">"; 
  switch (GetType()) {
  case CALLSITE:  
  case STATEMENT:
    if (ToDumpMetricsString(dmpFlag)!="") {
      os << endl;
      os << prefix << "  " << ToDumpMetricsString(dmpFlag); 
    }
    break;
  default:
    break;
  }
  if (!(dmpFlag & CSProfTree::COMPRESSED_OUTPUT)) { os << endl; }
}

void
CSProfNode::DumpSelfAfter(ostream &os, int dmpFlag, const char *prefix) const
{
  os << prefix << "</" << NodeTypeToName(GetType()) << ">";
  if (!(dmpFlag & CSProfTree::COMPRESSED_OUTPUT)) { os << endl; }
}

void
CSProfNode::Dump(ostream &os, int dmpFlag, const char *pre) const 
{
  string indent = "  ";
  if (dmpFlag & CSProfTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  
  if (/*(dmpFlag & CSProfTree::XML_TRUE) &&*/ IsLeaf()) { 
    dmpFlag |= CSProfTree::XML_EMPTY_TAG; 
  }
  
  DumpSelfBefore(os, dmpFlag, pre); 
  string prefix = pre + indent;
  for (CSProfNodeChildIterator it(this); it.Current(); it++) {
    it.CurNode()->Dump(os, dmpFlag, prefix.c_str()); 
  }
  DumpSelfAfter(os, dmpFlag, pre);
}

// circumvent pain caused by debuggers that choke on default arguments
// or that remove all traces of functions defined in the class declaration.
void
CSProfNode::DDump()
{
  Dump(std::cerr, CSProfTree::XML_TRUE, ""); 
} 

void
CSProfNode::DDumpSort()
{
  DumpLineSorted(std::cerr, CSProfTree::XML_TRUE, ""); 
}

void
CSProfNode::DumpLineSorted(ostream &os, int dmpFlag, const char *pre) const 
{
  string indent = "  ";
  if (dmpFlag & CSProfTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  
  if ( /*(dmpFlag & CSProfTree::XML_TRUE) &&*/ IsLeaf()) { 
    dmpFlag |= CSProfTree::XML_EMPTY_TAG; 
  }
  
  DumpSelfBefore(os, dmpFlag, pre); 
  string prefix = pre + indent;
  for (CSProfNodeLineSortedChildIterator it(this); it.Current(); it++) {
    CSProfNode* n = it.Current();
    n->DumpLineSorted(os, dmpFlag, prefix.c_str());
  }   
  DumpSelfAfter(os, dmpFlag, pre);
}

string 
CSProfNode::Types() 
{
  string types; 
  if (dynamic_cast<CSProfNode*>(this)) {
    types += "CSProfNode "; 
  } 
  if (dynamic_cast<CSProfCodeNode*>(this)) {
    types += "CSProfCodeNode "; 
  } 
  if (dynamic_cast<CSProfPgmNode*>(this)) {
    types += "CSProfPgmNode "; 
  } 
  if (dynamic_cast<CSProfGroupNode*>(this)) {
    types += "CSProfGroupNode "; 
  } 
  if (dynamic_cast<CSProfCallSiteNode*>(this)) {
    types += "CSProfCallSiteNode "; 
  } 
  if (dynamic_cast<CSProfLoopNode*>(this)) {
    types += "CSProfLoopNode "; 
  } 
  if (dynamic_cast<CSProfStmtRangeNode*>(this)) {
    types += "CSProfStmtRangeNode "; 
  } 
  return types; 
} 


//**********************************************************************
// CSProfCodeNode specific methods 
//**********************************************************************

void 
CSProfCodeNode::SetLineRange(suint start, suint end) 
{
  // Sanity Checking
  DIAG_Assert(start <= end, "");   // start <= end

  if (start == UNDEF_LINE) {
    DIAG_Assert(end == UNDEF_LINE, "");
    // simply relocate at beginning of sibling list 
    // no range update in parents is necessary
    DIAG_Assert((begLine == UNDEF_LINE) && (endLine == UNDEF_LINE), ""); 
    if (Parent() != NULL) Relocate(); 
  } else {
    bool changed = false; 
    if (begLine == UNDEF_LINE) {
      DIAG_Assert(endLine == UNDEF_LINE, ""); 
      // initialize range
      begLine = start; 
      endLine = end; 
      changed = true;
    } else {
      DIAG_Assert((begLine != UNDEF_LINE) && (endLine != UNDEF_LINE), "");
      // expand range ?
      if (start < begLine) { begLine = start; changed = true; }
      if (end   > endLine)   { endLine = end; changed = true; }
    }
    CSProfCodeNode* _parent = dynamic_cast<CSProfCodeNode*>(Parent()); 
    if (changed && (_parent != NULL)) {
      Relocate(); 
      _parent->SetLineRange(begLine, endLine); 
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
  else if (begLine == UNDEF_LINE) {
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
CSProfCodeNode::ContainsLine(suint ln) const
{
  DIAG_Assert(ln != UNDEF_LINE, ""); 
  return ((begLine >= 1) && (begLine <= ln) && (ln <= endLine)); 
} 

CSProfCodeNode* 
CSProfCodeNode::CSProfCodeNodeWithLine(suint ln) const
{
  DIAG_Assert(ln != UNDEF_LINE, ""); 
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
    return SimpleLineCmp(x->GetEndLine(), y->GetEndLine()); 
  } else {
    return SimpleLineCmp(x->GetBegLine(), y->GetBegLine());
  }
}

// - if x < y; 0 if x == y; + otherwise
int SimpleLineCmp(suint x, suint y)
{
  // We would typically wish to use the following for this simple
  // comparison, but it fails if the the differences are greater than
  // an 'int'
  // return (x - y)

  if (x < y)       { return -1; }
  else if (x == y) { return 0; }
  else             { return 1; }
}

// Given a set of flags 'dmpFlag', determines whether we need to
// ensure that certain characters are escaped.  Returns xml::ESC_TRUE
// or xml::ESC_FALSE. 
int AddXMLEscapeChars(int dmpFlag)
{
  if ((dmpFlag & CSProfTree::XML_TRUE) &&
      !(dmpFlag & CSProfTree::XML_NO_ESC_CHARS)) {
    return xml::ESC_TRUE;
  } else {
    return xml::ESC_FALSE;
  }
}

