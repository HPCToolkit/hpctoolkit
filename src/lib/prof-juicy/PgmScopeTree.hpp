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

#ifndef prof_juicy_PgmScopeTree 
#define prof_juicy_PgmScopeTree

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <list> // STL
#include <set>  // STL
#include <map>  // STL
#include <string>
 
//*************************** User Include Files ****************************

#include <include/general.h>

#include "PerfMetric.hpp"

#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Files.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/NaN.h>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/Unique.hpp>
#include <lib/support/VectorTmpl.hpp>
#include <lib/support/realpath.h>

//*************************** Forward Declarations **************************

class ScopeInfo;

// Some possibly useful containers
typedef std::list<ScopeInfo*> ScopeInfoList;
typedef std::set<ScopeInfo*> ScopeInfoSet;

//*************************** Forward Declarations **************************

class DoubleVector : public VectorTmpl<double> {
public: 
  DoubleVector() : VectorTmpl<double>(c_FP_NAN_d, true) { }
};


// FIXME: move these into their respective classes...
class GroupScope;
class GroupScopeMap     : public std::map<std::string, GroupScope*> { };

class LoadModScope;
class LoadModScopeMap   : public std::map<std::string, LoadModScope*> { };

// ProcScopeMap: This is is a multimap because procedure names are
// sometimes "generic", i.e. not qualified by types in the case of
// templates, resulting in duplicate names
class ProcScope;
class ProcScopeMap      : public std::multimap<std::string, ProcScope*> { };

class FileScope;
class FileScopeMap      : public std::map<std::string, FileScope*> { };

class StmtRangeScope;
class StmtRangeScopeMap : public std::map<SrcFile::ln, StmtRangeScope*> { };

//***************************************************************************
// PgmScopeTree
//***************************************************************************

class PgmScope;

class PgmScopeTree : public Unique {
public:
  enum {
    // User-level bit flags
    XML_FALSE =	(0 << 0),	/* No XML format */
    XML_TRUE  =	(1 << 0),	/* XML format */
    
    COMPRESSED_OUTPUT = (1 << 1),  /* Use compressed output format */

    DUMP_LEAF_METRICS = (1 << 2),  /* Dump only leaf metrics */
    
    // Not-generally-user-level bit flags
    XML_NO_ESC_CHARS = (1 << 10), /* don't substitute XML escape characters */

    // Private bit flags
    XML_EMPTY_TAG    = (1 << 15)  /* this is an empty XML tag */
    
  };

  static const std::string UnknownFileNm;
  static const std::string UnknownProcNm;
  
public:
  // Constructor/Destructor
  PgmScopeTree(const char* name, PgmScope* _root = NULL);
  PgmScopeTree(const std::string& name, PgmScope* _root = NULL)
    { PgmScopeTree(name.c_str(), _root); }

  virtual ~PgmScopeTree();

  // Tree data
  PgmScope* GetRoot() const { return root; }
  void SetRoot(PgmScope* x) { root = x; }
  bool IsEmpty() const { return (root == NULL); }
  
  void CollectCrossReferences();

  virtual void xml_dump(std::ostream& os = std::cerr, 
			int dmpFlag = XML_TRUE) const;


  // Dump contents for inspection (use flags from ScopeInfo)
  virtual void dump(std::ostream& os = std::cerr, 
		    int dmpFlag = XML_TRUE) const;
  virtual void ddump() const;
 
private:
  PgmScope* root;
};


//***************************************************************************
// ScopeInfo, CodeInfo.
//***************************************************************************

// FIXME: It would make more sense for GroupScope and LoadModScope to
// simply be ScopeInfos and not CodeInfos, but the assumption that
// *only* a PgmScope is not a CodeInfo is deeply embedded and would
// take a while to untangle.

class ScopeInfo;   // Base class for all scopes
class CodeInfo;    // Base class for everyone but PGM

class PgmScope;    // Tree root
class GroupScope;
class LoadModScope;
class FileScope;
class ProcScope;
class AlienScope;
class LoopScope;
class StmtRangeScope;
class RefScope;

// ---------------------------------------------------------
// ScopeInfo: The base node for a program scope tree
// ---------------------------------------------------------
class ScopeInfo: public NonUniformDegreeTreeNode {
public:
  enum ScopeType {
    PGM = 0,
    GROUP,
    LM,
    FILE,
    PROC,
    ALIEN,
    LOOP,
    STMT_RANGE,
    REF,
    ANY,
    NUMBER_OF_SCOPES
  };

  static const std::string& ScopeTypeToName(ScopeType tp);
  static const std::string& ScopeTypeToXMLelement(ScopeType tp);
  static ScopeType     IntToScopeType(long i);

protected:
  ScopeInfo(const ScopeInfo& x) { *this = x; }
  ScopeInfo& operator=(const ScopeInfo& x);

private:
  static const std::string ScopeNames[NUMBER_OF_SCOPES];

public:
  ScopeInfo(ScopeType ty, ScopeInfo* parent = NULL)
    : NonUniformDegreeTreeNode(parent), type(ty)
  { 
    if (0) { ctorCheck(); }
    uid = s_nextUniqueId++;
    height = 0;
    depth = 0;
    perfData = new DoubleVector();
  }

  virtual ~ScopeInfo()
  {
    if (0) { dtorCheck(); }
    delete perfData;
  }
  
  // --------------------------------------------------------
  // General Interface to fields 
  // --------------------------------------------------------
  ScopeType Type() const         { return type; }
  uint      UniqueId() const     { return uid; }

  // name: 
  // nameQual: qualified name [built dynamically]
  virtual const std::string& name() const { return ScopeTypeToName(Type()); }
  virtual std::string nameQual() const { return name(); }

  void CollectCrossReferences();
  int  NoteHeight();
  void NoteDepth();

  int ScopeHeight() const { return height; }
  int ScopeDepth() const { return depth; }


  // --------------------------------------------------------
  // Metric data
  // --------------------------------------------------------

  bool 
  HasPerfData() const 
  {
    uint end = NumPerfData();
    for (int i = 0; i < end; ++i) {
      if (HasPerfData(i)) {
	return true;
      }
    }
    return false;
  }

  bool 
  HasPerfData(int mId) const 
  {
    double x = (*perfData)[mId];
    return (x != 0.0 && !c_isnan_d(x));
  }
  
  double 
  PerfData(int mId) const 
  {
    return (*perfData)[mId];
  }

  void 
  SetPerfData(int mId, double d) 
  {
    // NOTE: VectorTmpl::operator[] automatically 'adds' the index if necessary
    if (c_isnan_d((*perfData)[mId])) {
      (*perfData)[mId] = d;
    }
    else {
      (*perfData)[mId] += d;
    }
  }

  uint 
  NumPerfData() const 
  {
    return perfData->GetNumElements();
  }

  // accumulates metrics from children. [mBegId, mEndId] forms an
  // inclusive interval for batch processing.  In particular, 'raw'
  // metrics are independent of all other raw metrics.
  void
  accumulateMetrics(uint mBegId, uint mEndId) 
  {
    // NOTE: Must not call HasPerfData() since this node may not have
    // any metric data yet!
    uint sz = mEndId + 1;
    double* valVec = new double[sz];
    bool* hasValVec = new bool[sz];
    accumulateMetrics(mBegId, mEndId, valVec, hasValVec);
    delete[] valVec;
    delete[] hasValVec;
  }

  void 
  accumulateMetrics(uint mBegId)
  {
    accumulateMetrics(mBegId, mBegId);
  }

private:
  void
  accumulateMetrics(uint mBegId, uint mEndId, double* valVec, bool* hasValVec);

public:

  // traverses the tree and removes all nodes for which HasPerfData() is false
  void pruneByMetrics();

  // --------------------------------------------------------
  // Parent
  // --------------------------------------------------------
  ScopeInfo *Parent() const 
  { return (ScopeInfo*) NonUniformDegreeTreeNode::Parent(); }
  
  CodeInfo* CodeInfoParent() const;  // return dyn_cast<CodeInfo*>(Parent())
  
  // --------------------------------------------------------
  // Ancestor: find first ScopeInfo in path from this to root with given type
  // (Note: We assume that a node *can* be an ancestor of itself.)
  // --------------------------------------------------------
  ScopeInfo* Ancestor(ScopeType type) const;
  ScopeInfo* Ancestor(ScopeType tp1, ScopeType tp2) const;
  
  PgmScope*       Pgm() const;           // return Ancestor(PGM)
  GroupScope*     Group() const;         // return Ancestor(GROUP)
  LoadModScope*   LoadMod() const;       // return Ancestor(LM)
  FileScope*      File() const;          // return Ancestor(FILE)
  ProcScope*      Proc() const;          // return Ancestor(PROC)
  AlienScope*     Alien() const;         // return Ancestor(ALIEN)
  LoopScope*      Loop() const;          // return Ancestor(LOOP)
  StmtRangeScope* StmtRange() const;     // return Ancestor(STMT_RANGE)

  CodeInfo*       CallingCtxt() const;   // return Ancestor(ALIEN|PROC)


  // LeastCommonAncestor: Given two ScopeInfo nodes, return the least
  // common ancestor (deepest nested common ancestor) or NULL.
  static ScopeInfo* LeastCommonAncestor(ScopeInfo* n1, ScopeInfo* n2);

  // --------------------------------------------------------
  // Tree navigation 
  //   1) all ScopeInfos contain CodeInfos as children 
  //   2) PgmRoot is the only ScopeInfo type that is not also a CodeInfo;
  //      since PgmScopes have no siblings, it is safe to make Next/PrevScope 
  //      return CodeInfo pointers 
  // --------------------------------------------------------
  CodeInfo* FirstEnclScope() const;      // return  FirstChild()
  CodeInfo* LastEnclScope()  const;      // return  LastChild()
  CodeInfo* NextScope()      const;      // return  NULL or NextSibling()
  CodeInfo* PrevScope()      const;      // return  NULL or PrevSibling()
  bool      IsLeaf()         const       { return  FirstEnclScope() == NULL; }

  CodeInfo* nextScopeNonOverlapping() const;

  // --------------------------------------------------------
  // Paths and Merging
  // --------------------------------------------------------

  // Distance: Given two ScopeInfo nodes, a node and some ancestor,
  // return the distance of the path between the two.  The distance
  // between a node and its direct ancestor is 1.  If there is no path
  // between the two nodes, returns a negative number; if the two
  // nodes are equal, returns 0.
  static int Distance(ScopeInfo* ancestor, ScopeInfo* descendent);

  // ArePathsOverlapping: Given two nodes and their least common
  // ancestor, lca, returns whether the paths from the nodes to lca
  // overlap.
  //
  // Let d1 and d2 be two nodes descended from their least common
  // ancestor, lca.  Furthermore, let the path p1 from d1 to lca be as
  // long or longer than the path p2 from d2 to lca.  (Thus, d1 is
  // nested as deep or more deeply than d2.)  If the paths p1 and p2 are
  // overlapping then d2 will be somewhere on the path between d1 and
  // lca.
  //
  // Examples: 
  // 1. Overlapping: lca --- d2 --- ... --- d1
  //
  // 2. Divergent:   lca --- d1
  //                    \--- d2
  //
  // 3. Divergent:   lca ---...--- d1
  //                    \---...--- d2
  static bool ArePathsOverlapping(ScopeInfo* lca, ScopeInfo* desc1, 
				  ScopeInfo* desc2);
  
  // MergePaths: Given divergent paths (as defined above), merges the path
  // from 'toDesc' into 'fromDesc'. If a merge takes place returns true.
  static bool MergePaths(ScopeInfo* lca, 
			 ScopeInfo* toDesc, ScopeInfo* fromDesc);
  
  // Merge: Given two nodes, 'fromNode' and 'toNode', merges the
  // former into the latter, if possible.  If the merge takes place,
  // deletes 'fromNode' and returns true; otherwise returns false.
  static bool Merge(ScopeInfo* toNode, ScopeInfo* fromNode);

  // IsMergable: Returns whether 'fromNode' is capable of being merged
  // into 'toNode'
  static bool IsMergable(ScopeInfo* toNode, ScopeInfo* fromNode);
  
  // --------------------------------------------------------
  // cloning
  // --------------------------------------------------------
  
  // Clone: return a shallow copy, unlinked from the tree
  virtual ScopeInfo* Clone() { return new ScopeInfo(*this); }
  
  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  std::string toStringXML(int dmpFlag = 0, const char* pre = "") const;
  
  virtual std::string toXML(int dmpFlag = 0) const;

  bool XML_DumpSelfBefore(std::ostream& os = std::cout,
		int dmpFlag = 0, const char* prefix = "") const;
  void XML_DumpSelfAfter (std::ostream& os = std::cout,
		int dmpFlag = 0, const char* prefix = "") const;
  void XML_dump(std::ostream& os = std::cout,
		int dmpFlag = 0, const char* pre = "") const;
  
  virtual void XML_DumpLineSorted(std::ostream& os = std::cout,
				  int dmpFlag = 0,
				  const char* pre = "") const;

  void xml_ddump() const;

  // --------------------------------------------------------
  // Other output
  // --------------------------------------------------------

  void CSV_DumpSelf(const PgmScope &root, std::ostream& os = std::cout) const;
  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::string Types() const; // instance's base and derived types 

  std::string toString(int dmpFlag = 0, const char* pre = "") const;

  std::string toString_id(int dmpFlag = 0) const;
  std::string toString_me(int dmpFlag = 0, const char* pre = "") const;

  // dump
  std::ostream& dump(std::ostream& os = std::cerr, 
		     int dmpFlag = 0, const char* pre = "") const;
  
  void ddump() const;

  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void ctorCheck() const;
  void dtorCheck() const;

  static uint s_nextUniqueId;
  
protected:
  ScopeType type;
  uint uid;
  int height; // cross reference information
  int depth;
  DoubleVector* perfData;
};


// --------------------------------------------------------------------------
// CodeInfo is a base class for all scopes other than PGM and LM.
// Describes some kind of code, i.e. Files, Procedures, Loops...
// --------------------------------------------------------------------------
class CodeInfo : public ScopeInfo {
protected: 
  CodeInfo(ScopeType t, ScopeInfo* parent = NULL, 
	   SrcFile::ln begLn = ln_NULL,
	   SrcFile::ln endLn = ln_NULL,
	   VMA begVMA = 0, VMA endVMA = 0)
    : ScopeInfo(t, parent), m_begLn(ln_NULL), m_endLn(ln_NULL)
  { 
    SetLineRange(begLn, endLn);
    if (begVMA != 0 && endVMA != 0) {
      m_vmaSet.insert(begVMA, endVMA);
    }
  }

  CodeInfo(const CodeInfo& x) : ScopeInfo(x.type) { *this = x; }

  CodeInfo& operator=(const CodeInfo& x);

public: 
  virtual ~CodeInfo()
  { }

  // Line range in source code
  SrcFile::ln  begLine() const { return m_begLn; }
  SrcFile::ln& begLine()       { return m_begLn; }

  SrcFile::ln  endLine() const { return m_endLn; }
  SrcFile::ln& endLine()       { return m_endLn; }
  
  // SetLineRange: 
  void SetLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate = 1);
  
  void ExpandLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate = 1);

  void LinkAndSetLineRange(CodeInfo* parent);

  void checkLineRange(SrcFile::ln begLn, SrcFile::ln endLn)
  {
    DIAG_Assert(Logic::equiv(begLn == ln_NULL, endLn == ln_NULL),
		"CodeInfo::checkLineRange: b=" << begLn << " e=" << endLn);
    DIAG_Assert(begLn <= endLn, 
		"CodeInfo::checkLineRange: b=" << begLn << " e=" << endLn);
    DIAG_Assert(Logic::equiv(m_begLn == ln_NULL, m_endLn == ln_NULL),
		"CodeInfo::checkLineRange: b=" << m_begLn << " e=" << m_endLn);
  }
  
  // A set of *unrelocated* VMAs associated with this scope
  VMAIntervalSet&       vmaSet()       { return m_vmaSet; }
  const VMAIntervalSet& vmaSet() const { return m_vmaSet; }
  
  // containsLine: returns true if this scope contains line number
  //   'ln'.  A non-zero beg_epsilon and end_epsilon allows fuzzy
  //   matches by expanding the interval of the scope.
  //
  // containsInterval: returns true if this scope fully contains the
  //   interval specified by [begLn...endLn].  A non-zero beg_epsilon
  //   and end_epsilon allows fuzzy matches by expanding the interval of
  //   the scope.
  //
  // Note: We assume that it makes no sense to compare against ln_NULL.
  bool containsLine(SrcFile::ln ln) const
    { return (m_begLn != ln_NULL && (m_begLn <= ln && ln <= m_endLn)); }
  bool containsLine(SrcFile::ln ln, int beg_epsilon, int end_epsilon) const;
  bool containsInterval(SrcFile::ln begLn, SrcFile::ln endLn) const
    { return (containsLine(begLn) && containsLine(endLn)); }
  bool containsInterval(SrcFile::ln begLn, SrcFile::ln endLn,
			int beg_epsilon, int end_epsilon) const
    { return (containsLine(begLn, beg_epsilon, end_epsilon) 
	      && containsLine(endLn, beg_epsilon, end_epsilon)); }

  CodeInfo* CodeInfoWithLine(SrcFile::ln ln) const;


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual std::string nameQual() const { return codeName(); }

  // returns a string representing the code name in the form:
  //  loadmodName
  //  [loadmodName]fileName
  //  [loadmodName]<fileName>procName
  //  [loadmodName]<fileName>begLn-endLn

  virtual std::string codeName() const;
  
  std::string LineRange() const;

  virtual ScopeInfo* Clone() { return new CodeInfo(*this); }

  // FIXME: not needed?
  //CodeInfo* GetFirst() const { return first; }
  //CodeInfo* GetLast() const { return last; } 

  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual std::string XMLLineRange(int dmpFlag) const;
  virtual std::string XMLVMAIntervals(int dmpFlag) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
               const char* file_name = NULL, const char* proc_name = NULL,
               int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr,
			       int dmpFlag = 0,
			       const char* pre = "") const;

protected: 
  // NOTE: currently designed for PROCs
  void Relocate();
  
  void RelocateIf() {
    if (Parent() && Type() == ScopeInfo::PROC) {
      Relocate();
    }
  }
  
protected:
  SrcFile::ln m_begLn;
  SrcFile::ln m_endLn;
  VMAIntervalSet m_vmaSet;

  CodeInfo* first; // FIXME: only used in ScopeInfo::CollectCrossReferences...
  CodeInfo* last;  //   what about NonUniformDegreeTree...
  friend void ScopeInfo::CollectCrossReferences();
};


// - if x < y; 0 if x == y; + otherwise
// N.B.: in the case that x == y, break ties using VMAIntervalSet and
// then by name attributes.
int CodeInfoLineComp(const CodeInfo* x, const CodeInfo* y);


//***************************************************************************
// PgmScope, GroupScope, LoadModScope, FileScope, ProcScope, LoopScope,
// StmtRangeScope
//***************************************************************************

// --------------------------------------------------------------------------
// PgmScope is root of the scope tree
// --------------------------------------------------------------------------
class PgmScope: public ScopeInfo {
protected:
  PgmScope(const PgmScope& x) : ScopeInfo(x.type) { *this = x; }
  PgmScope& operator=(const PgmScope& x);

public: 
  PgmScope(const char* nm)
    : ScopeInfo(PGM, NULL) 
  { 
    Ctor(nm);
  }
  
  PgmScope(const std::string& nm)
    : ScopeInfo(PGM, NULL)
  { 
    Ctor(nm.c_str());
  }

  virtual ~PgmScope()
  {
    frozen = false;
    delete groupMap;
    delete lmMap;
  }

  const std::string& name() const { return m_name; }
  void               SetName(const char* n) { m_name = n; }
  void               SetName(const std::string& n) { m_name = n; }

  LoadModScope* FindLoadMod(const char* nm) const // find by 'realpath'
  {
    std::string lmName = RealPath(nm);
    LoadModScopeMap::iterator it = lmMap->find(lmName);
    LoadModScope* x = (it != lmMap->end()) ? it->second : NULL;
    return x;
  }
  
  LoadModScope* FindLoadMod(const std::string& nm) const 
    { return FindLoadMod(nm.c_str()); }

  GroupScope*   FindGroup(const char* nm) const
  {
    GroupScopeMap::iterator it = groupMap->find(nm);
    GroupScope* x = (it != groupMap->end()) ? it->second : NULL;
    return x;
  }

  GroupScope*   FindGroup(const std::string& nm) const
    { return FindGroup(nm.c_str()); }

  void Freeze() { frozen = true;} // disallow additions to/deletions from tree
  bool IsFrozen() const { return frozen; }

  virtual ScopeInfo* Clone() { return new PgmScope(*this); }

  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void XML_DumpLineSorted(std::ostream& os = std::cout, 
				  int dmpFlag = 0, 
				  const char* pre = "") const;
  void CSV_TreeDump(std::ostream& os = std::cout) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;
   
protected: 
private: 
  void Ctor(const char* nm);

  void AddToGroupMap(GroupScope* grp);
  void AddToLoadModMap(LoadModScope* lm);
 
  friend class GroupScope;
  friend class LoadModScope;

private:
  bool frozen;
  std::string m_name; // the program name

  GroupScopeMap*     groupMap;
  LoadModScopeMap*   lmMap;     // mapped by 'realpath'
};


// --------------------------------------------------------------------------
// GroupScopes are children of PgmScope's, GroupScope's, LoadModScopes's, 
//   FileScope's, ProcScope's, LoopScope's
// children: GroupScope's, LoadModScope's, FileScope's, ProcScope's,
//   LoopScope's, StmtRangeScopes,
// They may be used to describe several different types of scopes
//   (including user-defined ones)
// --------------------------------------------------------------------------
class GroupScope: public CodeInfo {
public: 
  GroupScope(const char* nm, ScopeInfo* parent,
	     int begLn = ln_NULL, int endLn = ln_NULL)
    : CodeInfo(GROUP, parent, begLn, endLn, 0, 0)
  {
    Ctor(nm, parent);
  }
  
  GroupScope(const std::string& nm, ScopeInfo* parent,
	     int begLn = ln_NULL, int endLn = ln_NULL)
    : CodeInfo(GROUP, parent, begLn, endLn, 0, 0)
  {
    Ctor(nm.c_str(), parent);
  }
  
  virtual ~GroupScope() { }
  
  const std::string& name() const { return m_name; } // same as grpName

  virtual std::string codeName() const;

  virtual ScopeInfo* Clone() { return new GroupScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void Ctor(const char* nm, ScopeInfo* parent);

private: 
  std::string m_name;
};


// --------------------------------------------------------------------------
// LoadModScopes are children of PgmScope's or GroupScope's
// children: GroupScope's, FileScope's
// --------------------------------------------------------------------------
// FIXME: See note about LoadModScope above.
class LoadModScope: public CodeInfo {
protected:
  LoadModScope& operator=(const LoadModScope& x);

public: 
  LoadModScope(const char* nm, ScopeInfo* parent)
    : CodeInfo(LM, parent, ln_NULL, ln_NULL, 0, 0)
  { 
    Ctor(nm, parent);
  }

  LoadModScope(const std::string& nm, ScopeInfo* parent)
    : CodeInfo(LM, parent, ln_NULL, ln_NULL, 0, 0)
  {
    Ctor(nm.c_str(), parent);
  }

  virtual ~LoadModScope()
  {
    delete fileMap;
    delete procMap;
    delete stmtMap;
  }

  virtual std::string BaseName() const  { return BaseFileName(m_name); }

  const std::string& name() const { return m_name; }

  virtual std::string codeName() const { return name(); }

  virtual ScopeInfo* Clone() { return new LoadModScope(*this); }

  FileScope*    FindFile(const char* nm) const    // find by 'realpath'
  {
    std::string fName = RealPath(nm);
    FileScopeMap::iterator it = fileMap->find(fName);
    FileScope* x = (it != fileMap->end()) ? it->second : NULL;
    return x;
  }

  FileScope*    FindFile(const std::string& nm) const
    { return FindFile(nm.c_str()); }

  // find scope by *unrelocated* VMA
  CodeInfo* findByVMA(VMA vma);
  
  ProcScope* findProc(VMA vma) 
  {
    VMAInterval toFind(vma, vma+1); // [vma, vma+1)
    VMAIntervalMap<ProcScope*>::iterator it = procMap->find(toFind);
    return (it != procMap->end()) ? it->second : NULL;
  }

  StmtRangeScope* findStmtRange(VMA vma)
  {
    VMAInterval toFind(vma, vma+1); // [vma, vma+1)
    VMAIntervalMap<StmtRangeScope*>::iterator it = stmtMap->find(toFind);
    return (it != stmtMap->end()) ? it->second : NULL;
  }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void XML_DumpLineSorted(std::ostream& os = std::cout, 
				  int dmpFlag = 0, 
				  const char* pre = "") const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;
  void dumpmaps() const;

  bool verifyStmtMap() const;

public:
  typedef VMAIntervalMap<ProcScope*>      VMAToProcMap;
  typedef VMAIntervalMap<StmtRangeScope*> VMAToStmtRangeMap;

protected: 
  void Ctor(const char* nm, ScopeInfo* parent);

  void AddToFileMap(FileScope* file);

  template<typename T> 
  void buildMap(VMAIntervalMap<T>*& m, ScopeInfo::ScopeType ty) const;

  template<typename T>
  static bool 
  verifyMap(VMAIntervalMap<T>* m, const char* map_nm);

  friend class FileScope;

private:
  std::string m_name; // the load module name

  FileScopeMap*      fileMap;   // mapped by 'realpath'
  VMAToProcMap*      procMap;
  VMAToStmtRangeMap* stmtMap;
};


// --------------------------------------------------------------------------
// FileScopes are children of PgmScope's, GroupScope's and LoadModScope's.
// children: GroupScope's, ProcScope's, LoopScope's, or StmtRangeScope's.
// FileScopes may refer to an unreadable file
// --------------------------------------------------------------------------
class FileScope: public CodeInfo {
protected:
  FileScope(const FileScope& x) : CodeInfo(x.type) { *this = x; }
  FileScope& operator=(const FileScope& x);

public: 
  // fileNameWithPath/parent must not be NULL
  // srcIsReadable == fopen(fileNameWithPath, "r") works 
  FileScope(const char* srcFileWithPath, bool srcIsReadable_, 
	    ScopeInfo *parent, 
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(FILE, parent, begLn, endLn, 0, 0)
  {
    Ctor(srcFileWithPath, srcIsReadable_, parent);
  }
  
  FileScope(const std::string& srcFileWithPath, bool srcIsReadable_, 
	    ScopeInfo *parent, 
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(FILE, parent, begLn, endLn, 0, 0)
  {
    Ctor(srcFileWithPath.c_str(), srcIsReadable_, parent);
  }
  
  virtual ~FileScope()
  {
    delete procMap;
  }


  static FileScope* 
  findOrCreate(LoadModScope* lmScope, const std::string& filenm);


 // fileNameWithPath from constructor 
  const std::string& name() const { return m_name; }

  // FindProc: Attempt to find the procedure within the multimap.  If
  // 'lnm' is provided, require that link names match.
  ProcScope* FindProc(const char* nm, const char* lnm = NULL) const;
  ProcScope* FindProc(const std::string& nm, const std::string& lnm = "") const
    { 
      const char* x = lnm.c_str();
      return FindProc(nm.c_str(), (x[0] == '\0') ? NULL : x);
    }
  
                                        
  void SetName(const char* fname) { m_name = fname; }
  void SetName(const std::string& fname) { m_name = fname; }
    
  virtual std::string BaseName() const  { return BaseFileName(m_name); }
  virtual std::string codeName() const;

  bool HasSourceFile() const { return srcIsReadable; } // srcIsReadable

  virtual ScopeInfo* Clone() { return new FileScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private: 
  void Ctor(const char* srcFileWithPath, bool srcIsReadble_, 
	    ScopeInfo* parent);

  void AddToProcMap(ProcScope* proc);
  friend class ProcScope;

private:
  bool srcIsReadable;
  std::string m_name; // the file name including the path 
  ProcScopeMap* procMap;
};


// --------------------------------------------------------------------------
// ProcScopes are children of GroupScope's or FileScope's
// children: GroupScope's, LoopScope's, StmtRangeScope's
// 
//   (begLn == 0) <==> (endLn == 0)
//   (begLn != 0) <==> (endLn != 0)
// --------------------------------------------------------------------------
class ProcScope: public CodeInfo {
protected:
  ProcScope(const ProcScope& x) : CodeInfo(x.type) { *this = x; }
  ProcScope& operator=(const ProcScope& x);

public: 
  ProcScope(const char* name, CodeInfo* parent, 
	    const char* linkname, bool hasSym,
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(PROC, parent, begLn, endLn, 0, 0)
  {
    Ctor(name, parent, linkname, hasSym);
  }
  
  ProcScope(const std::string& name, CodeInfo* parent,
	    const std::string& linkname, bool hasSym,
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(PROC, parent, begLn, endLn, 0, 0)
  {
    Ctor(name.c_str(), parent, linkname.c_str(), hasSym);
  }

  virtual ~ProcScope()
  {
    delete stmtMap;
  }

  bool hasSymbolic() const { return m_hasSym; }
  
  static ProcScope*
  findOrCreate(FileScope* fScope, const std::string& procnm, SrcFile::ln line);
  
  
  virtual const std::string& name() const     { return m_name; }
  virtual const std::string& LinkName() const { return m_linkname; }

  virtual       std::string codeName() const;

  virtual ScopeInfo* Clone() { return new ProcScope(*this); }

  StmtRangeScope* 
  FindStmtRange(SrcFile::ln begLn)
  {
    StmtRangeScopeMap::iterator it = stmtMap->find(begLn);
    StmtRangeScope* x = (it != stmtMap->end()) ? it->second : NULL;
    return x;
  }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void Ctor(const char* n, CodeInfo* parent, const char* ln, bool hasSym);

  void AddToStmtMap(StmtRangeScope* stmt);

  friend class StmtRangeScope;

private:
  std::string m_name;
  std::string m_linkname;
  bool        m_hasSym;
  StmtRangeScopeMap* stmtMap;
};


// --------------------------------------------------------------------------
// AlienScope: represents an alien context (e.g. inlined procedure, macro).
//
// AlienScopes are children of GroupScope's, AlienScope's, ProcScope's
//   or LoopScope's
// children: GroupScope's, AlienScope's, LoopScope's, StmtRangeScope's
// 
//   (begLn == 0) <==> (endLn == 0)
//   (begLn != 0) <==> (endLn != 0)
// --------------------------------------------------------------------------
class AlienScope: public CodeInfo {
protected:
  AlienScope(const AlienScope& x) : CodeInfo(x.type) { *this = x; }
  AlienScope& operator=(const AlienScope& x);

public: 
  AlienScope(CodeInfo* parent, const char* filenm, const char* procnm,
	     SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(ALIEN, parent, begLn, endLn, 0, 0)
  {
    Ctor(parent, filenm, procnm);
  }

  AlienScope(CodeInfo* parent, 
	     const std::string& filenm, const std::string& procnm,
	     SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(ALIEN, parent, begLn, endLn, 0, 0)
  {
    Ctor(parent, filenm.c_str(), procnm.c_str());
  }

  virtual ~AlienScope()
  { }
  
  const std::string& fileName() const { return m_filenm; }
  void fileName(const std::string& fnm) { m_filenm = fnm; }

  virtual const std::string& name() const { return m_name; }
  
  virtual       std::string codeName() const;

  virtual ScopeInfo* Clone() { return new AlienScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void Ctor(CodeInfo* parent, const char* filenm, const char* procnm);

private:
  std::string m_filenm;
  std::string m_name;
};


// --------------------------------------------------------------------------
// LoopScopes are children of GroupScope's, FileScope's, ProcScope's,
//   or LoopScope's.
// children: GroupScope's, LoopScope's, or StmtRangeScope's
// --------------------------------------------------------------------------
class LoopScope: public CodeInfo {
public: 
  LoopScope(CodeInfo* parent, 
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : CodeInfo(LOOP, parent, begLn, endLn, 0, 0)
  {
    ScopeType t = (parent) ? parent->Type() : ANY;
    DIAG_Assert((parent == NULL) || (t == GROUP) || (t == FILE) || (t == PROC) 
		|| (t == ALIEN) || (t == LOOP), "");
  }

  virtual ~LoopScope()
  { }
  
  virtual std::string codeName() const;

  virtual ScopeInfo* Clone() { return new LoopScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

};


// --------------------------------------------------------------------------
// StmtRangeScopes are children of GroupScope's, FileScope's,
//   ProcScope's, or LoopScope's.
// children: none
// --------------------------------------------------------------------------
class StmtRangeScope: public CodeInfo {
public: 
  StmtRangeScope(CodeInfo* parent, SrcFile::ln begLn, SrcFile::ln endLn,
		 VMA begVMA = 0, VMA endVMA = 0)
    : CodeInfo(STMT_RANGE, parent, begLn, endLn, begVMA, endVMA),
      m_sortId((int)begLn)
  {
    ScopeType t = (parent) ? parent->Type() : ANY;
    DIAG_Assert((parent == NULL) || (t == GROUP) || (t == FILE) || (t == PROC)
		|| (t == ALIEN) || (t == LOOP), "");
    ProcScope* p = Proc();
    if (p) { 
      p->AddToStmtMap(this); 
      //DIAG_DevIf(0) { LoadMod()->verifyStmtMap(); }
    }
    //DIAG_DevMsg(3, "StmtRangeScope::StmtRangeScope: " << toString_me());
  }

  virtual ~StmtRangeScope()
  { }
  
  virtual std::string codeName() const;

  virtual ScopeInfo* Clone() { return new StmtRangeScope(*this); }

  // a handle for sorting within the enclosing procedure context
  int  sortId() { return m_sortId; }
  void sortId(int x) { m_sortId = x; }

  virtual std::string toXML(int dmpFlag = 0) const;  

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  int m_sortId;
};

// ----------------------------------------------------------------------
// RefScopes are chldren of LineScopes 
// They are used to describe a reference in source code.
// RefScopes are build only iff we have preprocessing information.
// ----------------------------------------------------------------------
class RefScope: public CodeInfo {
public: 
  RefScope(CodeInfo* parent, int _begPos, int _endPos, const char* refName);
  // parent->Type() == STMT_RANGE_SCOPE 
  
  uint BegPos() const { return begPos; };
  uint EndPos() const   { return endPos; };
  
  virtual const std::string& name() const { return m_name; }

  virtual std::string codeName() const;

  virtual ScopeInfo* Clone() { return new RefScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private: 
  void RelocateRef();
  uint begPos;
  uint endPos;
  std::string m_name;
};

/************************************************************************/
// Iterators
/************************************************************************/

#include "PgmScopeTreeIterator.hpp" 

/************************************************************************/
// testing 
/************************************************************************/
extern void ScopeInfoTester(int argc, const char** argv);

#endif /* prof_juicy_PgmScopeTree */
