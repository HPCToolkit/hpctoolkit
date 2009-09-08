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
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef prof_juicy_Prof_Struct_Tree_hpp 
#define prof_juicy_Prof_Struct_Tree_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
#include <list>
#include <set>
#include <map>

#include <typeinfo>
 
//*************************** User Include Files ****************************

#include <include/uint.h>

#include "PerfMetric.hpp"

#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/Unique.hpp>
#include <lib/support/VectorTmpl.hpp>

//*************************** Forward Declarations **************************

namespace Prof {
namespace Struct {

  class ANode;

  // Some possibly useful containers
  typedef std::list<ANode*> ANodeList;
  typedef std::set<ANode*> ANodeSet;

} // namespace Struct
} // namespace Prof


//*************************** Forward Declarations **************************

class DoubleVector : public VectorTmpl<double> {
public: 
  DoubleVector() 
    : VectorTmpl<double>(0.0, true/*autoExtend*/, 16/*size*/) { }
};


namespace Prof {
namespace Struct {

// FIXME: move these into their respective classes...
class Group;
class GroupMap : public std::map<std::string, Group*> { };

class LM;
class LMMap : public std::map<std::string, LM*> { };

// ProcMap: This is is a multimap because procedure names are
// sometimes "generic", i.e. not qualified by types in the case of
// templates, resulting in duplicate names
class Proc;
class ProcMap : public std::multimap<std::string, Proc*> { };

class File;
class FileMap : public std::map<std::string, File*> { };

class Stmt;
class StmtMap : public std::map<SrcFile::ln, Stmt*> { };

} // namespace Struct
} // namespace Prof


//***************************************************************************
// Tree
//***************************************************************************

namespace Prof {
namespace Struct {

class Root;

class Tree : public Unique {
public:

  enum {
    // Output flags
    OFlg_Compressed      = (1 << 1), // Write in compressed format
    OFlg_LeafMetricsOnly = (1 << 2), // Write metrics only at leaves
    OFlg_Debug           = (1 << 3), // Debug: show xtra source line info
  };

  static const std::string UnknownFileNm;
  static const std::string UnknownProcNm;
  
public:
  // -------------------------------------------------------
  // Create/Destroy
  // -------------------------------------------------------
  Tree(const char* name, Root* root = NULL);

  Tree(const std::string& name, Root* root = NULL)
  { Tree(name.c_str(), root); }

  virtual ~Tree();


  // -------------------------------------------------------
  // Tree data
  // -------------------------------------------------------
  Root*
  root() const
  { return m_root; }
  
  void
  root(Root* x)
  { m_root = x; }

  bool
  empty() const
  { return (m_root == NULL); }
  
  std::string
  name() const;


  // -------------------------------------------------------
  // Write contents
  // -------------------------------------------------------
  std::ostream& 
  writeXML(std::ostream& os = std::cerr, int oFlags = 0) const;

  // Dump contents for inspection (use flags from ANode)
  std::ostream& 
  dump(std::ostream& os = std::cerr, int oFlags = 0) const;
  
  void 
  ddump() const;
 
private:
  Root* m_root;
};

} // namespace Struct
} // namespace Prof


namespace Prof {
namespace Struct {

//***************************************************************************
// ANode, ACodeNode.
//***************************************************************************

// FIXME: It would make more sense for Group and LM to
// simply be ANodes and not ACodeNodes, but the assumption that
// *only* a Root is not a ACodeNode is deeply embedded and would
// take a while to untangle.

class ANode;     // Base class for all nodes
class ACodeNode; // Base class for everyone but Root

class Root;
class Group;
class LM;
class File;
class Proc;
class Alien;
class Loop;
class Stmt;
class Ref;

// ---------------------------------------------------------
// ANode: The base node for a program scope tree
// ---------------------------------------------------------
class ANode: public NonUniformDegreeTreeNode {
public:
  enum ANodeTy {
    TyRoot = 0,
    TyGroup,
    TyLM,
    TyFile,
    TyProc,
    TyAlien,
    TyLoop,
    TyStmt,
    TyRef,
    TyANY,
    TyNUMBER
  };

  static const std::string&
  ANodeTyToName(ANodeTy tp);

  static const std::string&
  ANodeTyToXMLelement(ANodeTy tp);

  static ANodeTy
  IntToANodeTy(long i);

protected:
  ANode(const ANode& x)
  { *this = x; }
  
  ANode&
  operator=(const ANode& x);

private:
  static const std::string ScopeNames[TyNUMBER];

public:
  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  ANode(ANodeTy ty, ANode* parent = NULL)
    : NonUniformDegreeTreeNode(parent), m_type(ty)
  { 
    m_id = s_nextUniqueId++;
    m_metrics = new DoubleVector();
  }

  virtual ~ANode()
  {
    DIAG_DevMsgIf(0, "~ANode::ANode: " << toString_id()
		  << " " << std::hex << this << std::dec);
    delete m_metrics;
  }
  
  // clone: return a shallow copy, unlinked from the tree
  virtual ANode* 
  clone()
  { return new ANode(*this); }


  // --------------------------------------------------------
  // General data
  // --------------------------------------------------------
  ANodeTy
  type() const
  { return m_type; }

  // id: a unique id; 0 is reserved for a NULL value
  uint
  id() const
  { return m_id; }

  // maxId: the maximum id of all structure nodes
  static uint
  maxId()
  { return s_nextUniqueId - 1; }

  // name: 
  // nameQual: qualified name [built dynamically]
  virtual const std::string&
  name() const
  { return ANodeTyToName(type()); }

  virtual std::string
  nameQual() const
  { return name(); }


  // --------------------------------------------------------
  // Tree navigation 
  // --------------------------------------------------------
  ANode*
  parent() const 
  { return static_cast<ANode*>(NonUniformDegreeTreeNode::Parent()); }

  ANode*
  firstChild() const
  { return static_cast<ANode*>(NonUniformDegreeTreeNode::FirstChild()); }

  ANode*
  lastChild() const
  { return static_cast<ANode*>(NonUniformDegreeTreeNode::LastChild()); }

  ANode*
  nextSibling() const
  {
    // siblings are linked in a circular list
    if ((parent()->lastChild() != this)) {
      return static_cast<ANode*>(NonUniformDegreeTreeNode::NextSibling()); 
    }
    return NULL;  
  }
  
  ANode*
  prevSibling() const
  { 
    // siblings are linked in a circular list
    if ((parent()->firstChild() != this)) {
      return static_cast<ANode*>(NonUniformDegreeTreeNode::PrevSibling()); 
    }
    return NULL;
  }
  

  // --------------------------------------------------------
  // ancestor: find first ANode in path from this to root with given type
  // (Note: We assume that a node *can* be an ancestor of itself.)
  // --------------------------------------------------------
  ANode*
  ancestor(ANodeTy type) const;

  ANode*
  ancestor(ANodeTy ty1, ANodeTy ty2) const;

  ANode*
  ancestor(ANodeTy ty1, ANodeTy ty2, ANodeTy ty3) const;

  Root*
  ancestorRoot() const;

  Group*
  ancestorGroup() const;

  LM*
  ancestorLM() const;

  File*
  ancestorFile() const;

  Proc*
  ancestorProc() const;

  Alien*
  ancestorAlien() const;

  Loop*
  ancestorLoop() const;

  Stmt*
  ancestorStmt() const;

  ACodeNode*
  ancestorProcCtxt() const; // return ancestor(TyAlien|TyProc)

  ACodeNode*
  ACodeNodeParent() const;


  // --------------------------------------------------------
  // Metrics
  // --------------------------------------------------------

  bool
  hasMetrics() const
  {
    uint end = numMetrics();
    for (uint i = 0; i < end; ++i) {
      if (hasMetric(i)) {
	return true;
      }
    }
    return false;
  }

  bool
  hasMetric(int mId) const
  {
    double x = (*m_metrics)[mId];
    return (x != 0.0);
  }
  
  double
  metric(int mId) const
  {
    return (*m_metrics)[mId];
  }

  void
  metricIncr(int mId, double d)
  {
    // NOTE: VectorTmpl::operator[] automatically 'adds' the index if necessary
    (*m_metrics)[mId] += d;
  }

  uint
  numMetrics() const
  {
    return m_metrics->GetNumElements();
  }

  // accumulates metrics from children. [mBegId, mEndId] forms an
  // inclusive interval for batch processing.  In particular, 'raw'
  // metrics are independent of all other raw metrics.
  void
  accumulateMetrics(uint mBegId, uint mEndId)
  {
    // NOTE: Must not call hasMetrics() since this node may not have
    // any metric data yet!
    double* valVec = new double[mEndId + 1];
    accumulateMetrics(mBegId, mEndId, valVec);
    delete[] valVec;
  }

  void
  accumulateMetrics(uint mBegId)
  {
    accumulateMetrics(mBegId, mBegId);
  }

  // traverses the tree and removes all nodes for which hasMetrics() is false
  void
  pruneByMetrics();


private:
  void
  accumulateMetrics(uint mBegId, uint mEndId, double* valVec);

public:

  // --------------------------------------------------------
  // Paths and Merging
  // --------------------------------------------------------

  // leastCommonAncestor: Given two ANode nodes, return the least
  // common ancestor (deepest nested common ancestor) or NULL.
  static ANode*
  leastCommonAncestor(ANode* n1, ANode* n2);


  // distance: Given two ANode nodes, a node and some ancestor,
  // return the distance of the path between the two.  The distance
  // between a node and its direct ancestor is 1.  If there is no path
  // between the two nodes, returns a negative number; if the two
  // nodes are equal, returns 0.
  static int
  distance(ANode* ancestor, ANode* descendent);

  // arePathsOverlapping: Given two nodes and their least common
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
  static bool
  arePathsOverlapping(ANode* lca, ANode* desc1, ANode* desc2);
  
  // MergePaths: Given divergent paths (as defined above), merges the path
  // from 'toDesc' into 'fromDesc'. If a merge takes place returns true.
  static bool
  mergePaths(ANode* lca, ANode* toDesc, ANode* fromDesc);
  
  // Merge: Given two nodes, 'fromNode' and 'toNode', merges the
  // former into the latter, if possible.  If the merge takes place,
  // deletes 'fromNode' and returns true; otherwise returns false.
  static bool
  merge(ANode* toNode, ANode* fromNode);

  // IsMergable: Returns whether 'fromNode' is capable of being merged
  // into 'toNode'
  static bool
  isMergable(ANode* toNode, ANode* fromNode);
  
  
  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  std::string
  toStringXML(int oFlags = 0, const char* pre = "") const;
  
  virtual std::string
  toXML(int oFlags = 0) const;

  virtual std::ostream&
  writeXML(std::ostream& os = std::cout, int oFlags = 0,
	   const char* pre = "") const;

  std::ostream&
  writeMetricsXML(std::ostream& os = std::cout, int oFlags = 0, 
		  const char* prefix = "") const;

  void
  ddumpXML() const;

  // --------------------------------------------------------
  // Other output
  // --------------------------------------------------------

  void
  CSV_DumpSelf(const Root &root, std::ostream& os = std::cout) const;

  virtual void
  CSV_dump(const Root &root, std::ostream& os = std::cout, 
	   const char* file_name = NULL, 
	   const char* proc_name = NULL,
	   int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::string
  Types() const; // instance's base and derived types 

  std::string
  toString(int oFlags = 0, const char* pre = "") const;

  std::string
  toString_id(int oFlags = 0) const;

  std::string
  toString_me(int oFlags = 0, const char* pre = "") const;

  // dump
  std::ostream&
  dump(std::ostream& os = std::cerr, int oFlags = 0, 
       const char* pre = "") const;
  
  void ddump() const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

protected:
  bool
  writeXML_pre(std::ostream& os = std::cout, int oFlags = 0,
	       const char* prefix = "") const;

  void
  writeXML_post(std::ostream& os = std::cout, int oFlags = 0,
		const char* prefix = "") const;
private:
  void ctorCheck() const;
  void dtorCheck() const;

  static uint s_nextUniqueId;
  
protected:
  ANodeTy m_type; // obsolete with typeid(), but hard to replace
  uint m_id;
  DoubleVector* m_metrics;
};


// --------------------------------------------------------------------------
// ACodeNode is a base class for all scopes other than TyRoot and TyLM.
// Describes some kind of code, i.e. Files, Procedures, Loops...
// --------------------------------------------------------------------------
class ACodeNode : public ANode {
protected: 
  ACodeNode(ANodeTy t, ANode* parent = NULL, 
	   SrcFile::ln begLn = ln_NULL,
	   SrcFile::ln endLn = ln_NULL,
	   VMA begVMA = 0, VMA endVMA = 0)
    : ANode(t, parent), m_begLn(ln_NULL), m_endLn(ln_NULL)
  { 
    setLineRange(begLn, endLn);
    if (begVMA != 0 && endVMA != 0) {
      m_vmaSet.insert(begVMA, endVMA);
    }
  }


  ACodeNode(const ACodeNode& x) 
    : ANode(x.m_type) 
  { *this = x; }


  ACodeNode&
  operator=(const ACodeNode& x)
  {
    // shallow copy
    if (&x != this) {
      ANode::operator=(x);
      m_begLn = x.m_begLn;
      m_endLn = x.m_endLn;
      // m_vmaSet
    }
    return *this;
  }

public: 
  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------

  virtual ~ACodeNode()
  { }

  virtual ANode*
  clone() 
  { return new ACodeNode(*this); }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  // Line range in source code
  SrcFile::ln
  begLine() const
  { return m_begLn; }

  void
  begLine(SrcFile::ln x)
  { m_begLn = x; }

  SrcFile::ln
  endLine() const
  { return m_endLn; }

  void
  endLine(SrcFile::ln x)
  { m_endLn = x; }
  
  void
  setLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate = 1);
  
  void
  expandLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate = 1);

  void
  linkAndSetLineRange(ACodeNode* parent);

  void
  checkLineRange(SrcFile::ln begLn, SrcFile::ln endLn)
  {
    DIAG_Assert(Logic::equiv(begLn == ln_NULL, endLn == ln_NULL),
		"ACodeNode::checkLineRange: b=" << begLn << " e=" << endLn);
    DIAG_Assert(begLn <= endLn, 
		"ACodeNode::checkLineRange: b=" << begLn << " e=" << endLn);
    DIAG_Assert(Logic::equiv(m_begLn == ln_NULL, m_endLn == ln_NULL),
		"ACodeNode::checkLineRange: b=" << m_begLn << " e=" << m_endLn);
  }
  
  // -------------------------------------------------------
  // A set of *unrelocated* VMAs associated with this scope
  // -------------------------------------------------------

  const VMAIntervalSet&
  vmaSet() const
  { return m_vmaSet; }

  VMAIntervalSet&
  vmaSet()
  { return m_vmaSet; }

  
  // -------------------------------------------------------
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
  // -------------------------------------------------------
  bool
  containsLine(SrcFile::ln ln) const
  { return (m_begLn != ln_NULL && (m_begLn <= ln && ln <= m_endLn)); }

  bool
  containsLine(SrcFile::ln ln, int beg_epsilon, int end_epsilon) const;

  bool
  containsInterval(SrcFile::ln begLn, SrcFile::ln endLn) const
  { return (containsLine(begLn) && containsLine(endLn)); }

  bool
  containsInterval(SrcFile::ln begLn, SrcFile::ln endLn,
		   int beg_epsilon, int end_epsilon) const
  { return (containsLine(begLn, beg_epsilon, end_epsilon) 
	    && containsLine(endLn, beg_epsilon, end_epsilon)); }

  ACodeNode*
  ACodeNodeWithLine(SrcFile::ln ln) const;


  // compare: Return negative if x < y; 0 if x == y; positive
  //   otherwise.  If x == y, break ties using VMAIntervalSet and then
  //   by name attributes.
  static int 
  compare(const ACodeNode* x, const ACodeNode* y);

  ACodeNode*
  nextSiblingNonOverlapping() const;


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual std::string
  nameQual() const { return codeName(); }

  // returns a string representing the code name in the form:
  //  loadmodName
  //  [loadmodName]fileName
  //  [loadmodName]<fileName>procName
  //  [loadmodName]<fileName>begLn-endLn

  virtual std::string
  codeName() const;
  
  std::string
  lineRange() const;


  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  virtual std::string
  toXML(int oFlags = 0) const;

  virtual std::string
  XMLLineRange(int oFlags) const;

  virtual std::string
  XMLVMAIntervals(int oFlags) const;

  virtual void
  CSV_dump(const Root &root, std::ostream& os = std::cout, 
	   const char* file_name = NULL, const char* proc_name = NULL,
	   int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

protected: 
  // NOTE: currently designed for PROCs
  void
  relocate();
  
  void
  relocateIf() 
  {
    if (parent() && type() == ANode::TyProc) { 
                 // typeid(*this) == typeid(ANode::Proc)
      relocate();
    }
  }

  std::string codeName_LM_F() const;
  
protected:
  SrcFile::ln m_begLn;
  SrcFile::ln m_endLn;
  VMAIntervalSet m_vmaSet;
};



//***************************************************************************
// Root, Group, LM, File, Proc, Loop,
// Stmt
//***************************************************************************

// --------------------------------------------------------------------------
// Root is root of the scope tree
// --------------------------------------------------------------------------
class Root: public ANode {
protected:
  Root(const Root& x) 
    : ANode(x.m_type)
  { 
    *this = x; 
  }
  
  Root& operator=(const Root& x);

public: 
  Root(const char* nm)
    : ANode(TyRoot, NULL)
  { 
    Ctor(nm);
  }
  
  Root(const std::string& nm)
    : ANode(TyRoot, NULL)
  { 
    Ctor(nm.c_str());
  }

  virtual ~Root()
  {
    delete groupMap;
    delete lmMap;
  }

  virtual const std::string& name() const { return m_name; }
  void name(const char* n) { m_name = n; }
  void name(const std::string& n) { m_name = n; }

  // --------------------------------------------------------  
  //
  // --------------------------------------------------------

  // find by RealPathMgr
  LM*
  findLM(const char* nm) const;

  LM*
  findLM(const std::string& nm) const 
  { return findLM(nm.c_str()); }

  Group*
  findGroup(const char* nm) const
  {
    GroupMap::iterator it = groupMap->find(nm);
    Group* x = (it != groupMap->end()) ? it->second : NULL;
    return x;
  }

  Group*
  findGroup(const std::string& nm) const
  { return findGroup(nm.c_str()); }

  virtual ANode*
  clone() 
  { return new Root(*this); }


  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  virtual std::string
  toXML(int oFlags = 0) const;

  virtual std::ostream&
  writeXML(std::ostream& os = std::cout, int oFlags = 0, 
	   const char* pre = "") const;

  void
  CSV_TreeDump(std::ostream& os = std::cout) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;
   
protected: 
private: 
  void
  Ctor(const char* nm);

  void
  AddToGroupMap(Group* grp);

  void
  AddToLoadModMap(LM* lm);
 
  friend class Group;
  friend class LM;

private:
  std::string m_name; // the program name

  GroupMap* groupMap;
  LMMap*    lmMap; // mapped by 'realpath'

  static RealPathMgr& s_realpathMgr;
};


// --------------------------------------------------------------------------
// Groups are children of Root's, Group's, LMs's, 
//   File's, Proc's, Loop's
// children: Group's, LM's, File's, Proc's,
//   Loop's, Stmts,
// They may be used to describe several different types of scopes
//   (including user-defined ones)
// --------------------------------------------------------------------------
class Group: public ACodeNode {
public:
  Group(const char* nm, ANode* parent,
	int begLn = ln_NULL, int endLn = ln_NULL)
    : ACodeNode(TyGroup, parent, begLn, endLn, 0, 0)
  {
    Ctor(nm, parent);
  }
  
  Group(const std::string& nm, ANode* parent,
	     int begLn = ln_NULL, int endLn = ln_NULL)
    : ACodeNode(TyGroup, parent, begLn, endLn, 0, 0)
  {
    Ctor(nm.c_str(), parent);
  }
  
  virtual ~Group() { }
  
  static Group* 
  demand(Root* pgm, const std::string& nm, ANode* parent);

  virtual const std::string&
  name() const { return m_name; } // same as grpName

  virtual std::string
  codeName() const;

  virtual ANode* 
  clone() 
  { return new Group(*this); }

  virtual std::string
  toXML(int oFlags = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

private:
  void
  Ctor(const char* nm, ANode* parent);

private: 
  std::string m_name;
};


// --------------------------------------------------------------------------
// LMs are children of Root's or Group's
// children: Group's, File's
// --------------------------------------------------------------------------
class LM: public ACodeNode {
protected:
  LM& operator=(const LM& x);

public: 

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  LM(const char* nm, ANode* parent)
    : ACodeNode(TyLM, parent, ln_NULL, ln_NULL, 0, 0)
  { 
    Ctor(nm, parent);
  }

  LM(const std::string& nm, ANode* parent)
    : ACodeNode(TyLM, parent, ln_NULL, ln_NULL, 0, 0)
  {
    Ctor(nm.c_str(), parent);
  }

  virtual ~LM()
  {
    delete m_fileMap;
    delete m_procMap;
    delete m_stmtMap;
  }

  virtual ANode*
  clone() 
  { return new LM(*this); }

  static LM*
  demand(Root* pgm, const std::string& lm_fnm);


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual const std::string&
  name() const
  { return m_name; }

  virtual std::string
  codeName() const
  { return name(); }

  std::string
  baseName() const
  { return FileUtil::basename(m_name); }


  // --------------------------------------------------------
  // search for enclosing nodes
  // --------------------------------------------------------

  // findFile: find using RealPathMgr
  File*
  findFile(const char* nm) const;

  File*
  findFile(const std::string& nm) const
  { return findFile(nm.c_str()); }

  // findByVMA: find scope by *unrelocated* VMA
  // findProc:
  // findStmt
  ACodeNode*
  findByVMA(VMA vma);
  
  Proc*
  findProc(VMA vma)
  {
    if (!m_procMap) {
      buildMap(m_procMap, ANode::TyProc);
    }
    VMAInterval toFind(vma, vma+1); // [vma, vma+1)
    VMAIntervalMap<Proc*>::iterator it = m_procMap->find(toFind);
    return (it != m_procMap->end()) ? it->second : NULL;
  }

  Stmt*
  findStmt(VMA vma)
  {
    if (!m_stmtMap) {
      buildMap(m_stmtMap, ANode::TyStmt);
    }
    VMAInterval toFind(vma, vma+1); // [vma, vma+1)
    VMAIntervalMap<Stmt*>::iterator it = m_stmtMap->find(toFind);
    return (it != m_stmtMap->end()) ? it->second : NULL;
  }


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(int oFlags = 0) const;

  virtual std::ostream&
  writeXML(std::ostream& os = std::cout, int oFlags = 0, 
	   const char* pre = "") const;


  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

  void
  dumpmaps() const;

  bool
  verifyStmtMap() const;

public:
  typedef VMAIntervalMap<Proc*> VMAToProcMap;
  typedef VMAIntervalMap<Stmt*> VMAToStmtRangeMap;

protected: 
  void
  Ctor(const char* nm, ANode* parent);

  void
  AddToFileMap(File* file);

  template<typename T> 
  void
  buildMap(VMAIntervalMap<T>*& m, ANode::ANodeTy ty) const;

  template<typename T>
  static bool 
  verifyMap(VMAIntervalMap<T>* m, const char* map_nm);

  friend class File;

private:
  std::string m_name; // the load module name

  FileMap*           m_fileMap; // mapped by RealPathMgr
  VMAToProcMap*      m_procMap;
  VMAToStmtRangeMap* m_stmtMap;

  static RealPathMgr& s_realpathMgr;
};


// --------------------------------------------------------------------------
// Files are children of Root's, Group's and LM's.
// children: Group's, Proc's, Loop's, or Stmt's.
// Files may refer to an unreadable file
// --------------------------------------------------------------------------
class File: public ACodeNode {
protected:
  File(const File& x) 
    : ACodeNode(x.m_type) 
  { *this = x; }

  File&
  operator=(const File& x);

public: 

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  File(const char* filenm, ANode *parent, 
       SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyFile, parent, begLn, endLn, 0, 0)
  {
    Ctor(filenm, parent);
  }
  
  File(const std::string& filenm, ANode *parent, 
       SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyFile, parent, begLn, endLn, 0, 0)
  {
    Ctor(filenm.c_str(), parent);
  }
  
  virtual ~File()
  {
    delete m_procMap;
  }

  virtual ANode* 
  clone() 
  { return new File(*this); }

  static File* 
  demand(LM* lm, const std::string& filenm);


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual const std::string&
  name() const
  { return m_name; }

  virtual std::string
  codeName() const;


  void
  name(const char* fname) { m_name = fname; }

  void
  name(const std::string& fname) { m_name = fname; }

  std::string
  baseName() const
  { return FileUtil::basename(m_name); }


  // --------------------------------------------------------
  // search for enclosing nodes
  // --------------------------------------------------------

  // FindProc: Attempt to find the procedure within the multimap.  If
  // 'lnm' is provided, require that link names match.
  Proc*
  findProc(const char* name, const char* linkname = NULL) const;

  Proc*
  findProc(const std::string& name, const std::string& linkname = "") const
  { return findProc(name.c_str(), linkname.c_str()); }

                                           
  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string 
  toXML(int oFlags = 0) const;

  virtual void 
  CSV_dump(const Root &root, std::ostream& os = std::cout, 
	   const char* file_name = NULL, const char* proc_name = NULL,
	   int lLevel = 0) const;
  
  virtual std::ostream& 
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;
  
private: 
  void
  Ctor(const char* filenm, ANode* parent);

  void
  AddToProcMap(Proc* proc);

  friend class Proc;

private:
  std::string m_name; // the file name including the path 
  ProcMap*    m_procMap;

  static RealPathMgr& s_realpathMgr;
};


// --------------------------------------------------------------------------
// Procs are children of Group's or File's
// children: Group's, Loop's, Stmt's
// 
//   (begLn == 0) <==> (endLn == 0)
//   (begLn != 0) <==> (endLn != 0)
// --------------------------------------------------------------------------
class Proc: public ACodeNode {
protected:
  Proc(const Proc& x) : ACodeNode(x.m_type) { *this = x; }
  Proc& operator=(const Proc& x);

public: 

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  Proc(const char* name, ACodeNode* parent, const char* linkname, bool hasSym,
       SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyProc, parent, begLn, endLn, 0, 0)
  {
    Ctor(name, parent, linkname, hasSym);
  }
  
  Proc(const std::string& name, ACodeNode* parent, const std::string& linkname,
       bool hasSym, SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyProc, parent, begLn, endLn, 0, 0)
  {
    Ctor(name.c_str(), parent, linkname.c_str(), hasSym);
  }

  virtual ~Proc()
  {
    delete m_stmtMap;
  }

  virtual ANode* 
  clone() 
  { return new Proc(*this); }

  // demand: if didCreate is non-NULL, it will be set to true if a new
  //   Proc was created and false otherwise.
  // Note: currently sets hasSymbolic() to false on creation
  static Proc*
  demand(File* file, const std::string& name, const std::string& linkname,
	 SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL, 
	 bool* didCreate = NULL);

  static Proc*
  demand(File* file, const std::string& name)
  { return demand(file, name, "", ln_NULL, ln_NULL, NULL); }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual const std::string&
  name() const
  { return m_name; }

  virtual std::string
  codeName() const;


  void
  name(const char* n)
  { m_name = n; }

  void
  name(const std::string& n)
  { m_name = n; }

  const std::string&
  linkName() const
  { return m_linkname; }

  bool
  hasSymbolic() const
  { return m_hasSym; }
  
  void
  hasSymbolic(bool x)
  { m_hasSym = x; }


  // --------------------------------------------------------
  // search for enclosing nodes
  // --------------------------------------------------------

  // FIXME: confusion between native and alien statements
  Stmt*
  findStmt(SrcFile::ln begLn)
  {
    StmtMap::iterator it = m_stmtMap->find(begLn);
    Stmt* x = (it != m_stmtMap->end()) ? it->second : NULL;
    return x;
  }


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(int oFlags = 0) const;

  virtual void
  CSV_dump(const Root &root, std::ostream& os = std::cout, 
	   const char* file_name = NULL, const char* proc_name = NULL,
	   int lLevel = 0) const;
  
  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;
  
private:
  void
  Ctor(const char* n, ACodeNode* parent, const char* ln, bool hasSym);

  void
  AddToStmtMap(Stmt* stmt);

  friend class Stmt;

private:
  std::string m_name;
  std::string m_linkname;
  bool m_hasSym;
  StmtMap* m_stmtMap;
};


// --------------------------------------------------------------------------
// Alien: represents an alien context (e.g. inlined procedure, macro).
//
// Aliens are children of Group's, Alien's, Proc's
//   or Loop's
// children: Group's, Alien's, Loop's, Stmt's
// 
//   (begLn == 0) <==> (endLn == 0)
//   (begLn != 0) <==> (endLn != 0)
// --------------------------------------------------------------------------
class Alien: public ACodeNode {
protected:
  Alien(const Alien& x) : ACodeNode(x.m_type) { *this = x; }
  Alien& operator=(const Alien& x);

public: 

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  Alien(ACodeNode* parent, const char* filenm, const char* procnm,
	     SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyAlien, parent, begLn, endLn, 0, 0)
  {
    Ctor(parent, filenm, procnm);
  }

  Alien(ACodeNode* parent, 
	     const std::string& filenm, const std::string& procnm,
	     SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyAlien, parent, begLn, endLn, 0, 0)
  {
    Ctor(parent, filenm.c_str(), procnm.c_str());
  }

  virtual ~Alien()
  { }

  virtual ANode*
  clone()
  { return new Alien(*this); }

  
  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  const std::string&
  fileName() const
  { return m_filenm; }

  void
  fileName(const std::string& fnm)
  { m_filenm = fnm; }

  virtual const std::string&
  name() const
  { return m_name; }

  void
  name(const char* n)
  { m_name = n; }

  void
  name(const std::string& n)
  { m_name = n; }
  
  virtual std::string
  codeName() const;


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(int oFlags = 0) const;

  virtual void
  CSV_dump(const Root &root, std::ostream& os = std::cout,
	   const char* file_name = NULL, const char* proc_name = NULL,
	   int lLevel = 0) const;
  
  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

private:
  void
  Ctor(ACodeNode* parent, const char* filenm, const char* procnm);

private:
  std::string m_filenm;
  std::string m_name;

  static RealPathMgr& s_realpathMgr;
};


// --------------------------------------------------------------------------
// Loops are children of Group's, File's, Proc's,
//   or Loop's.
// children: Group's, Loop's, or Stmt's
// --------------------------------------------------------------------------
class Loop: public ACodeNode {
public:

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  Loop(ACodeNode* parent, 
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyLoop, parent, begLn, endLn, 0, 0)
  {
    ANodeTy t = (parent) ? parent->type() : TyANY;
    DIAG_Assert((parent == NULL) || (t == TyGroup) || (t == TyFile) || 
		(t == TyProc) || (t == TyAlien) || (t == TyLoop), "");
  }

  virtual ~Loop()
  { }
  
  virtual ANode* 
  clone() 
  { return new Loop(*this); }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual std::string
  codeName() const;


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string 
  toXML(int oFlags = 0) const;

  virtual std::ostream& 
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

};


// --------------------------------------------------------------------------
// Stmts are children of Group's, File's,
//   Proc's, or Loop's.
// children: none
// --------------------------------------------------------------------------
class Stmt: public ACodeNode {
public: 

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  Stmt(ACodeNode* parent, SrcFile::ln begLn, SrcFile::ln endLn,
       VMA begVMA = 0, VMA endVMA = 0)
    : ACodeNode(TyStmt, parent, begLn, endLn, begVMA, endVMA),
      m_sortId((int)begLn)
  {
    ANodeTy t = (parent) ? parent->type() : TyANY;
    DIAG_Assert((parent == NULL) || (t == TyGroup) || (t == TyFile)
		|| (t == TyProc) || (t == TyAlien) || (t == TyLoop), "");
    Proc* p = ancestorProc();
    if (p) { 
      p->AddToStmtMap(this);
      //DIAG_DevIf(0) { LoadMod()->verifyStmtMap(); }
    }
    //DIAG_DevMsg(3, "Stmt::Stmt: " << toString_me());
  }

  virtual ~Stmt()
  { }

  virtual ANode* 
  clone() 
  { return new Stmt(*this); }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------
  
  virtual std::string codeName() const;

  // a handle for sorting within the enclosing procedure context
  int
  sortId()
  { return m_sortId; }

  void
  sortId(int x)
  { m_sortId = x; }


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(int oFlags = 0) const;
  
  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

private:
  int m_sortId;
};


// ----------------------------------------------------------------------
// Refs are chldren of LineScopes 
// They are used to describe a reference in source code.
// Refs are build only iff we have preprocessing information.
// ----------------------------------------------------------------------
class Ref: public ACodeNode {
public: 
  Ref(ACodeNode* parent, int _begPos, int _endPos, const char* refName);
  // parent->type() == TyStmt 
  
  uint
  BegPos() const
  { return begPos; };

  uint
  EndPos() const
  { return endPos; };
  
  virtual const std::string&
  name() const
  { return m_name; }

  virtual std::string
  codeName() const;

  virtual ANode*
  clone()
  { return new Ref(*this); }

  virtual std::string
  toXML(int oFlags = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& 
  dumpme(std::ostream& os = std::cerr, int oFlags = 0,
	 const char* pre = "") const;

private: 
  void RelocateRef();
  uint begPos;
  uint endPos;
  std::string m_name;
};


} // namespace Struct
} // namespace Prof


/************************************************************************/
// Iterators
/************************************************************************/

#include "Struct-TreeIterator.hpp" 


#endif /* prof_juicy_Prof_Struct_Tree_hpp */
