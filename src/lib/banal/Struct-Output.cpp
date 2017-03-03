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
// Copyright ((c)) 2002-2017, Rice University
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

// This file writes an hpcstruct file directly from the internal
// TreeNode format (Struct-Inline.hpp) to an output stream without
// using the Prof::Struct objects (prof/Struct-Tree.hpp).
//
// Notes:
// 1. All output to the hpcstruct file should come from this file.
//
// 2. Opening the file and creating the ostream is handled in
// tool/hpcstruct/main.c and lib/support/IOUtil.hpp.
//
// 3. We allow ostream = NULL to mean that we don't want output.

// FIXME and TODO:
//
// 4. Add vma ranges v="{[0x400c2d-0x400c32) [0x400c8c-0x400c94)}"
// with multiple ranges.
//
// 5. Add full vma ranges for <P> proc tags.
//
// 7. Fix targ401420 names in <P> tags.
//
// 12. Better method for proc line num.
//
// 13. Handle unknown file/line more explicitly.
//
// 14. Deal with alien ln fields.

//***************************************************************************

#include <limits.h>

#include <list>
#include <map>
#include <ostream>
#include <string>

#include <lib/binutils/BinUtils.hpp>
#include <lib/isa/ISATypes.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/StringTable.hpp>
#include <lib/xml/xml.hpp>

#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"
#include "Struct-Skel.hpp"

#define INDENT  "  "
#define INIT_LM_INDEX  2

#define GUARD_NAME  "<inline>"

using namespace Inline;
using namespace std;

static long next_index;

static const char * hpcstruct_xml_head =
#include <lib/xml/hpc-structure.dtd.h>
  ;

//----------------------------------------------------------------------

// Helpers to generate fields inside tags.  The macros are designed to
// fit within << operators.

// this generates pre-order
#define INDEX  \
  " i=\"" << next_index++ << "\""

#define NUMBER(label, num)  \
  " " << label << "=\"" << num << "\""

#define STRING(label, str)  \
  " " << label << "=\"" << xml::EscapeStr(str) << "\""

#define VRANGE(vma, len)  \
  " v=\"{[0x" << hex << vma << "-0x" << vma + len << dec << ")}\""

static void
doIndent(ostream * os, int depth)
{
  for (int n = 1; n <= depth; n++) {
    *os << INDENT;
  }
}

//----------------------------------------------------------------------

namespace BAnal {
namespace Output {

typedef map <long, TreeNode *> AlienMap;

class ScopeInfo {
public:
  long  file_index;
  long  base_index;
  long  line_num;

  ScopeInfo(long file, long base, long line = 0)
  {
    file_index = file;
    base_index = base;
    line_num = line;
  }
};

static void
doTreeNode(ostream *, int, TreeNode *, ScopeInfo, HPC::StringTable &);

static void
doStmtList(ostream *, int, TreeNode *);

static void
doLoopList(ostream *, int, TreeNode *, HPC::StringTable &);

static void
locateTree(TreeNode *, ScopeInfo &, bool = false);

//----------------------------------------------------------------------

// DOCTYPE header and <HPCToolkitStructure> tag.
void
printStructFileBegin(ostream * os)
{
  if (os == NULL) {
    return;
  }

  *os << "<?xml version=\"1.0\"?>\n"
      << "<!DOCTYPE HPCToolkitStructure [\n"
      << hpcstruct_xml_head
      << "]>\n"
      << "<HPCToolkitStructure i=\"0\" version=\"4.6\" n=\"\">\n";
}

// Closing tag.
void
printStructFileEnd(ostream * os)
{
  if (os == NULL) {
    return;
  }

  *os << "</HPCToolkitStructure>\n";
  os->flush();
}

//----------------------------------------------------------------------

// Begin <LM> load module tag.
void
printLoadModuleBegin(ostream * os, string lmName)
{
  if (os == NULL) {
    return;
  }

  next_index = INIT_LM_INDEX;

  *os << "<LM"
      << INDEX
      << STRING("n", lmName)
      << " v=\"{}\">\n";
}

// Closing </LM> tag.
void
printLoadModuleEnd(ostream * os)
{
  if (os == NULL) {
    return;
  }

  *os << "</LM>\n";
}

//----------------------------------------------------------------------

// Begin <F> file tag.
void
printFileBegin(ostream * os, FileInfo * finfo)
{
  if (os == NULL || finfo == NULL) {
    return;
  }

  doIndent(os, 1);
  *os << "<F"
      << INDEX
      << STRING("n", finfo->name)
      << ">\n";
}

// Closing </F> tag.
void
printFileEnd(ostream * os, FileInfo * finfo)
{
  if (os == NULL || finfo == NULL) {
    return;
  }

  doIndent(os, 1);
  *os << "</F>\n";
}

//----------------------------------------------------------------------

// Entry point for <P> proc tag and its subtree.
void
printProc(ostream * os, FileInfo * finfo, ProcInfo * pinfo,
	  HPC::StringTable & strTab)
{
  if (os == NULL || finfo == NULL || pinfo == NULL) {
    return;
  }

  TreeNode * root = pinfo->root;
  long file_index = strTab.str2index(finfo->name);
  long base_index = strTab.str2index(FileUtil::basename(finfo->name.c_str()));
  ScopeInfo scope(file_index, base_index, pinfo->line_num);

  doIndent(os, 2);
  *os << "<P"
      << INDEX
      << STRING("n", pinfo->prettyName);

  if (pinfo->linkName != pinfo->prettyName) {
    *os << STRING("ln", pinfo->linkName);
  }
  *os << NUMBER("l", pinfo->line_num)
      << VRANGE(pinfo->entry_vma, 1)
      << ">\n";

  doTreeNode(os, 3, root, scope, strTab);

  doIndent(os, 2);
  *os << "</P>\n";
}

//----------------------------------------------------------------------

// Print the inline tree at 'root' and its statements, loops, inline
// subtrees and guard aliens.
//
// Note: 'scope' is the enclosing file scope such that stmts and loops
// that don't match this scope require a guard alien.
//
static void
doTreeNode(ostream * os, int depth, TreeNode * root, ScopeInfo scope,
	   HPC::StringTable & strTab)
{
  if (root == NULL) {
    return;
  }

  // partition the stmts and loops into separate trees by file (base)
  // name.  the ones that don't match the enclosing scope require a
  // guard alien.  this doesn't apply to inline subtrees.
  //
  AlienMap alienMap;

  // divide stmts by file name
  for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;
    VMA vma = sinfo->vma;
    TreeNode * node;

    // stmts with unknown file/line do not need a guard alien
    long base_index = sinfo->base_index;
    if (sinfo->line_num == 0) {
      base_index = scope.base_index;
    }

    auto ait = alienMap.find(base_index);
    if (ait != alienMap.end()) {
      node = ait->second;
    }
    else {
      node = new TreeNode(sinfo->file_index);
      alienMap[base_index] = node;
    }
    node->stmtMap[vma] = sinfo;
  }

  // divide loops by file name
  for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;
    TreeNode * node;

    // loops with unknown file/line do not need a guard alien
    long base_index = linfo->base_index;
    if (linfo->line_num == 0) {
      base_index = scope.base_index;
    }

    auto ait = alienMap.find(base_index);
    if (ait != alienMap.end()) {
      node = ait->second;
    }
    else {
      node = new TreeNode(linfo->file_index);
      alienMap[base_index] = node;
    }
    node->loopList.push_back(linfo);
  }

  // first, print the stmts and loops that don't need a guard alien.
  // delete the one tree node and remove it from the map.
  auto ait = alienMap.find(scope.base_index);

  if (ait != alienMap.end()) {
    TreeNode * node = ait->second;

    doStmtList(os, depth, node);
    doLoopList(os, depth, node, strTab);
    alienMap.erase(ait);
    node->clear();
    delete node;
  }

  // second, print the stmts and loops that do need a guard alien and
  // delete the remaining tree nodes.
  for (auto nit = alienMap.begin(); nit != alienMap.end(); ++nit) {
    TreeNode * node = nit->second;
    long file_index = node->file_index;
    long base_index = nit->first;
    ScopeInfo alien_scope(file_index, base_index);

    locateTree(node, alien_scope, true);

    // guard alien
    doIndent(os, depth);
    *os << "<A"
	<< INDEX
	<< NUMBER("l", alien_scope.line_num)
	<< STRING("f", strTab.index2str(file_index))
	<< STRING("n", GUARD_NAME)
	<< " v=\"{}\""
	<< ">\n";

    doStmtList(os, depth + 1, node);
    doLoopList(os, depth + 1, node, strTab);

    doIndent(os, depth);
    *os << "</A>\n";

    node->clear();
    delete node;
  }
  alienMap.clear();

  // inline call sites, use double alien
  for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;
    TreeNode * subtree = nit->second;
    string callname = BinUtil::demangleProcName(strTab.index2str(flp.proc_index));
    ScopeInfo subscope(0, 0);

    locateTree(subtree, subscope);

    // outer, caller alien.  use file and line from flp call site, but
    // empty proc name.
    doIndent(os, depth);
    *os << "<A"
	<< INDEX
	<< NUMBER("l", flp.line_num)
	<< STRING("f", strTab.index2str(flp.file_index))
	<< STRING("n", "")
	<< " v=\"{}\""
	<< ">\n";

    // inner, callee alien.  use proc name from flp call site, but
    // file and line from subtree.
    doIndent(os, depth + 1);
    *os << "<A"
	<< INDEX
	<< NUMBER("l", subscope.line_num)
	<< STRING("f", strTab.index2str(subscope.file_index))
	<< STRING("n", callname)
	<< " v=\"{}\""
	<< ">\n";

    doTreeNode(os, depth + 2, subtree, subscope, strTab);

    doIndent(os, depth + 1);
    *os << "</A>\n";

    doIndent(os, depth);
    *os << "</A>\n";
  }
}

//----------------------------------------------------------------------

// Print the terminal statements at 'node'.  Any guard alien, if
// needed, has already been printed.
//
static void
doStmtList(ostream * os, int depth, TreeNode * node)
{
  for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;

    doIndent(os, depth);
    *os << "<S"
	<< INDEX
	<< NUMBER("l", sinfo->line_num)
	<< VRANGE(sinfo->vma, sinfo->len)
	<< "/>\n";
  }
}

// Print the loops at 'node' and their subtrees.  Any guard alien, if
// needed, has already been printed.
//
static void
doLoopList(ostream * os, int depth, TreeNode * node, HPC::StringTable & strTab)
{
  for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;
    ScopeInfo scope(linfo->file_index, linfo->base_index);

    doIndent(os, depth);
    *os << "<L"
	<< INDEX
	<< NUMBER("l", linfo->line_num)
	<< STRING("f", strTab.index2str(linfo->file_index))
	<< VRANGE(linfo->entry_vma, 1)
	<< ">\n";

    doTreeNode(os, depth + 1, linfo->node, scope, strTab);

    doIndent(os, depth);
    *os << "</L>\n";
  }
}

//----------------------------------------------------------------------

// Try to guess the file and line location of a subtree based on info
// in the subtree and return the answer in 'scope'.
//
// This is used as the target location for alien nodes (both guard and
// double aliens).  Inline subtrees are the most reliable, then loops
// and statements.
//
// Note: if 'use_file' is true, then use the file from 'scope',
// otherwise try to guess the correct file.
//
static void
locateTree(TreeNode * node, ScopeInfo & scope, bool use_file)
{
  const long max_line = LONG_MAX;

  if (node == NULL) {
    return;
  }

  // first, find the correct file and base.  if use_file is true, then
  // use the answer in 'scope', else try to guess the file.
  //
  if (! use_file) {

    // inline subtrees are the most reliable
    for (auto nit = node->nodeMap.begin(); nit != node->nodeMap.end(); ++nit) {
      FLPIndex flp = nit->first;

      if (flp.line_num != 0) {
	scope.file_index = flp.file_index;
	scope.base_index = flp.base_index;
	goto found_file;
      }
    }

    // next, try loops
    for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
      LoopInfo * linfo = *lit;

      if (linfo->line_num != 0) {
	scope.file_index = linfo->file_index;
	scope.base_index = linfo->base_index;
	goto found_file;
      }
    }

    // finally, try stmts
    for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
      StmtInfo * sinfo = sit->second;

      if (sinfo->line_num != 0) {
	scope.file_index = sinfo->file_index;
	scope.base_index = sinfo->base_index;
	goto found_file;
      }
    }
  }
found_file:

  // second, guess the line number.  rescan the inline trees and loops
  // and take the min line number among files that match.
  //
  scope.line_num = max_line;

  for (auto nit = node->nodeMap.begin(); nit != node->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    if (flp.base_index == scope.base_index && flp.line_num > 0
	&& flp.line_num < scope.line_num) {
      scope.line_num = flp.line_num;
    }
  }	
 
  for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;

    if (linfo->base_index == scope.base_index && linfo->line_num > 0
	&& linfo->line_num < scope.line_num) {
      scope.line_num = linfo->line_num;
    }
  }

  // found valid line number
  if (scope.line_num < max_line) {
    return;
  }

  // use stmts as a backup only if no inlines or loops
  for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;

    if (sinfo->base_index == scope.base_index && sinfo->line_num > 0
	&& sinfo->line_num < scope.line_num) {
      scope.line_num = sinfo->line_num;
    }
  }

  // if nothing matches, then 0 is the unknown line number
  if (scope.line_num == max_line) {
    scope.line_num = 0;
  }
}

}  // namespace Output
}  // namespace BAnal
