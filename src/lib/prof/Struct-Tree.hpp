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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef prof_Prof_Struct_Tree_hpp
#define prof_Prof_Struct_Tree_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
#include <list>
#include <set>
#include <map>

#include <typeinfo>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/isa/ISA.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/Unique.hpp>

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
    OFlg_Debug           = (1 << 3)  // Debug: show xtra source line info
  };

  static const std::string UnknownLMNm;
  static const std::string UnknownFileNm;
  static const std::string UnknownProcNm;
  static const SrcFile::ln UnknownLine;

  static const std::string PartialUnwindProcNm;

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
  writeXML(std::ostream& os = std::cerr, uint oFlags = 0) const;

  // Dump contents for inspection (use flags from ANode)
  std::ostream&
  dump(std::ostream& os = std::cerr, uint oFlags = 0) const;

  void
  ddump() const;

private:
  Root* m_root;
};


// TODO: integrate with Tree::writeXML()
void
writeXML(std::ostream& os, const Prof::Struct::Tree& strctTree,
	 bool prettyPrint = true);


} // namespace Struct
} // namespace Prof


//***************************************************************************


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

private:
  static const std::string ScopeNames[TyNUMBER];

public:
  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  ANode(ANodeTy ty, ANode* parent = NULL)
    : NonUniformDegreeTreeNode(parent),
      m_type(ty),
      m_visible(true)
  {
    m_id = s_nextUniqueId++;
    m_origId = 0;
  }

  virtual ~ANode()
  {
    DIAG_DevMsgIf(0, "~ANode::ANode: " << toString_id()
		  << " " << std::hex << this << std::dec);
  }

  // clone: return a shallow copy, unlinked from the tree
  virtual ANode*
  clone()
  { return new ANode(*this); }


protected:
  ANode(const ANode& x)
  { *this = x; }

  // deep copy of internals (but without children)
  ANode&
  operator=(const ANode& x)
  {
    if (this != &x) {
      NonUniformDegreeTreeNode::zeroLinks();
      m_type    = x.m_type;
      m_id      = x.m_id;
      m_visible = x.m_visible;
      m_origId  = x.m_origId;
    }
    return *this;
  }


public:

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

  static const uint Id_NULL = 0;

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

  void 
  setInvisible() 
  { m_visible = false; }

  bool 
  isVisible() const
  { return m_visible == true; }

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

  // mergePaths: Given divergent paths (as defined above), merges the
  // path 'lca' -> 'node_src' into the path 'lca' --> 'node_dst'. If a
  // merge takes place returns true.
  static bool
  mergePaths(ANode* lca, ANode* node_dst, ANode* node_src);

  // merge: Given two nodes, 'node_src' and 'node_dst', merges the
  // former into the latter, if possible.  If the merge takes place,
  // deletes 'node_src' and returns true; otherwise returns false.
  static bool
  merge(ANode* node_dst, ANode* node_src);

  // isMergable: Returns whether 'node_src' is capable of being merged
  // into 'node_dst'
  static bool
  isMergable(ANode* node_dst, ANode* node_src);


  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  std::string
  toStringXML(uint oFlags = 0, const char* pre = "") const;

  virtual std::string
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  writeXML(std::ostream& os = std::cout, uint oFlags = 0,
	   const char* pre = "") const;

  void
  ddumpXML() const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::string
  toString(uint oFlags = 0, const char* pre = "") const;

  std::string
  toString_id(uint oFlags = 0) const;

  std::string
  toStringMe(uint oFlags = 0, const char* pre = "") const;

  // dump
  std::ostream&
  dump(std::ostream& os = std::cerr, uint oFlags = 0,
       const char* pre = "") const;

  void ddump() const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

  virtual std::ostream&
  dumpmePath(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

protected:
  bool
  writeXML_pre(std::ostream& os = std::cout, uint oFlags = 0,
	       const char* prefix = "") const;

  void
  writeXML_post(std::ostream& os = std::cout, uint oFlags = 0,
		const char* prefix = "") const;
private:
  void
  ctorCheck() const;

  void
  dtorCheck() const;

  static uint s_nextUniqueId;

protected:
  ANodeTy m_type; // obsolete with typeid(), but hard to replace
  uint m_id;
  bool m_visible;

public:
  // original node id from .hpcstruct file (for debug)
  uint m_origId;
};


// --------------------------------------------------------------------------
// ACodeNode is a base class for all scopes other than TyRoot and TyLM.
// Describes some kind of code, i.e. Files, Procedures, Loops...
// --------------------------------------------------------------------------
class ACodeNode : public ANode {
protected:
  ACodeNode(ANodeTy ty, ANode* parent = NULL,
	   SrcFile::ln begLn = ln_NULL,
	   SrcFile::ln endLn = ln_NULL,
	   VMA begVMA = 0, VMA endVMA = 0)
    : ANode(ty, parent), m_begLn(ln_NULL), m_endLn(ln_NULL)
  {
    m_lineno_frozen = false;
    setLineRange(begLn, endLn);
    if (!VMAInterval::empty(begVMA, endVMA)) {
      m_vmaSet.insert(begVMA, endVMA);
    }
    m_scope_filenm = "";
    m_scope_lineno = 0;
  }


  ACodeNode(const ACodeNode& x)
    : ANode(x.m_type)
  { *this = x; }


  ACodeNode&
  operator=(const ACodeNode& x)
  {
    // shallow copy
    if (this != &x) {
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
  
  void
  freezeLine() 
  { m_lineno_frozen = true; }

  void
  thawLine() 
  { m_lineno_frozen = false; }


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
  nameQual() const
  { return codeName(); }

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
  toXML(uint oFlags = 0) const;

  virtual std::string
  XMLLineRange(uint oFlags) const;

  virtual std::string
  XMLVMAIntervals(uint oFlags) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
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

  // --------------------------------------------------------
  // Inline support -- Save the location (file name, line num) of an
  // alien scope (loop) node to help find the same scope in a later
  // InlineNode sequence inside the location manager.
  // --------------------------------------------------------
private:
  std::string m_scope_filenm;
  SrcFile::ln m_scope_lineno;
  bool m_lineno_frozen;
  
public:
  void setScopeLocation(std::string &file, SrcFile::ln line) {
    m_scope_filenm = file;
    m_scope_lineno = line;
  }
  std::string & getScopeFileName() { return m_scope_filenm; }
  SrcFile::ln getScopeLineNum() { return m_scope_lineno; }
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

  Root&
  operator=(const Root& x);

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
    delete lmMap_realpath;
    delete lmMap_basename;
  }

  virtual const std::string&
  name() const
  { return m_name; }

  void
  name(const char* x)
  { m_name = (x) ? x : ""; }

  void
  name(const std::string& x)
  { m_name = x; }

  // --------------------------------------------------------
  //
  // --------------------------------------------------------

  // findLM: First, try to find by nm_real = realpath(nm).  If that is
  // unsuccesful, try to find by nm_base = basename(nm_real) if
  // nm_base == nm_real.
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
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  writeXML(std::ostream& os = std::cout, uint oFlags = 0,
	   const char* pre = "") const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

protected:
private:
  void
  Ctor(const char* nm);

  void
  insertGroupMap(Group* grp);

  void
  insertLMMap(LM* lm);

  friend class Group;
  friend class LM;

private:
  std::string m_name; // the program name

  GroupMap* groupMap;

  LMMap* lmMap_realpath; // mapped by 'realpath'
  LMMap* lmMap_basename;

#if 0
  static RealPathMgr& s_realpathMgr;
#endif
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

  virtual ~Group()
  { }

  static Group*
  demand(Root* pgm, const std::string& nm, ANode* parent);

  virtual const std::string&
  name() const
  { return m_name; } // same as grpName

  virtual std::string
  codeName() const;

  virtual ANode*
  clone()
  { return new Group(*this); }

  virtual std::string
  toXML(uint oFlags = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
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
  LM&
  operator=(const LM& x);

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

  void
  pretty_name(const std::string& new_name)
  {
    m_pretty_name = new_name;
  }


  // --------------------------------------------------------
  // search for enclosing nodes
  // --------------------------------------------------------

  // findFile: find using RealPathMgr
  File*
  findFile(const char* nm) const;

  File*
  findFile(const std::string& nm) const
  { return findFile(nm.c_str()); }


  // --------------------------------------------------------
  // search by VMA
  // --------------------------------------------------------

  // findByVMA: find scope by *unrelocated* VMA
  // findProc: VMA interval -> Struct::Proc*
  // findStmt: VMA interval -> Struct::Stmt*
  //
  // N.B. these maps are maintained when new Struct::Proc or
  // Struct::Stmt are created
  ACodeNode*
  findByVMA(VMA vma) const;

  void
  computeVMAMaps() const
  {
    delete m_procMap;
    m_procMap = NULL;
    delete m_stmtMap;
    m_stmtMap = NULL;
    findProc(0);
    findStmt(0);
  }


  Proc*
  findProc(VMA vma) const;

  bool
  insertProcIf(Proc* proc) const
  {
    if (m_procMap) {
      insertInMap(m_procMap, proc);
      return true;
    }
    return false;
  }


  Stmt*
  findStmt(VMA vma) const;

  bool
  insertStmtIf(Stmt* stmt) const
  {
    if (m_stmtMap) {
      insertInMap(m_stmtMap, stmt);
      return true;
    }
    return false;
  }

  bool
  eraseStmtIf(Stmt* stmt) const
  {
    if (m_stmtMap) {
      eraseFromMap(m_stmtMap, stmt);
      return true;
    }
    return false;
  }


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  writeXML(std::ostream& os = std::cout, uint oFlags = 0,
	   const char* pre = "") const;


  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
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
  insertFileMap(File* file);


  template<typename T>
  void
  buildMap(VMAIntervalMap<T>*& mp, ANode::ANodeTy ty) const;

  template<typename T>
  void
  insertInMap(VMAIntervalMap<T>* mp, T x) const
  {
    const VMAIntervalSet& vmaset = x->vmaSet();
    for (VMAIntervalSet::const_iterator it = vmaset.begin();
	 it != vmaset.end(); ++it) {
      const VMAInterval& vmaint = *it;
      DIAG_MsgIf(0, vmaint.toString());
      mp->insert(std::make_pair(vmaint, x));
    }
  }


  template<typename T>
  void
  eraseFromMap(VMAIntervalMap<T>* mp, T x) const
  {
    const VMAIntervalSet& vmaset = x->vmaSet();
    for (VMAIntervalSet::const_iterator it = vmaset.begin();
	 it != vmaset.end(); ++it) {
      const VMAInterval& vmaint = *it;
      mp->erase(vmaint);
    }
  }


  template<typename T>
  static bool
  verifyMap(VMAIntervalMap<T>* mp, const char* map_nm);


  friend class File;

private:
  std::string m_name; // the load module name

  // for pseudo modules, this will be set
  // keep this in addition to m_name to avoid disturbing other
  // things that depend upon a full path
  std::string m_pretty_name;

  // maps to support fast lookups; building them does not logically
  // change the object
  FileMap*                   m_fileMap; // mapped by RealPathMgr
  mutable VMAToProcMap*      m_procMap;
  mutable VMAToStmtRangeMap* m_stmtMap;

#if 0
  static RealPathMgr& s_realpathMgr;
#endif
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
  name(const char* fname)
  { m_name = fname; }

  void
  name(const std::string& fname)
  { m_name = fname; }

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
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

private:
  void
  Ctor(const char* filenm, ANode* parent);

  void
  insertProcMap(Proc* proc);

  friend class Proc;

private:
  std::string m_name; // the file name including the path
  ProcMap*    m_procMap;

#if 0
  static RealPathMgr& s_realpathMgr;
#endif
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
  Proc(const Proc& x)
    : ACodeNode(x.m_type)
  { *this = x; }

  Proc&
  operator=(const Proc& x);

public:

  // map of file name to alien node, for stmts with a single, guard
  // alien from struct simple
  typedef std::map <std::string, Alien *> AlienFileMap;

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
    delete m_alienMap;
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

  // find or create direct stmt node (no alien) for struct simple
  Stmt*
  demandStmtSimple(SrcFile::ln line, VMA beg_vma, VMA end_vma);

  // find or create guard alien for struct simple
  Alien*
  demandGuardAlien(std::string & filenm, SrcFile::ln line);

  // --------------------------------------------------------
  //
  // --------------------------------------------------------

  virtual const std::string&
  name() const
  { return m_name; }

  virtual std::string
  codeName() const;


  void
  name(const char* x)
  { m_name = (x) ? x : ""; }

  void
  name(const std::string& x)
  { m_name = x; }

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

#if 0
  // FIXME: confusion between native and alien statements
  Stmt*
  findStmt(SrcFile::ln begLn)
  {
    StmtMap::iterator it = m_stmtMap->find(begLn);
    Stmt* x = (it != m_stmtMap->end()) ? it->second : NULL;
    return x;
  }
#endif


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

private:
  void
  Ctor(const char* n, ACodeNode* parent, const char* ln, bool hasSym);

#if 0
  void
  insertStmtMap(Stmt* stmt);
#endif

  friend class Stmt;

private:
  std::string m_name;
  std::string m_linkname;
  bool m_hasSym;

  // for struct simple and guard aliens only.  all access should go
  // through demandStmtSimple() and demandGuardAlien().
  StmtMap* m_stmtMap;
  AlienFileMap* m_alienMap;
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
  Alien(const Alien& x)
    : ACodeNode(x.m_type)
  { *this = x; }

  Alien&
  operator=(const Alien& x);

public:

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  Alien(ACodeNode* parent, const char* filenm, const char* procnm,
	     const char* displaynm,
	     SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyAlien, parent, begLn, endLn, 0, 0)
  {
    Ctor(parent, filenm, procnm, displaynm);
  }

  Alien(ACodeNode* parent,
	     const std::string& filenm, const std::string& procnm,
	     const std::string& displaynm,
	     SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyAlien, parent, begLn, endLn, 0, 0)
  {
    Ctor(parent, filenm.c_str(), procnm.c_str(), displaynm.c_str());
  }

  virtual ~Alien()
  { delete m_stmtMap; }

  virtual ANode*
  clone()
  { return new Alien(*this); }

  // find or create a stmt inside the alien
  Stmt*
  demandStmt(SrcFile::ln line, VMA beg_vma, VMA end_vma);

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

  void
  proc(Prof::Struct::Proc *proc)
  { m_proc = proc; }

  const std::string&
  displayName() const
  { return m_displaynm; }

  virtual std::string
  codeName() const;


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

private:
  void
  Ctor(ACodeNode* parent, const char* filenm, const char* procnm,
       const char* displaynm);

  friend class Stmt;

private:
  std::string m_filenm;
  std::string m_name;
  std::string m_displaynm;

  // for struct simple only
  StmtMap *   m_stmtMap;

  Prof::Struct::Proc *m_proc;

#if 0
  static RealPathMgr& s_realpathMgr;
#endif
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
  Loop(ACodeNode* parent, std::string &filenm, 
	    SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL)
    : ACodeNode(TyLoop, parent, begLn, endLn, 0, 0)
  {
    ANodeTy t = (parent) ? parent->type() : TyANY;
    setFile(filenm);
    DIAG_Assert((parent == NULL) || (t == TyGroup) || (t == TyFile) ||
		(t == TyProc) || (t == TyAlien) || (t == TyLoop), "");
  }

  void setFile(std::string filenm);

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
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

  const std::string&
  fileName() const
  { return m_filenm; }

  void
  fileName(const std::string& fnm)
  { m_filenm = fnm; }

private:
  std::string m_filenm;
};


// --------------------------------------------------------------------------
// Stmts are children of Group's, File's,
//   Proc's, or Loop's.
// children: none
// --------------------------------------------------------------------------
class Stmt: public ACodeNode {
public:
  enum StmtType {
    STMT_STMT,
    STMT_CALL
  };

  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  Stmt(ACodeNode* parent, SrcFile::ln begLn, SrcFile::ln endLn,
       VMA begVMA = 0, VMA endVMA = 0,
       StmtType stmt_type = STMT_STMT)
    : ACodeNode(TyStmt, parent, begLn, endLn, begVMA, endVMA),
      m_stmt_type(stmt_type), m_sortId((int)begLn)
  {
    ANodeTy t = (parent) ? parent->type() : TyANY;
    DIAG_Assert((parent == NULL) || (t == TyGroup) || (t == TyFile)
		|| (t == TyProc) || (t == TyAlien) || (t == TyLoop), "");

#if 0
    // if parent is proc or alien, add to stmt map
    if (t == TyProc) {
      ((Proc *) parent)->insertStmtMap(this);
    }
    else if (t == TyAlien) {
      Alien * alien = (Alien *) parent;
      (* (alien->m_stmtMap))[begLn] = this;
    }
#endif

    // add vma to LM vma interval map
    LM* lmStrct = parent->ancestorLM();
    if (lmStrct && begVMA) {
      lmStrct->insertStmtIf(this);
    }

    //DIAG_DevMsg(3, "Stmt::Stmt: " << toStringMe());
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

  // a handle for differentiating a CALL and a STMT
  StmtType
  stmtType()
  { return m_stmt_type; }

  void
  stmtType(StmtType type)
  { m_stmt_type = type; }

  VMA &
  target()
  { return m_target; }

  void
  target(VMA x)
  { m_target = x; }

  std::string
  device()
  { return m_device; }

  void
  device(const std::string &device)
  { m_device = device; }

  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toXML(uint oFlags = 0) const;

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
	 const char* pre = "") const;

private:
  StmtType m_stmt_type;
  std::string m_device;
  VMA m_target;
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
  toXML(uint oFlags = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::ostream&
  dumpme(std::ostream& os = std::cerr, uint oFlags = 0,
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


#endif /* prof_Prof_Struct_Tree_hpp */
