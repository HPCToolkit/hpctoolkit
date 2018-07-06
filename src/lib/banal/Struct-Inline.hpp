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
// Copyright ((c)) 2002-2018, Rice University
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

// This file defines the API between symtabAPI and the banal code for
// the static inline support.  Now with support for building a tree of
// inlined call sites.
//
// For each VM address that came from inlined code, symtab provides a
// C++ list representing the sequence of inlining steps.  (The list
// front is outermost, back is innermost.)  An empty list means not
// inlined code (or else missing dwarf info).
//
// Each node in the list contains: (1) the file name and (2) line
// number in the source code, and (3) the procedure name at the call
// site.
//
// The inline TreeNode represents several InlineNode paths combined
// into a single tree.  Edges in the tree represent one (file, line,
// proc) call site and nodes contain a map of subtrees and a list of
// terminal statements.

//***************************************************************************

#ifndef Banal_Struct_Inline_hpp
#define Banal_Struct_Inline_hpp

#include <list>
#include <map>

#include <lib/isa/ISATypes.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/SrcFile.hpp>
#include <lib/support/StringTable.hpp>

#include <Symtab.h>
#include <Function.h>

class ElfFile;

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;

namespace Inline {

class InlineNode;
class FLPIndex;
class FLPCompare;
class StmtInfo;
class StmtMap;
class LoopInfo;
class TreeNode;

typedef list <InlineNode> InlineSeqn;
typedef list <FLPIndex> FLPSeqn;
typedef list <LoopInfo *> LoopList;
typedef map  <FLPIndex, TreeNode *, FLPCompare> NodeMap;


// File, proc, line we get from Symtab inline call sites.
class InlineNode {
private:
  std::string  m_filenm;
  std::string  m_procnm;
  std::string  m_prettynm;
  SrcFile::ln  m_lineno;

public:
  InlineNode(std::string &file, std::string &proc, std::string &pretty,
	     SrcFile::ln line)
  {
    m_filenm = file;
    m_procnm = proc;
    m_prettynm = pretty;
    m_lineno = line;
  }

  std::string & getFileName() { return m_filenm; }
  std::string & getProcName() { return m_procnm; }
  std::string & getPrettyName() { return m_prettynm; }
  SrcFile::ln getLineNum() { return m_lineno; }
};


// 3-tuple of indices for file, line, proc.
class FLPIndex {
public:
  long  file_index;
  long  base_index;
  long  line_num;
  long  proc_index;
  long  pretty_index;

  // constructor by index
  FLPIndex(long file, long base, long line, long proc, long pretty)
  {
    file_index = file;
    base_index = base;
    line_num = line;
    proc_index = proc;
    pretty_index = pretty;
  }

  // constructor by InlineNode strings
  FLPIndex(HPC::StringTable & strTab, InlineNode & node)
  {
    string & fname = node.getFileName();

    file_index = strTab.str2index(fname);
    base_index = strTab.str2index(FileUtil::basename(fname.c_str()));
    line_num = (long) node.getLineNum();
    proc_index = strTab.str2index(node.getProcName());
    pretty_index = strTab.str2index(node.getPrettyName());
  }

  bool operator == (const FLPIndex rhs)
  {
    return file_index == rhs.file_index
      && line_num == rhs.line_num
      && proc_index == rhs.proc_index;
  }

  bool operator != (const FLPIndex rhs)
  {
    return ! (*this == rhs);
  }
};


// Compare (file, line, proc) indices lexigraphically.
class FLPCompare {
public:
  bool operator() (const FLPIndex t1, const FLPIndex t2)
  {
    if (t1.file_index < t2.file_index) { return true; }
    if (t1.file_index > t2.file_index) { return false; }
    if (t1.line_num < t2.line_num) { return true; }
    if (t1.line_num > t2.line_num) { return false; }
    if (t1.proc_index < t2.proc_index) { return true; }
    return false;
  }
};


// Info for one terminal statement (vma) in the inline tree.  Now used
// for multiple, consecutive instructions, all with the same file and
// line info.
//
class StmtInfo {
public:
  VMA   vma;
  int   len;
  long  file_index;
  long  base_index;
  long  line_num;

  // constructor by index
  StmtInfo(VMA vm, int ln, long file, long base, long line)
  {
    vma = vm;
    len = ln;
    file_index = file;
    base_index = base;
    line_num = line;
  }

  // constructor by string name
  StmtInfo(HPC::StringTable & strTab, VMA vm, int ln,
	   const std::string & filenm, long line)
  {
    vma = vm;
    len = ln;
    file_index = strTab.str2index(filenm);
    base_index = strTab.str2index(FileUtil::basename(filenm.c_str()));
    line_num = line;
  }

  // returns: true if vma is contained within this range
  bool member(VMA x)
  {
    return vma <= x && x < vma + len;
  }
};


// Map of vma ranges and their file and line info.
//
// The intervals are disjoint, semi-open [...), the map key is by left
// endpoint, the addresses within a single range share a common file
// and line and may contain multiple instructions, and we compact
// adjacent ranges with identical attributes into one interval.
//
class StmtMap : public std::map <VMA, StmtInfo *> {
public:

  // returns: pointer to StmtInfo that contains vma, else NULL
  StmtInfo * findStmt(VMA vma)
  {
    auto it = this->upper_bound(vma);

    if (it == this->begin()) {
      return NULL;
    }
    --it;
    StmtInfo * sinfo = it->second;

    return (sinfo->member(vma)) ? sinfo : NULL;
  }

  // returns: true if vma is contained within some range in the map
  bool member(VMA vma)
  {
    return findStmt(vma) != NULL;
  }

  // insert one statement range into the map
  void insert(StmtInfo *);
};


// Info for one loop scope in the inline tree.  Note: 'node' is the
// TreeNode containing the loop vma without the FLP path above it.
class LoopInfo {
public:
  TreeNode *node;
  FLPSeqn   path;
  std::string  name;
  VMA   entry_vma;
  long  file_index;
  long  base_index;
  long  line_num;

  LoopInfo(TreeNode *nd, FLPSeqn &pth, const std::string &nm, VMA vma,
	   long file, long base, long line)
  {
    node = nd;
    path = pth;
    name = nm;
    entry_vma = vma;
    file_index = file;
    base_index = base;
    line_num = line;
  }

  // delete the subtree 'node' in ~TreeNode(), not here.
  ~LoopInfo() {
  }
};


// One node in the inline tree.
class TreeNode {
public:
  NodeMap  nodeMap;
  StmtMap  stmtMap;
  LoopList loopList;
  long  file_index;

  TreeNode(long file = 0)
  {
    nodeMap.clear();
    stmtMap.clear();
    loopList.clear();
    file_index = file;
  }

  // shallow delete: erase the maps, but don't delete their contents.
  // we use this when the elements are copies from other trees.
  void
  clear()
  {
    nodeMap.clear();
    stmtMap.clear();
    loopList.clear();
  }

  // recursively delete the stmts and subtrees
  ~TreeNode()
  {
    for (StmtMap::iterator sit = stmtMap.begin(); sit != stmtMap.end(); ++sit) {
      delete sit->second;
    }
    stmtMap.clear();

    for (LoopList::iterator lit = loopList.begin(); lit != loopList.end(); ++lit) {
      delete (*lit)->node;
    }
    loopList.clear();

    for (NodeMap::iterator nit = nodeMap.begin(); nit != nodeMap.end(); ++nit) {
      delete nit->second;
    }
    nodeMap.clear();
  }
};

//***************************************************************************

Symtab * openSymtab(ElfFile *elfFile);
bool closeSymtab();

bool analyzeAddr(InlineSeqn &nodelist, VMA addr);

void
addStmtToTree(TreeNode * root, HPC::StringTable & strTab, VMA vma,
	      int len, string & filenm, SrcFile::ln line);

void
mergeInlineStmts(TreeNode * dest, TreeNode * src);

void
mergeInlineEdge(TreeNode * dest, FLPIndex flp, TreeNode * src);

void
mergeInlineTree(TreeNode * dest, TreeNode * src);

void
mergeInlineLoop(TreeNode * dest, FLPSeqn & path, LoopInfo * info);

}  // namespace Inline

#endif
