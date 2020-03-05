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
// Copyright ((c)) 2002-2020, Rice University
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

// This file uses SymtabAPI to provide support for the location
// manager for sequences of inlined function calls.  Symtab provides
// the file name and line number for each step of a inlined sequence.
// This file just calls symtab and packages the results into a C++
// list for the location manager.
//
// Notes:
//
// 1. Symtab does not work cross platform.  You can't run symtab on
// x86 and analyze a ppc binary, or vice versa.  So, we isolate the
// code as much as possible, compile banal twice and link it into
// hpcstruct but not hpcprof.
//
// 2. Many symtab functions have asserts in the code.  In particular,
// openFile() on a binary of the wrong arch type raises SIGABRT rather
// than returning failure. (argh!)  We want the inline support to be
// optional, not required.  So, wrap the symtab calls with sigsetjmp()
// so that failure does not kill the process.
//
//   Note -- (2) is gone.  Symtab doesn't throw asserts very often
//   anymore, there isn't any reasonable way to continue if it did,
//   and this is broken for the parallel case.
//
// 3. Apologies for the single static buffer.  Ideally, we would read
// the dwarf info from inside lib/binutils and carry it down into
// makeStructure() and determineContext().  But that requires
// integrating symtab too deeply into our code.  We may rework this,
// especially if we rewrite the inline support directly from libdwarf
// (which is cross platform).

//***************************************************************************

#include <string.h>

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <lib/binutils/BinUtils.hpp>
#include <lib/binutils/ElfHelper.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/FileNameMap.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/StringTable.hpp>
#include <lib/support/dictionary.h>

#include "Struct-Inline.hpp"

#include <Module.h>
#include <Symtab.h>
#include <Function.h>

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;

// FIXME: uses a single static buffer.
static Symtab *the_symtab = NULL;

#define DEBUG_INLINE_SEQNS  0

//***************************************************************************

namespace Inline {

// These functions return true on success.
Symtab *
openSymtab(ElfFile *elfFile)
{
  bool ret = Symtab::openFile(the_symtab, elfFile->getMemory(),
			      elfFile->getLength(), elfFile->getFileName());

  if (! ret || the_symtab == NULL) {
    DIAG_WMsgIf(1, "SymtabAPI was unable to open: " << elfFile->getFileName());
    the_symtab = NULL;
    return NULL;
  }

  the_symtab->parseTypesNow();
  the_symtab->parseFunctionRanges();

  return the_symtab;
}

bool
closeSymtab()
{
  bool ret = false;

  if (the_symtab != NULL) {
    ret = Symtab::closeSymtab(the_symtab);
  }
  the_symtab = NULL;

  return ret;
}

//***************************************************************************

// Returns nodelist as a list of InlineNodes for the inlined sequence
// at VMA addr.  The front of the list is the outermost frame, back is
// innermost.
//
bool
analyzeAddr(InlineSeqn & nodelist, VMA addr, RealPathMgr * realPath)
{
  FunctionBase *func, *parent;
  bool ret = false;

  if (the_symtab == NULL) {
    return false;
  }
  nodelist.clear();

  if (the_symtab->getContainingInlinedFunction(addr, func) && func != NULL)
  {
    ret = true;

    parent = func->getInlinedParent();
    while (parent != NULL) {
      //
      // func is inlined iff it has a parent
      //
      InlinedFunction *ifunc = static_cast <InlinedFunction *> (func);
      pair <string, Offset> callsite = ifunc->getCallsite();
      string filenm = callsite.first;
      if (filenm != "") { realPath->realpath(filenm); }
      long lineno = callsite.second;

      // symtab does not provide mangled and pretty names for
      // inlined functions, so we have to decide this ourselves
      string procnm = func->getName();
      string prettynm =
	  (procnm == "") ? UNKNOWN_PROC : BinUtil::demangleProcName(procnm);

#if DEBUG_INLINE_SEQNS
      cout << "\n0x" << hex << addr << dec
	   << "  l=" << lineno << "  file:  " << filenm << "\n"
	   << "0x" << hex << addr << "  symtab:  " << procnm << "\n"
	   << "0x" << addr << dec << "  demang:  " << prettynm << "\n";
#endif

      nodelist.push_front(InlineNode(filenm, prettynm, lineno));

      func = parent;
      parent = func->getInlinedParent();
    }
  }

  return ret;
}

//***************************************************************************

// Insert one statement range into the map.
//
// Note: we pass the stmt info in 'sinfo', but we don't link sinfo
// itself into the map.  Instead, insert a copy.
//
// Note: call stmts never merge with adjacent stmts.  They are always
// a single instruction.
//
void
StmtMap::insert(StmtInfo * sinfo)
{
  if (sinfo == NULL) {
    return;
  }

  VMA  vma = sinfo->vma;
  VMA  end_vma = vma + sinfo->len;
  long file = sinfo->file_index;
  long base = sinfo->base_index;
  long line = sinfo->line_num;

  StmtInfo * info = NULL;
  StmtInfo * left = NULL;
  StmtInfo * right = NULL;
  VMA left_end = 0;

  // fixme: this is the single interval implementation (for now).
  // one stmt may merge with an adjacent stmt, but we assume that it
  // doesn't extend past its immediate neighbors.

  // find stmts left and right of vma, if they exist
  auto sit = this->upper_bound(vma);

  if (sit != this->begin()) {
    auto it = sit;  --it;
    left = it->second;
    left_end = left->vma + left->len;
  }
  if (sit != this->end()) {
    right = sit->second;
  }

  // compare vma with stmt to the left
  if (left == NULL || left_end < vma) {
    // intervals don't overlap, insert new one
    info = new StmtInfo(vma, end_vma - vma, file, base, line, sinfo->device,
			sinfo->is_call, sinfo->is_sink, sinfo->target);
    (*this)[vma] = info;
  }
  else if (left->base_index == base && left->line_num == line
	   && !left->is_call && !sinfo->is_call)
  {
    // intervals overlap and match file and line (and not calls).
    // merge with left stmt
    end_vma = std::max(end_vma, left_end);
    left->len = end_vma - left->vma;
    info = left;
  }
  else {
    // intervals overlap but don't match file and line
    // truncate interval to start at left_end and insert
    if (left_end < end_vma && (right == NULL || left_end < right->vma)) {
      vma = left_end;
      info = new StmtInfo(vma, end_vma - vma, file, base, line, sinfo->device,
			  sinfo->is_call, sinfo->is_sink, sinfo->target);
      (*this)[vma] = info;
    }
  }

  // compare interval with stmt to the right
  if (info != NULL && right != NULL && end_vma >= right->vma)
  {
    if (right->base_index == base && right->line_num == line
	&& !info->is_call && !right->is_call)
    {
      // intervals overlap and match file and line (and not calls).
      // merge with right stmt
      end_vma = right->vma + right->len;
      info->len = end_vma - info->vma;
      this->erase(sit);
      delete right;
    }
    else {
      // intervals overlap but don't match file and line
      // truncate info at right vma
      info->len = right->vma - info->vma;
    }
  }
}

//***************************************************************************

// Add one terminal statement to the inline tree and merge with
// adjacent stmts if their file and line match.
//
void
addStmtToTree(TreeNode * root, HPC::StringTable & strTab, RealPathMgr * realPath, 
              VMA vma, int len, string & filenm, SrcFile::ln line, 
              string & device, bool is_call, bool is_sink, VMA target)
{
  InlineSeqn path;
  TreeNode *node;

  analyzeAddr(path, vma, realPath);

  // follow 'path' down the tree and insert any edges that don't exist
  node = root;
  for (auto it = path.begin(); it != path.end(); ++it) {
    FLPIndex flp(strTab, *it);
    auto nit = node->nodeMap.find(flp);

    if (nit != node->nodeMap.end()) {
      node = nit->second;
    }
    else {
      // add new node with edge 'flp'
      TreeNode *child = new TreeNode();
      node->nodeMap[flp] = child;
      node = child;
    }
  }

  // insert statement at this level
  long file = strTab.str2index(filenm);
  long base = strTab.str2index(FileUtil::basename(filenm.c_str()));
  StmtInfo info(vma, len, file, base, line, device, is_call, is_sink, target);

  node->stmtMap.insert(&info);
}


// Merge the StmtInfo nodes from the 'src' node to 'dest' and delete
// them from src.  If there are duplicate statement vma's, then we
// keep the original.
//
void
mergeInlineStmts(TreeNode * dest, TreeNode * src)
{
  if (src == dest) {
    return;
  }

  for (auto sit = src->stmtMap.begin(); sit != src->stmtMap.end(); ++sit) {
    dest->stmtMap.insert(sit->second);
  }
  src->stmtMap.clear();
}


// Merge the edge 'flp' and tree 'src' into 'dest' tree.  Delete any
// leftover nodes from src that are not linked into dest.
//
void
mergeInlineEdge(TreeNode * dest, FLPIndex flp, TreeNode * src)
{
  auto dit = dest->nodeMap.find(flp);

  // if flp not in dest, then attach and done
  if (dit == dest->nodeMap.end()) {
    dest->nodeMap[flp] = src;
    return;
  }

  // merge src into dest's subtree with flp index
  mergeInlineTree(dit->second, src);
}


// Merge the tree 'src' into 'dest' tree.  Merge the statements, then
// iterate through src's subtrees.  Delete any leftover nodes from src
// that are not linked into dest.
//
void
mergeInlineTree(TreeNode * dest, TreeNode * src)
{
  if (src == dest) {
    return;
  }

  mergeInlineStmts(dest, src);
  src->stmtMap.clear();

  // merge the loops
  for (auto lit = src->loopList.begin(); lit != src->loopList.end(); ++lit) {
    dest->loopList.push_back(*lit);
  }
  src->loopList.clear();

  // merge the subtrees
  for (auto sit = src->nodeMap.begin(); sit != src->nodeMap.end(); ++sit) {
    mergeInlineEdge(dest, sit->first, sit->second);
  }
  src->nodeMap.clear();

  // now empty
  delete src;
}


// Merge the detached loop 'info' into tree 'dest'.  If dest is a
// detached loop, then 'path' is the FLP path above it.
void
mergeInlineLoop(TreeNode * dest, FLPSeqn & path, LoopInfo * info)
{
  // follow source and dest paths and remove any common prefix.
  auto dit = path.begin();
  auto sit = info->path.begin();

  while (dit != path.end() && sit != info->path.end() && *dit == *sit) {
    dit++;
    sit++;
  }

  // follow or insert the rest of source path in the dest tree.
  while (sit != info->path.end()) {
    FLPIndex flp = *sit;
    auto nit = dest->nodeMap.find(flp);
    TreeNode *node;

    if (nit != dest->nodeMap.end()) {
      node = nit->second;
    }
    else {
      node = new TreeNode();
      dest->nodeMap[flp] = node;
    }

    dest = node;
    sit++;
  }

  dest->loopList.push_back(info);
}

}  // namespace Inline
