// $Id$
// -*-C++-*-
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

//************************ System Include Files ******************************

#include <iostream>
#include <map> // STL

#ifdef NO_STD_CHEADERS
# include <stdio.h>
# include <limits.h>
#else
# include <cstdio> // for 'fopen'
# include <climits>
using namespace std; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include <include/general.h>

#include "ScopeInfo.h"
#include "PerfMetric.h"
#include "HTMLFile.h" // for HTMLEscapeStr
#include <lib/support/PtrSetIterator.h>
#include <lib/support/realpath.h>
#include <lib/support/Assertion.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::cout;
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

//****************************************************************************

/*****************************************************************************/
// forward fct declarations 
/*****************************************************************************/

/*****************************************************************************/
// template classes 
/*****************************************************************************/

class DoubleVector : public VectorTmpl<double> 
{
public: 
  DoubleVector() : VectorTmpl<double>(NaNVal, true) {}; 
}; 

class IntLt {
public:
  // strict (less-than) operator for ints
  int operator()(const int& i1, const int& i2 ) const 
    { return i1 < i2; }
};

class LoadModScopeMap : public std::map<String, LoadModScope*, StringLt> { };
class ProcScopeMap    : public std::map<String, ProcScope*, StringLt> { }; 
class FileScopeMap    : public std::map<String, FileScope*, StringLt> { };
class LineScopeMap    : public std::map<suint, LineScope*, IntLt> { };

/*****************************************************************************/
// ScopeType `methods'
/*****************************************************************************/

const char* ScopeNames[ScopeInfo::NUMBER_OF_SCOPES] = {
  "PGM", "GRP", "LM", "FIL", "PRC", "LP", "SR",
  "LN", "REF", "ANY"
}; 

const char* 
ScopeInfo::ScopeTypeToName(ScopeType tp) 
{
  return ScopeNames[tp]; 
}

ScopeInfo::ScopeType 
ScopeInfo::IntToScopeType(long i)
{
  BriefAssertion((i >= 0) && (i < NUMBER_OF_SCOPES));
  return (ScopeType) i; 
}

//***************************************************************************
// ScopeInfo, etc: constructors/destructors
//***************************************************************************

ScopeInfo::ScopeInfo(ScopeType t, ScopeInfo* mom) 
  : NonUniformDegreeTreeNode(mom), type(t)
{ 
  BriefAssertion((type == PGM) || (Pgm() == NULL) || !Pgm()->IsFrozen());
  perfData = new DoubleVector(); 
  static unsigned int uniqueId = 0; 
  uid = uniqueId++; 
  height = 0;
}

void ScopeInfo::CollectCrossReferences() 
{
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CollectCrossReferences();
  } 

  CodeInfo *self = dynamic_cast<CodeInfo*>(this);  
  if (self) {
    if (IsLeaf()) {
      self->first = self->last = self;
    } else {
      self->first = self->last = 0;

      int minline = INT_MAX;
      int maxline = -1;
      for (ScopeInfoChildIterator it(this); it.Current(); it++) {
	CodeInfo *child = dynamic_cast<CodeInfo*>(it.CurScope());
	int cmin = child->BegLine();
	int cmax = child->EndLine();
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

int ScopeInfo::NoteHeight() 
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

void ScopeInfo::NoteDepth() 
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
  if (pgm != NULL && pgm->Type() != ScopeInfo::PGM)
     return 1;
  BriefAssertion( pgm == NULL || pgm->Type() == ScopeInfo::PGM);
  return ((pgm == NULL) || !(pgm->IsFrozen())); 
} 

ScopeInfo::~ScopeInfo() 
{
  BriefAssertion(OkToDelete(this)); 
  IFTRACE << "~ScopeInfo " << this << " " << ToString() << endl; 
}


CodeInfo::CodeInfo(ScopeType t, ScopeInfo* mom, suint beg, suint end) 
  : ScopeInfo(t, mom), begLine(UNDEF_LINE), endLine(UNDEF_LINE)
{ 
  // beg <= end
  // (beg == UNDEF_LINE) <==> (end == UNDEF_LINE)
  BriefAssertion(((beg != UNDEF_LINE) || (end == UNDEF_LINE)) && 
		 ((end != UNDEF_LINE) || (beg == UNDEF_LINE)));  
  BriefAssertion(beg <= end); 
  SetLineRange(beg, end); 
}

CodeInfo::~CodeInfo() 
{
}


PgmScope::PgmScope(const char* n) 
  : ScopeInfo(PGM, NULL) 
{ 
  BriefAssertion(n);
  frozen = false;
  name = n;
  lmMap = new LoadModScopeMap();
  fileMap = new FileScopeMap(); 
}

PgmScope::~PgmScope() 
{
  frozen = false; 
  delete lmMap;
  delete fileMap;
}


GroupScope::GroupScope(const char* grpName,
                       CodeInfo *mom, int beg, int end) 
  : CodeInfo(GROUP, mom, beg, end)
{
  BriefAssertion(grpName);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == PGM) || (t == GROUP) || (t == LM) 
		 || (t == FILE) || (t == PROC) || (t == LOOP));
  name = grpName;
}

GroupScope::~GroupScope()
{
}


LoadModScope::LoadModScope(const char* lmName, ScopeInfo* mom) 
  : CodeInfo(LM, mom, UNDEF_LINE, UNDEF_LINE) // ScopeInfo
{ 
  BriefAssertion(lmName);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == PGM) || (t == GROUP));

  name = lmName;
  Pgm()->AddToLoadModMap(*this); 
}

LoadModScope::~LoadModScope() 
{
}


FileScope::FileScope(const char* srcFileWithPath, 
		     bool srcIsReadble_, 
		     const char* preproc, 
		     ScopeInfo *mom, 
		     int beg, int end) 
  : CodeInfo(FILE, mom, beg, end)
{
  BriefAssertion(srcFileWithPath); 
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == PGM) || (t == GROUP) || (t == LM));

  srcIsReadable = srcIsReadble_; 
  name = srcFileWithPath; 
  Pgm()->AddToFileMap(*this); 
  procMap = new ProcScopeMap(); 
}

FileScope::~FileScope() 
{
  delete procMap;
}


ProcScope::ProcScope(const char* n, CodeInfo *mom, int beg, int end) 
  : CodeInfo(PROC, mom, beg, end)
{
  BriefAssertion(n);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == GROUP) || (t == FILE));

  name = n; 
  if (File()) File()->AddToProcMap(*this); 
  lineMap = new LineScopeMap(); // FIXME: to be deprecated
}

ProcScope::~ProcScope() 
{
  delete lineMap;
}

// record a new line scope in map (FIXME: deprecated)
LineScope* 
ProcScope::CreateLineScope(CodeInfo *mom, suint lineNumber)
{
  LineScope *line = new LineScope(mom, lineNumber);
  (*lineMap)[line->BegLine()] = line; 
  IFTRACE << "ProcScope: (" << hex << this << dec << ") '" << Name() 
	  << "': mapping line " << lineNumber
	  << " to LineScope* " << hex << line << dec << endl;

  return line;
} 

// record a new line scope in map (FIXME: deprecated)
LineScope* 
ProcScope::GetLineScope(suint line) 
{
  LineScope *scope = (*lineMap)[line];
  if (scope == NULL) {
    scope = CreateLineScope(this, line);
  }
  return scope;
} 


LoopScope::LoopScope(CodeInfo *mom, suint beg, suint end) 
  : CodeInfo(LOOP, mom, beg, end)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == GROUP) || (t == FILE) || (t == PROC) 
		 || (t == LOOP)); 
}

LoopScope::~LoopScope()
{
}


StmtRangeScope::StmtRangeScope(CodeInfo *mom, int beg, int end) 
  : CodeInfo(STMT_RANGE, mom, beg, end)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == GROUP) || (t == FILE) || (t == PROC)
		 || (t == LOOP));
}

StmtRangeScope::~StmtRangeScope()
{
}

// FIXME: deprecated
LineScope::LineScope(CodeInfo *mom, suint l) 
  : CodeInfo(LINE, mom, l, l)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == STMT_RANGE || t == PROC || t == LOOP));
}

// FIXME: deprecated
RefScope::RefScope(CodeInfo *mom, int _begPos, int _endPos, 
		   const char* refName) 
  : CodeInfo(REF, mom, mom->BegLine(), mom->BegLine())      
{
  BriefAssertion(mom->Type() == LINE); 
  begPos = _begPos;
  endPos = _endPos;
  name = refName; 
  RelocateRef(); 
  BriefAssertion(begPos <= endPos); 
  BriefAssertion(begPos > 0); 
  BriefAssertion(name.Length() > 0); 
}

// **********************************************************************
// Tree Navigation 
// **********************************************************************

#define dyn_cast_return(base, derived, expr) \
    { base* ptr = expr;  \
      if (ptr == NULL) {  \
        return NULL;  \
      } else {  \
	return dynamic_cast<derived*>(ptr);  \
      } \
    }

CodeInfo *
ScopeInfo::CodeInfoParent() const
{
  dyn_cast_return(ScopeInfo, CodeInfo, Parent()); 
}

ScopeInfo *
ScopeInfo::Ancestor(ScopeType tp) const
{
  const ScopeInfo *s = this; 
  while (s && s->Type() != tp) {
    s = s->Parent(); 
  } 
  return (ScopeInfo*) s;
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
  } else { 
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


// FIXME: deprecated
LineScope*
ScopeInfo::Line() const 
{
  dyn_cast_return(ScopeInfo, LineScope, Ancestor(LINE)); 
}

// -------------------------------------------------------------------
// siblings are linked in a circular list
// Parent()->FirstChild()->PrevSibling() == Parent->LastChild() and 
// Parent()->LastChild()->NextSibling()  == Parent->FirstChild()
// -------------------------------------------------------------------

CodeInfo*
ScopeInfo::NextScope() const
{
  // siblings are linked in a circular list, 
  NonUniformDegreeTreeNode* next = NULL; 
  if (dynamic_cast<ScopeInfo*>(Parent()->LastChild()) != this) {
    next = NextSibling(); 
  } 
  if (next) { 
    CodeInfo *ci = dynamic_cast<CodeInfo*>(next); 
    BriefAssertion(ci); 
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
    CodeInfo *ci = dynamic_cast<CodeInfo*>(prev); 
    BriefAssertion(ci); 
    return ci; 
  }
  return NULL; 
}

// **********************************************************************
// Performance Data
// **********************************************************************

void 
ScopeInfo::SetPerfData(int i, double d) 
{
  BriefAssertion(IsPerfDataIndex(i)); 
  if (IsNaN((*perfData)[i])) {
    (*perfData)[i] = d;
  } else {
    (*perfData)[i] += d;
  }
}

bool 
ScopeInfo::HasPerfData(int i) const
{
  BriefAssertion(IsPerfDataIndex(i)); 
  ScopeInfo *si = (ScopeInfo*) this; 
  return ! IsNaN((*si->perfData)[i]); 
}

double 
ScopeInfo::PerfData(int i) const
{
  //BriefAssertion(HasPerfData(i));
  ScopeInfo *si = (ScopeInfo*) this; 
  return (*si->perfData)[i]; 
}

// **********************************************************************
// Names, Name Maps, and Retrieval by Name
// **********************************************************************

void 
PgmScope::AddToLoadModMap(LoadModScope &lm) 
{
  String lmName = RealPath(lm.Name());
  // STL::map is a Unique Associative Container  
  BriefAssertion((*lmMap).count(lmName) == 0); 
  (*lmMap)[lmName] = &lm; 
} 

void 
PgmScope::AddToFileMap(FileScope &f) 
{
  String fName = RealPath(f.Name()); 
  // STL::map is a Unique Associative Container  
  BriefAssertion((*fileMap).count(fName) == 0); 
  (*fileMap)[fName] = &f; 

  IFTRACE << "PgmScope namemap: mapping file name '" << fName 
	  << "' to FileScope* " << hex << &f << dec << endl;
} 

void 
FileScope::AddToProcMap(ProcScope &p) 
{
  // STL::map is a Unique Associative Container
  BriefAssertion((*procMap).count(p.Name()) == 0); 
  (*procMap)[p.Name()] = &p; 

  IFTRACE << "FileScope (" << hex << this << dec
	  << ") namemap: mapping proc name '" << p.Name()
	  << "' to ProcScope* " << hex << &p << dec << endl;
} 

LoadModScope*
PgmScope::FindLoadMod(const char* nm) const
{
  String lmName = RealPath(nm); 
  if ((*lmMap).count(lmName) != 0) 
    return (*lmMap)[lmName]; 
  return NULL; 
}

FileScope*
PgmScope::FindFile(const char* nm) const
{
  String fName = RealPath(nm); 
  if ((*fileMap).count(fName) != 0) 
    return (*fileMap)[fName]; 
  return NULL; 
}

ProcScope*
FileScope::FindProc(const char* nm) const
{
  if ((*procMap).count(nm) != 0) 
    return (*procMap)[nm]; 
  return NULL; 
}

//***************************************************************************
// CodeInfo, etc: CodeName methods
//***************************************************************************

String
CodeInfo::CodeName() const
{
  FileScope *f = File(); 
  String name; 
  if (f != NULL) { 
    name = File()->BaseName();
  } 
  if (begLine != UNDEF_LINE || endLine != UNDEF_LINE) {
    name += ": " + CodeLineName(begLine);
    if (begLine != endLine) {
      name += "-" +  CodeLineName(endLine); 
    }
  }
  return name;
} 

String
CodeInfo::CodeLineName(suint line) const
{
  if (line == UNDEF_LINE) return String("?");
  else return String(line);
}

String
GroupScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
}

String
LoadModScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
}

String
FileScope::CodeName() const 
{
  return BaseName(); 
}

String
ProcScope::CodeName() const 
{
  FileScope *f = File(); 
  String  cName = name;
  if (f != NULL) { 
     cName += " (" + f->BaseName();
     if (begLine != UNDEF_LINE) {
       cName += ":" + CodeLineName(begLine);
     }
     cName += ")";
  } 
  return cName; 
} 

String
LoopScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  FileScope *f = File(); 
  BriefAssertion (f != NULL);
  result += " " + String(begLine);
  if (begLine != endLine) {
    result += "-" +  String(endLine) ; 
  }
  result += ":" + f->BaseName();
  return result;
} 

String
StmtRangeScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
}


// FIXME: deprecated
String
RefScope::CodeName() const 
{
  return name + " " + CodeInfo::CodeName(); 
}

//***************************************************************************
// ScopeInfo, etc: Output and Debugging support 
//***************************************************************************

String 
ScopeInfo::ToString()  const
{ 
  String self = String(ScopeTypeToName(Type())) + 
    " uid=" + String((unsigned long)UniqueId());
  return self; 
}

String
CodeInfo::ToString()  const
{
  String self = ScopeInfo::ToString() +" lines:" + String(begLine) +
    "-" + String(endLine);
  return self;
}

String
PgmScope::ToString() const
{
  String self = ScopeInfo::ToString() + " name=" + name;
  return self;
}

String 
GroupScope::ToString()  const
{
  String self = ScopeInfo::ToString() + " name:" +  name; 
  return self;
}

String 
LoadModScope::ToString()  const
{
  String self = ScopeInfo::ToString() + " name:" +  name; 
  return self;
}

String
FileScope::ToString()  const
{ 
  String self = CodeInfo::ToString() + 
    " name=" +  name;
  return self; 
}

String 
ProcScope::ToString()  const
{ 
  String self = CodeInfo::ToString() +" name:" + name; 
  return self; 
}

String 
LoopScope::ToString()  const
{
  String self;
  self = CodeInfo::ToString();  
  return self;
}

String
StmtRangeScope::ToString()  const
{
  String self;
  self = CodeInfo::ToString();
  return self;
}

// FIXME: deprecated
String
RefScope::ToString()  const
{ 
  String self = CodeInfo::ToString() + " pos:" 
    + String((unsigned long)begPos) + "-" + String((unsigned long)endPos);
  return self;
}

void 
ScopeInfo::DumpSelf(std::ostream &os, const char *prefix)  const
{ 
  os << prefix << ToString() << endl; 
  os << prefix << "   " ; 
  for (unsigned int i = 0; i < NumberOfPerfDataInfos(); i++) {
    os << IndexToPerfDataInfo(i).Name() << "=" ;
    if (HasPerfData(i)) {
      os << PerfData(i); 
    } else {
      os << "UNDEF"; 
    }
    os << "; "; 
  }
  os << endl; 
}

void
ScopeInfo::Dump(std::ostream &os, const char *pre) const 
{
  DumpSelf(os, pre); 
  String prefix = String(pre) + "   "; 
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->Dump(os, prefix); 
  } 
}

String 
ScopeInfo::Types() 
{
  String types; 
  if (dynamic_cast<ScopeInfo*>(this)) {
    types += "ScopeInfo "; 
  } 
  if (dynamic_cast<CodeInfo*>(this)) {
    types += "CodeInfo "; 
  } 
  if (dynamic_cast<PgmScope*>(this)) {
    types += "PgmScope "; 
  } 
  if (dynamic_cast<GroupScope*>(this)) {
    types += "GroupScope "; 
  } 
  if (dynamic_cast<LoadModScope*>(this)) {
    types += "LoadModScope "; 
  } 
  if (dynamic_cast<FileScope*>(this)) {
    types += "FileScope "; 
  } 
  if (dynamic_cast<ProcScope*>(this)) {
    types += "ProcScope "; 
  } 
  if (dynamic_cast<LoopScope*>(this)) {
    types += "LoopScope "; 
  } 
  if (dynamic_cast<StmtRangeScope*>(this)) {
    types += "StmtRangeScope "; 
  } 

  // FIXME: deprecated
  if (dynamic_cast<LineScope*>(this)) {
    types += "LineScope "; 
  } 
  if (dynamic_cast<RefScope*>(this)) {
    types += "RefScope ";
  }
  return types;
}

//**********************************************************************
// XML output support 
//**********************************************************************

static const char* XMLelements[ScopeInfo::NUMBER_OF_SCOPES] = {
  "PGM", "G", "LM", "F", "P", "L", "S",
  "LN", "REF", "ANY"
};

const char*
ScopeInfo::ScopeTypeToXMLelement(ScopeType tp)
{
   return XMLelements[tp];
}

String 
ScopeInfo::ToXML()  const
{ 
  String self = String(ScopeTypeToXMLelement(Type()));
  return self; 
}

String
CodeInfo::ToXML()  const
{
  String self = ScopeInfo::ToXML() + " " + XMLLineNumbers();
  return self;
}

String
CodeInfo::XMLLineNumbers() const
{
  String self = "b=\042" + String(begLine) + "\042 e=\042" +
    String(endLine) + "\042";
  return self;
}

String
PgmScope::ToXML() const
{
  String self = ScopeInfo::ToXML() + " n=\042" + HTMLEscapeStr(name) + "\042";
  return self;
}

String 
GroupScope::ToXML()  const
{
  String self = ScopeInfo::ToXML() + " n=\042" + HTMLEscapeStr(name) + "\042"; 
  return self;
}

String 
LoadModScope::ToXML()  const
{
  String self = ScopeInfo::ToXML() + " n=\042" + HTMLEscapeStr(name) + "\042"; 
  return self;
}

String
FileScope::ToXML()  const
{ 
  String self = ScopeInfo::ToXML() + " n=\042" +  HTMLEscapeStr(name) + "\042";
  return self; 
}

String 
ProcScope::ToXML()  const
{ 
  String self = ScopeInfo::ToXML() + " n=\042" + HTMLEscapeStr(name) 
    + "\042 " + XMLLineNumbers();
  return self; 
}

String 
LoopScope::ToXML()  const
{
  String self;
  self = CodeInfo::ToXML();  
  return self;
}

String
StmtRangeScope::ToXML()  const
{
  String self;
  self = CodeInfo::ToXML();
  return self;
}

String
RefScope::ToXML()  const
{ 
  String self = CodeInfo::ToXML() + " b=\042" 
    + String((unsigned long)begPos) + "\042 e=\042"
    + String((unsigned long)endPos) + "\042";
  return self;
}

void 
ScopeInfo::XML_DumpSelf(std::ostream &os, int dmpFlag, 
			const char *prefix) const
{ 
  os << prefix << "<" << ToXML() << ">" << endl;

  bool attemptToDumpMetrics = true;
  if ((dmpFlag & DUMP_LEAF_METRICS) && this->Type() != LINE) {
    attemptToDumpMetrics = false;
  }
  
  if (attemptToDumpMetrics) {
    for (unsigned int i=0; i < NumberOfPerfDataInfos(); i++) {
      if (HasPerfData(i)) {
	os << prefix << "  <M n=\042" << i << "\042 v=\042"
	   << PerfData(i) << "\042/>"
	   << endl;
      }
    }
  }
}


// Dumps the scope tree PGM
void
ScopeInfo::XML_Dump(std::ostream &os, int dmpFlag, const char *pre) const 
{
  ScopeType myScopeType = this->Type();
  XML_DumpSelf(os, dmpFlag, pre); 
  String prefix = String(pre) + "  "; 
  if ( (myScopeType == PGM) || (myScopeType == FILE) ) {
    for (NameSortedChildIterator it(this); it.Current(); it++) {
      it.Current()->XML_Dump(os, dmpFlag, prefix); 
    }
  } else {
    for (LineSortedChildIterator it(this); it.Current(); it++) {
      it.CurScope()->XML_Dump(os, dmpFlag, prefix); 
    }
  } 
  String selfClosure = String(ScopeTypeToXMLelement(myScopeType)); 
  os << pre << "</" << selfClosure << ">" << endl;
}

void
PgmScope::XML_Dump(std::ostream &os, int dmpFlag, const char *pre) const
{
  ScopeInfo::XML_Dump(os, dmpFlag, pre);
}

void 
ScopeInfo::CSV_DumpSelf(const PgmScope &root, std::ostream &os) const
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


// Dumps the scope tree PGM
void
ScopeInfo::CSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << Name() << ",,,,";
  CSV_DumpSelf(root, os); 
  for (NameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->CSV_Dump(root, os); 
  }
}

void
FileScope::CSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << BaseName() << ",," << begLine << "," << endLine << ",";
  CSV_DumpSelf(root, os); 
  for (NameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->CSV_Dump(root, os, BaseName()); 
  }
}

void
ProcScope::CSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  os << file_name << "," << Name() << "," << begLine << "," << endLine 
     << ",0";
  CSV_DumpSelf(root, os); 
  for (LineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CSV_Dump(root, os, file_name, Name(), 1); 
  } 
}

void
CodeInfo::CSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  ScopeType myScopeType = this->Type();
  // do not display info for single lines
  if (myScopeType == LINE)
    return;
  // print file name, routine name, start and end line, loop level
  os << (file_name?file_name:(const char*)Name()) << "," 
     << (routine_name?routine_name:"") << "," 
     << begLine << "," << endLine << ",";
  if (lLevel)
    os << lLevel;
  CSV_DumpSelf(root, os);
  for (LineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->CSV_Dump(root, os, file_name, routine_name, lLevel+1); 
  } 
}

void
PgmScope::CSV_TreeDump(std::ostream &os) const
{
  ScopeInfo::CSV_Dump(*this, os);
}

// **********************************************************************

void 
ScopeInfo::TSV_DumpSelf(const PgmScope &root, std::ostream &os) const
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
ScopeInfo::TSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  //Dump children
  for (NameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->TSV_Dump(root, os); 
  }
}

void
FileScope::TSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  //Dump children
  for (NameSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->TSV_Dump(root, os, BaseName()); 
  }
}

void
ProcScope::TSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  // print file name, routine name, start and end line, loop level
  // os << file_name << "," << Name() << "," << begLine << "," << endLine << ",0";
  for (LineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->TSV_Dump(root, os, file_name, Name(), 1); 
  } 
}

void
CodeInfo::TSV_Dump(const PgmScope &root, std::ostream &os, 
          const char *file_name, const char *routine_name,
          int lLevel) const 
{
  ScopeType myScopeType = this->Type();
  // recurse into loops until we have single lines
  if (myScopeType == LINE) {
	  // print file name, routine name, start and end line, loop level
	  os << (file_name?file_name:(const char*)Name()) << "," 
	     << (routine_name?routine_name:"") << ":" 
	     << begLine;
	  TSV_DumpSelf(root, os);
	  return;
  }

  for (LineSortedChildIterator it(this); it.Current(); it++) {
    it.CurScope()->TSV_Dump(root, os, file_name, routine_name, lLevel+1); 
  } 
}

void
PgmScope::TSV_TreeDump(std::ostream &os) const
{
  ScopeInfo::TSV_Dump(*this, os);
}

// **********************************************************************
// RefScope specific methods 
// **********************************************************************

void
RefScope::RelocateRef() 
{
  RefScope *prev = dynamic_cast<RefScope*>(PrevScope());
  RefScope *next = dynamic_cast<RefScope*>(NextScope());
  BriefAssertion((PrevScope() == prev) && (NextScope() == next)); 
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
      BriefAssertion(ref == sibling); 
      if (ref->endPos < begPos)  
	break; 
      } 
    if (sibling != NULL) {
      RefScope *nxt = dynamic_cast<RefScope*>(sibling->NextScope());
      BriefAssertion((nxt == NULL) || (nxt->begPos > endPos)); 
      LinkAfter(sibling); 
    } else {
      LinkBefore(mom->FirstChild()); 
    } 
  }
}


// **********************************************************************
// CodeInfo specific methods 
// **********************************************************************

void 
CodeInfo::SetLineRange(suint beg, suint end) 
{
  if (beg == UNDEF_LINE) {
    BriefAssertion(end == UNDEF_LINE); 
    // simply relocate at beginning of sibling list 
    // no range update in parents is necessary
    BriefAssertion((begLine == UNDEF_LINE) && (endLine == UNDEF_LINE)); 
    if (Parent() != NULL) Relocate(); 
  }  else {
    bool changed = false; 
    if (begLine == UNDEF_LINE) {
      BriefAssertion(endLine == UNDEF_LINE); 
      // initialize range
      begLine = beg; 
      endLine = end; 
      changed = true; 
    } else {
      BriefAssertion(begLine != UNDEF_LINE); 
      BriefAssertion(endLine != UNDEF_LINE); 
      // expand range ?
      if (beg < begLine) { begLine = beg; changed = true; }
      if (end > endLine) { endLine = end; changed = true; }
      // changed => (Type() != BLOCK_SCOPE)
      //    i.e. never expand range of a BLOCK_SCOPE
      //BriefAssertion((!changed) || (Type() != BLOCK)); 
    }
    CodeInfo *mom = CodeInfoParent();
    if (changed && (mom != NULL)) {
#if 0 // this doesn't achieve the result we want
      Relocate(); 
#endif
      mom->SetLineRange(begLine, endLine); 
    }
  }
}

void
CodeInfo::Relocate() 
{
  CodeInfo *prev = PrevScope(); 
  CodeInfo *next = NextScope(); 
  if (((!prev) || (prev->endLine <= begLine)) && 
      ((!next) || (next->begLine >= endLine))) {
     return; 
  }
/*
  cout << "should not go here" << endl;

  cout << " Relocate self : " << begLine << " " << endLine;
  if (prev)
    cout << " prev : " << prev->endLine;
  if (next)
    cout << " next : " << next->begLine;
  cout << endl;
*/  
  ScopeInfo *mom = Parent(); 
  Unlink(); 
  if (mom->FirstChild() == NULL) {
    Link(mom); 
  }
  else if (begLine == UNDEF_LINE) {
    // insert as first child
    LinkBefore(mom->FirstChild()); 
  } else {
    // insert after sibling with sibling->endLine < begLine 
    // or iff that does not exist insert as first in sibling list
    CodeInfo* sibling;
    for (sibling = mom->LastEnclScope(); 
	 sibling; 
	 sibling = sibling->PrevScope()) {
      if (sibling->endLine < begLine)  
	break; 
    } 
    if (sibling != NULL) {
      LinkAfter(sibling); 
    } else {
      LinkBefore(mom->FirstChild()); 
    } 
  }
}

bool 
CodeInfo::ContainsLine(suint ln)  const
{
   if (Type() == FILE) {
     return true; 
   } 
   return ((begLine >= 1) && (begLine <= ln) && (ln <= endLine)); 
} 

CodeInfo* 
CodeInfo::CodeInfoWithLine(suint ln)  const
{
   BriefAssertion(ln != UNDEF_LINE); 
   CodeInfo *ci; 
   // ln > endLine means there is no child that contains ln
   if (ln <= endLine) {
     for (ScopeInfoChildIterator it(this); it.Current(); it++) {
       ci = dynamic_cast<CodeInfo*>(it.Current()); 
       BriefAssertion(ci); 
       if  (ci->ContainsLine(ln)) {
         if (ci->Type() == LINE) {  
	   return ci; // never look inside LINE_SCOPEs 
         } 

	 // desired line might be in inner scope; however, it might be
	 // elsewhere because optimization left procedure with 
	 // non-contiguous line ranges in scopes at various levels.
	 CodeInfo *inner = ci->CodeInfoWithLine(ln); 
	 if (inner) return inner;
       } 
     }
   }
   if (ci->Type() == PROC) return (CodeInfo*) this;
   else return 0;
}

//****************************************************************************

#if 0 // very deprecated

void 
ScopeInfoTester(int argc, const char** argv) 
{
  PgmScope *root = new PgmScope("ScopeInfoTester"); 
  FileScope *file; 
  ProcScope *proc; 
  BlockScope *block;
  LineScope *line;
  RefScope *ref; 
  
  FILE *srcFile = fopen("file.c", "r");
  bool known = (srcFile != NULL); 
  if (srcFile) {
    fclose(srcFile); 
  }  
  
  file = new FileScope("file.c", known, "", root); 
  proc = new ProcScope("proc", file); 
  block = new BlockScope(proc->CodeInfoWithLine(2), 2, 10); 
  block = new BlockScope(proc->CodeInfoWithLine(5), 5, 9);  
  block = new BlockScope(proc->CodeInfoWithLine(12), 12, 25); 
  line = new LineScope(proc->CodeInfoWithLine(4), 4); 
  line = new LineScope(proc->CodeInfoWithLine(3), 3); 
  line = new LineScope(proc->CodeInfoWithLine(5), 5); 
  line = new LineScope(proc->CodeInfoWithLine(13), 13); 
  line = new LineScope(proc->CodeInfoWithLine(15), 15); 
  ref = new RefScope(proc->CodeInfoWithLine(3), 34, 38, "LINE"); 
  ref = new RefScope(proc->CodeInfoWithLine(3), 7, 11, "line"); 
  ref = new RefScope(proc->CodeInfoWithLine(3), 21, 25, "Line"); 
  ref = new RefScope(proc->CodeInfoWithLine(13), 8, 15, "atheline"); 
  ref = new RefScope(proc->CodeInfoWithLine(15), 8, 12, "line");
  
  line = new LineScope(NULL, 1110); 
  ref = new RefScope(line, 14, 18, "REF"); 
  
  cout << "root->Dump()" << endl; 
  root->Dump(); 
  cout << "------------------------------------------" << endl << endl; 
  
  cout << "Type Casts" << endl; 
  {
    ScopeInfo* sinfo[10]; 
    int sn = 0; 
    sinfo[sn++] = root; 
    sinfo[sn++] = file; 
    sinfo[sn++] = proc; 
    sinfo[sn++] = block; 
    sinfo[sn++] = line; 
    sinfo[sn++] = ref; 
    
    for (int i = 0; i < sn; i++) {
      cout << sinfo[i]->ToString() << "::\t" ; 
      cout << sinfo[i]->Types() << endl; 
      cout << endl; 
    } 
    cout << endl; 
  } 
  cout << "------------------------------------------" << endl << endl; 
  
  cout << "Iterators " << endl; 
  { 
    FileScope *file_c = root->FindFile("file.c"); 
    BriefAssertion(file_c); 
    ProcScope *proc = file_c->CreateProcScope("proc"); 
    BriefAssertion(proc); 
    CodeInfo *line_3 = proc->CodeInfoWithLine(3); 
    BriefAssertion(line_3); 
    
    {
      cerr << "*** everything under root " << endl; 
      ScopeInfoIterator it(root); 
      for (; it.CurScope(); it++) {
	CodeInfo *ci = dynamic_cast<CodeInfo*>(it.CurScope());
	cout << it.CurScope()->ToString(); 
	if (ci != NULL) { 
	  cout << " Name=" << ci->Name(); 
	} 
	cout << endl; 
      }  
    }

    { 
      cerr << "*** file.c in line order" << endl;  
      LineSortedIterator it(file_c, NULL, false); 
      it.DumpAndReset(); 
      cerr << "***" << endl << endl; 
      cerr << "*** file.c refs in line order" << endl;  
      LineSortedIterator leaves(file_c, ScopeTypeFilter + REF); 
      leaves.DumpAndReset(); 
      cerr << "***" << endl << endl; 
    }
    { 
      cerr << "*** line_3's children in line order" << endl;  
// the original LineSortedChildIterator was just a wrapper for 
// ScopeInfoChildIterator; re-written LineSortedChildIterator doesn't have
// a DumpAndReset
//      LineSortedChildIterator it(line_3); 
      ScopeInfoChildIterator it(line_3); 
      it.DumpAndReset(); 
      cerr << "***" << endl << endl; 
    }
  }
  cout << "------------------------------------------" << endl << endl; 

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
    15       line(15)        C comment comment comment long lon warp around ...
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
