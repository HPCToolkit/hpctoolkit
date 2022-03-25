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
// 5. Add full vma ranges for <P> proc tags.
// ---> is this needed ?
//
// 14. Deal with alien ln fields.
// ---> is this needed ?

//***************************************************************************

#include <sys/types.h>
#include <limits.h>

#include <list>
#include <map>
#include <ostream>
#include <string>

#include <lib/binutils/VMAInterval.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/StringTable.hpp>
#include <lib/support/dictionary.h>

#include <lib/xml/xml.hpp>

#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"
#include "Struct-Skel.hpp"

#define INDENT  "  "
#define INIT_LM_INDEX  2

using namespace Inline;
using namespace std;

static long next_index;
static long gaps_line;
static bool pretty_print_output;

static const char * hpcstruct_xml_head =
#include <lib/xml/hpc-structure.dtd.h>
  ;

// temp options for call <C> tags, target (t) field, and device (d) field
#define ENABLE_CALL_TAGS     1
#define ENABLE_TARGET_FIELD  1
#define ENABLE_DEVICE_FIELD  1

#define USE_INDEX_NUMBERS  1

//----------------------------------------------------------------------

// Helpers to generate fields inside tags.  The macros are designed to
// fit within << operators.

#define MARKER  "<>"
#define MARKER_LEN  2

#define GAPS_MARKER   "<>g"

#if USE_INDEX_NUMBERS
// this generates pre-order
#define INDEX  " i=\"" << next_index++ << "\""
#define INDEX_MARKER  "<>i"

#else
#define INDEX         ""
#define INDEX_MARKER  ""
#endif

#define NUMBER(label, num)  \
  " " << label << "=\"" << num << "\""

#define HEX(label, num)  \
  " " << label << "=\"0x" << hex << num << dec << "\""

#define STRING(label, str)  \
  " " << label << "=\"" << xml::EscapeStr(str) << "\""

#define VRANGE(vma, len)  \
  " v=\"{[0x" << hex << vma << "-0x" << vma + len << dec << ")}\""

static void
doIndent(ostream * os, int depth)
{
  if (pretty_print_output) {
    for (int n = 1; n <= depth; n++) {
      *os << INDENT;
    }
  }
}

//----------------------------------------------------------------------

namespace BAnal {
namespace Output {

typedef map <long, TreeNode *> AlienMap;
typedef map <long, VMAIntervalSet *> LineNumberMap;

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
doGaps(ostream *, ostream *, string, FileInfo *, GroupInfo *, ProcInfo *);

static void
doTreeNode(ostream *, int, TreeNode *, ScopeInfo, HPC::StringTable &);

static void
doStmtList(ostream *, int, TreeNode *);

static void
doLoopList(ostream *, int, TreeNode *, HPC::StringTable &);

static void
locateTree(TreeNode *, ScopeInfo &, HPC::StringTable &, bool = false);

//----------------------------------------------------------------------

// Turn on indenting in XML output
void setPrettyPrint(bool _pretty_print_output)
{
  pretty_print_output = _pretty_print_output;
}

//----------------------------------------------------------------------

// Sort StmtInfo by line number and then by vma.
static bool
StmtLessThan(StmtInfo * s1, StmtInfo * s2)
{
  if (s1->line_num < s2->line_num) { return true; }
  if (s1->line_num > s2->line_num) { return false; }
  return s1->vma < s2->vma;
}

//----------------------------------------------------------------------

// DOCTYPE header and <HPCToolkitStructure> tag.
void
printStructFileBegin(ostream * os, ostream * gaps, string filenm)
{
  if (os == NULL) {
    return;
  }

  *os << "<?xml version=\"1.0\"?>\n"
      << "<!DOCTYPE HPCToolkitStructure [\n"
      << hpcstruct_xml_head
      << "]>\n"
      << "<HPCToolkitStructure i=\"0\" version=\"4.7\" n=\"\">\n";

  if (gaps != NULL) {
    *gaps << "This file describes the unclaimed vma ranges (gaps) in the control\n"
	  << "flow graph for the following file.  This is mostly for debugging and\n"
	  << "improving ParseAPI.\n\n"
	  << filenm << "\n";
    gaps_line = 5;
  }
}

// Closing tag.
void
printStructFileEnd(ostream * os, ostream * gaps)
{
  if (os == NULL) {
    return;
  }

  *os << "</HPCToolkitStructure>\n";
  os->flush();

  if (gaps != NULL) {
    gaps->flush();
  }
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
      << STRING("n", finfo->fileName)
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
//
// This now does internal formatting of everything except index and
// gap fields to a string stream buffer.  Note: this runs concurrently
// with other procs, so must not touch internal state.  So, the index
// and gap fields must be delayed.
//
void
earlyFormatProc(ostream * os, FileInfo * finfo, GroupInfo * ginfo, ProcInfo * pinfo,
		bool do_gaps, HPC::StringTable & strTab)
{
  if (os == NULL || finfo == NULL || ginfo == NULL
      || pinfo == NULL || pinfo->root == NULL) {
    return;
  }

  TreeNode * root = pinfo->root;
  long file_index = strTab.str2index(finfo->fileName);
  long base_index = strTab.str2index(FileUtil::basename(finfo->fileName.c_str()));
  ScopeInfo scope(file_index, base_index, pinfo->line_num);

  doIndent(os, 2);
  *os << "<P"
      << INDEX_MARKER
      << STRING("n", pinfo->prettyName);

  if (pinfo->linkName != pinfo->prettyName) {
    *os << STRING("ln", pinfo->linkName);
  }
  if (pinfo->symbol_index != 0) {
    *os << NUMBER("s", pinfo->symbol_index);
  }
  *os << NUMBER("l", pinfo->line_num)
      << VRANGE(pinfo->entry_vma, 1)
      << ">\n";

  // write the gaps to the first proc (low vma) of the group.  this
  // only applies to full gaps.
  if (do_gaps && (! ginfo->alt_file) && pinfo == ginfo->procMap.begin()->second) {
    *os << GAPS_MARKER;
  }

  doTreeNode(os, 3, root, scope, strTab);

  doIndent(os, 2);
  *os << "</P>\n";
}

//----------------------------------------------------------------------

//
// Do the final translation of the index and gaps markers (<>i, <>g)
// from 'buf' and write the output to 'os' (hpcstruct file) and 'gaps'
// (gaps file, if used).  This part is run serial, inside a lock.
//
// Note: this is called once per group, where pinfo is the group leader.
//
void
finalPrintProc(ostream * os, ostream * gaps, string & buf, string & gaps_filenm,
	       FileInfo * finfo, GroupInfo * ginfo, ProcInfo * pinfo)
{
  // if no index or gaps, then nothing to translate, just dump 'buf'
  // directly to 'os'
#if ! USE_INDEX_NUMBERS
  if (gaps == NULL) {
    *os << buf;
    return;
  }
#endif

  size_t start = 0;

  for (;;) {
    size_t pos = buf.find(MARKER, start);

    // no more markers to translate
    if (pos == string::npos) {
      *os << buf.substr(start);
      break;
    }

    *os << buf.substr(start, pos - start);

    switch (buf[pos + MARKER_LEN]) {
    case 'g':
      doGaps(os, gaps, gaps_filenm, finfo, ginfo, pinfo);
      break;

    case 'i':
      *os << INDEX;
      break;

    default:
      cerr << "internal error (unknown marker) in hpcstruct: "
	   << buf.substr(pos, 3) << "\n";
      break;
    }

    start = pos + MARKER_LEN + 1;
  }
}

//----------------------------------------------------------------------

// Write the unclaimed vma ranges (parseapi gaps) for one Symtab
// function to the .hpcstruct and .hpcstruct.gaps files.  This only
// applies to the group leader.
//
// This is the full version -- each gap has a separate <S> stmt linked
// to the .gaps file, all within a alien scope.  The basic version is
// folded into the inline tree in Struct.cpp.
//
static void
doGaps(ostream * os, ostream * gaps, string gaps_file,
       FileInfo * finfo, GroupInfo * ginfo, ProcInfo * pinfo)
{
  if (gaps == NULL || ginfo->gapSet.empty()) {
    return;
  }

  *gaps << "\nfunc:  " << pinfo->prettyName << "\n"
	<< "link:  " << pinfo->linkName << "\n"
	<< "file:  " << finfo->fileName << "  line: " << pinfo->line_num << "\n"
	<< "0x" << hex << ginfo->start << "--0x" << ginfo->end << dec << "\n\n";
  gaps_line += 6;

  doIndent(os, 3);
  *os << "<A"
      << INDEX
      << NUMBER("l", pinfo->line_num)
      << STRING("f", finfo->fileName)
      << STRING("n", "")
      << " v=\"{}\""
      << ">\n";

  doIndent(os, 4);
  *os << "<A"
      << INDEX
      << NUMBER("l", gaps_line - 4)
      << STRING("f", gaps_file)
      << STRING("n", "unclaimed region in: " + pinfo->prettyName)
      << " v=\"{}\""
      << ">\n";

  for (auto git = ginfo->gapSet.begin(); git != ginfo->gapSet.end(); ++git) {
    long start = git->beg();
    long end = git->end();
    long len = end - start;

    *gaps << "gap:  0x" << hex << start << "--0x" << end
	  << dec << "  (" << len << ")\n";
    gaps_line++;

    doIndent(os, 5);
    *os << "<S"
	<< INDEX
	<< NUMBER("l", gaps_line)
	<< VRANGE(start, len)
	<< "/>\n";
  }

  doIndent(os, 4);
  *os << "</A>\n";

  doIndent(os, 3);
  *os << "</A>\n";
}

//----------------------------------------------------------------------

// Print the inline tree at 'root' and its statements, loops, inline
// subtrees and guard aliens.
//
// Note: 'scope' is the enclosing file scope such that stmts and loops
// that don't match this scope require a guard alien.  This includes
// stmts with an earlier line number (hpcprof reacts badly to this
// inside a proc scope).
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
  // localNode contains the stmts and loops without a guard alien.
  //
  AlienMap alienMap;
  TreeNode localNode;

  // divide stmts by file name
  for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;
    VMA vma = sinfo->vma;
    TreeNode * node;

    // use guard alien if files don't match or if earlier line number
    // in same file (but must be known).  stmts with unknown file/line
    // do not need a guard alien.
    if (sinfo->line_num > 0
	&& (sinfo->base_index != scope.base_index || sinfo->line_num < scope.line_num))
    {
      auto ait = alienMap.find(sinfo->base_index);
      if (ait != alienMap.end()) {
	node = ait->second;
      }
      else {
	node = new TreeNode(sinfo->file_index);
	alienMap[sinfo->base_index] = node;
      }
    }
    else {
      // not a guard alien
      node = &localNode;
    }

    node->stmtMap[vma] = sinfo;
  }

  // divide loops by file name
  for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;
    TreeNode * node;

    // use guard alien if files don't match or if earlier line number
    // in same file (but must be known).  loops with unknown file/line
    // do not need a guard alien.
    if (linfo->line_num > 0
	&& (linfo->base_index != scope.base_index || linfo->line_num < scope.line_num))
    {
      auto ait = alienMap.find(linfo->base_index);
      if (ait != alienMap.end()) {
	node = ait->second;
      }
      else {
	node = new TreeNode(linfo->file_index);
	alienMap[linfo->base_index] = node;
      }
    }
    else {
      // not a guard alien
      node = &localNode;
    }

    node->loopList.push_back(linfo);
  }

  // first, print the stmts with no alien
  doStmtList(os, depth, &localNode);

  // second, print the stmts and loops that do need a guard alien and
  // delete the remaining tree nodes.
  for (auto nit = alienMap.begin(); nit != alienMap.end(); ++nit) {
    TreeNode * node = nit->second;
    long file_index = node->file_index;
    long base_index = nit->first;
    ScopeInfo alien_scope(file_index, base_index);

    locateTree(node, alien_scope, strTab, true);

    // guard alien
    doIndent(os, depth);
    *os << "<A"
	<< INDEX_MARKER
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

  // third, print the loops with no alien
  doLoopList(os, depth, &localNode, strTab);
  localNode.clear();

  // inline call sites, use double alien
  for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;
    TreeNode * subtree = nit->second;
    string callname = strTab.index2str(flp.pretty_index);
    ScopeInfo subscope(0, 0);

    locateTree(subtree, subscope, strTab);

    // outer, caller alien.  use file and line from flp call site, but
    // empty proc name.
    doIndent(os, depth);
    *os << "<A"
	<< INDEX_MARKER
	<< NUMBER("l", flp.line_num)
	<< STRING("f", strTab.index2str(flp.file_index))
	<< STRING("n", "")
	<< " v=\"{}\""
	<< ">\n";

    // inner, callee alien.  use proc name from flp call site, but
    // file and line from subtree.
    doIndent(os, depth + 1);
    *os << "<A"
	<< INDEX_MARKER
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

// Print the terminal statements at 'node' as <S> and <C> tags.
// Non-call <S> stmts combine their vma ranges by line number.
// Call <C> stmts are always single instructions and never merged.
//
// Any guard alien, if needed, has already been printed.
//
static void
doStmtList(ostream * os, int depth, TreeNode * node)
{
  LineNumberMap lineMap;
  vector <StmtInfo *> callVec;

  // split StmtInfo's into call and non-call sets.  the non-call stmts
  // with the same line number are merged into a single vma set.  call
  // stmts are never merged.
  for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;

    if (sinfo->is_call && ENABLE_CALL_TAGS) {
      callVec.push_back(sinfo);
    }
    else {
      VMAIntervalSet * vset = NULL;
      auto mit = lineMap.find(sinfo->line_num);

      if (mit != lineMap.end()) {
	vset = mit->second;
      }
      else {
	vset = new VMAIntervalSet;
	lineMap[sinfo->line_num] = vset;
      }
      vset->insert(sinfo->vma, sinfo->vma + sinfo->len);
    }
  }

  std::sort(callVec.begin(), callVec.end(), StmtLessThan);

  // print non-call vma set as a single <S> stmt
  for (auto mit = lineMap.begin(); mit != lineMap.end(); ++mit) {
    long line = mit->first;
    VMAIntervalSet * vset = mit->second;

    doIndent(os, depth);
    *os << "<S"
	<< INDEX_MARKER
	<< NUMBER("l", line)
	<< " v=\"" << vset->toString() << "\""
	<< "/>\n";

    delete vset;
  }

  // print each call stmt as a separate <C> tag
  for (uint i = 0; i < callVec.size(); i++) {
    StmtInfo * sinfo = callVec[i];

    doIndent(os, depth);
    *os << "<C"
	<< INDEX_MARKER
	<< NUMBER("l", sinfo->line_num)
	<< VRANGE(sinfo->vma, sinfo->len);

    if (! sinfo->is_sink && ENABLE_TARGET_FIELD) {
      *os << HEX("t", sinfo->target);
    }
    if (ENABLE_DEVICE_FIELD) {
      *os << STRING("d", sinfo->device);
    }
    *os << "/>\n";
  }
}

//----------------------------------------------------------------------

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
	<< INDEX_MARKER
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
locateTree(TreeNode * node, ScopeInfo & scope, HPC::StringTable & strTab, bool use_file)
{
  const long max_line = LONG_MAX;
  long empty_index = strTab.str2index("");

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

      if (flp.file_index != empty_index) {
	scope.file_index = flp.file_index;
	scope.base_index = flp.base_index;
	goto found_file;
      }
    }

    // next, try loops
    for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
      LoopInfo * linfo = *lit;

      if (linfo->file_index != empty_index) {
	scope.file_index = linfo->file_index;
	scope.base_index = linfo->base_index;
	goto found_file;
      }
    }

    // finally, try stmts
    for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
      StmtInfo * sinfo = sit->second;

      if (sinfo->file_index != empty_index) {
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
