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
// 9. Add stmts and loops that use guard aliens.
//
// 10. Rework locateSubtree() to better identify callee scope.
//
// 11. Demangle proc names.
//
// 12. Better method for proc line num.
//
// 13. Handle unknown file/line more explicitly.

//***************************************************************************

#include <list>
#include <map>
#include <ostream>
#include <string>

#include <CFG.h>

#include <lib/support/FileUtil.hpp>
#include <lib/support/StringTable.hpp>
#include <lib/xml/xml.hpp>

#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"
#include "Struct-Skel.hpp"

#define INDENT  "  "
#define INIT_LM_INDEX  2

using namespace Dyninst;
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

class ScopeInfo {
public:
  long  base_index;

  ScopeInfo(long base)
  {
    base_index = base;
  }
};

static void
doTreeNode(ostream *, int, TreeNode *, ScopeInfo, HPC::StringTable &);

static void
locateSubtree(TreeNode *, long &, long &);

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

  ParseAPI::Function * func = pinfo->func;
  TreeNode * root = pinfo->root;
  long base_index = strTab.str2index(FileUtil::basename(finfo->name.c_str()));

  doIndent(os, 2);
  *os << "<P"
      << INDEX
      << STRING("n", pinfo->prettyName);

  if (pinfo->linkName != pinfo->prettyName) {
    *os << STRING("ln", pinfo->linkName);
  }
  *os << NUMBER("l", pinfo->line_num)
      << VRANGE(func->addr(), 1)
      << ">\n";

  doTreeNode(os, 3, root, ScopeInfo(base_index), strTab);

  doIndent(os, 2);
  *os << "</P>\n";
}

//----------------------------------------------------------------------

static void
doTreeNode(ostream * os, int depth, TreeNode * root, ScopeInfo scinfo,
	   HPC::StringTable & strTab)
{
  if (root == NULL) {
    return;
  }

  // statements that match this scope (no guard alien)
  for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;

    if (1 || sinfo->base_index == scinfo.base_index) {
      doIndent(os, depth);
      *os << "<S"
	  << INDEX
	  << NUMBER("l", sinfo->line_num)
	  << VRANGE(sinfo->vma, sinfo->len)
	  << "/>\n";
    }
  }

  // loops that match this scope (no guard alien)
  for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;

    if (1 || linfo->base_index == scinfo.base_index) {
      doIndent(os, depth);
      *os << "<L"
	  << INDEX
	  << NUMBER("l", linfo->line_num)
	  << STRING("f", strTab.index2str(linfo->file_index))
	  << VRANGE(linfo->entry_vma, 1)
	  << ">\n";

      doTreeNode(os, depth + 1, linfo->node, ScopeInfo(linfo->base_index), strTab);

      doIndent(os, depth);
      *os << "</L>\n";
    }
  }

  // inline call sites, use double alien
  for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;
    TreeNode * subtree = nit->second;
    long subtree_file;
    long subtree_line;

    locateSubtree(subtree, subtree_file, subtree_line);

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
	<< NUMBER("l", subtree_line)
	<< STRING("f", strTab.index2str(subtree_file))
	<< STRING("n", strTab.index2str(flp.proc_index))
	<< " v=\"{}\""
	<< ">\n";

    doTreeNode(os, depth + 2, subtree, ScopeInfo(flp.base_index), strTab);

    doIndent(os, depth + 1);
    *os << "</A>\n";

    doIndent(os, depth);
    *os << "</A>\n";
  }
}

//----------------------------------------------------------------------

static void
locateSubtree(TreeNode * tree, long & file, long & line)
{
  // try inline subtree
  if (! tree->nodeMap.empty()) {
    FLPIndex flp = tree->nodeMap.begin()->first;
    file = flp.file_index;
    line = flp.line_num;
    return;
  }

  // try loop
  if (! tree->loopList.empty()) {
    LoopInfo * linfo = *(tree->loopList.begin());
    file = linfo->file_index;
    line = linfo->line_num;
    return;
  }

  // try stmt
  if (! tree->stmtMap.empty()) {
    StmtInfo * sinfo = tree->stmtMap.begin()->second;
    file = sinfo->file_index;
    line = sinfo->line_num;
    return;
  }

  // should not happen
  file = 0;
  line = 0;
}

}  // namespace Output
}  // namespace BAnal
