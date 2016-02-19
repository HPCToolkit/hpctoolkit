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
// Copyright ((c)) 2002-2016, Rice University
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
//    $HeadURL$
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
using std::hex;
using std::dec;
using std::endl;

#include <string>
using std::string;

#include <typeinfo>

#include <algorithm>

#include <cstring> // for 'strcmp'


//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "Struct-Tree.hpp"

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

using namespace xml;

#define DBG_FILE 0

//***************************************************************************

namespace Prof {
namespace Struct {


RealPathMgr& s_realpathMgr = RealPathMgr::singleton();

//***************************************************************************
// Tree
//***************************************************************************

uint ANode::s_nextUniqueId = 1;

const std::string Tree::UnknownLMNm   = "<unknown load module>";

const std::string Tree::UnknownFileNm = "<unknown file>";

const std::string Tree::UnknownProcNm = "<unknown procedure>";

const std::string Tree::PartialUnwindProcNm = "<partial call paths>";

const SrcFile::ln Tree::UnknownLine = SrcFile::ln_NULL;


Tree::Tree(const char* name, Root* root)
  : m_root(root)
{
  if (!m_root) {
    m_root = new Root(name);
  }
}


Tree::~Tree()
{
  delete m_root;
}

string
Tree::name() const
{
  string nm = (m_root) ? std::string(m_root->name()) : "";
  return nm;
}


ostream&
Tree::writeXML(ostream& os, uint oFlags) const
{
  if (m_root) {
    m_root->writeXML(os, oFlags);
  }
  return os;
}


ostream&
Tree::dump(ostream& os, uint oFlags) const
{
  writeXML(os, oFlags);
  return os;
}


void
Tree::ddump() const
{
  dump(std::cerr, Tree::OFlg_Debug);
}


void
writeXML(std::ostream& os, const Prof::Struct::Tree& strctTree,
	 bool prettyPrint)
{
  static const char* structureDTD =
#include <lib/xml/hpc-structure.dtd.h>

  os << "<?xml version=\"1.0\"?>\n";
  os << "<!DOCTYPE HPCToolkitStructure [\n" << structureDTD << "]>\n";
  os.flush();

  int oFlags = 0;
  if (!prettyPrint) {
    oFlags |= Prof::Struct::Tree::OFlg_Compressed;
  }
  
  os << "<HPCToolkitStructure i=\"0\" version=\"4.6\" n"
     << xml::MakeAttrStr(strctTree.name()) << ">\n";
  strctTree.writeXML(os, oFlags);
  os << "</HPCToolkitStructure>\n";
  os.flush();
}


//***************************************************************************
// ANodeTy `methods' (could completely replace with dynamic typing)
//***************************************************************************

const string ANode::ScopeNames[ANode::TyNUMBER] = {
  "Pgm", "Group", "LM", "File", "Proc", "Alien", "Loop", "Stmt", "Ref", "ANY"
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

void
ACodeNode::linkAndSetLineRange(ACodeNode* parent)
{
  this->link(parent);
  if (begLine() != ln_NULL) {
    SrcFile::ln bLn = std::min(parent->begLine(), begLine());
    SrcFile::ln eLn = std::max(parent->endLine(), endLine());
    parent->setLineRange(bLn, eLn);
  }
}


#if 0
RealPathMgr& Root::s_realpathMgr = RealPathMgr::singleton();
#endif

void
Root::Ctor(const char* nm)
{
  DIAG_Assert(nm, "");
  setInvisible();
  m_name = nm;
  groupMap = new GroupMap();
  lmMap_realpath = new LMMap();
  lmMap_basename = new LMMap();
}


Root&
Root::operator=(const Root& x)
{
  // shallow copy
  if (&x != this) {
    m_name = x.m_name;
    groupMap = NULL;
    lmMap_realpath = NULL;
    lmMap_basename = NULL;
  }
  return *this;
}


LM*
Root::findLM(const char* nm) const
{
  // TODO: if the map is empty and Root has LM children, we should
  // populate the map

  string nm_real = nm;
  s_realpathMgr.realpath(nm_real);

  LMMap::iterator it1 = lmMap_realpath->find(nm_real);
  LM* x = (it1 != lmMap_realpath->end()) ? it1->second : NULL;

  if (!x) {
    string nm_base = FileUtil::basename(nm_real);

    // N.B. Must be careful about simply searching by nm_base.  If we have
    //    nm = <path1>/base
    // then we could mistakenly find <path2>/base.
    if (nm_real == nm_base) {
      LMMap::iterator it2 = lmMap_basename->find(nm_base);
      // N.B. it2->second is NULL if nm_base was inserted multiple times
      x = (it2 != lmMap_basename->end()) ? it2->second : NULL;
    }
  }

  return x;
}


void
Group::Ctor(const char* nm, ANode* parent)
{
  DIAG_Assert(nm, "");
  ANodeTy t = (parent) ? parent->type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyRoot) || (t == TyGroup) || (t == TyLM)
	      || (t == TyFile) || (t == TyProc) || (t == TyAlien)
	      || (t == TyLoop), "");
  m_name = nm;
  ancestorRoot()->insertGroupMap(this);
}


Group*
Group::demand(Root* pgm, const string& nm, ANode* parent)
{
  Group* grp = pgm->findGroup(nm);
  if (!grp) {
    grp = new Group(nm, parent);
  }
  return grp;
}


#if 0
RealPathMgr& LM::s_realpathMgr = RealPathMgr::singleton();
#endif

void
LM::Ctor(const char* nm, ANode* parent)
{
  DIAG_Assert(nm, "");
  ANodeTy t = (parent) ? parent->type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyRoot) || (t == TyGroup), "");

  m_name = nm;
  m_fileMap = new FileMap();
  m_procMap = NULL;
  m_stmtMap = NULL;

  Root* root = ancestorRoot();
  if (root) {
    root->insertLMMap(this);
  }
}


LM&
LM::operator=(const LM& x)
{
  // shallow copy
  if (&x != this) {
    m_name     = x.m_name;
    m_fileMap  = NULL;
    m_procMap  = NULL;
    m_stmtMap  = NULL;
  }
  return *this;
}


LM*
LM::demand(Root* pgm, const string& lm_nm)
{
  LM* lm = pgm->findLM(lm_nm);
  if (!lm) {
    lm = new LM(lm_nm, pgm);
  }
  return lm;
}


#if 0
RealPathMgr& File::s_realpathMgr = RealPathMgr::singleton();
#endif

void
File::Ctor(const char* fname, ANode* parent)
{
  ANodeTy t = (parent) ? parent->type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyRoot) || (t == TyGroup) || (t == TyLM), "");

  m_name = (fname) ? fname : "";
  m_procMap = new ProcMap();

  ancestorLM()->insertFileMap(this);
}


File&
File::operator=(const File& x)
{
  // shallow copy
  if (&x != this) {
    m_name    = x.m_name;
    m_procMap = NULL;
  }
  return *this;
}


File*
File::demand(LM* lm, const string& filenm)
{
  const char* note = "(found)";

  string nm_real = filenm;
  s_realpathMgr.realpath(nm_real);

  File* file = lm->findFile(nm_real);
  if (!file) {
    note = "(created)";
    file = new File(nm_real, lm);
  }
  DIAG_DevMsgIf(DBG_FILE, "Struct::File::demand: " << note << endl
		<< "\tin : " << filenm << endl
		<< "\tout: " << file->name());
  return file;
}


void
Proc::Ctor(const char* n, ACodeNode* parent, const char* ln, bool hasSym)
{
  DIAG_Assert(n, "");
  ANodeTy t = (parent) ? parent->type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyGroup) || (t == TyFile), "");

  m_name = (n) ? n : "";
  m_linkname = (ln) ? ln : "";
  m_hasSym = hasSym;
  m_stmtMap = new StmtMap();
  if (parent) {
    relocate();
  }

  File* fileStrct = ancestorFile();
  if (fileStrct) {
    fileStrct->insertProcMap(this);
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
    m_stmtMap  = new StmtMap();
  }
  return *this;
}


Proc*
Proc::demand(File* file, const string& name, const std::string& linkname,
	     SrcFile::ln begLn, SrcFile::ln endLn, bool* didCreate)
{
  Proc* proc = file->findProc(name, linkname);
  if (!proc) {
    proc = new Proc(name, file, linkname, false, begLn, endLn);
    if (didCreate) {
      *didCreate = true;
    }
  }
  else if (didCreate) {
    *didCreate = false;
  }
  return proc;
}


#if 0
RealPathMgr& Alien::s_realpathMgr = RealPathMgr::singleton();
#endif

void
Alien::Ctor(ACodeNode* parent, const char* filenm, const char* nm,
	    const char* displaynm)
{
  ANodeTy t = (parent) ? parent->type() : TyANY;
  DIAG_Assert((parent == NULL) || (t == TyGroup) || (t == TyAlien)
	      || (t == TyProc) || (t == TyLoop), "");

  m_filenm = (filenm) ? filenm : "";
  s_realpathMgr.realpath(m_filenm);

  m_name   = (nm) ? nm : "";
  m_displaynm = (displaynm) ? displaynm : "";
  freezeLine();
}


Alien&
Alien::operator=(const Alien& x)
{
  // shallow copy
  if (&x != this) {
    m_filenm = x.m_filenm;
    m_name   = x.m_name;
    m_displaynm = x.m_displaynm;
  }
  return *this;
}


Ref::Ref(ACodeNode* parent, int _begPos, int _endPos, const char* refName)
  : ACodeNode(TyRef, parent, parent->begLine(), parent->begLine(), 0, 0)
{
  DIAG_Assert(parent->type() == TyStmt, "");
  begPos = _begPos;
  endPos = _endPos;
  m_name = refName;
  RelocateRef();
  DIAG_Assert(begPos <= endPos, "");
  DIAG_Assert(begPos > 0, "");
  DIAG_Assert(m_name.length() > 0, "");
}


//***************************************************************************
// ANode: ancestor
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
  return dynamic_cast<ACodeNode*>(parent());
}


ANode*
ANode::ancestor(ANodeTy ty) const
{
  const ANode* x = this;
  while (x && x->type() != ty) {
    x = x->parent();
  }
  return const_cast<ANode*>(x);
}


ANode*
ANode::ancestor(ANodeTy ty1, ANodeTy ty2) const
{
  const ANode* x = this;
  while (x && x->type() != ty1 && x->type() != ty2) {
    x = x->parent();
  }
  return const_cast<ANode*>(x);
}


ANode*
ANode::ancestor(ANodeTy ty1, ANodeTy ty2, ANodeTy ty3) const
{
  const ANode* x = this;
  while (x && x->type() != ty1 && x->type() != ty2 && x->type() != ty3) {
    x = x->parent();
  }
  return const_cast<ANode*>(x);
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
ANode::leastCommonAncestor(ANode* n1, ANode* n2)
{
  // Collect all ancestors of n1 and n2.  The root will be at the front
  // of the ancestor list.
  ANodeList anc1, anc2;
  for (ANode* a = n1->parent(); (a); a = a->parent()) {
    anc1.push_front(a);
  }
  for (ANode* a = n2->parent(); (a); a = a->parent()) {
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


Root*
ANode::ancestorRoot() const
{
  // iff this is called during ANode construction within the Root
  // construction dyn_cast does not do the correct thing
  if (Parent() == NULL) {
    // eraxxon: This cannot be a good thing to do!  Root() was being
    //   called on a LoopInfo object; then the resulting pointer
    //   (the LoopInfo itself) was queried for Root member data.  Ouch!
    // eraxxon: return (Root*) this;
    return NULL;
  }
  else {
    dyn_cast_return(ANode, Root, ancestor(TyRoot));
  }
}


Group*
ANode::ancestorGroup() const
{
  dyn_cast_return(ANode, Group, ancestor(TyGroup));
}


LM*
ANode::ancestorLM() const
{
  dyn_cast_return(ANode, LM, ancestor(TyLM));
}


File*
ANode::ancestorFile() const
{
  dyn_cast_return(ANode, File, ancestor(TyFile));
}


Proc*
ANode::ancestorProc() const
{
  dyn_cast_return(ANode, Proc, ancestor(TyProc));
}


Alien*
ANode::ancestorAlien() const
{
  dyn_cast_return(ANode, Alien, ancestor(TyAlien));
}


Loop*
ANode::ancestorLoop() const
{
  dyn_cast_return(ANode, Loop, ancestor(TyLoop));
}


Stmt*
ANode::ancestorStmt() const
{
  dyn_cast_return(ANode, Stmt, ancestor(TyStmt));
}


ACodeNode*
ANode::ancestorProcCtxt() const
{
  return dynamic_cast<ACodeNode*>(ancestor(TyProc, TyAlien));
}


//***************************************************************************
// Metrics
//***************************************************************************

void
ANode::aggregateMetrics(uint mBegId, uint mEndId)
{
  if ( !(mBegId < mEndId) ) {
    return; // short circuit
  }

  const ANode* root = this;
  ANodeIterator it(root, NULL/*filter*/, false/*leavesOnly*/,
		   IteratorStack::PostOrder);
  for (ANode* n = NULL; (n = it.current()); ++it) {
    ANode* n_parent = n->parent();
    if (n != root) {
      for (uint mId = mBegId; mId < mEndId; ++mId) {
	double mVal = n->demandMetric(mId, mEndId/*size*/);
	n_parent->demandMetric(mId, mEndId/*size*/) += mVal;
      }
    }
  }
}


void
ANode::pruneByMetrics()
{
  for (ANodeChildIterator it(this); it.Current(); /* */) {
    ANode* x = it.current();
    it++; // advance iterator -- it is pointing at 'x'

    if (x->hasMetrics()) {
      x->pruneByMetrics();
    }
    else {
      x->unlink(); // unlink 'x' from tree
      delete x;
    }
  }
}


//***************************************************************************
// ANode: Paths and Merging
//***************************************************************************

int
ANode::distance(ANode* anc, ANode* desc)
{
  int distance = 0;
  for (ANode* x = desc; x != NULL; x = x->parent()) {
    if (x == anc) {
      return distance;
    }
    ++distance;
  }

  // If we arrive here, there was no path between 'anc' and 'desc'
  return -1;
}


bool
ANode::arePathsOverlapping(ANode* lca, ANode* desc1, ANode* desc2)
{
  // Ensure that d1 is on the longest path
  ANode* d1 = desc1, *d2 = desc2;
  int dist1 = distance(lca, d1);
  int dist2 = distance(lca, d2);
  if (dist2 > dist1) {
    ANode* t = d1;
    d1 = d2;
    d2 = t;
  }
  
  // Iterate over the longest path (d1 -> lca) searching for d2.  Stop
  // when x is NULL or lca.
  for (ANode* x = d1; (x && x != lca); x = x->parent()) {
    if (x == d2) {
      return true;
    }
  }
  
  // If we arrive here, we did not encounter d2.  Divergent.
  return false;
}


bool
ANode::mergePaths(ANode* lca, ANode* node_dst, ANode* node_src)
{
  bool merged = false;
  // Should we verify that lca is really the lca?
  
  // Collect nodes along the paths 'lca' --> 'node_dst', 'node_src'.
  // Exclude 'lca'.  Shallowest nodes are at beginning of list.
  ANodeList path_dst, path_src;
  for (ANode* x = node_dst; x != lca; x = x->parent()) {
    path_dst.push_front(x);
  }
  for (ANode* x = node_src; x != lca; x = x->parent()) {
    path_src.push_front(x);
  }
  DIAG_Assert(path_dst.size() > 0 && path_src.size() > 0, "");
  
  // Merge nodes in 'path_src' into 'path_dst', shallowest to deepest,
  // exiting as soon as a merge fails
  ANodeList::iterator it_dst = path_dst.begin();
  ANodeList::iterator it_src = path_src.begin();

  for ( ; (it_dst != path_dst.end() && it_src != path_src.end());
	++it_src, ++it_dst) {
    ANode* x_src = *it_src;
    ANode* x_dst = *it_dst;
    if (isMergable(x_dst, x_src)) {
      merged |= merge(x_dst, x_src);
    }
    else {
      break; // done
    }
  }
  
  return merged;
}


bool
ANode::merge(ANode* node_dst, ANode* node_src)
{
  // Do we really want this?
  //if (!IsMergable(node_dst, node_src)) {
  //  return false;
  //}
  
  // Perform the merge
  // 1. Move all children of 'node_src' into 'node_dst'
  for (ANodeChildIterator it(node_src); it.Current(); /* */) {
    ANode* x = it.current();
    it++; // advance iterator so we can unlink 'x'
    
    x->unlink();
    x->link(node_dst);
  }
  
  // 2. If merging ACodeNodes, update line ranges
  ACodeNode* dst0 = dynamic_cast<ACodeNode*>(node_dst);
  ACodeNode* src0 = dynamic_cast<ACodeNode*>(node_src);
  DIAG_Assert(Logic::equiv(dst0, src0), "Invariant broken!");
  if (dst0 && src0) {
    SrcFile::ln begLn = std::min(dst0->begLine(), src0->begLine());
    SrcFile::ln endLn = std::max(dst0->endLine(), src0->endLine());
    dst0->setLineRange(begLn, endLn);
    dst0->vmaSet().merge(src0->vmaSet()); // merge VMAs
  }
  
  // 3. Unlink 'node_src' from the tree and delete it
  node_src->unlink();
  delete node_src;
  
  return true;
}


bool
ANode::isMergable(ANode* node_dst, ANode* node_src)
{
  ANode::ANodeTy ty_dst = node_dst->type(), ty_src = node_src->type();

  // 1. For now, merges are only defined on following types
  bool goodTy = (ty_dst == TyLoop || ty_dst == TyAlien || ty_dst == TyGroup);

  // 2. Check bounds
  bool goodBnds = true;
  if (goodTy) {
    ACodeNode* dst0 = dynamic_cast<ACodeNode*>(node_dst);
    ACodeNode* src0 = dynamic_cast<ACodeNode*>(node_src);
    goodBnds = Logic::equiv(SrcFile::isValid(dst0->begLine(), dst0->endLine()),
			    SrcFile::isValid(src0->begLine(), src0->endLine()));
  }

  return ((ty_dst == ty_src) && goodTy && goodBnds);
}


//***************************************************************************
// ANode, etc: Names, Name Maps, and Retrieval by Name
//***************************************************************************

void
Root::insertGroupMap(Group* grp)
{
  std::pair<GroupMap::iterator, bool> ret =
    groupMap->insert(std::make_pair(grp->name(), grp));
  DIAG_Assert(ret.second, "Duplicate!");
}

void
Loop::setFile(std::string filenm)
{
  m_filenm = filenm;
  s_realpathMgr.realpath(m_filenm);
}


void
Root::insertLMMap(LM* lm)
{
  string nm_real = lm->name();
  s_realpathMgr.realpath(nm_real);

  std::pair<LMMap::iterator, bool> ret1 =
    lmMap_realpath->insert(std::make_pair(nm_real, lm));
  DIAG_Assert(ret1.second, "Root::insertLMMap: Duplicate!");

  string nm_base = FileUtil::basename(nm_real);
  std::pair<LMMap::iterator, bool> ret2 =
    lmMap_basename->insert(std::make_pair(nm_base, lm));
  if (!ret2.second /* no insertion performed */) {
    // Set target to NULL to indicate 'nm_base' appears more than once
    LMMap::iterator entry = ret2.first;
    entry->second = NULL;
  }
}


void
LM::insertFileMap(File* f)
{
  string nm_real = f->name();
  s_realpathMgr.realpath(nm_real);
  DIAG_DevMsg(2, "LM: mapping file name '" << nm_real << "' to File* " << f);
  std::pair<FileMap::iterator, bool> ret =
    m_fileMap->insert(std::make_pair(nm_real, f));
  DIAG_Assert(ret.second, "Duplicate instance: " << f->name() << "\n" << toStringXML());
}


void
File::insertProcMap(Proc* p)
{
  DIAG_DevMsg(2, "File (" << this << "): mapping proc name '" << p->name()
	      << "' to Proc* " << p);
  m_procMap->insert(make_pair(p->name(), p)); // multimap
}


void
Proc::insertStmtMap(Stmt* stmt)
{
  // FIXME: confusion between native and alien statements
  (*m_stmtMap)[stmt->begLine()] = stmt;
}


File*
LM::findFile(const char* nm) const
{
  string nm_real = nm;
  s_realpathMgr.realpath(nm_real);
  FileMap::iterator it = m_fileMap->find(nm_real);
  File* x = (it != m_fileMap->end()) ? it->second : NULL;
  return x;
}


ACodeNode*
LM::findByVMA(VMA vma) const
{
  // Attempt to find StatementRange and then Proc
  ACodeNode* found = findStmt(vma);
  if (!found) {
    found = findProc(vma);
  }
  return found;
}


Proc*
LM::findProc(VMA vma) const
{
  if (!m_procMap) {
    buildMap(m_procMap, ANode::TyProc);
  }
  VMAInterval toFind(vma, vma+1); // [vma, vma+1)
  VMAIntervalMap<Proc*>::iterator it = m_procMap->find(toFind);
  return (it != m_procMap->end()) ? it->second : NULL;
}


Stmt*
LM::findStmt(VMA vma) const
{
  if (!m_stmtMap) {
    buildMap(m_stmtMap, ANode::TyStmt);
  }
  VMAInterval toFind(vma, vma+1); // [vma, vma+1)
  VMAIntervalMap<Stmt*>::iterator it = m_stmtMap->find(toFind);
  return (it != m_stmtMap->end()) ? it->second : NULL;
}


template<typename T>
void
LM::buildMap(VMAIntervalMap<T>*& mp, ANode::ANodeTy ty) const
{
  if (!mp) {
    mp = new VMAIntervalMap<T>;
  }
  
  ANodeIterator it(this, &ANodeTyFilter[ty]);
  for (; it.Current(); ++it) {
    T x = dynamic_cast<T>(it.Current());
    insertInMap(mp, x);
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
  if (!m_stmtMap) {
    VMAToStmtRangeMap* mp;
    buildMap(mp, ANode::TyStmt);
    verifyMap(mp, "stmtMap");
    delete mp;
    return true;
 }
  else {
    return verifyMap(m_stmtMap, "stmtMap");
  }
}


Proc*
File::findProc(const char* name, const char* linkname) const
{
  Proc* found = NULL;

  ProcMap::const_iterator it = m_procMap->find(name);
  if (it != m_procMap->end()) {
    if (linkname && linkname[0] != '\0') {
      for ( ; (it != m_procMap->end() && strcmp(it->first.c_str(), name) == 0);
	    ++it) {
	Proc* p = it->second;
	if (strcmp(p->linkName().c_str(), linkname) == 0) {
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
// ACodeNode methods
//***************************************************************************

void
ACodeNode::setLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate)
{
  if (m_lineno_frozen) return;

  checkLineRange(begLn, endLn);
  
  m_begLn = begLn;
  m_endLn = endLn;
  
  relocateIf();

  // never propagate changes outside an Alien
  if (propagate && SrcFile::isValid(begLn)
      && ACodeNodeParent()
      && type() != ANode::TyAlien
      // tallent: for some reason this is not equivalent to the above...
      /* typeid(*this) != typeid(Struct::Alien)*/) {
    ACodeNodeParent()->expandLineRange(m_begLn, m_endLn);
  }
}

void
ACodeNode::expandLineRange(SrcFile::ln begLn, SrcFile::ln endLn, int propagate)
{
  if (m_lineno_frozen) return;

  if (type() == ANode::TyAlien) return; // never expand an alien line range
  if (type() == ANode::TyLoop) return;  // never expand a loop range

  checkLineRange(begLn, endLn);

  if (begLn == ln_NULL) {
    DIAG_Assert(m_begLn == ln_NULL, "");
    // simply relocate at beginning of sibling list
    relocateIf();
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
      relocateIf();
      if (propagate && ACodeNodeParent() && type() != ANode::TyAlien) {
        ACodeNodeParent()->expandLineRange(m_begLn, m_endLn);
      }
    }
  }
}


void
ACodeNode::relocate()
{
  ACodeNode* prev = dynamic_cast<ACodeNode*>(prevSibling());
  ACodeNode* next = dynamic_cast<ACodeNode*>(nextSibling());

  // NOTE: Technically should check for ln_NULL
  if ((!prev || (prev->begLine() <= m_begLn))
      && (!next || (m_begLn <= next->begLine()))) {
    return;
  }
  
  // INVARIANT: The parent scope contains at least two children
  DIAG_Assert(parent()->childCount() >= 2, "");

  ANode* prnt = parent();
  unlink();

  //if (prnt->firstChild() == NULL) {
  //  link(prnt);
  //}
  if (m_begLn == ln_NULL) {
    // insert as first child
    linkBefore(prnt->firstChild());
  }
  else {
    // insert after sibling with sibling->begLine() <= begLine()
    // or iff that does not exist insert as first in sibling list
    ACodeNode* sibling = NULL;
    for (sibling = dynamic_cast<ACodeNode*>(prnt->lastChild()); sibling;
	 sibling = dynamic_cast<ACodeNode*>(sibling->prevSibling())) {
      if (sibling->begLine() <= m_begLn) {
	break;
      }
    }
    if (sibling != NULL) {
      linkAfter(sibling);
    }
    else {
      linkBefore(prnt->FirstChild());
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
  ACodeNode* fnd = NULL;
  // ln > m_endLn means there is no child that contains ln
  if (ln <= m_endLn) {
    for (ANodeChildIterator it(this); it.Current(); it++) {
      fnd = dynamic_cast<ACodeNode*>(it.Current());
      DIAG_Assert(fnd, "");
      if  (fnd->containsLine(ln)) {
	if (fnd->type() == TyStmt) {
	  return fnd; // never look inside LINE_SCOPEs
	}
	
	// desired line might be in inner scope; however, it might be
	// elsewhere because optimization left procedure with
	// non-contiguous line ranges in scopes at various levels.
	ACodeNode* inner = fnd->ACodeNodeWithLine(ln);
	if (inner) return inner;
      }
    }
  }
  if (fnd && fnd->type() == TyProc) {
    return const_cast<ACodeNode*>(this);
  }
  else {
    return NULL;
  }
}


int
ACodeNode::compare(const ACodeNode* x, const ACodeNode* y)
{
  if (x->begLine() == y->begLine()) {
    bool endLinesEqual = (x->endLine() == y->endLine());
    if (endLinesEqual) {
      // We have two ACodeNode's with identical line intervals...
      
      // Use lexicographic comparison for procedures
      if (x->type() == ANode::TyProc && y->type() == ANode::TyProc) {
	Proc *px = (Proc*)x, *py = (Proc*)y;
	int cmp1 = px->name().compare(py->name());
	if (cmp1 != 0) { return cmp1; }
	int cmp2 = px->linkName().compare(py->linkName());
	if (cmp2 != 0) { return cmp2; }
      }
      
      // Use VMAInterval sets otherwise.
      bool x_lt_y = (x->vmaSet() < y->vmaSet());
      bool y_lt_x = (y->vmaSet() < x->vmaSet());
      bool vmaSetsEqual = (!x_lt_y && !y_lt_x);

      if (vmaSetsEqual) {
	// Try ranking a leaf node before a non-leaf node
	if ( !(x->isLeaf() && y->isLeaf())) {
	  if      (x->isLeaf()) { return -1; } // x < y
	  else if (y->isLeaf()) { return  1; } // x > y
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
      return SrcFile::compare(x->endLine(), y->endLine());
    }
  }
  else {
    return SrcFile::compare(x->begLine(), y->begLine());
  }
}


ACodeNode*
ACodeNode::nextSiblingNonOverlapping() const
{
  const ACodeNode* x = this;

  // FIXME: possibly convert this to static_cast
  const ACodeNode* y = dynamic_cast<ACodeNode*>(nextSibling()); // sorted
  while (y) {
    if (!x->containsLine(y->begLine())) {
      break;
    }
    y = dynamic_cast<const ACodeNode*>(y->nextSibling());
  }
  return const_cast<ACodeNode*>(y);
}


//***************************************************************************
// ACodeNode, etc: CodeName methods
//***************************************************************************

string
ACodeNode::lineRange() const
{
  string self = "l=" + StrUtil::toStr(m_begLn) + "-" + StrUtil::toStr(m_endLn);
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
  File* fileStrct = ancestorFile();
  LM* lmStrct = (fileStrct) ? fileStrct->ancestorLM() : NULL;
  string nm;
  if (lmStrct && fileStrct) {
    nm = "[" + lmStrct->name() + "]<" + fileStrct->name() + ">";
  }
  return nm;
}


string
Group::codeName() const
{
  string self = ANodeTyToName(type()) + " " + ACodeNode::codeName();
  return self;
}


string
File::codeName() const
{
  string nm;
  LM* lmStrct = ancestorLM();
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
// ANode, etc: XML output
//***************************************************************************

static const string XMLelements[ANode::TyNUMBER] = {
  "HPCToolkitStructure", "G", "LM", "F", "P", "A", "L", "S", "REF", "ANY"
};

const string&
ANode::ANodeTyToXMLelement(ANodeTy tp)
{
   return XMLelements[tp];
}


string
ANode::toStringXML(uint oFlags, const char* pre) const
{
  std::ostringstream os;
  writeXML(os, oFlags, pre);
  return os.str();
}


string
ANode::toXML(uint GCC_ATTR_UNUSED oFlags) const
{
  string self = ANodeTyToXMLelement(type()) + " i" + MakeAttrNum(id());
  return self;
}


string
ACodeNode::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags)
    + " " + XMLLineRange(oFlags) + " " + XMLVMAIntervals(oFlags);
  return self;
}


string
ACodeNode::XMLLineRange(uint GCC_ATTR_UNUSED oFlags) const
{
  string line = StrUtil::toStr(begLine());
  if (begLine() != endLine()) {
    line += "-" + StrUtil::toStr(endLine());
  }

  string self = "l" + xml::MakeAttrStr(line);
  return self;
}


string
ACodeNode::XMLVMAIntervals(uint GCC_ATTR_UNUSED oFlags) const
{
  string self = "v" + MakeAttrStr(m_vmaSet.toString(), xml::ESC_FALSE);
  return self;
}


string
Root::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags) + " n" + MakeAttrStr(m_name);
  return self;
}


string
Group::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags) + " n" + MakeAttrStr(m_name);
  return self;
}


string
LM::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags)
    + " n" + MakeAttrStr(m_name) + " " + XMLVMAIntervals(oFlags);
  return self;
}


string
File::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags) + " n" + MakeAttrStr(m_name);
  return self;
}


string
Proc::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags) + " n" + MakeAttrStr(m_name);
  if (!m_linkname.empty() && m_name != m_linkname) { // print if different
    self = self + " ln" + MakeAttrStr(m_linkname);
  }
  self = self + " " + XMLLineRange(oFlags) + " " + XMLVMAIntervals(oFlags);
  return self;
}


string
Alien::toXML(uint oFlags) const
{
  string self = ANode::toXML(oFlags)
    + " f" + MakeAttrStr(m_filenm) + " n" + MakeAttrStr(m_displaynm);
  self = self + " " + XMLLineRange(oFlags) + " " + XMLVMAIntervals(oFlags);

  // add information on the function definition
  // If the alien has an ancestor and the same procedure definition exists,
  // we'll add an attribute 'ln' to point to the ID of the function definition.
  // This is needed to inform hpcprof that the alien shouldn't be considered
  // as different function.
  
  // 1a: get the load module of the alien
  LM   *lm   = ancestorLM();

  // 1b: check if the alien has the file pointer to its definition
  File *file = lm->findFile(m_filenm);
  if (file) {
    
    // 2: check if there's the same procedure name in the file
    Proc *proc = file->findProc(m_displaynm);
    
    if (proc) {
#if 0
      // 3: check if alien's line range is within the function definition
      bool inc = SrcFile::include(proc->begLine(), proc->endLine(), begLine(), endLine());
      if (inc) {
	// 4: add the link attribute to the function definition
      	self = self + " ln" + xml::MakeAttrStr(StrUtil::toStr(proc->id())); 
      }
#endif
      self = self + " ln" + xml::MakeAttrStr(StrUtil::toStr(proc->id())); 
    }
  }
  return self;
}


string
Loop::toXML(uint oFlags) const
{
  string self = ACodeNode::toXML(oFlags) + " f" + MakeAttrStr(m_filenm);
  return self;
}


string
Stmt::toXML(uint oFlags) const
{
  string self = ACodeNode::toXML(oFlags);
  return self;
}


string
Ref::toXML(uint oFlags) const
{
  string self = ACodeNode::toXML(oFlags)
    + " b" + MakeAttrNum(begPos) + " e" + MakeAttrNum(endPos);
  return self;
}


bool
ANode::writeXML_pre(ostream& os, uint oFlags, const char* pfx) const
{
  bool doTag = isVisible();
  bool doMetrics = ((oFlags & Tree::OFlg_LeafMetricsOnly) ?
		    isLeaf() && hasMetrics() : hasMetrics());
  bool isXMLLeaf = isLeaf() && !doMetrics;

  // 1. Write element name
  if (doTag) {
    if (isXMLLeaf) {
      os << pfx << "<" << toXML(oFlags) << "/>\n";
    }
    else {
      os << pfx << "<" << toXML(oFlags) << ">\n";
    }
  }

  // 2. Write associated metrics
  if (doMetrics) {
    writeMetricsXML(os, Metric::IData::npos, Metric::IData::npos, oFlags, pfx);
    os << "\n";
  }
 
  return !isXMLLeaf; // whether to execute writeXML_post()
}


void
ANode::writeXML_post(ostream& os, uint GCC_ATTR_UNUSED oFlags,
		     const char* pfx) const
{
  bool doTag = isVisible();

  if (doTag) {
    os << pfx << "</" << ANodeTyToXMLelement(type()) << ">\n";
  }
}


ostream&
ANode::writeXML(ostream& os, uint oFlags, const char* pfx) const
{
  // indent childen of visible nodes
  string indent = isVisible() ? "  " : ""; 

  if (oFlags & Tree::OFlg_Compressed) {
    pfx = "";
    indent = "";
  }
 
  bool doPost = writeXML_pre(os, oFlags, pfx);
  string pfx_new = pfx + indent;
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByLine);
       it.current(); it++) {
    it.current()->writeXML(os, oFlags, pfx_new.c_str());
  }
  if (doPost) {
    writeXML_post(os, oFlags, pfx);
  }
  return os;
}


void
ANode::ddumpXML() const
{
  writeXML(std::cerr, Tree::OFlg_Debug);
}


ostream&
Root::writeXML(ostream& os, uint oFlags, const char* pfx) const
{
  if (oFlags & Tree::OFlg_Compressed) {
    pfx = "";
  }

  // N.B.: Assume that my children are LM's
  bool doPost = ANode::writeXML_pre(os, oFlags, pfx);
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByName);
       it.current(); it++) {
    ANode* scope = it.current();
    scope->writeXML(os, oFlags, pfx);
  }
  if (doPost) {
    ANode::writeXML_post(os, oFlags, pfx);
  }
  return os;
}


ostream&
LM::writeXML(ostream& os, uint oFlags, const char* pre) const
{
  string indent = "  ";
  if (oFlags & Tree::OFlg_Compressed) {
    pre = "";
    indent = "";
  }

  // N.B.: Assume my children are Files
  bool doPost = ANode::writeXML_pre(os, oFlags, pre);
  string prefix = pre + indent;
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByName);
       it.current(); it++) {
    ANode* scope = it.current();
    scope->writeXML(os, oFlags, prefix.c_str());
  }
  if (doPost) {
    ANode::writeXML_post(os, oFlags, pre);
  }
  return os;
}


//***************************************************************************
// ANode, etc: CSV output
//***************************************************************************

void
ANode::CSV_DumpSelf(const Root& root, ostream& os) const
{
  char temp[32];
  for (uint i = 0; i < numMetrics(); i++) {
    double val = (hasMetric(i) ? metric(i) : 0);
    os << "," << val;
#if 0
    // FIXME: tallent: Conversioon from static perf-table to MetricDescMgr
    const PerfMetric& metric = IndexToPerfDataInfo(i);
    bool percent = metric.Percent();
#else
    bool percent = true;
#endif

    if (percent) {
      double percVal = val / root.metric(i) * 100;
      sprintf(temp, "%5.2lf", percVal);
      os << "," << temp;
    }
  }
  os << endl;
}


void
ANode::CSV_dump(const Root& root, ostream& os,
		const char* GCC_ATTR_UNUSED file_name,
		const char* GCC_ATTR_UNUSED proc_name,
		int GCC_ATTR_UNUSED lLevel) const
{
  // print file name, routine name, start and end line, loop level
  os << name() << ",,,,";
  CSV_DumpSelf(root, os);
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByName);
       it.current(); it++) {
    it.current()->CSV_dump(root, os);
  }
}


void
File::CSV_dump(const Root& root, ostream& os,
	       const char* GCC_ATTR_UNUSED file_name,
	       const char* GCC_ATTR_UNUSED proc_name,
	       int GCC_ATTR_UNUSED lLevel) const
{
  // print file name, routine name, start and end line, loop level
  os << baseName() << ",," << m_begLn << "," << m_endLn << ",";
  CSV_DumpSelf(root, os);
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByName);
       it.current(); it++) {
    it.current()->CSV_dump(root, os, baseName().c_str());
  }
}


void
Proc::CSV_dump(const Root& root, ostream& os,
	       const char* file_name,
	       const char* GCC_ATTR_UNUSED proc_name,
	       int GCC_ATTR_UNUSED lLevel) const
{
  // print file name, routine name, start and end line, loop level
  os << file_name << "," << name() << "," << m_begLn << "," << m_endLn
     << ",0";
  CSV_DumpSelf(root, os);
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByLine);
       it.current(); it++) {
    it.current()->CSV_dump(root, os, file_name, name().c_str(), 1);
  }
}


void
Alien::CSV_dump(const Root& GCC_ATTR_UNUSED root, ostream& GCC_ATTR_UNUSED os,
		const char* GCC_ATTR_UNUSED file_name,
		const char* GCC_ATTR_UNUSED proc_name,
		int GCC_ATTR_UNUSED lLevel) const
{
  DIAG_Die(DIAG_Unimplemented);
}


void
ACodeNode::CSV_dump(const Root& root, ostream& os,
		    const char* file_name, const char* proc_name,
		    int lLevel) const
{
  ANodeTy myANodeTy = this->type();
  // do not display info for single lines
  if (myANodeTy == TyStmt)
    return;
  // print file name, routine name, start and end line, loop level
  os << (file_name ? file_name : name().c_str()) << ","
     << (proc_name ? proc_name : "") << ","
     << m_begLn << "," << m_endLn << ",";
  if (lLevel)
    os << lLevel;
  CSV_DumpSelf(root, os);
  for (ANodeSortedChildIterator it(this, ANodeSortedIterator::cmpByLine);
       it.current(); it++) {
    it.current()->CSV_dump(root, os, file_name, proc_name, lLevel+1);
  }
}


void
Root::CSV_TreeDump(ostream& os) const
{
  ANode::CSV_dump(*this, os);
}


//***************************************************************************
// ANode, etc: Output and Debugging support
//***************************************************************************

string
ANode::toString(uint oFlags, const char* pre) const
{
  std::ostringstream os;
  dump(os, oFlags, pre);
  return os.str();
}


string
ANode::toString_id(uint GCC_ATTR_UNUSED oFlags) const
{
  string str = "<" + ANodeTyToName(type()) + " uid="
    + StrUtil::toStr(id()) + ">";
  return str;
}


string
ANode::toStringMe(uint oFlags, const char* prefix) const
{
  std::ostringstream os;
  dumpme(os, oFlags, prefix);
  return os.str();
}


std::ostream&
ANode::dump(ostream& os, uint oFlags, const char* pre) const
{
  string prefix = string(pre) + "  ";

  dumpme(os, oFlags, pre);

  for (uint i = 0; i < numMetrics(); i++) {
    os << i << " = " ;
    if (hasMetric(i)) {
      os << metric(i);
    }
    else {
      os << "UNDEF";
    }
    os << "; ";
  }
 
  for (ANodeChildIterator it(this); it.Current(); it++) {
    it.current()->dump(os, oFlags, prefix.c_str());
  }
  return os;
}


void
ANode::ddump() const
{
  //writeXML(std::cerr, 0, "");
  dump(std::cerr, Tree::OFlg_Debug, "");
}


ostream&
ANode::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  os << prefix << toString_id(oFlags) << endl;
  return os;
}


ostream&
ACodeNode::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  os << prefix << toString_id(oFlags) << " "
     << lineRange() << " " << m_vmaSet.toString();
  return os;
}


ostream&
Root::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  os << prefix << toString_id(oFlags) << " n=" << m_name;
  return os;
}


ostream&
Group::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  os << prefix << toString_id(oFlags) << " n=" << m_name;
  return os;
}


ostream&
LM::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  os << prefix << toString_id(oFlags) << " n=" << m_name;
  return os;
}


ostream&
File::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  ACodeNode::dumpme(os, oFlags, prefix) << " n=" <<  m_name;
  return os;
}


ostream&
Proc::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  ACodeNode::dumpme(os, oFlags, prefix) << " n=" << m_name;
  return os;
}


ostream&
Alien::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  ACodeNode::dumpme(os, oFlags, prefix);
  return os;
}


ostream&
Loop::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  ACodeNode::dumpme(os, oFlags, prefix);
  return os;
}


ostream&
Stmt::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  ACodeNode::dumpme(os, oFlags, prefix);
  return os;
}


ostream&
Ref::dumpme(ostream& os, uint oFlags, const char* prefix) const
{
  ACodeNode::dumpme(os, oFlags, prefix);
  os << " pos:"  << begPos << "-" << endPos;
  return os;
}


void
LM::dumpmaps() const
{
  ostream& os = std::cerr;
 
  os << "Procedure map\n";
  for (VMAToProcMap::const_iterator it = m_procMap->begin();
       it != m_procMap->end(); ++it) {
    it->first.dump(os);
    os << " --> " << hex << "Ox" << it->second << dec << endl;
  }
  
  os << endl;

  os << "Statement map\n";
  for (VMAToStmtRangeMap::const_iterator it = m_stmtMap->begin();
       it != m_stmtMap->end(); ++it) {
    it->first.dump(os);
    os << " --> " << hex << "Ox" << it->second << dec << endl;
  }
}


//***************************************************************************
// Ref specific methods
//***************************************************************************

void
Ref::RelocateRef()
{
  Ref* prev = dynamic_cast<Ref*>(prevSibling());
  Ref* next = dynamic_cast<Ref*>(nextSibling());
  DIAG_Assert((prevSibling() == prev) && (nextSibling() == next), "");
  if (((!prev) || (prev->endPos <= begPos)) &&
      ((!next) || (next->begPos >= endPos))) {
    return;
  }
  ANode* prnt = parent();
  unlink();
  if (prnt->firstChild() == NULL) {
    link(prnt);
  }
  else {
    // insert after sibling with sibling->endPos < begPos
    // or iff that does not exist insert as first in sibling list
    ACodeNode* sibling;
    for (sibling = dynamic_cast<ACodeNode*>(prnt->lastChild());
	 sibling; sibling = dynamic_cast<ACodeNode*>(sibling->prevSibling())) {
      Ref *ref = dynamic_cast<Ref*>(sibling);
      DIAG_Assert(ref == sibling, "");
      if (ref->endPos < begPos) {
	break;
      }
    }
    if (sibling != NULL) {
      Ref *nxt = dynamic_cast<Ref*>(sibling->nextSibling());
      DIAG_Assert((nxt == NULL) || (nxt->begPos > endPos), "");
      linkAfter(sibling);
    }
    else {
      linkBefore(prnt->FirstChild());
    }
  }
}

//***************************************************************************

} // namespace Struct
} // namespace Prof

