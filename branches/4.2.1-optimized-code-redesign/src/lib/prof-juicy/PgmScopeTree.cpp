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

#include <iostream>
using std::ostream;
using std::endl;

#include <string>
using std::string;

#include <map> // STL
using std::map;

#ifdef NO_STD_CHEADERS
# include <stdio.h>
# include <limits.h>
#else
# include <cstdio> // for 'fopen'
# include <climits>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

#include "PgmScopeTree.hpp"
#include "PerfMetric.hpp"

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/VectorTmpl.hpp>
#include <lib/support/Files.hpp>
#include <lib/support/SrcFile.hpp>
#include <lib/support/PtrSetIterator.hpp>
#include <lib/support/realpath.h>

//*************************** Forward Declarations **************************

int SimpleLineCmp(suint x, suint y);
int AddXMLEscapeChars(int dmpFlag);

using namespace xml;

//***************************************************************************

//***************************************************************************
// template classes 
//***************************************************************************

class DoubleVector : public VectorTmpl<double> {
public: 
  DoubleVector() : VectorTmpl<double>(NaNVal, true) { }
};

class GroupScopeMap     : public map<string, GroupScope*> { };

class LoadModScopeMap   : public map<string, LoadModScope*> { };

// ProcScopeMap: This is is a multimap because procedure names are
// sometimes "generic", i.e. not qualified by types in the case of
// templates, resulting in duplicate names
class ProcScopeMap      : public multimap<string, ProcScope*> { };

class FileScopeMap      : public map<string, FileScope*> { };

class StmtRangeScopeMap : public map<suint, StmtRangeScope*> { };

//***************************************************************************
// PgmScopeTree
//***************************************************************************

PgmScopeTree::PgmScopeTree(const char* name, PgmScope* _root)
  : root(_root)
{
}


PgmScopeTree::~PgmScopeTree()
{
  delete root;
}


void 
PgmScopeTree::CollectCrossReferences() 
{ 
  root->NoteHeight();
  root->NoteDepth();
  root->CollectCrossReferences();
}


void 
PgmScopeTree::xml_dump(ostream& os, int dmpFlags) const
{
  if (root) {
    root->XML_DumpLineSorted(os, dmpFlags);
  }
}


void 
PgmScopeTree::dump(ostream& os, int dmpFlags) const
{
  xml_dump(os, dmpFlags);
}


void 
PgmScopeTree::ddump() const
{
  dump();
}

/*****************************************************************************/
// ScopeType `methods' (could completely replace with dynamic typing)
/*****************************************************************************/

const string ScopeInfo::ScopeNames[ScopeInfo::NUMBER_OF_SCOPES] = {
  "PGM", "GRP", "LM", "FIL", "PRC", "A", "LP", "SR", "REF", "ANY"
};


const string&
ScopeInfo::ScopeTypeToName(ScopeType tp) 
{
  return ScopeNames[tp];
}


ScopeInfo::ScopeType 
ScopeInfo::IntToScopeType(long i)
{
  DIAG_Assert((i >= 0) && (i < NUMBER_OF_SCOPES), "");
  return (ScopeType) i;
}


//***************************************************************************
// ScopeInfo, etc: constructors/destructors
//***************************************************************************

ScopeInfo::ScopeInfo(ScopeType t, ScopeInfo* mom) 
  : NonUniformDegreeTreeNode(mom), type(t)
{ 
  DIAG_Assert((type == PGM) || (Pgm() == NULL) || !Pgm()->IsFrozen(), "");
  static unsigned int uniqueId = 0;
  uid = uniqueId++;
  height = 0;
  depth = 0;
  perfData = new DoubleVector();
}


ScopeInfo& 
ScopeInfo::operator=(const ScopeInfo& x) 
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
ScopeInfo::CollectCrossReferences() 
{
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CollectCrossReferences();
  } 

  CodeInfo* self = dynamic_cast<CodeInfo*>(this);  
  if (self) {
    if (IsLeaf()) {
      self->first = self->last = self;
    } else {
      self->first = self->last = 0;

      int minline = INT_MAX;
      int maxline = -1;
      for (ScopeInfoChildIterator it(this); it.Current(); it++) {
	CodeInfo* child = dynamic_cast<CodeInfo*>(it.CurScope());
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
ScopeInfo::NoteHeight() 
{
  if (IsLeaf()) {
    height = 0;
  } else {
    height = 0;
    for (ScopeInfoChildIterator it(this); it.Current(); it++) {
      int childHeight = it.CurScope()->NoteHeight();
      height = MAX(height, childHeight + 1);
    } 
  }
  return height;
}


void 
ScopeInfo::NoteDepth() 
{
  ScopeInfo* p = Parent();
  if (p) {
    depth = p->depth + 1;
  } else {
    depth = 0;
  }
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->NoteDepth();
  } 
}


static bool
OkToDelete(ScopeInfo *si) 
{
  PgmScope *pgm = si->Pgm();
  if (pgm && pgm->Type() != ScopeInfo::PGM)
     return true;
  DIAG_Assert( pgm == NULL || pgm->Type() == ScopeInfo::PGM, "");
  return ((pgm == NULL) || !(pgm->IsFrozen()));
} 


ScopeInfo::~ScopeInfo() 
{
  DIAG_DevMsg(3, "~ScopeInfo::ScopeInfo: " << toString_id()
	      << " " << hex << this << dec);
  DIAG_Assert(OkToDelete(this), "ScopeInfo '" << this << " " << ToString() 
	      << "' is not ready for deletion!");
}


CodeInfo::CodeInfo(ScopeType t, ScopeInfo* mom, suint begLn, suint endLn,
		   VMA begVMA, VMA endVMA) 
  : ScopeInfo(t, mom), mbegLine(UNDEF_LINE), mendLine(UNDEF_LINE)
{ 
  SetLineRange(begLn, endLn);
  if (begVMA != 0 && endVMA != 0) {
    mvmaSet.insert(begVMA, endVMA);
  }
}


CodeInfo& 
CodeInfo::operator=(const CodeInfo& x) 
{
  // shallow copy
  if (&x != this) {
    mbegLine = x.mbegLine;
    mendLine = x.mendLine;
    first = last = NULL;
  }
  return *this;
}


CodeInfo::~CodeInfo() 
{
}


void 
CodeInfo::LinkAndSetLineRange(CodeInfo* parent)
{
  this->Link(parent);
  if (begLine() != UNDEF_LINE) {
    suint bLn = MIN(parent->begLine(), begLine());
    suint eLn = MAX(parent->endLine(), endLine());
    parent->SetLineRange(bLn, eLn);
  }
}


PgmScope::PgmScope(const char* nm) 
  : ScopeInfo(PGM, NULL) 
{ 
  Ctor(nm);
}


PgmScope::PgmScope(const std::string& nm)
  : ScopeInfo(PGM, NULL)
{ 
  Ctor(nm.c_str());
}


void
PgmScope::Ctor(const char* nm)
{
  DIAG_Assert(nm, "");
  frozen = false;
  m_name = nm;
  groupMap = new GroupScopeMap();
  lmMap = new LoadModScopeMap();
  fileMap = new FileScopeMap();
}


PgmScope& 
PgmScope::operator=(const PgmScope& x) 
{
  // shallow copy
  if (&x != this) {
    frozen   = x.frozen;
    m_name     = x.m_name;
    groupMap = NULL;
    lmMap    = NULL;
    fileMap  = NULL;
  }
  return *this;
}


PgmScope::~PgmScope() 
{
  frozen = false;
  delete groupMap;
  delete lmMap;
  delete fileMap;
}


GroupScope::GroupScope(const char* nm, ScopeInfo* mom, 
		       int begLn, int endLn) 
  : CodeInfo(GROUP, mom, begLn, endLn, 0, 0)
{
  Ctor(nm, mom);
}


GroupScope::GroupScope(const string& nm, ScopeInfo* mom, 
		       int begLn, int endLn) 
  : CodeInfo(GROUP, mom, begLn, endLn, 0, 0)
{
  Ctor(nm.c_str(), mom);
}


void
GroupScope::Ctor(const char* nm, ScopeInfo* mom)
{
  DIAG_Assert(nm, "");
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == PGM) || (t == GROUP) || (t == LM) 
	      || (t == FILE) || (t == PROC) || (t == LOOP), "");
  m_name = nm;
  Pgm()->AddToGroupMap(*this);
}


GroupScope::~GroupScope()
{
}


LoadModScope::LoadModScope(const char* nm, ScopeInfo* mom)
  : CodeInfo(LM, mom, UNDEF_LINE, UNDEF_LINE, 0, 0)
{ 
  Ctor(nm, mom);
}


LoadModScope::LoadModScope(const std::string& nm, ScopeInfo* mom)
  : CodeInfo(LM, mom, UNDEF_LINE, UNDEF_LINE, 0, 0)
{
  Ctor(nm.c_str(), mom);
}


void
LoadModScope::Ctor(const char* nm, ScopeInfo* mom)
{
  DIAG_Assert(nm, "");
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == PGM) || (t == GROUP), "");

  m_name = nm;
  procMap = NULL;
  stmtMap = NULL;

  Pgm()->AddToLoadModMap(*this);
}


LoadModScope& 
LoadModScope::operator=(const LoadModScope& x)
{
  // shallow copy
  if (&x != this) {
    m_name     = x.m_name;
    procMap  = NULL;
    stmtMap  = NULL;
  }
  return *this;
}


LoadModScope::~LoadModScope() 
{
  delete procMap;
  delete stmtMap;
}


FileScope::FileScope(const char* srcFileWithPath, bool srcIsReadble_, 
		     ScopeInfo *mom,
		     suint begLn, suint endLn)
  : CodeInfo(FILE, mom, begLn, endLn, 0, 0)
{
  Ctor(srcFileWithPath, srcIsReadble_, mom);
}


FileScope::FileScope(const string& srcFileWithPath, bool srcIsReadble_, 
		     ScopeInfo *mom,
		     suint begLn, suint endLn)
  : CodeInfo(FILE, mom, begLn, endLn, 0, 0)
{
  Ctor(srcFileWithPath.c_str(), srcIsReadble_, mom);
}


void
FileScope::Ctor(const char* srcFileWithPath, bool srcIsReadble_, 
		ScopeInfo* mom)
{
  DIAG_Assert(srcFileWithPath, "");
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == PGM) || (t == GROUP) || (t == LM), "");

  srcIsReadable = srcIsReadble_;
  m_name = srcFileWithPath;
  Pgm()->AddToFileMap(*this);
  procMap = new ProcScopeMap();
}


FileScope& 
FileScope::operator=(const FileScope& x) 
{
  // shallow copy
  if (&x != this) {
    srcIsReadable = x.srcIsReadable;
    m_name          = x.m_name;
    procMap       = NULL;
  }
  return *this;
}

FileScope::~FileScope() 
{
  delete procMap;
}


FileScope* 
FileScope::findOrCreate(LoadModScope* lmScope, const string& filenm)
{
  FileScope* fScope = lmScope->Pgm()->FindFile(filenm);
  if (fScope == NULL) {
    bool fileIsReadable = FileIsReadable(filenm.c_str());
    fScope = new FileScope(filenm, fileIsReadable, lmScope);
  }
  return fScope; // guaranteed to be a valid pointer
}


ProcScope::ProcScope(const char* n, CodeInfo* mom, const char* ln, 
		     suint begLn, suint endLn) 
  : CodeInfo(PROC, mom, begLn, endLn, 0, 0)
{
  Ctor(n, mom, ln);
}


ProcScope::ProcScope(const string& n, CodeInfo* mom, const string& ln, 
		     suint begLn, suint endLn) 
  : CodeInfo(PROC, mom, begLn, endLn, 0, 0)
{
  Ctor(n.c_str(), mom, ln.c_str());
}


void
ProcScope::Ctor(const char* n, CodeInfo* mom, const char* ln)
{
  DIAG_Assert(n, "");
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == GROUP) || (t == FILE), "");

  m_name = (n) ? n : "";
  m_linkname = (ln) ? ln : "";
  stmtMap = new StmtRangeScopeMap();
  if (File()) { File()->AddToProcMap(*this); }
}


ProcScope& 
ProcScope::operator=(const ProcScope& x) 
{
  // shallow copy
  if (&x != this) {
    m_name    = x.m_name;
    stmtMap = new StmtRangeScopeMap();
  }
  return *this;
}


ProcScope::~ProcScope() 
{
  delete stmtMap;
}


ProcScope*
ProcScope::findOrCreate(FileScope* fScope, const string& procnm, suint line)
{
  ProcScope* pScope = fScope->FindProc(procnm);
  if (!pScope) {
    pScope = new ProcScope(procnm, fScope, procnm, line, line);
  }
  return pScope;
}


AlienScope::AlienScope(CodeInfo* mom, const char* filenm, const char* nm,
		       suint begLn, suint endLn) 
  : CodeInfo(ALIEN, mom, begLn, endLn, 0, 0)
{
  Ctor(mom, filenm, nm);
}


AlienScope::AlienScope(CodeInfo* mom, 
		       const std::string& filenm, const std::string& nm,
		       suint begLn, suint endLn) 
  : CodeInfo(ALIEN, mom, begLn, endLn, 0, 0)
{
  Ctor(mom, filenm.c_str(), nm.c_str());
}


void
AlienScope::Ctor(CodeInfo* mom, const char* filenm, const char* nm)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == GROUP) || (t == ALIEN) 
	      || (t == PROC) || (t == LOOP), "");

  m_filenm = (filenm) ? filenm : "";
  m_name   = (nm) ? nm : "";
}


AlienScope& 
AlienScope::operator=(const AlienScope& x) 
{
  // shallow copy
  if (&x != this) {
    m_filenm = x.m_filenm;
    m_name   = x.m_name;
  }
  return *this;
}


AlienScope::~AlienScope() 
{
}


LoopScope::LoopScope(CodeInfo* mom, suint begLn, suint endLn) 
  : CodeInfo(LOOP, mom, begLn, endLn, 0, 0)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == GROUP) || (t == FILE) || (t == PROC) 
	      || (t == LOOP), "");
}


LoopScope::~LoopScope()
{
}


StmtRangeScope::StmtRangeScope(CodeInfo* mom, suint begLn, suint endLn,
			       VMA begVMA, VMA endVMA)
  : CodeInfo(STMT_RANGE, mom, begLn, endLn, begVMA, endVMA)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  DIAG_Assert((mom == NULL) || (t == GROUP) || (t == FILE) || (t == PROC)
	      || (t == LOOP), "");
  ProcScope* p = Proc();
  if (p) { 
    p->AddToStmtMap(*this); 
    //DIAG_DevIf(0) { LoadMod()->verifyStmtMap(); }
  }
  //DIAG_DevMsg(3, "StmtRangeScope::StmtRangeScope: " << toString_me());
}


StmtRangeScope::~StmtRangeScope()
{
}


RefScope::RefScope(CodeInfo* mom, int _begPos, int _endPos, 
		   const char* refName) 
  : CodeInfo(REF, mom, mom->begLine(), mom->begLine(), 0, 0)
{
  DIAG_Assert(mom->Type() == STMT_RANGE, "");
  begPos = _begPos;
  endPos = _endPos;
  m_name = refName;
  RelocateRef();
  DIAG_Assert(begPos <= endPos, "");
  DIAG_Assert(begPos > 0, "");
  DIAG_Assert(m_name.length() > 0, "");
}


//***************************************************************************
// ScopeInfo: Ancestor
//***************************************************************************

#define dyn_cast_return(base, derived, expr) \
    { base* ptr = expr;  \
      if (ptr == NULL) {  \
        return NULL;  \
      } else {  \
	return dynamic_cast<derived*>(ptr);  \
      } \
    }


CodeInfo* 
ScopeInfo::CodeInfoParent() const
{
  dyn_cast_return(ScopeInfo, CodeInfo, Parent());
}


ScopeInfo *
ScopeInfo::Ancestor(ScopeType tp) const
{
  const ScopeInfo* s = this;
  while (s && s->Type() != tp) {
    s = s->Parent();
  } 
  return (ScopeInfo*) s;
} 


ScopeInfo *
ScopeInfo::Ancestor(ScopeType tp1, ScopeType tp2) const
{
  const ScopeInfo* s = this;
  while (s && s->Type() != tp1 && s->Type() != tp2) {
    s = s->Parent();
  }
  return (ScopeInfo*) s;
} 


#if 0
int IsAncestorOf(ScopeInfo *parent, ScopeInfo *son, int difference)
{
  ScopeInfo *iter = son;
  while (iter && difference > 0 && iter != parent) {
    iter = iter->Parent();
    difference--;
  }
  if (iter && iter == parent)
     return 1;
  return 0;
}
#endif


ScopeInfo* 
ScopeInfo::LeastCommonAncestor(ScopeInfo* n1, ScopeInfo* n2)
{
  // Collect all ancestors of n1 and n2.  The root will be at the front
  // of the ancestor list.
  ScopeInfoList anc1, anc2;
  for (ScopeInfo* a = n1->Parent(); (a); a = a->Parent()) {
    anc1.push_front(a);
  }
  for (ScopeInfo* a = n2->Parent(); (a); a = a->Parent()) {
    anc2.push_front(a);
  }
  
  // Find the most deeply nested common ancestor
  ScopeInfo* lca = NULL;
  while ( (!anc1.empty() && !anc2.empty()) && (anc1.front() == anc2.front())) {
    lca = anc1.front();
    anc1.pop_front();
    anc2.pop_front();
  }
  
  return lca;
}


PgmScope*
ScopeInfo::Pgm() const 
{
  // iff this is called during ScopeInfo construction within the PgmScope 
  // construction dyn_cast does not do the correct thing
  if (Parent() == NULL) {
    // eraxxon: This cannot be a good thing to do!  Pgm() was being
    //   called on a LoopInfo object; then the resulting pointer
    //   (the LoopInfo itself) was queried for PgmInfo member data.  Ouch!
    // eraxxon: return (PgmScope*) this;
    return NULL;
  } 
  else { 
    dyn_cast_return(ScopeInfo, PgmScope, Ancestor(PGM));
  }
}


GroupScope*
ScopeInfo::Group() const 
{
  dyn_cast_return(ScopeInfo, GroupScope, Ancestor(GROUP));
}


LoadModScope*
ScopeInfo::LoadMod() const 
{
  dyn_cast_return(ScopeInfo, LoadModScope, Ancestor(LM));
}


FileScope*
ScopeInfo::File() const 
{
  dyn_cast_return(ScopeInfo, FileScope, Ancestor(FILE));
}


ProcScope*
ScopeInfo::Proc() const 
{
  dyn_cast_return(ScopeInfo, ProcScope, Ancestor(PROC));
}


AlienScope*
ScopeInfo::Alien() const 
{
  dyn_cast_return(ScopeInfo, AlienScope, Ancestor(ALIEN));
}


LoopScope*
ScopeInfo::Loop() const 
{
  dyn_cast_return(ScopeInfo, LoopScope, Ancestor(LOOP));
}


StmtRangeScope*
ScopeInfo::StmtRange() const 
{
  dyn_cast_return(ScopeInfo, StmtRangeScope, Ancestor(STMT_RANGE));
}


//***************************************************************************
// ScopeInfo: Tree Navigation
//***************************************************************************

CodeInfo* 
ScopeInfo::FirstEnclScope() const
{
  dyn_cast_return(NonUniformDegreeTreeNode, CodeInfo, FirstChild());
}


CodeInfo*
ScopeInfo::LastEnclScope() const
{
  dyn_cast_return(NonUniformDegreeTreeNode, CodeInfo, LastChild());
}


// ----------------------------------------------------------------------
// siblings are linked in a circular list
// Parent()->FirstChild()->PrevSibling() == Parent->LastChild() and 
// Parent()->LastChild()->NextSibling()  == Parent->FirstChild()
// ----------------------------------------------------------------------

CodeInfo*
ScopeInfo::NextScope() const
{
  // siblings are linked in a circular list, 
  NonUniformDegreeTreeNode* next = NULL;
  if (dynamic_cast<ScopeInfo*>(Parent()->LastChild()) != this) {
    next = NextSibling();
  } 
  if (next) { 
    CodeInfo* ci = dynamic_cast<CodeInfo*>(next);
    DIAG_Assert(ci, "");
    return ci;
  }
  return NULL;  
}


CodeInfo*
ScopeInfo::PrevScope() const
{
  NonUniformDegreeTreeNode* prev = NULL;
  if (dynamic_cast<ScopeInfo*>(Parent()->FirstChild()) != this) { 
    prev = PrevSibling();
  } 
  if (prev) { 
    CodeInfo* ci = dynamic_cast<CodeInfo*>(prev);
    DIAG_Assert(ci, "");
    return ci;
  }
  return NULL;
}


//***************************************************************************
// ScopeInfo: Paths and Merging
//***************************************************************************

int 
ScopeInfo::Distance(ScopeInfo* anc, ScopeInfo* desc)
{
  int distance = 0;
  for (ScopeInfo* x = desc; x != NULL; x = x->Parent()) {
    if (x == anc) {
      return distance;
    }
    ++distance;
  }

  // If we arrive here, there was no path between 'anc' and 'desc'
  return -1;
}


bool 
ScopeInfo::ArePathsOverlapping(ScopeInfo* lca, ScopeInfo* desc1, 
			       ScopeInfo* desc2)
{
  // Ensure that d1 is on the longest path
  ScopeInfo* d1 = desc1, *d2 = desc2;
  int dist1 = Distance(lca, d1);
  int dist2 = Distance(lca, d2);
  if (dist2 > dist1) {
    ScopeInfo* t = d1;
    d1 = d2;
    d2 = t;
  } 
  
  // Iterate over the longest path (d1 -> lca) searching for d2.  Stop
  // when x is NULL or lca.
  for (ScopeInfo* x = d1; (x && x != lca); x = x->Parent()) {
    if (x == d2) { 
      return true;
    }
  }
  
  // If we arrive here, we did not encounter d2.  Divergent.
  return false;
}


bool
ScopeInfo::MergePaths(ScopeInfo* lca, ScopeInfo* toDesc, ScopeInfo* fromDesc)
{
  bool merged = false;
  // Should we verify that lca is really the lca?
  
  // Collect nodes along paths between 'lca' and 'toDesc', 'fromDesc'.
  // The descendent nodes will be at the end of the list.
  ScopeInfoList toPath, fromPath;
  for (ScopeInfo* x = toDesc; x != lca; x = x->Parent()) {
    toPath.push_front(x);
  }
  for (ScopeInfo* x = fromDesc; x != lca; x = x->Parent()) {
    fromPath.push_front(x);
  }
  DIAG_Assert(toPath.size() > 0 && fromPath.size() > 0, "");
  
  // We merge from the deepest _common_ level of nesting out to lca
  // (shallowest).  
  ScopeInfoList::reverse_iterator toPathIt = toPath.rbegin();
  ScopeInfoList::reverse_iterator fromPathIt = fromPath.rbegin();
  
  // Determine which path is longer (of course, they can be equal) so
  // we can properly set up the iterators
  ScopeInfoList::reverse_iterator* lPathIt = &toPathIt; // long path iter
  int difference = toPath.size() - fromPath.size();
  if (difference < 0) {
    lPathIt = &fromPathIt;
    difference = -difference;
  }
  
  // Align the iterators
  for (int i = 0; i < difference; ++i) { (*lPathIt)++; }
  
  // Now merge the nodes in 'fromPath' into 'toPath'
  for ( ; fromPathIt != fromPath.rend(); ++fromPathIt, ++toPathIt) {
    ScopeInfo* from = (*fromPathIt);
    ScopeInfo* to = (*toPathIt);
    if (IsMergable(to, from)) {
      merged |= Merge(to, from);
    }
  }
  
  return merged;
}


bool 
ScopeInfo::Merge(ScopeInfo* toNode, ScopeInfo* fromNode)
{
  if (!IsMergable(toNode, fromNode)) {
    return false;
  }
  
  // Perform the merge
  // 1. Move all children of 'fromNode' into 'toNode'
  for (ScopeInfoChildIterator it(fromNode); it.Current(); /* */) {
    ScopeInfo* child = dynamic_cast<ScopeInfo*>(it.Current());
    it++; // advance iterator -- it is pointing at 'child'
    
    child->Unlink();
    child->Link(toNode);
  }
  
  // 2. If merging CodeInfos, update line ranges
  CodeInfo* toCI = dynamic_cast<CodeInfo*>(toNode);
  CodeInfo* fromCI = dynamic_cast<CodeInfo*>(fromNode);
  DIAG_Assert(logic::equiv(toCI, fromCI), "Invariant broken!");
  if (toCI && fromCI) {
    suint begLn = MIN(toCI->begLine(), fromCI->begLine());
    suint endLn = MAX(toCI->endLine(), fromCI->endLine());
    toCI->SetLineRange(begLn, endLn);
    toCI->vmaSet().merge(fromCI->vmaSet()); // merge VMAs
  }
  
  // 3. Unlink 'fromNode' from the tree and delete it
  fromNode->Unlink();
  delete fromNode;
  
  return true;
}


bool 
ScopeInfo::IsMergable(ScopeInfo* toNode, ScopeInfo* fromNode)
{
  // For now, merges are only defined on LOOPs and GROUPs
  ScopeInfo::ScopeType toTy = toNode->Type(), fromTy = fromNode->Type();
  bool goodTy = (toTy == ScopeInfo::LOOP || toTy == ScopeInfo::GROUP);
  return ((toTy == fromTy) && goodTy);
}


//***************************************************************************
// Performance Data
//***************************************************************************

void 
ScopeInfo::SetPerfData(int i, double d) 
{
  DIAG_Assert(IsPerfDataIndex(i), "");
  if (IsNaN((*perfData)[i])) {
    (*perfData)[i] = d;
  } else {
    (*perfData)[i] += d;
  }
}


bool 
ScopeInfo::HasPerfData(int i) const
{
  DIAG_Assert(IsPerfDataIndex(i), "");
  ScopeInfo *si = (ScopeInfo*) this;
  return ! IsNaN((*si->perfData)[i]);
}


double 
ScopeInfo::PerfData(int i) const
{
  //DIAG_Assert(HasPerfData(i), "");
  ScopeInfo *si = (ScopeInfo*) this;
  return (*si->perfData)[i];
}


//***************************************************************************
// ScopeInfo, etc: Names, Name Maps, and Retrieval by Name
//***************************************************************************

void 
PgmScope::AddToGroupMap(GroupScope& grp) 
{
  string grpName = grp.name();
  // STL::map is a Unique Associative Container
  DIAG_Assert(groupMap->count(grpName) == 0, "");
  (*groupMap)[grpName] = &grp;
}


void 
PgmScope::AddToLoadModMap(LoadModScope& lm)
{
  string lmName = RealPath(lm.name().c_str());
  // STL::map is a Unique Associative Container  
  DIAG_Assert(lmMap->count(lmName) == 0, "");
  (*lmMap)[lmName] = &lm;
}


void 
PgmScope::AddToFileMap(FileScope& f)
{
  string fName = RealPath(f.name().c_str());
  DIAG_DevMsg(2, "PgmScope: mapping file name '" << fName 
	      << "' to FileScope* " << &f);
  DIAG_Assert(fileMap->count(fName) == 0, "Adding multiple instances of file " 
	      << f.name() << " to\n" << toStringXML());
  (*fileMap)[fName] = &f;
}


void 
FileScope::AddToProcMap(ProcScope& p) 
{
  DIAG_DevMsg(2, "FileScope (" << this << "): mapping proc name '" << p.name()
	      << "' to ProcScope* " << &p);
  
  procMap->insert(make_pair(p.name(), &p));
} 


void 
ProcScope::AddToStmtMap(StmtRangeScope& stmt)
{
  // STL::map is a Unique Associative Container
  (*stmtMap)[stmt.begLine()] = &stmt;
}


LoadModScope*
PgmScope::FindLoadMod(const char* nm) const
{
  string lmName = RealPath(nm);
  if (lmMap->count(lmName) != 0) 
    return (*lmMap)[lmName];
  return NULL;
}


FileScope*
PgmScope::FindFile(const char* nm) const
{
  string fName = RealPath(nm);
  if (fileMap->count(fName) != 0) 
    return (*fileMap)[fName];
  return NULL;
}


GroupScope*
PgmScope::FindGroup(const char* nm) const
{
  if (groupMap->count(nm) != 0)
    return (*groupMap)[nm];
  return NULL;
}


CodeInfo*
LoadModScope::findByVMA(VMA vma)
{
  if (!procMap) {
    buildMap(procMap, ScopeInfo::PROC);
  }
  if (!stmtMap) {
    buildMap(stmtMap, ScopeInfo::STMT_RANGE);
  }
  
  // Attempt to find StatementRange and then Proc
  CodeInfo* found = findStmtRange(vma);
  if (!found) {
    found = findProc(vma);
  }
  return found;
}


template<typename T>
void 
LoadModScope::buildMap(VMAIntervalMap<T>*& m, ScopeInfo::ScopeType ty) const
{
  if (!m) {
    m = new VMAIntervalMap<T>;
  }
  
  ScopeInfoIterator it(this, &ScopeTypeFilter[ty]);
  for (; it.Current(); ++it) {
    T x = dynamic_cast<T>(it.Current());
    
    const VMAIntervalSet& vmaset = x->vmaSet();
    for (VMAIntervalSet::const_iterator it = vmaset.begin();
	 it != vmaset.end(); ++it) {
      m->insert(make_pair(*it, x));
    }
  }
}


template<typename T>
bool 
LoadModScope::verifyMap(VMAIntervalMap<T>* m, const char* map_nm)
{
  if (!m) { return true; }

  for (typename VMAIntervalMap<T>::const_iterator it = m->begin(); 
       it != m->end(); ) {
    const VMAInterval& x = it->first;
    ++it;
    if (it != m->end()) {
      const VMAInterval& y = it->first;
      DIAG_Assert(!x.overlaps(y), "LoadModScope::verifyMap: found overlapping elements within " << map_nm << ": " << x.toString() << " and " << y.toString());
    }
  }
  
  return true;
}


bool 
LoadModScope::verifyStmtMap() const
{ 
  if (!stmtMap) {
    VMAToStmtRangeMap* mp;
    buildMap(mp, ScopeInfo::STMT_RANGE);
    verifyMap(mp, "stmtMap"); 
    delete mp;
    return true; 
 }
  else {
    return verifyMap(stmtMap, "stmtMap"); 
  }
}


ProcScope*
FileScope::FindProc(const char* nm, const char* lnm) const
{
  ProcScope* found = NULL;
  if (procMap->count(nm) != 0) {
    ProcScopeMap::const_iterator it = procMap->find(nm);
    if (lnm && lnm != '\0') {
      for ( ; (it != procMap->end() && it->first == nm); ++it) {
	ProcScope* p = it->second;
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


StmtRangeScope*
ProcScope::FindStmtRange(suint begLn)
{
  return (*stmtMap)[begLn];
}


//***************************************************************************
// CodeInfo, etc: CodeName methods
//***************************************************************************

string
CodeInfo::CodeName() const
{
  FileScope *f = File();
  string nm;
  if (f != NULL) { 
    nm = File()->BaseName() + ": ";
  } 
  nm += StrUtil::toStr(mbegLine);
  if (mbegLine != mendLine) {
    nm += "-" +  StrUtil::toStr(mendLine);
  }
  return nm;
} 


string
CodeInfo::LineRange() const
{
  string self = "b=" + StrUtil::toStr(mbegLine) 
    + " e=" + StrUtil::toStr(mendLine);
  return self;
}


string
GroupScope::CodeName() const 
{
  string self = ScopeTypeToName(Type()) + " " + CodeInfo::CodeName();
  return self;
}


string
LoadModScope::CodeName() const 
{
  string self = ScopeTypeToName(Type()) + " " + CodeInfo::CodeName();
  return self;
}


string
FileScope::CodeName() const 
{
  return BaseName();
}


string
ProcScope::CodeName() const 
{
  FileScope *f = File();
  string nm = m_name + " (";
  if (f != NULL) { 
    nm += f->BaseName() + ":";
  } 
  nm += StrUtil::toStr(mbegLine) + ")";
  return nm;
} 


string
AlienScope::CodeName() const 
{
  string nm = "<" + m_filenm + ">[" + m_name + "]:";
  nm += StrUtil::toStr(mbegLine);
  return nm;
} 


string
LoopScope::CodeName() const 
{
  string nm = ScopeTypeToName(Type()) + " " + CodeInfo::CodeName();
  return nm;
} 


string
StmtRangeScope::CodeName() const 
{
  string nm = ScopeTypeToName(Type()) + " " + CodeInfo::CodeName();
  return nm;
}


string
RefScope::CodeName() const 
{
  return m_name + " " + CodeInfo::CodeName();
}


//***************************************************************************
// ScopeInfo, etc: Output and Debugging support 
//***************************************************************************

string 
ScopeInfo::Types() const
{
  string types;
  if (dynamic_cast<const ScopeInfo*>(this)) {
    types += "ScopeInfo ";
  } 
  if (dynamic_cast<const CodeInfo*>(this)) {
    types += "CodeInfo ";
  } 
  if (dynamic_cast<const PgmScope*>(this)) {
    types += "PgmScope ";
  } 
  if (dynamic_cast<const GroupScope*>(this)) {
    types += "GroupScope ";
  } 
  if (dynamic_cast<const LoadModScope*>(this)) {
    types += "LoadModScope ";
  } 
  if (dynamic_cast<const FileScope*>(this)) {
    types += "FileScope ";
  } 
  if (dynamic_cast<const ProcScope*>(this)) {
    types += "ProcScope ";
  } 
  if (dynamic_cast<const AlienScope*>(this)) {
    types += "AlienScope ";
  } 
  if (dynamic_cast<const LoopScope*>(this)) {
    types += "LoopScope ";
  } 
  if (dynamic_cast<const StmtRangeScope*>(this)) {
    types += "StmtRangeScope ";
  } 
  if (dynamic_cast<const RefScope*>(this)) {
    types += "RefScope ";
  }
  return types;
}


string 
ScopeInfo::toString(int dmpFlag, const char* pre) const
{ 
  std::ostringstream os;
  dump(os, dmpFlag, pre);
  os << ends;
  return os.str();
}


string
ScopeInfo::toString_id(int dmpFlag) const
{ 
  string str = "<" + ScopeTypeToName(Type()) + " uid=" 
    + StrUtil::toStr(UniqueId()) + ">";
  return str;
}


string
ScopeInfo::toString_me(int dmpFlag, const char* prefix) const
{ 
  std::ostringstream os;
  dumpme(os, dmpFlag, prefix);
  os << ends;
  return os.str();
}


std::ostream&
ScopeInfo::dump(ostream& os, int dmpFlag, const char* pre) const 
{
  string prefix = string(pre) + "  ";

  dumpme(os, dmpFlag, pre);
  
  for (unsigned int i = 0; i < NumberOfPerfDataInfos(); i++) {
    os << IndexToPerfDataInfo(i).Name() << "=" ;
    if (HasPerfData(i)) {
      os << PerfData(i);
    } 
    else {
      os << "UNDEF";
    }
    os << "; ";
  }
  
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->dump(os, dmpFlag, prefix.c_str());
  } 
  return os;
}


void
ScopeInfo::ddump() const
{ 
  //XML_DumpLineSorted(std::cerr, 0, "");
  dump(std::cerr, 0, "");
}


ostream&
ScopeInfo::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  os << prefix << toString_id(dmpFlag) << endl;
  return os;
}


ostream&
CodeInfo::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  os << prefix << toString_id(dmpFlag) << " " 
     << LineRange() << " " << mvmaSet.toString();
  return os;
}


ostream&
PgmScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  os << prefix << toString_id(dmpFlag) << " n=" << m_name;
  return os;
}


ostream& 
GroupScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  os << prefix << toString_id(dmpFlag) << " n=" << m_name;
  return os;
}


ostream& 
LoadModScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  os << prefix << toString_id(dmpFlag) << " n=" << m_name;
  return os;
}


ostream&
FileScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  CodeInfo::dumpme(os, dmpFlag, prefix) << " n=" <<  m_name;
  return os;
}


ostream& 
ProcScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  CodeInfo::dumpme(os, dmpFlag, prefix) << " n=" << m_name;
  return os;
}


ostream& 
AlienScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  CodeInfo::dumpme(os, dmpFlag, prefix);
  return os;
}


ostream& 
LoopScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  CodeInfo::dumpme(os, dmpFlag, prefix);
  return os;
}


ostream&
StmtRangeScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{
  CodeInfo::dumpme(os, dmpFlag, prefix);
  return os;
}


ostream&
RefScope::dumpme(ostream& os, int dmpFlag, const char* prefix) const
{ 
  CodeInfo::dumpme(os, dmpFlag, prefix);
  os << " pos:"  << begPos << "-" << endPos;
  return os;
}


void
LoadModScope::dumpmaps() const 
{
  ostream& os = std::cerr;
  int dmpFlag = 0;
  const char* pre = "";
  
  for (VMAToProcMap::const_iterator it = procMap->begin(); 
       it != procMap->end(); ++it) {
    it->first.dump(os);
    os << " --> " << hex << "Ox" << it->second << dec << endl;
  }
  
  os << endl;

  for (VMAToStmtRangeMap::const_iterator it = stmtMap->begin(); 
       it != stmtMap->end(); ++it) {
    it->first.dump(os);
    os << " --> " << hex << "Ox" << it->second << dec << endl;
  }
}


//***************************************************************************
// ScopeInfo, etc: XML output support
//***************************************************************************

static const string XMLelements[ScopeInfo::NUMBER_OF_SCOPES] = {
  "PGM", "G", "LM", "F", "P", "A", "L", "S", "REF", "ANY"
};

const string&
ScopeInfo::ScopeTypeToXMLelement(ScopeType tp)
{
   return XMLelements[tp];
}


string 
ScopeInfo::toStringXML(int dmpFlag, const char* pre) const
{ 
  std::ostringstream os;
  XML_DumpLineSorted(os, dmpFlag, pre);
  os << ends;
  return os.str();
}


string 
ScopeInfo::toXML(int dmpFlag) const
{
  string self = ScopeTypeToXMLelement(Type());
  return self;
}


string
CodeInfo::toXML(int dmpFlag) const
{ 
  string self = ScopeInfo::toXML(dmpFlag) 
    + " " + XMLLineRange(dmpFlag)
    + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string
CodeInfo::XMLLineRange(int dmpFlag) const
{
  string self = "b" + MakeAttrNum(mbegLine) + " e" + MakeAttrNum(mendLine);
  return self;
}


string
CodeInfo::XMLVMAIntervals(int dmpFlag) const
{
  string self = "vma" + MakeAttrStr(mvmaSet.toString(), xml::ESC_FALSE);
  return self;
}


string
PgmScope::toXML(int dmpFlag) const
{
  string self = ScopeInfo::toXML(dmpFlag)
    + " version=\"4.5\""
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
GroupScope::toXML(int dmpFlag) const
{
  string self = ScopeInfo::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
LoadModScope::toXML(int dmpFlag) const
{
  string self = ScopeInfo::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag))
    + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string
FileScope::toXML(int dmpFlag) const
{
  string self = ScopeInfo::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  return self;
}


string 
ProcScope::toXML(int dmpFlag) const
{ 
  string self = ScopeInfo::toXML(dmpFlag) 
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  if (m_name != m_linkname) { // if different, print both
    self = self + " ln" + MakeAttrStr(m_linkname, AddXMLEscapeChars(dmpFlag));
  }
  self = self + " " + XMLLineRange(dmpFlag) + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string 
AlienScope::toXML(int dmpFlag) const
{ 
  string self = ScopeInfo::toXML(dmpFlag) 
    + " f" + MakeAttrStr(m_filenm, AddXMLEscapeChars(dmpFlag))
    + " n" + MakeAttrStr(m_name, AddXMLEscapeChars(dmpFlag));
  self = self + " " + XMLLineRange(dmpFlag) + " " + XMLVMAIntervals(dmpFlag);
  return self;
}


string 
LoopScope::toXML(int dmpFlag) const
{
  string self = CodeInfo::toXML(dmpFlag);
  return self;
}


string
StmtRangeScope::toXML(int dmpFlag) const
{
  string self = CodeInfo::toXML(dmpFlag);
  return self;
}


string
RefScope::toXML(int dmpFlag) const
{ 
  string self = CodeInfo::toXML(dmpFlag) 
    + " b" + MakeAttrNum(begPos) + " e" + MakeAttrNum(endPos);
  return self;
}


bool 
ScopeInfo::XML_DumpSelfBefore(ostream& os, int dmpFlag, 
			      const char* prefix) const
{
  bool attemptToDumpMetrics = true;
  if ((dmpFlag & PgmScopeTree::DUMP_LEAF_METRICS) && Type() != STMT_RANGE) {
    attemptToDumpMetrics = false;
  }
  
  bool dumpMetrics = false;
  if (attemptToDumpMetrics) {
    for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
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
    for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
      if (HasPerfData(i)) {
	if (!(dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT)) { os << endl; }
	os << prefix << "  <M n=\"" << i << "\" v=\"" << PerfData(i) << "\"/>";
      }
    }
  }
  else {
    if (dmpFlag & PgmScopeTree::XML_EMPTY_TAG) {
      os << "/>";
    } 
    else { 
      os << ">";
    }
  }
  if (!(dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT)) { os << endl; }

  return (dumpMetrics);
}


void
ScopeInfo::XML_DumpSelfAfter(ostream& os, int dmpFlag, const char* prefix) const
{
  if (!(dmpFlag & PgmScopeTree::XML_EMPTY_TAG)) {
    os << prefix << "</" << ScopeTypeToXMLelement(Type()) << ">";
    if (!(dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT)) { os << endl; }
  } 
}


void
ScopeInfo::XML_dump(ostream& os, int dmpFlag, const char* pre) const 
{
  string indent = "  ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  
  if (IsLeaf()) { 
    dmpFlag |= PgmScopeTree::XML_EMPTY_TAG;
  }

  bool dumpedMetrics = XML_DumpSelfBefore(os, dmpFlag, pre);
  if (dumpedMetrics) {
    dmpFlag &= ~PgmScopeTree::XML_EMPTY_TAG; // clear empty flag
  }
  string prefix = pre + indent;
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->XML_dump(os, dmpFlag, prefix.c_str());
  }
  XML_DumpSelfAfter(os, dmpFlag, pre);
}


void
ScopeInfo::XML_DumpLineSorted(ostream& os, int dmpFlag, const char* pre) const 
{
  string indent = "  ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }
  if (IsLeaf()) {
    dmpFlag |= PgmScopeTree::XML_EMPTY_TAG;
  }
  
  bool dumpedMetrics = XML_DumpSelfBefore(os, dmpFlag, pre);
  if (dumpedMetrics) {
    dmpFlag &= ~PgmScopeTree::XML_EMPTY_TAG; // clear empty flag
  }
  string prefix = pre + indent;
  for (ScopeInfoLineSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->XML_DumpLineSorted(os, dmpFlag, prefix.c_str());
  }
  XML_DumpSelfAfter(os, dmpFlag, pre);
}


void
ScopeInfo::xml_ddump() const
{
  XML_DumpLineSorted();
}


void
PgmScope::XML_DumpLineSorted(ostream& os, int dmpFlag, const char* pre) const
{
  // N.B.: Typically LoadModScope are children
  string indent = "  ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }

  ScopeInfo::XML_DumpSelfBefore(os, dmpFlag, pre);
  string prefix = pre + indent;
  for (ScopeInfoNameSortedChildIterator it(this); it.Current(); it++) { 
    ScopeInfo* scope = it.Current();
    scope->XML_DumpLineSorted(os, dmpFlag, prefix.c_str());
  }
  ScopeInfo::XML_DumpSelfAfter(os, dmpFlag, pre);
}


void
LoadModScope::XML_DumpLineSorted(ostream& os, int dmpFlag, const char* pre) const
{
  // N.B.: Typically FileScopes are children
  string indent = "  ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }

  ScopeInfo::XML_DumpSelfBefore(os, dmpFlag, pre);
  string prefix = pre + indent;
  for (ScopeInfoNameSortedChildIterator it(this); it.Current(); it++) {
    ScopeInfo* scope = it.Current();
    scope->XML_DumpLineSorted(os, dmpFlag, prefix.c_str());
  }
  ScopeInfo::XML_DumpSelfAfter(os, dmpFlag, pre);
}


//***************************************************************************
// ScopeInfo, etc: 
//***************************************************************************

void 
ScopeInfo::CSV_DumpSelf(const PgmScope &root, ostream& os) const
{ 
  char temp[32];
  for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
    double val = (HasPerfData(i) ? PerfData(i) : 0);
    os << "," << val;
    const PerfMetric& metric = IndexToPerfDataInfo(i);
    if (metric.Percent()) {
      double percVal = val / root.PerfData(i) * 100;
      sprintf(temp, "%5.2lf", percVal);
      os << "," << temp;
    }
  }
  os << endl;
}


void
ScopeInfo::CSV_dump(const PgmScope &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << name() << ",,,,";
  CSV_DumpSelf(root, os);
  for (ScopeInfoNameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->CSV_dump(root, os);
  }
}


void
FileScope::CSV_dump(const PgmScope &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << BaseName() << ",," << mbegLine << "," << mendLine << ",";
  CSV_DumpSelf(root, os);
  for (ScopeInfoNameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->CSV_dump(root, os, BaseName().c_str());
  }
}


void
ProcScope::CSV_dump(const PgmScope &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << file_name << "," << name() << "," << mbegLine << "," << mendLine 
     << ",0";
  CSV_DumpSelf(root, os);
  for (ScopeInfoLineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CSV_dump(root, os, file_name, name().c_str(), 1);
  } 
}


void
AlienScope::CSV_dump(const PgmScope &root, ostream& os, 
		   const char* file_name, const char* proc_name,
		   int lLevel) const 
{
  DIAG_Die(DIAG_Unimplemented);
}


void
CodeInfo::CSV_dump(const PgmScope &root, ostream& os, 
		   const char* file_name, const char* proc_name,
		   int lLevel) const 
{
  ScopeType myScopeType = this->Type();
  // do not display info for single lines
  if (myScopeType == STMT_RANGE)
    return;
  // print file name, routine name, start and end line, loop level
  os << (file_name ? file_name : name().c_str()) << "," 
     << (proc_name ? proc_name : "") << "," 
     << mbegLine << "," << mendLine << ",";
  if (lLevel)
    os << lLevel;
  CSV_DumpSelf(root, os);
  for (ScopeInfoLineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CSV_dump(root, os, file_name, proc_name, lLevel+1);
  } 
}


void
PgmScope::CSV_TreeDump(ostream& os) const
{
  ScopeInfo::CSV_dump(*this, os);
}


//***************************************************************************
// ScopeInfo, etc: 
//***************************************************************************

void 
ScopeInfo::TSV_DumpSelf(const PgmScope &root, ostream& os) const
{ 
  char temp[32];
  for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
    double val = (HasPerfData(i) ? PerfData(i) : 0);
    os << "\t" << val;
    /*const PerfMetric& metric = IndexToPerfDataInfo(i);
    if (metric.Percent()) {
      double percVal = val / root.PerfData(i) * 100;
      sprintf(temp, "%5.2lf", percVal);
      os << "\t" << temp;
    }
    */
  }
  os << endl;
}


// Dumps the scope tree PGM
void
ScopeInfo::TSV_dump(const PgmScope &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  //Dump children
  for (ScopeInfoNameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->TSV_dump(root, os);
  }
}


void
FileScope::TSV_dump(const PgmScope &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  //Dump children
  for (ScopeInfoNameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->TSV_dump(root, os, BaseName().c_str());
  }
}


void
ProcScope::TSV_dump(const PgmScope &root, ostream& os, 
		    const char* file_name, const char* proc_name,
		    int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  // os << file_name << "," << name() << "," << mbegLine << "," << mendLine << ",0";
  for (ScopeInfoLineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->TSV_dump(root, os, file_name, name().c_str(), 1);
  } 
}


void
AlienScope::TSV_dump(const PgmScope &root, ostream& os, 
		     const char* file_name, const char* proc_name,
		     int lLevel) const 
{
  DIAG_Die(DIAG_Unimplemented);
}


void
CodeInfo::TSV_dump(const PgmScope &root, ostream& os, 
		   const char* file_name, const char* proc_name,
		   int lLevel) const 
{
  ScopeType myScopeType = this->Type();
  // recurse into loops until we have single lines
  if (myScopeType == STMT_RANGE) {
	  // print file name, routine name, start and end line, loop level
	  os << (file_name ? file_name : name().c_str()) << "," 
	     << (proc_name ? proc_name : "") << ":" 
	     << mbegLine;
	  TSV_DumpSelf(root, os);
	  return;
  }

  for (ScopeInfoLineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->TSV_dump(root, os, file_name, proc_name, lLevel+1);
  } 
}


void
PgmScope::TSV_TreeDump(ostream& os) const
{
  ScopeInfo::TSV_dump(*this, os);
}


//***************************************************************************
// CodeInfo specific methods 
//***************************************************************************

void 
CodeInfo::SetLineRange(suint begLn, suint endLn) 
{
  // Sanity Checking:
  //   begLn <= endLn
  //   (begLn == UNDEF_LINE) <==> (endLn == UNDEF_LINE)
  DIAG_Assert(begLn <= endLn, "CodeInfo::SetLineRange: b=" << begLn 
	      << " e=" << endLn);

  if (begLn == UNDEF_LINE) {
    DIAG_Assert(endLn == UNDEF_LINE, "CodeInfo::SetLineRange: b=" << begLn 
		<< " e=" << endLn);
    // simply relocate at beginning of sibling list 
    // no range update in parents is necessary
    DIAG_Assert((mbegLine == UNDEF_LINE) && (mendLine == UNDEF_LINE), "");
    if (Parent() != NULL) Relocate();
  } 
  else {
    bool changed = false;
    if (mbegLine == UNDEF_LINE) {
      DIAG_Assert(mendLine == UNDEF_LINE, "");
      // initialize range
      mbegLine = begLn;
      mendLine = endLn;
      changed = true;
    } 
    else {
      DIAG_Assert((mbegLine != UNDEF_LINE) && (mendLine != UNDEF_LINE), "");
      // expand range ?
      if (begLn < mbegLine) { mbegLine = begLn; changed = true; }
      if (endLn > mendLine) { mendLine = endLn; changed = true; }
    }
    CodeInfo* mom = CodeInfoParent();
    if (changed && (mom != NULL)) {
      Relocate();
      
      // never propagate changes outside an AlienScope
      if (Type() != ScopeInfo::ALIEN) {
	mom->SetLineRange(mbegLine, mendLine);
      }
    }
  }
}


void
CodeInfo::Relocate() 
{
  CodeInfo* prev = PrevScope();
  CodeInfo* next = NextScope();
  if (((!prev) || (prev->mendLine <= mbegLine)) && 
      ((!next) || (next->mbegLine >= mendLine))) {
     return;
  } 
  ScopeInfo *mom = Parent();
  Unlink();
  if (mom->FirstChild() == NULL) {
    Link(mom);
  }
  else if (mbegLine == UNDEF_LINE) {
    // insert as first child
    LinkBefore(mom->FirstChild());
  } 
  else {
    // insert after sibling with sibling->mendLine < mbegLine 
    // or iff that does not exist insert as first in sibling list
    CodeInfo* sibling = NULL;
    for (sibling = mom->LastEnclScope(); sibling;
	 sibling = sibling->PrevScope()) {
      if (sibling->mendLine < mbegLine) {
	break;
      }
    } 
    if (sibling != NULL) {
      LinkAfter(sibling);
    } 
    else {
      LinkBefore(mom->FirstChild());
    } 
  }
}


bool
CodeInfo::ContainsLine(suint ln) const
{
   DIAG_Assert(ln != UNDEF_LINE, "");
   if (Type() == FILE) {
     return true;
   } 
   return ((mbegLine >= 1) && (mbegLine <= ln) && (ln <= mendLine));
} 


CodeInfo* 
CodeInfo::CodeInfoWithLine(suint ln) const
{
   DIAG_Assert(ln != UNDEF_LINE, "");
   CodeInfo* ci;
   // ln > mendLine means there is no child that contains ln
   if (ln <= mendLine) {
     for (ScopeInfoChildIterator it(this); it.Current(); it++) {
       ci = dynamic_cast<CodeInfo*>(it.Current());
       DIAG_Assert(ci, "");
       if  (ci->ContainsLine(ln)) {
         if (ci->Type() == STMT_RANGE) {  
	   return ci; // never look inside LINE_SCOPEs 
         } 

	 // desired line might be in inner scope; however, it might be
	 // elsewhere because optimization left procedure with 
	 // non-contiguous line ranges in scopes at various levels.
	 CodeInfo* inner = ci->CodeInfoWithLine(ln);
	 if (inner) return inner;
       } 
     }
   }
   if (ci->Type() == PROC) return (CodeInfo*) this;
   else return 0;
}


int 
CodeInfoLineComp(CodeInfo* x, CodeInfo* y)
{
  if (x->begLine() == y->begLine()) {
    // Given two CodeInfo's with identical endpoints consider two
    // special cases:
    bool endLinesEqual = (x->endLine() == y->endLine());
    
    // 1. If a ProcScope, use a lexiocraphic compare
    if (endLinesEqual
	&& (x->Type() == ScopeInfo::PROC && y->Type() == ScopeInfo::PROC)) {
      ProcScope *px = ((ProcScope*)x), *py = ((ProcScope*)y);
      int ret1 = px->name().compare(py->name());
      if (ret1 != 0) { return ret1; }
      int ret2 = px->LinkName().compare(py->LinkName());
      if (ret2 != 0) { return ret2; }
    }

    // 2. Otherwise: rank a leaf node before a non-leaf node
    if (endLinesEqual && !(x->IsLeaf() && y->IsLeaf())) {
      if      (x->IsLeaf()) { return -1; } // x < y 
      else if (y->IsLeaf()) { return 1; }  // x > y  
    }
    
    // 3. General case
    return SimpleLineCmp(x->endLine(), y->endLine());
  } else {
    return SimpleLineCmp(x->begLine(), y->begLine());
  }
}


// - if x < y; 0 if x == y; + otherwise
int 
SimpleLineCmp(suint x, suint y)
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
int 
AddXMLEscapeChars(int dmpFlag)
{
  if (dmpFlag & PgmScopeTree::XML_NO_ESC_CHARS) {
    return xml::ESC_FALSE;
  }
  else {
    return xml::ESC_TRUE;
  }
}


//***************************************************************************
// RefScope specific methods 
//***************************************************************************

void
RefScope::RelocateRef() 
{
  RefScope *prev = dynamic_cast<RefScope*>(PrevScope());
  RefScope *next = dynamic_cast<RefScope*>(NextScope());
  DIAG_Assert((PrevScope() == prev) && (NextScope() == next), "");
  if (((!prev) || (prev->endPos <= begPos)) && 
      ((!next) || (next->begPos >= endPos))) {
    return;
  } 
  ScopeInfo *mom = Parent();
  Unlink();
  if (mom->FirstChild() == NULL) {
    Link(mom);
  } else {
    // insert after sibling with sibling->endPos < begPos 
    // or iff that does not exist insert as first in sibling list
    CodeInfo* sibling;
    for (sibling = mom->LastEnclScope();
	 sibling;
	 sibling = sibling->PrevScope()) {
      RefScope *ref = dynamic_cast<RefScope*>(sibling);
      DIAG_Assert(ref == sibling, "");
      if (ref->endPos < begPos)  
	break;
      } 
    if (sibling != NULL) {
      RefScope *nxt = dynamic_cast<RefScope*>(sibling->NextScope());
      DIAG_Assert((nxt == NULL) || (nxt->begPos > endPos), "");
      LinkAfter(sibling);
    } else {
      LinkBefore(mom->FirstChild());
    } 
  }
}


//***************************************************************************

#if 0
void 
ScopeInfoTester(int argc, const char** argv) 
{
  PgmScope *root = new PgmScope("ScopeInfoTester");
  LoadModScope *lmScope = new LoadModScope("load module", root);
  FileScope *file;
  ProcScope *proc;
  LoopScope *loop;
  GroupScope *group;
  StmtRangeScope *stmtRange;
  
  FILE *srcFile = fopen("file.c", "r");
  bool known = (srcFile != NULL);
  if (srcFile) {
    fclose(srcFile);
  }  
  
  file = new FileScope("file.c", known, lmScope);
  proc = new ProcScope("proc", file);
  loop = new LoopScope(proc->CodeInfoWithLine(2), 2, 10);
  loop = new LoopScope(proc->CodeInfoWithLine(5), 5, 9);  
  loop = new LoopScope(proc->CodeInfoWithLine(12), 12, 25);

  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(4), 4, 4);
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(3), 3, 3);
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(5), 5, 5);
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(13), 13, 13);

  group = new GroupScope("g1", proc->CodeInfoWithLine(14), 14, 18);
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(15), 15, 15);
  
  std::cout << "root->dump()" << endl;
  root->dump();
  std::cout << "------------------------------------------" << endl << endl;
  
  std::cout << "Type Casts" << endl;
  {
    ScopeInfo* sinfo[10];
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
    FileScope *file_c = lmScope->FindFile("file.c");
    DIAG_Assert(file_c, "");
    ProcScope *proc = file_c->FindProc("proc");
    DIAG_Assert(proc, "");
    CodeInfo* loop3 = proc->CodeInfoWithLine(12);
    DIAG_Assert(loop3, "");
    
    {
      std::cerr << "*** everything under root " << endl;
      ScopeInfoIterator it(root);
      for (; it.CurScope(); it++) {
	CodeInfo* ci = dynamic_cast<CodeInfo*>(it.CurScope());
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
      ScopeInfoLineSortedChildIterator it(loop3);
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
from ScopeInfoTester()
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
