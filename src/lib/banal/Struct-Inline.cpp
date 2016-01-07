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
// But be careful about overusing sigsetjmp().  There is only one
// handler per process, so this could break other libraries or the
// main program.
//
// 3. Apologies for the single static buffer.  Ideally, we would read
// the dwarf info from inside lib/binutils and carry it down into
// makeStructure() and determineContext().  But that requires
// integrating symtab too deeply into our code.  We may rework this,
// especially if we rewrite the inline support directly from libdwarf
// (which is cross platform).

//***************************************************************************

#include <setjmp.h>
#include <signal.h>
#include <string.h>

#include <list>
#include <vector>

#include <lib/support/diagnostics.h>
#include <lib/support/FileNameMap.hpp>
#include <lib/support/realpath.h>
#include <lib/support/StringTable.hpp>

#include "Struct-Inline.hpp"

#include <Symtab.h>
#include <Function.h>

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;

static const string UNKNOWN_PROC ("unknown-proc");

// FIXME: uses a single static buffer.
static Symtab *the_symtab = NULL;

static struct sigaction old_act_abrt;
static struct sigaction old_act_segv;
static sigjmp_buf jbuf;
static int jbuf_active = 0;

static int num_queries = 0;
static int num_errors = 0;

//***************************************************************************

static void
banal_sighandler(int sig)
{
  if (jbuf_active) {
    siglongjmp(jbuf, 1);
  }

  // caught a signal, but it didn't come from symtab
  DIAG_Die("banal caught unexpected signal " << sig);
}

static void
init_sighandler(void)
{
  struct sigaction act;

  memset(&act, 0, sizeof(act));
  act.sa_handler = banal_sighandler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);

  jbuf_active = 0;
  sigaction(SIGABRT, &act, &old_act_abrt);
  sigaction(SIGSEGV, &act, &old_act_segv);
}

static void
restore_sighandler(void)
{
  sigaction(SIGABRT, &old_act_abrt, NULL);
  sigaction(SIGSEGV, &old_act_segv, NULL);
  jbuf_active = 0;
}

//***************************************************************************

namespace Inline {

// These functions return true on success.
Symtab *
openSymtab(string filename)
{
  bool ret = false;

  init_sighandler();
  num_queries = 0;
  num_errors = 0;

  if (sigsetjmp(jbuf, 1) == 0) {
    // normal return
    jbuf_active = 1;
    ret = Symtab::openFile(the_symtab, filename);
    if (ret) {
      the_symtab->parseTypesNow();
    }
  }
  else {
    // error return
    ret = false;
  }
  jbuf_active = 0;

  if (! ret) {
    DIAG_WMsgIf(1, "SymtabAPI was unable to open: " << filename);
    DIAG_WMsgIf(1, "The static inline support does not work cross platform,");
    DIAG_WMsgIf(1, "so check that this file has the same arch type as hpctoolkit.");
    the_symtab = NULL;
  }

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

  restore_sighandler();

  if (num_errors > 0) {
    DIAG_WMsgIf(1, "SymtabAPI had " << num_errors << " errors in "
		<< num_queries << " queries.");
  }

  return ret;
}

// Returns nodelist as a list of InlineNodes for the inlined sequence
// at VMA addr.  The front of the list is the outermost frame, back is
// innermost.
//
bool
analyzeAddr(InlineSeqn &nodelist, VMA addr)
{
  FunctionBase *func, *parent;
  bool ret = false;

  if (the_symtab == NULL) {
    return false;
  }
  nodelist.clear();

  num_queries++;

  if (sigsetjmp(jbuf, 1) == 0) {
    //
    // normal return
    //
    jbuf_active = 1;
    if (the_symtab->getContainingInlinedFunction(addr, func))
    {
      parent = func->getInlinedParent();
      while (parent != NULL) {
	//
	// func is inlined iff it has a parent
	//
	InlinedFunction *ifunc = static_cast <InlinedFunction *> (func);
	pair <string, Offset> callsite = ifunc->getCallsite();

#ifdef SYMTAB_NEW_NAME_ITERATOR
	string procnm = func->getName();
        if (procnm == "") { procnm = UNKNOWN_PROC; }
#else
	vector <string> name_vec = func->getAllMangledNames();
	string procnm = (! name_vec.empty()) ? name_vec[0] : UNKNOWN_PROC;
#endif
	string &filenm = getRealPath(callsite.first.c_str());
	long lineno = callsite.second;
	nodelist.push_front(InlineNode(filenm, procnm, lineno));

	func = parent;
	parent = func->getInlinedParent();
      }
      ret = true;
    }
  }
  else {
    // error return
    num_errors++;
    ret = false;
  }
  jbuf_active = 0;

  return ret;
}


// Add one terminal statement to the inline tree.
StmtInfo *
addStmtToTree(TreeNode * root, StringTable & strTab, VMA vma, int len,
	      string & filenm, SrcFile::ln line, string & procnm)
{
  InlineSeqn path;
  TreeNode *node;

  analyzeAddr(path, vma);

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

  // add statement to last node in the sequence.  if there are
  // duplicates, then keep the original.
  auto sit = node->stmtMap.find(vma);
  StmtInfo *info;

  if (sit != node->stmtMap.end()) {
    info = sit->second;
  }
  else {
    info = new StmtInfo(strTab, vma, len, filenm, line, procnm);
    node->stmtMap[vma] = info;
  }

  return info;
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
    VMA vma = sit->first;
    auto dit = dest->stmtMap.find(vma);

    if (dit == dest->stmtMap.end()) {
      dest->stmtMap[vma] = sit->second;
    }
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
