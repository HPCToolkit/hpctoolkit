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
//    $Source$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <cstdio> // for 'fopen'
#include <climits>

#include <iostream>
using std::ostream;
using std::hex;
using std::dec;
using std::endl;

#include <string>
using std::string;

#include <algorithm>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "Struct-Tree.hpp"
#include "PerfMetric.hpp"

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/StrUtil.hpp>
#include <lib/support/realpath.h>

//*************************** Forward Declarations **************************

static int SimpleLineCmp(SrcFile::ln x, SrcFile::ln y);
static int AddXMLEscapeChars(int dmpFlag);

using namespace xml;

//***************************************************************************

//***************************************************************************
// Tree
//***************************************************************************

namespace Prof {
namespace Struct {

uint ANode::s_nextUniqueId = 1;

const std::string Tree::UnknownFileNm = "~~~<unknown-file>~~~";

const std::string Tree::UnknownProcNm = "~<unknown-proc>~";


Tree::Tree(const char* name, Pgm* _root)
  : root(_root)
{
}


Tree::~Tree()
{
  delete root;
}


void 
Tree::CollectCrossReferences() 
{ 
  root->NoteHeight();
  root->NoteDepth();
  root->CollectCrossReferences();
}


void 
Tree::xml_dump(ostream& os, int dmpFlags) const
{
  if (root) {
    root->XML_DumpLineSorted(os, dmpFlags);
  }
}


void 
Tree::dump(ostream& os, int dmpFlags) const
{
  xml_dump(os, dmpFlags);
}


void 
Tree::ddump() const
{
  dump();
}

} // namespace Struct
} // namespace Prof


namespace Prof {
namespace Struct {

/*****************************************************************************/
// ANodeTy `methods' (could completely replace with dynamic typing)
/*****************************************************************************/

const string ANode::ScopeNames[ANode::TyNUMBER] = {
  "PGM", "GRP", "LM", "FIL", "PRC", "A", "LP", "SR", "REF", "ANY"
};


const string&
ANode::ANodeTyToName(ANodeTy tp) 
{
  return ScopeNames[tp];
}


ANode::ANodeTy 
ANode::IntToANodeTy(long i)
{
  DIAG_Assert((i >= 0) && (i < TyNUMBER), "");
  return (ANodeTy) i;
}


//***************************************************************************
// ANode, etc: constructors/destructors
//***************************************************************************

ANode& 
ANode::operator=(const ANode& x) 
{
  // shallow copy
  if (&x != this) {
    type     = x.type;
    uid      = x.uid;
    height   = 0;
    depth    = 0;
    perfData = x.perfData;
    
    ZeroLinks(); // NonUniformDegreeTreeNode
  }
  return *this;
}


void 
ANode::CollectCrossReferences() 
{
  for (ANodeChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CollectCrossReferences();
  } 

  ACodeNode* self = dynamic_cast<ACodeNode*>(this);  
  if (self) {
    if (IsLeaf()) {
      self->first = self->last = self;
    } 
    else {
      self->first = self->last = 0;

      int minline = INT_MAX;
      int maxline = -1;
      for (ANodeChildIterator it(this); it.Current(); it++) {
	ACodeNode* child = dynamic_cast<ACodeNode*>(it.CurScope());
	int cmin = child->begLine();
	int cmax = child->endLine();
	if (cmin < minline) {
	  minline = cmin;
	  self->first = child->first;
	}
	if (cmax > maxline) {
	  maxline = cmax;
	  self->last = child->last;
	} 
      }
    }
  }
}


int 
ANode::NoteHeight() 
{
  if (IsLeaf()) {
    height = 0;
  } 
  else {
    height = 0;
    for (ANodeChildIterator it(this); it.Current(); it++) {
      int childHeight = it.CurScope()->NoteHeight();
      height = std::max(height, childHeight + 1);
    } 
  }
  return height;
}


void 
ANode::NoteDepth() 
{
  ANode* p = Parent();
  if (p) {
    depth = p->depth + 1;
  } 
  else {
    depth = 0;
  }
  for (ANodeChildIterator it(this); it.Current(); it++) {
    it.CurScope()->NoteDepth();
  } 
}


void
ANode::ctorCheck() const
{
  bool isOK = ((type == TyPGM) || (AncPgm() == NULL) || !AncPgm()->IsFrozen());
  DIAG_Assert(isOK, "");
} 


void
ANode::dtorCheck() const
{
  DIAG_DevMsgIf(0, "~ANode::ANode: " << toString_id()
		<< " " << std::hex << this << std::dec);
  bool isOK = false;

  Pgm* pgm = AncPgm();
  if (pgm && pgm->Type() != TyPGM) {
    isOK = true;
  }
  else {
    DIAG_Assert(pgm == NULL || pgm->Type() == TyPGM, "");
    isOK = ((pgm == NULL) || !(pgm->IsFrozen()));
  }
  
  DIAG_Assert(isOK, "ANode '" << this << " " << ToString() 
	      << "' is not ready for deletion!");
} 


ACodeNode& 
ACodeNode::operator=(const ACodeNode& x) 
{
  // shallow copy
  if (&x != this) {
    m_begLn = x.m_begLn;
    m_endLn = x.m_endLn;
    first = last = NULL;
  }
  return *this;
}


void 
ACodeNode::LinkAndSetLineRange(ACodeNode* parent)
{
  this->Link(parent);
  if (begLine() != ln_NULL) {
    SrcFile::ln bLn = std::min(parent->begLine(), begLine());
    SrcFile::ln eLn = std::max(parent->endLine(), endLine());
    parent->SetLineRange(bLn, eLn);
  }
}


void
Pgm::Ctor(const char* nm)
{
  DIAG_Assert(nm, "");
  frozen = false;
  m_name = nm;
  groupMap = new GroupMap();
  lmMap = new LMMap();
}


Pgm& 
Pgm::operator=(const Pgm& x) 
{
  // shallow copy
  if (&x != this) {
    frozen   = x.frozen;
    m_name     = x.m_name;
    groupMap = NULL;
    lmMap    = NULL;
  }
  return *this;
}


void
Group::Ctor(const char* nm, ANode* parent)
{
  DIAG_Assert(nm, "");
  ANodeTy t = (parent) ? parent->Type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyPGM) || (t == TyGROUP) || (t == TyLM) 
	      || (t == TyFILE) || (t == TyPROC) || (t == TyALIEN) 
	      || (t == TyLOOP), "");
  m_name = nm;
  AncPgm()->AddToGroupMap(this);
}


void
LM::Ctor(const char* nm, ANode* parent)
{
  DIAG_Assert(nm, "");
  ANodeTy t = (parent) ? parent->Type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyPGM) || (t == TyGROUP), "");

  m_name = nm;
  fileMap = new FileMap();
  procMap = NULL;
  stmtMap = NULL;

  AncPgm()->AddToLoadModMap(this);
}


LM& 
LM::operator=(const LM& x)
{
  // shallow copy
  if (&x != this) {
    m_name     = x.m_name;
    fileMap  = NULL;
    procMap  = NULL;
    stmtMap  = NULL;
  }
  return *this;
}


void
File::Ctor(const char* srcFileWithPath, bool srcIsReadble_, 
		ANode* parent)
{
  DIAG_Assert(srcFileWithPath, "");
  ANodeTy t = (parent) ? parent->Type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyPGM) || (t == TyGROUP) || (t == TyLM), "");

  srcIsReadable = srcIsReadble_;
  m_name = srcFileWithPath;
  AncLM()->AddToFileMap(this);
  procMap = new ProcMap();
}


File& 
File::operator=(const File& x) 
{
  // shallow copy
  if (&x != this) {
    srcIsReadable = x.srcIsReadable;
    m_name          = x.m_name;
    procMap       = NULL;
  }
  return *this;
}


File* 
File::findOrCreate(LM* lmScope, const string& filenm)
{
  File* fScope = lmScope->FindFile(filenm);
  if (fScope == NULL) {
    bool fileIsReadable = FileUtil::isReadable(filenm.c_str());
    fScope = new File(filenm, fileIsReadable, lmScope);
  }
  return fScope; // guaranteed to be a valid pointer
}


void
Proc::Ctor(const char* n, ACodeNode* parent, const char* ln, bool hasSym)
{
  DIAG_Assert(n, "");
  ANodeTy t = (parent) ? parent->Type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyGROUP) || (t == TyFILE), "");

  m_name = (n) ? n : "";
  m_linkname = (ln) ? ln : "";
  m_hasSym = hasSym;
  stmtMap = new StmtMap();
  if (parent) {
    Relocate();
  }

  File* fileStrct = AncFile();
  if (fileStrct) { 
    fileStrct->AddToProcMap(this); 
  }
}


Proc& 
Proc::operator=(const Proc& x) 
{
  // shallow copy
  if (&x != this) {
    m_name     = x.m_name;
    m_linkname = x.m_linkname;
    m_hasSym   = x.m_hasSym;
    stmtMap = new StmtMap();
  }
  return *this;
}


Proc*
Proc::findOrCreate(File* fScope, const string& procnm, 
			SrcFile::ln line)
{
  DIAG_Die("The Proc is always created as non-symbolic...");
  Proc* pScope = fScope->FindProc(procnm);
  if (!pScope) {
    pScope = new Proc(procnm, fScope, procnm, false, line, line);
  }
  return pScope;
}


void
Alien::Ctor(ACodeNode* parent, const char* filenm, const char* nm)
{
  ANodeTy t = (parent) ? parent->Type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyGROUP) || (t == TyALIEN) 
	      || (t == TyPROC) || (t == TyLOOP), "");

  m_filenm = (filenm) ? filenm : "";
  m_name   = (nm) ? nm : "";
}


Alien& 
Alien::operator=(const Alien& x) 
{
  // shallow copy
  if (&x != this) {
    m_filenm = x.m_filenm;
    m_name   = x.m_name;
  }
  return *this;
}


Ref::Ref(ACodeNode* parent, int _begPos, int _endPos, const char* refName) 
  : ACodeNode(TyREF, parent, parent->begLine(), parent->begLine(), 0, 0)
{
  DIAG_Assert(parent->Type() == TySTMT, "");
  begPos = _begPos;
  endPos = _endPos;
  m_name = refName;
  RelocateRef();
  DIAG_Assert(begPos <= endPos, "");
  DIAG_Assert(begPos > 0, "");
  DIAG_Assert(m_name.length() > 0, "");
}


//***************************************************************************
// ANode: Ancestor
//***************************************************************************

#define dyn_cast_return(base, derived, expr) \
    { base* ptr = expr;  \
      if (ptr == NULL) {  \
        return NULL;  \
      } else {  \
	return dynamic_cast<derived*>(ptr);  \
      } \
    }


ACodeNode* 
ANode::ACodeNodeParent() const
{
  dyn_cast_return(ANode, ACodeNode, Parent());
}


ANode*
ANode::Ancestor(ANodeTy tp) const
{
  const ANode* s = this;
  while (s && s->Type() != tp) {
    s = s->Parent();
  } 
  return (ANode*) s;
} 


ANode *
ANode::Ancestor(ANodeTy tp1, ANodeTy tp2) const
{
  const ANode* s = this;
  while (s && s->Type() != tp1 && s->Type() != tp2) {
    s = s->Parent();
  }
  return (ANode*) s;
} 


#if 0
int IsAncestorOf(ANode *parent, ANode *son, int difference)
{
  ANode *iter = son;
  while (iter && difference > 0 && iter != parent) {
    iter = iter->Parent();
    difference--;
  }
  if (iter && iter == parent)
     return 1;
  return 0;
}
#endif


ANode* 
ANode::LeastCommonAncestor(ANode* n1, ANode* n2)
{
  // Collect all ancestors of n1 and n2.  The root will be at the front
  // of the ancestor list.
  ANodeList anc1, anc2;
  for (ANode* a = n1->Parent(); (a); a = a->Parent()) {
    anc1.push_front(a);
  }
  for (ANode* a = n2->Parent(); (a); a = a->Parent()) {
    anc2.push_front(a);
  }
  
  // Find the most deeply nested common ancestor
  ANode* lca = NULL;
  while ( (!anc1.empty() && !anc2.empty()) && (anc1.front() == anc2.front())) {
    lca = anc1.front();
    anc1.pop_front();
    anc2.pop_front();
  }
  
  return lca;
}


Pgm*
ANode::AncPgm() const 
{
  // iff this is called during ANode construction within the Pgm 
  // construction dyn_cast does not do the correct thing
  if (Parent() == NULL) {
    // eraxxon: This cannot be a good thing to do!  Pgm() was being
    //   called on a LoopInfo object; then the resulting pointer
    //   (the LoopInfo itself) was queried for PgmInfo member data.  Ouch!
    // eraxxon: return (Pgm*) this;
    return NULL;
  } 
  else { 
    dyn_cast_return(ANode, Pgm, Ancestor(TyPGM));
  }
}


Group*
ANode::AncGroup() const 
{
  dyn_cast_return(ANode, Group, Ancestor(TyGROUP));
}


LM*
ANode::AncLM() const 
{
  dyn_cast_return(ANode, LM, Ancestor(TyLM));
}


File*
ANode::AncFile() const 
{
  dyn_cast_return(ANode, File, Ancestor(TyFILE));
}


Proc*
ANode::AncProc() const 
{
  dyn_cast_return(ANode, Proc, Ancestor(TyPROC));
}


Alien*
ANode::AncAlien() const 
{
  dyn_cast_return(ANode, Alien, Ancestor(TyALIEN));
}


Loop*
ANode::AncLoop() const 
{
  dyn_cast_return(ANode, Loop, Ancestor(TyLOOP));
}


Stmt*
ANode::AncStmt() const 
{
  dyn_cast_return(ANode, Stmt, Ancestor(TySTMT));
}


ACodeNode*
ANode::AncCallingCtxt() const 
{
  return dynamic_cast<ACodeNode*>(Ancestor(TyPROC, TyALIEN));
}



//***************************************************************************
// ANode: Tree Navigation
//***************************************************************************

ACodeNode* 
ANode::FirstEnclScope() const
{
  dyn_cast_return(NonUniformDegreeTreeNode, ACodeNode, FirstChild());
}


ACodeNode*
ANode::LastEnclScope() const
{
  dyn_cast_return(NonUniformDegreeTreeNode, ACodeNode, LastChild());
}


// ----------------------------------------------------------------------
// siblings are linked in a circular list
// Parent()->FirstChild()->PrevSibling() == Parent->LastChild() and 
// Parent()->LastChild()->NextSibling()  == Parent->FirstChild()
// ----------------------------------------------------------------------

ACodeNode*
ANode::NextScope() const
{
  // siblings are linked in a circular list, 
  NonUniformDegreeTreeNode* next = NULL;
  if (dynamic_cast<ANode*>(Parent()->LastChild()) != this) {
    next = NextSibling();
  } 
  if (next) { 
    ACodeNode* ci = dynamic_cast<ACodeNode*>(next);
    DIAG_Assert(ci, "");
    return ci;
  }
  return NULL;  
}


ACodeNode*
ANode::PrevScope() const
{
  NonUniformDegreeTreeNode* prev = NULL;
  if (dynamic_cast<ANode*>(Parent()->FirstChild()) != this) { 
    prev = PrevSibling();
  } 
  if (prev) { 
    ACodeNode* ci = dynamic_cast<ACodeNode*>(prev);
    DIAG_Assert(ci, "");
    return ci;
  }
  return NULL;
}


ACodeNode*
ANode::nextScopeNonOverlapping() const
{
  const ACodeNode* x = dynamic_cast<const ACodeNode*>(this);
  if (!x) { return NULL; }
  
  ACodeNode* z = NextScope();
  for (; z && x->containsLine(z->begLine()); z = z->NextScope()) { }
  return z;
}


//***************************************************************************
// ANode: Paths and Merging
//***************************************************************************

int 
ANode::Distance(ANode* anc, ANode* desc)
{
  int distance = 0;
  for (ANode* x = desc; x != NULL; x = x->Parent()) {
    if (x == anc) {
      return distance;
    }
    ++distance;
  }

  // If we arrive here, there was no path between 'anc' and 'desc'
  return -1;
}


bool 
ANode::ArePathsOverlapping(ANode* lca, ANode* desc1, 
			       ANode* desc2)
{
  // Ensure that d1 is on the longest path
  ANode* d1 = desc1, *d2 = desc2;
  int dist1 = Distance(lca, d1);
  int dist2 = Distance(lca, d2);
  if (dist2 > dist1) {
    ANode* t = d1;
    d1 = d2;
    d2 = t;
  } 
  
  // Iterate over the longest path (d1 -> lca) searching for d2.  Stop
  // when x is NULL or lca.
  for (ANode* x = d1; (x && x != lca); x = x->Parent()) {
    if (x == d2) { 
      return true;
    }
  }
  
  // If we arrive here, we did not encounter d2.  Divergent.
  return false;
}


bool
ANode::MergePaths(ANode* lca, ANode* toDesc, ANode* fromDesc)
{
  bool merged = false;
  // Should we verify that lca is really the lca?
  
  // Collect nodes along paths between 'lca' and 'toDesc', 'fromDesc'.
  // The descendent nodes will be at the end of the list.
  ANodeList toPath, fromPath;
  for (ANode* x = toDesc; x != lca; x = x->Parent()) {
    toPath.push_front(x);
  }
  for (ANode* x = fromDesc; x != lca; x = x->Parent()) {
    fromPath.push_front(x);
  }
  DIAG_Assert(toPath.size() > 0 && fromPath.size() > 0, "");
  
  // We merge from the deepest _common_ level of nesting out to lca
  // (shallowest).  
  ANodeList::reverse_iterator toPathIt = toPath.rbegin();
  ANodeList::reverse_iterator fromPathIt = fromPath.rbegin();
  
  // Determine which path is longer (of course, they can be equal) so
  // we can properly set up the iterators
  ANodeList::reverse_iterator* lPathIt = &toPathIt; // long path iter
  int difference = toPath.size() - fromPath.size();
  if (difference < 0) {
    lPathIt = &fromPathIt;
    difference = -difference;
  }
  
  // Align the iterators
  for (int i = 0; i < difference; ++i) { (*lPathIt)++; }
  
  // Now merge the nodes in 'fromPath' into 'toPath'
  for ( ; fromPathIt != fromPath.rend(); ++fromPathIt, ++toPathIt) {
    ANode* from = (*fromPathIt);
    ANode* to = (*toPathIt);
    if (IsMergable(to, from)) {
      merged |= Merge(to, from);
    }
  }
  
  return merged;
}


bool 
ANode::Merge(ANode* toNode, ANode* fromNode)
{
  // Do we really want this?
  //if (!IsMergable(toNode, fromNode)) {
  //  return false;
  //}
  
  // Perform the merge
  // 1. Move all children of 'fromNode' into 'toNode'
  for (ANodeChildIterator it(fromNode); it.Current(); /* */) {
    ANode* child = dynamic_cast<ANode*>(it.Current());
    it++; // advance iterator -- it is pointing at 'child'
    
    child->Unlink();
    child->Link(toNode);
  }
  
  // 2. If merging ACodeNodes, update line ranges
  ACodeNode* toCI = dynamic_cast<ACodeNode*>(toNode);
  ACodeNode* fromCI = dynamic_cast<ACodeNode*>(fromNode);
  DIAG_Assert(Logic::equiv(toCI, fromCI), "Invariant broken!");
  if (toCI && fromCI) {
    SrcFile::ln begLn = std::min(toCI->begLine(), fromCI->begLine());
    SrcFile::ln endLn = std::max(toCI->endLine(), fromCI->endLine());
    toCI->SetLineRange(begLn, endLn);
    toCI->vmaSet().merge(fromCI->vmaSet()); // merge VMAs
  }
  
  // 3. Unlink 'fromNode' from the tree and delete it
  fromNode->Unlink();
  delete fromNode;
  
  return true;
}


bool 
ANode::IsMergable(ANode* toNode, ANode* fromNode)
{
  ANode::ANodeTy toTy = toNode->Type(), fromTy = fromNode->Type();

  // 1. For now, merges are only defined on TyLOOPs and TyGROUPs
  bool goodTy = (toTy == TyLOOP || toTy == TyGROUP);

  // 2. Check bounds
  bool goodBnds = true;
  if (goodTy) {
    ACodeNode* toCI = dynamic_cast<ACodeNode*>(toNode);
    ACodeNode* fromCI = dynamic_cast<ACodeNode*>(fromNode);
    goodBnds = 
      Logic::equiv(SrcFile::isValid(toCI->begLine(), toCI->endLine()),
		   SrcFile::isValid(fromCI->begLine(), fromCI->endLine()));
  }

  return ((toTy == fromTy) && goodTy && goodBnds);
}


//***************************************************************************
// Metric Data
//***************************************************************************

void
ANode::accumulateMetrics(uint mBegId, uint mEndId, double* valVec)
{
  ANodeChildIterator it(this); 
  for (; it.Current(); it++) { 
    it.CurScope()->accumulateMetrics(mBegId, mEndId, valVec);
  }

  it.Reset(); 
  if (it.Current() != NULL) { // it's not a leaf 
    // initialize helper data
    for (uint i = mBegId; i <= mEndId; ++i) {
      valVec[i] = 0.0; 
    }

    for (; it.Current(); it++) { 
      for (uint i = mBegId; i <= mEndId; ++i) {
	valVec[i] += it.CurScope()->PerfData(i);
      }
    } 
    
    for (uint i = mBegId; i <= mEndId; ++i) {
      SetPerfData(i, valVec[i]);
    }
  }
}


void 
ANode::pruneByMetrics()
{
  std::vector<ANode*> toBeRemoved;
  
  for (ANodeChildIterator it(this, NULL); it.Current(); ++it) {
    ANode* si = it.CurScope();
    if (si->HasPerfData()) {
      si->pruneByMetrics();
    }
    else {
      toBeRemoved.push_back(si);
    }
  }
  
  for (uint i = 0; i < toBeRemoved.size(); i++) {
    ANode* si = toBeRemoved[i];
    si->Unlink();
    delete si;
  }
}


//***************************************************************************
// ANode, etc: Names, Name Maps, and Retrieval by Name
//***************************************************************************

void 
Pgm::AddToGroupMap(Group* grp) 
{
  std::pair<GroupMap::iterator, bool> ret = 
    groupMap->insert(std::make_pair(grp->name(), grp));
  DIAG_Assert(ret.second, "Duplicate!");
}


void 
Pgm::AddToLoadModMap(LM* lm)
{
  string lmName = RealPath(lm->name().c_str());
  std::pair<LMMap::iterator, bool> ret = 
    lmMap->insert(std::make_pair(lmName, lm));
  DIAG_Assert(ret.second, "Duplicate!");
}


void 
LM::AddToFileMap(File* f)
{
  string fName = RealPath(f->name().c_str());
  DIAG_DevMsg(2, "LM: mapping file name '" << fName << "' to File* " << f);
  std::pair<FileMap::iterator, bool> ret = 
    fileMap->insert(std::make_pair(fName, f));
  DIAG_Assert(ret.second, "Duplicate instance: " << f->name() << "\n" << toStringXML());
}


void 
File::AddToProcMap(Proc* p) 
{
  DIAG_DevMsg(2, "File (" << this << "): mapping proc name '" << p->name()
	      << "' to Proc* " << p);
  procMap->insert(make_pair(p->name(), p)); // multimap
} 


void 
Proc::AddToStmtMap(Stmt* stmt)
{
  (*stmtMap)[stmt->begLine()] = stmt;
}


ACodeNode*
LM::findByVMA(VMA vma)
{
  if (!procMap) {
    buildMap(procMap, ANode::TyPROC);
  }
  if (!stmtMap) {
    buildMap(stmtMap, ANode::TySTMT);
  }
  
  // Attempt to find StatementRange and then Proc
  ACodeNode* found = findStmt(vma);
  if (!found) {
    found = findProc(vma);
  }
  return found;
}


template<typename T>
void 
LM::buildMap(VMAIntervalMap<T>*& m, ANode::ANodeTy ty) const
{
  if (!m) {
    m = new VMAIntervalMap<T>;
  }
  
  ANodeIterator it(this, &ANodeTyFilter[ty]);
  for (; it.Current(); ++it) {
    T x = dynamic_cast<T>(it.Current());

    const VMAIntervalSet& vmaset = x->vmaSet();
    for (VMAIntervalSet::const_iterator it = vmaset.begin();
	 it != vmaset.end(); ++it) {
      const VMAInterval& vmaint = *it;
      DIAG_MsgIf(0, vmaint.toString());
      m->insert(std::make_pair(vmaint, x));
    }
  }
}


template<typename T>
bool 
LM::verifyMap(VMAIntervalMap<T>* m, const char* map_nm)
{
  if (!m) { return true; }

  for (typename VMAIntervalMap<T>::const_iterator it = m->begin(); 
       it != m->end(); ) {
    const VMAInterval& x = it->first;
    ++it;
    if (it != m->end()) {
      const VMAInterval& y = it->first;
      DIAG_Assert(!x.overlaps(y), "LM::verifyMap: found overlapping elements within " << map_nm << ": " << x.toString() << " and " << y.toString());
    }
  }
  
  return true;
}


bool 
LM::verifyStmtMap() const
{ 
  if (!stmtMap) {
    VMAToStmtRangeMap* mp;
    buildMap(mp, ANode::TySTMT);
    verifyMap(mp, "stmtMap"); 
    delete mp;
    return true; 
 }
  else {
    return verifyMap(stmtMap, "stmtMap"); 
  }
}


Proc*
File::FindProc(const char* nm, const char* lnm) const
{
  Proc* found = NULL;

  ProcMap::const_iterator it = procMap->find(nm);
  if (it != procMap->end()) {
    if (lnm && lnm != '\0') {
      for ( ; (it != procMap->end() && it->first == nm); ++it) {
	Proc* p = it->second;
	if (p->LinkName() == lnm) {
	  return p; // found = p
	}
      }
    }
    else {
      found = it->second;
    }
  }
  
  return found;
}


//***************************************************************************
// ACodeNode, etc: CodeName methods
//***************************************************************************

string
ACodeNode::LineRange() const
{
  string self = "b=" + StrUtil::toStr(m_begLn) 
    + " e=" + StrUtil::toStr(m_endLn);
  return self;
}


string
ACodeNode::codeName() const
{
  string nm = StrUtil::toStr(m_begLn);
  if (m_begLn != m_endLn) {
    nm += "-" + StrUtil::toStr(m_endLn);
  }
  return nm;
} 


string
ACodeNode::codeName_LM_F() const 
{
  File* fileStrct = AncFile();
  LM* lmStrct = (fileStrct) ? fileStrct->AncLM() : NULL;
  string nm;
  if (lmStrct && fileStrct) {
    nm = "[" + lmStrct->name() + "]<" + fileStrct->name() + ">";
  }
  return nm;
}


string
Group::codeName() const 
{
  string self = ANodeTyToName(Type()) + " " + ACodeNode::codeName();
  return self;
}


string
File::codeName() const 
{
  string nm;
  LM* lmStrct = AncLM();
  if (lmStrct) {
    nm = "[" + lmStrct->name() + "]";
  }
  nm += name();
  return nm;
}


string
Proc::codeName() const 
{
  string nm = codeName_LM_F();
  nm += name();
  return nm;
}


string
Alien::codeName() const 
{
  string nm = "<" + m_filenm + ">[" + m_name + "]:";
  nm += StrUtil::toStr(m_begLn);
  return nm;
} 


string
Loop::codeName() const 
{
  string nm = codeName_LM_F();
  nm += ACodeNode::codeName();
  return nm;
} 


string
Stmt::codeName() const 
{
  string nm = codeName_LM_F();
  nm += ACodeNode::codeName();
  return nm;
}


string
Ref::codeName() const 
{
  return m_name + " " + ACodeNode::codeName();
}


//***************************************************************************
// ANode, etc: Output and Debugging support 
//***************************************************************************

string 
ANode::Types() const
{
  string types;
  if (dynamic_cast<const ANode*>(this)) {
    types += "ANode ";
  } 
  if (dynamic_cast<const ACodeNode*>(this)) {
    types += "ACodeNode ";
  } 
  if (dynamic_cast<const Pgm*>(this)) {
    types += "Pgm ";
  } 
  if (dynamic_cast<const Group*>(this)) {
    types += "Group ";
  } 
  if (dynamic_cast<const LM*>(this)) {
    types += "LM ";
  } 
  if (dynamic_cast<const File*>(this)) {
    types += "File ";
  } 
  if (dynamic_cast<const Proc*>(this)) {
    types += "Proc ";
  } 
  if (dynamic_cast<const Alien*>(this)) {
    types += "Alien ";
  } 
  if (dynamic_cast<const Loop*>(this)) {
    types += "Loop ";
  } 
  if (dynamic_cast<const Stmt*>(this)) {
    types += "Stmt ";
  } 
  if (dynamic_cast<const Ref*>(this)) {
    types += "Ref ";
  }
  return types;
}


string 
ANode::toString(int dmpFlag, const char* pre) const
{ 
  std::ostringstream os;
  dump(os, dmpFlag, pre);
  return os.str();
}


string
ANode::toString_id(int dmpFlag) const
{ 
  string str = "<" + ANodeTyToName(Type()) + " uid=" 
    + StrUtil::toStr(id()) + ">";
  return str;
}


string
ANode::toString_me(int dmpFlag, const char* prefix) const
{ 
  std::ostringstream os;
  dumpme(os, dmpFlag, prefix);
  return os.str();
}


std::ostream&
ANode::dump(ostream& os, int dmpFlag, const char* pre) const 
{
  string prefix = string(pre) + "  ";

  dumpme(os, dmpFlag, pre);

  for (uint i = 0; i < NumPerfData(); i++) {
    os << i << " = " ;
    if (HasPerfData(i)) {
      os << PerfData(i);
    } 
    else {
      os << "UNDEF";
    }
    os << "; ";
  }
  
  for (ANodeChildIterator it(this); it.Current(); it++) {
    it.CurScope()->dump(os, dmpFlag, prefix.c_str());
  } 
  return os;
}


void
ANode::ddump() const
{ 
  //XML_DumpLineSorted(std::cerr, 0, "");
  dump(std::cerr, 0, "");
}


ostream&
ANode::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  os << prefix << toString_id(dmpFlag) << endl;
  return os;
}


ostream&
ACodeNode::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  os << prefix << toString_id(dmpFlag) << " " 
     << LineRange() << " " << m_vmaSet.toString();
  return os;
}


ostream&
Pgm::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  os << prefix << toString_id(dmpFlag) << " n=" << m_name;
  return os;
}


ostream& 
Group::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  os << prefix << toString_id(dmpFlag) << " n=" << m_name;
  return os;
}


ostream& 
LM::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  os << prefix << toString_id(dmpFlag) << " n=" << m_name;
  return os;
}


ostream&
File::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  ACodeNode::dumpme(os, dmpFlag, prefix) << " n=" <<  m_name;
  return os;
}


ostream& 
Proc::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  ACodeNode::dumpme(os, dmpFlag, prefix) << " n=" << m_name;
  return os;
}


ostream& 
Alien::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  ACodeNode::dumpme(os, dmpFlag, prefix);
  return os;
}


ostream& 
Loop::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  ACodeNode::dumpme(os, dmpFlag, prefix);
  return os;
}


ostream&
Stmt::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  ACodeNode::dumpme(os, dmpFlag, prefix);
  return os;
}


ostream&
Ref::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  ACodeNode::dumpme(os, dmpFlag, prefix);
  os << " pos:"  << begPos << "-" << endPos;
  return os;
}


void
LM::dumpmaps() const 
{
  ostream& os = std::cerr;
  int dmpFlag = 0;
  const char* pre = "";
  
  os << "Procedure map\n";
  for (VMAToProcMap::const_iterator it = procMap->begin(); 
       it != procMap->end(); ++it) {
    it->first.dump(os);
    os << " --> " << hex << "Ox" << it->second << dec << endl;
  }
  
  os << endl;

  os << "Statement map\n";
  for (VMAToStmtRangeMap::const_iterator it = stmtMap->begin(); 
       it != stmtMap->end(); ++it) {
    it->first.dump(os);
    os << " --> " << hex << "Ox" << it->second << dec << endl;
  }
}


//***************************************************************************
// ANode, etc: XML output support
//***************************************************************************

static const string XMLelements[ANode::TyNUMBER] = {
  "PGM", "G", "LM", "F", "P", "A", "L", "S", "REF", "ANY"
};

const string&
ANode::ANodeTyToXMLelement(ANodeTy tp)
{
   return XMLelements[tp];
}


string 
ANode::toStringXML(int dmpFlag, const char* pre) const
{ 
  std::ostringstream os;
  XML_DumpLineSorted(os, dmpFlag, pre);
  return os.str();
}


string 
ANode::toXML(int dmpFlag) const
{
  string self = ANodeTyToXMLelement(Type()) + " id" + MakeAttrNum(id());
  return self;
}


string
ACodeNode::toXML(int dmpFlag) const
{ 
  string self = ANode::toXML(dmpFlag) 
    + " " + XMLLineRange(dmpFlag)
    + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string
ACodeNode::XMLLineRange(int dmpFlag) const
{
  string self = "b" + MakeAttrNum(m_begLn) + " e" + MakeAttrNum(m_endLn);
  return self;
}


string
ACodeNode::XMLVMAIntervals(int dmpFlag) const
{
  string self = "vma" + MakeAttrStr(m_vmaSet.toString(), xml::ESC_FALSE);
  return self;
}


string
Pgm::toXML(int dmpFlag) const
{
  string self = ANode::toXML(dmpFlag)
    + " version=\"4.5\""
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
Group::toXML(int dmpFlag) const
{
  string self = ANode::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
LM::toXML(int dmpFlag) const
{
  string self = ANode::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag))
    + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string
File::toXML(int dmpFlag) const
{
  string self = ANode::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
Proc::toXML(int dmpFlag) const
{ 
  string self = ANode::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  if (m_name != m_linkname) { // if different, print both
    self = self + " ln" + MakeAttrStr(m_linkname, AddXMLEscapeChars(dmpFlag));
  }
  self = self + " " + XMLLineRange(dmpFlag) + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string 
Alien::toXML(int dmpFlag) const
{ 
  string self = ANode::toXML(dmpFlag) 
    + " f" + MakeAttrStr(m_filenm, AddXMLEscapeChars(dmpFlag))
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  self = self + " " + XMLLineRange(dmpFlag) + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string 
Loop::toXML(int dmpFlag) const
{
  string self = ACodeNode::toXML(dmpFlag);
  return self;
}


string
Stmt::toXML(int dmpFlag) const
{
  string self = ACodeNode::toXML(dmpFlag);
  return self;
}


string
Ref::toXML(int dmpFlag) const
{ 
  string self = ACodeNode::toXML(dmpFlag) 
    + " b" + MakeAttrNum(begPos) + " e" + MakeAttrNum(endPos);
  return self;
}


bool 
ANode::XML_DumpSelfBefore(ostream& os, int dmpFlag, 
			      const char* prefix) const
{
  bool attemptToDumpMetrics = true;
  if ((dmpFlag & Tree::DUMP_LEAF_METRICS) && Type() != TySTMT) {
    attemptToDumpMetrics = false;
  }
  
  bool dumpMetrics = false;
  if (attemptToDumpMetrics) {
    for (uint i = 0; i < NumPerfData(); i++) {
      if (HasPerfData(i)) {
	dumpMetrics = true;
	break;
      }
    }
  }

  os << prefix << "<" << toXML(dmpFlag);
  if (dumpMetrics) {
    // by definition this element is not empty
    os << ">";
    for (uint i = 0; i < NumPerfData(); i++) {
      if (HasPerfData(i)) {
	if (!(dmpFlag & Tree::COMPRESSED_OUTPUT)) { os << endl; }
	os << prefix << "  <M n=\"" << i << "\" v=\"" << PerfData(i) << "\"/>";
      }
    }
  }
  else {
    if (dmpFlag & Tree::XML_EMPTY_TAG) {
      os << "/>";
    } 
    else { 
      os << ">";
    }
  }
  if (!(dmpFlag & Tree::COMPRESSED_OUTPUT)) { os << endl; }

  return (dumpMetrics);
}


void
ANode::XML_DumpSelfAfter(ostream& os, int dmpFlag, const char* prefix) const
{
  if (!(dmpFlag & Tree::XML_EMPTY_TAG)) {
    os << prefix << "</" << ANodeTyToXMLelement(Type()) << ">";
    if (!(dmpFlag & Tree::COMPRESSED_OUTPUT)) { os << endl; }
  } 
}


void
ANode::XML_dump(ostream& os, int dmpFlag, const char* pre) const 
{
  string indent = "  ";
  if (dmpFlag & Tree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  
  if (IsLeaf()) { 
    dmpFlag |= Tree::XML_EMPTY_TAG;
  }

  bool dumpedMetrics = XML_DumpSelfBefore(os, dmpFlag, pre);
  if (dumpedMetrics) {
    dmpFlag &= ~Tree::XML_EMPTY_TAG; // clear empty flag
  }
  string prefix = pre + indent;
  for (ANodeChildIterator it(this); it.Current(); it++) {
    it.CurScope()->XML_dump(os, dmpFlag, prefix.c_str());
  }
  XML_DumpSelfAfter(os, dmpFlag, pre);
}


void
ANode::XML_DumpLineSorted(ostream& os, int dmpFlag, const char* pre) const 
{
  string indent = "  ";
  if (dmpFlag & Tree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }
  if (IsLeaf()) {
    dmpFlag |= Tree::XML_EMPTY_TAG;
  }
  
  bool dumpedMetrics = XML_DumpSelfBefore(os, dmpFlag, pre);
  if (dumpedMetrics) {
    dmpFlag &= ~Tree::XML_EMPTY_TAG; // clear empty flag
  }
  string prefix = pre + indent;
  for (ANodeLineSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->XML_DumpLineSorted(os, dmpFlag, prefix.c_str());
  }
  XML_DumpSelfAfter(os, dmpFlag, pre);
}


void
ANode::xml_ddump() const
{
  XML_DumpLineSorted();
}


void
Pgm::XML_DumpLineSorted(ostream& os, int dmpFlag, const char* pre) const
{
  // N.B.: Typically LM are children
  string indent = "  ";
  if (dmpFlag & Tree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }

  ANode::XML_DumpSelfBefore(os, dmpFlag, pre);
  string prefix = pre + indent;
  for (ANodeNameSortedChildIterator it(this); it.Current(); it++) { 
    ANode* scope = it.Current();
    scope->XML_DumpLineSorted(os, dmpFlag, prefix.c_str());
  }
  ANode::XML_DumpSelfAfter(os, dmpFlag, pre);
}


void
LM::XML_DumpLineSorted(ostream& os, int dmpFlag, const char* pre) const
{
  // N.B.: Typically Files are children
  string indent = "  ";
  if (dmpFlag & Tree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }

  ANode::XML_DumpSelfBefore(os, dmpFlag, pre);
  string prefix = pre + indent;
  for (ANodeNameSortedChildIterator it(this); it.Current(); it++) {
    ANode* scope = it.Current();
    scope->XML_DumpLineSorted(os, dmpFlag, prefix.c_str());
  }
  ANode::XML_DumpSelfAfter(os, dmpFlag, pre);
}


//***************************************************************************
// ANode, etc: 
//***************************************************************************

void 
ANode::CSV_DumpSelf(const Pgm &root, ostream& os) const
{ 
  char temp[32];
  for (uint i = 0; i < NumPerfData(); i++) {
    double val = (HasPerfData(i) ? PerfData(i) : 0);
    os << "," << val;
#if 0
    // FIXME: tallent: Conversioon from static perf-table to MetricDescMgr
    const PerfMetric& metric = IndexToPerfDataInfo(i);
    bool percent = metric.Percent();
#else
    bool percent = true;
#endif

    if (percent) {
      double percVal = val / root.PerfData(i) * 100;
      sprintf(temp, "%5.2lf", percVal);
      os << "," << temp;
    }
  }
  os << endl;
}


void
ANode::CSV_dump(const Pgm &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << name() << ",,,,";
  CSV_DumpSelf(root, os);
  for (ANodeNameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->CSV_dump(root, os);
  }
}


void
File::CSV_dump(const Pgm &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << BaseName() << ",," << m_begLn << "," << m_endLn << ",";
  CSV_DumpSelf(root, os);
  for (ANodeNameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->CSV_dump(root, os, BaseName().c_str());
  }
}


void
Proc::CSV_dump(const Pgm &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << file_name << "," << name() << "," << m_begLn << "," << m_endLn 
     << ",0";
  CSV_DumpSelf(root, os);
  for (ANodeLineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CSV_dump(root, os, file_name, name().c_str(), 1);
  } 
}


void
Alien::CSV_dump(const Pgm &root, ostream& os, 
		     const char* file_name, const char* proc_name,
		     int lLevel) const 
{
  DIAG_Die(DIAG_Unimplemented);
}


void
ACodeNode::CSV_dump(const Pgm &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  ANodeTy myANodeTy = this->Type();
  // do not display info for single lines
  if (myANodeTy == TySTMT)
    return;
  // print file name, routine name, start and end line, loop level
  os << (file_name ? file_name : name().c_str()) << "," 
     << (proc_name ? proc_name : "") << "," 
     << m_begLn << "," << m_endLn << ",";
  if (lLevel)
    os << lLevel;
  CSV_DumpSelf(root, os);
  for (ANodeLineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CSV_dump(root, os, file_name, proc_name, lLevel+1);
  } 
}


void
Pgm::CSV_TreeDump(ostream& os) const
{
  ANode::CSV_dump(*this, os);
}


//***************************************************************************
// ACodeNode specific methods 
//***************************************************************************

void 
ACodeNode::SetLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate) 
{
  checkLineRange(begLn, endLn);
  
  m_begLn = begLn;
  m_endLn = endLn;
  
  RelocateIf();

  // never propagate changes outside an Alien
  if (propagate && begLn != ln_NULL 
      && ACodeNodeParent() && Type() != ANode::TyALIEN) {
    ACodeNodeParent()->ExpandLineRange(m_begLn, m_endLn);
  }
}


void 
ACodeNode::ExpandLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate)
{
  checkLineRange(begLn, endLn);

  if (begLn == ln_NULL) {
    DIAG_Assert(m_begLn == ln_NULL, "");
    // simply relocate at beginning of sibling list 
    RelocateIf();
  } 
  else {
    bool changed = false;
    if (m_begLn == ln_NULL) {
      m_begLn = begLn;
      m_endLn = endLn;
      changed = true;
    } 
    else {
      if (begLn < m_begLn) { m_begLn = begLn; changed = true; }
      if (endLn > m_endLn) { m_endLn = endLn; changed = true; }
    }

    if (changed) {
      // never propagate changes outside an Alien
      RelocateIf();
      if (propagate && ACodeNodeParent() && Type() != ANode::TyALIEN) {
        ACodeNodeParent()->ExpandLineRange(m_begLn, m_endLn);
      }
    }
  }
}


void
ACodeNode::Relocate() 
{
  ACodeNode* prev = PrevScope();
  ACodeNode* next = NextScope();

  // NOTE: Technically should check for ln_NULL
  if ((!prev || (prev->begLine() <= m_begLn)) 
      && (!next || (m_begLn <= next->begLine()))) {
    return;
  } 
  
  // INVARIANT: The parent scope contains at least two children
  DIAG_Assert(parent->ChildCount() >= 2, "");

  ANode* parent = Parent();
  Unlink();

  //if (parent->FirstChild() == NULL) {
  //  Link(parent);
  //}
  if (m_begLn == ln_NULL) {
    // insert as first child
    LinkBefore(parent->FirstChild());
  } 
  else {
    // insert after sibling with sibling->begLine() <= begLine() 
    // or iff that does not exist insert as first in sibling list
    ACodeNode* sibling = NULL;
    for (sibling = parent->LastEnclScope(); sibling;
	 sibling = sibling->PrevScope()) {
      if (sibling->begLine() <= m_begLn) {
	break;
      }
    } 
    if (sibling != NULL) {
      LinkAfter(sibling);
    } 
    else {
      LinkBefore(parent->FirstChild());
    } 
  }
}


bool
ACodeNode::containsLine(SrcFile::ln ln, int beg_epsilon, int end_epsilon) const
{
  // We assume it makes no sense to compare against ln_NULL
  if (m_begLn != ln_NULL) {
    if (containsLine(ln)) {
      return true;
    }
    else if (beg_epsilon != 0 || end_epsilon != 0) {
      // INVARIANT: 'ln' is strictly outside the range of this context
      int beg_delta = begLine() - ln; // > 0 if line is before beg
      int end_delta = ln - endLine(); // > 0 if end is before line
      return ((beg_delta > 0 && beg_delta <= beg_epsilon)
	      || (end_delta > 0 && end_delta <= end_epsilon));
    }
  }
  return false;
}


ACodeNode* 
ACodeNode::ACodeNodeWithLine(SrcFile::ln ln) const
{
  DIAG_Assert(ln != ln_NULL, "ACodeNode::ACodeNodeWithLine: invalid line");
  ACodeNode* ci;
  // ln > m_endLn means there is no child that contains ln
  if (ln <= m_endLn) {
    for (ANodeChildIterator it(this); it.Current(); it++) {
      ci = dynamic_cast<ACodeNode*>(it.Current());
      DIAG_Assert(ci, "");
      if  (ci->containsLine(ln)) {
	if (ci->Type() == TySTMT) {  
	  return ci; // never look inside LINE_SCOPEs 
	} 
	
	// desired line might be in inner scope; however, it might be
	// elsewhere because optimization left procedure with 
	// non-contiguous line ranges in scopes at various levels.
	ACodeNode* inner = ci->ACodeNodeWithLine(ln);
	if (inner) return inner;
      } 
    }
  }
  if (ci->Type() == TyPROC) return (ACodeNode*) this;
  else return 0;
}


int 
ACodeNodeLineComp(const ACodeNode* x, const ACodeNode* y)
{
  if (x->begLine() == y->begLine()) {
    bool endLinesEqual = (x->endLine() == y->endLine());
    if (endLinesEqual) {
      // We have two ACodeNode's with identical line intervals...
      
      // Use lexicographic comparison for procedures
      if (x->Type() == ANode::TyPROC && y->Type() == ANode::TyPROC) {
	Proc *px = (Proc*)x, *py = (Proc*)y;
	int cmp1 = px->name().compare(py->name());
	if (cmp1 != 0) { return cmp1; }
	int cmp2 = px->LinkName().compare(py->LinkName());
	if (cmp2 != 0) { return cmp2; }
      }
      
      // Use VMAInterval sets otherwise.
      bool x_lt_y = (x->vmaSet() < y->vmaSet());
      bool y_lt_x = (y->vmaSet() < x->vmaSet());
      bool vmaSetsEqual = (!x_lt_y && !y_lt_x);

      if (vmaSetsEqual) {
	// Try ranking a leaf node before a non-leaf node
	if ( !(x->IsLeaf() && y->IsLeaf())) {
	  if      (x->IsLeaf()) { return -1; } // x < y
	  else if (y->IsLeaf()) { return  1; } // x > y
	}
	
	// Give up!
	return 0;
      }
      else if (x_lt_y) { return -1; }
      else if (y_lt_x) { return  1; }
      else {
	DIAG_Die(DIAG_Unimplemented);
      }
    }
    else {
      return SimpleLineCmp(x->endLine(), y->endLine());
    }
  }
  else {
    return SimpleLineCmp(x->begLine(), y->begLine());
  }
}


//***************************************************************************
// Ref specific methods 
//***************************************************************************

void
Ref::RelocateRef() 
{
  Ref* prev = dynamic_cast<Ref*>(PrevScope());
  Ref* next = dynamic_cast<Ref*>(NextScope());
  DIAG_Assert((PrevScope() == prev) && (NextScope() == next), "");
  if (((!prev) || (prev->endPos <= begPos)) && 
      ((!next) || (next->begPos >= endPos))) {
    return;
  } 
  ANode* parent = Parent();
  Unlink();
  if (parent->FirstChild() == NULL) {
    Link(parent);
  } 
  else {
    // insert after sibling with sibling->endPos < begPos 
    // or iff that does not exist insert as first in sibling list
    ACodeNode* sibling;
    for (sibling = parent->LastEnclScope();
	 sibling;
	 sibling = sibling->PrevScope()) {
      Ref *ref = dynamic_cast<Ref*>(sibling);
      DIAG_Assert(ref == sibling, "");
      if (ref->endPos < begPos)  
	break;
      } 
    if (sibling != NULL) {
      Ref *nxt = dynamic_cast<Ref*>(sibling->NextScope());
      DIAG_Assert((nxt == NULL) || (nxt->begPos > endPos), "");
      LinkAfter(sibling);
    } 
    else {
      LinkBefore(parent->FirstChild());
    } 
  }
}


} // namespace Struct
} // namespace Prof

//***************************************************************************

// - if x < y; 0 if x == y; + otherwise
static int 
SimpleLineCmp(SrcFile::ln x, SrcFile::ln y)
{
  // We would typically wish to use the following for this simple
  // comparison, but it fails if the the differences are greater than
  // an 'int'
  // return (x - y)

  if (x < y)       { return -1; }
  else if (x == y) { return 0; }
  else             { return 1; }
}


// Returns a flag indicating whether XML escape characters should be used
// not modify 'str'
static int 
AddXMLEscapeChars(int dmpFlag)
{
  if (dmpFlag & Prof::Struct::Tree::XML_NO_ESC_CHARS) {
    return xml::ESC_FALSE;
  }
  else {
    return xml::ESC_TRUE;
  }
}


//***************************************************************************

#if 0
void 
ANodeTester(int argc, const char** argv) 
{
  Pgm *root = new Pgm("ANodeTester");
  LM *lmScope = new LM("load module", root);
  File *file;
  Proc *proc;
  Loop *loop;
  Group *group;
  Stmt *stmtRange;
  
  FILE *srcFile = fopen("file.c", "r");
  bool known = (srcFile != NULL);
  if (srcFile) {
    fclose(srcFile);
  }  
  
  file = new File("file.c", known, lmScope);
  proc = new Proc("proc", file);
  loop = new Loop(proc->ACodeNodeWithLine(2), 2, 10);
  loop = new Loop(proc->ACodeNodeWithLine(5), 5, 9);  
  loop = new Loop(proc->ACodeNodeWithLine(12), 12, 25);

  stmtRange = new Stmt(proc->ACodeNodeWithLine(4), 4, 4);
  stmtRange = new Stmt(proc->ACodeNodeWithLine(3), 3, 3);
  stmtRange = new Stmt(proc->ACodeNodeWithLine(5), 5, 5);
  stmtRange = new Stmt(proc->ACodeNodeWithLine(13), 13, 13);

  group = new Group("g1", proc->ACodeNodeWithLine(14), 14, 18);
  stmtRange = new Stmt(proc->ACodeNodeWithLine(15), 15, 15);
  
  std::cout << "root->dump()" << endl;
  root->dump();
  std::cout << "------------------------------------------" << endl << endl;
  
  std::cout << "Type Casts" << endl;
  {
    ANode* sinfo[10];
    int sn = 0;
    sinfo[sn++] = root;
    sinfo[sn++] = file;
    sinfo[sn++] = proc;
    sinfo[sn++] = loop;
    sinfo[sn++] = group;
    sinfo[sn++] = stmtRange;
    
    for (int i = 0; i < sn; i++) {
      std::cout << sinfo[i]->ToString() << "::\t" ;
      std::cout << sinfo[i]->Types() << endl;
      std::cout << endl;
    } 
    std::cout << endl;
  } 
  std::cout << "------------------------------------------" << endl << endl;
  
  std::cout << "Iterators " << endl;
  { 
    File *file_c = lmScope->FindFile("file.c");
    DIAG_Assert(file_c, "");
    Proc *proc = file_c->FindProc("proc");
    DIAG_Assert(proc, "");
    ACodeNode* loop3 = proc->ACodeNodeWithLine(12);
    DIAG_Assert(loop3, "");
    
    {
      std::cerr << "*** everything under root " << endl;
      ANodeIterator it(root);
      for (; it.CurScope(); it++) {
	ACodeNode* ci = dynamic_cast<ACodeNode*>(it.CurScope());
	std::cout << it.CurScope()->ToString();
	if (ci != NULL) { 
	  std::cout << " Name=" << ci->name();
	} 
	std::cout << endl;
      }  
    }

    { 
      std::cerr << "*** file.c in line order" << endl;  
      LineSortedIterator it(file_c, NULL, false);
      it.DumpAndReset();
      std::cerr << "***" << endl << endl;
    }
    
    { 
      std::cerr << "*** loop3's children in line order" << endl;  
      ANodeLineSortedChildIterator it(loop3);
      it.DumpAndReset();
      std::cerr << "***" << endl << endl;
    }
  }
  std::cout << "------------------------------------------" << endl << endl;

  delete root;
} 
#endif

#ifdef NEVER 
The following is a 'correct' input (file.c) for the statically build tree 
from ANodeTester()
     1  proc p() 
     2    do i = 1,10
     3      line(3) =     Line(3+10) * LINE(3+356)
     4      line(4) = 2390
     5      do j = 5, 9
     6    
     7   
     8   
     9      enddo 
    10    enddo
    11    
    12    do k = 12, 25
    13       atheline(13)
    14  
    15       line(15)        C comment comment comment long lon wrap around ...
    16      
    17
    18
    19
    20
    21
    22
    23
    24
    25    enddo 
    26
    27
    28
#endif 
