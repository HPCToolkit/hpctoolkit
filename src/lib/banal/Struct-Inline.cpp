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
// Copyright ((c)) 2002-2019, Rice University
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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <lib/binutils/BinUtils.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/FileNameMap.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/StringTable.hpp>

#include "ElfHelper.hpp"
#include "Struct-Inline.hpp"

#include <Module.h>
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

#define DEBUG_INLINE_NAMES  0

//***************************************************************************

static void restore_sighandler(void);

static void
banal_sighandler(int sig)
{
  if (jbuf_active) {
    siglongjmp(jbuf, 1);
  }

  // caught a signal, but it didn't come from symtab
  restore_sighandler();
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
openSymtab(ElfFile *elfFile)
{
  bool ret = false;

  init_sighandler();
  num_queries = 0;
  num_errors = 0;

  if (sigsetjmp(jbuf, 1) == 0) {
    // normal return
    jbuf_active = 1;
    ret = Symtab::openFile(the_symtab, elfFile->getMemory(), elfFile->getLength(),
			   elfFile->getFileName());
    if (ret) {
      the_symtab->parseTypesNow();
      the_symtab->parseFunctionRanges();
    }
  }
  else {
    // error return
    ret = false;
  }
  jbuf_active = 0;

  if (! ret) {
    DIAG_WMsgIf(1, "SymtabAPI was unable to open: " << elfFile->getFileName());
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

//***************************************************************************

// Lookup the Module (comp unit) containing 'vma' to see if it is from
// a source file that mangles function names.  A full Symtab Function
// already does this, but inlined functions do not, so we have to
// decide this ourselves.
//
// Returns: true if 'vma' is from a C++ module (mangled names).
//
static string cplus_exts[] = {
  ".C", ".cc", ".cpp", ".cxx", ".c++",
  ".CC", ".CPP", ".CXX", ".hpp", ".hxx", ""
};

static bool
analyzeDemangle(VMA vma)
{
  if (the_symtab == NULL) {
    return false;
  }

  // find module (comp unit) containing vma
  set <Module *> modSet;
  the_symtab->findModuleByOffset(modSet, vma);

  if (modSet.empty()) {
    return false;
  }

  Module * mod = *(modSet.begin());
  if (mod == NULL) {
    return false;
  }

  // languages that need demangling
  supportedLanguages lang = mod->language();
  if (lang == lang_CPlusPlus || lang == lang_GnuCPlusPlus) {
    return true;
  }
  if (lang != lang_Unknown) {
    return false;
  }

  // if language is unknown, then try file name
  string filenm = mod->fileName();
  long file_len = filenm.length();

  if (filenm == "") {
    return false;
  }

  for (auto i = 0; cplus_exts[i] != ""; i++) {
    string ext = cplus_exts[i];
    long len = ext.length();

    if (file_len > len && filenm.compare(file_len - len, len, ext) == 0) {
      return true;
    }
  }

  return false;
}


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

  num_queries++;

  if (sigsetjmp(jbuf, 1) == 0) {
    //
    // normal return
    //
    jbuf_active = 1;
    if (the_symtab->getContainingInlinedFunction(addr, func))
    {
      bool demangle = analyzeDemangle(addr);

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
	string prettynm = procnm;

        if (procnm == "") {
	  procnm = UNKNOWN_PROC;
	  prettynm = UNKNOWN_PROC;
	}
	else if (demangle) {
	  prettynm = BinUtil::demangleProcName(procnm);
	}

#if DEBUG_INLINE_NAMES
	cout << "raw-inline:  0x" << hex << addr << dec
	     << "  link:  " << procnm << "  pretty:  " << prettynm << "\n";
#endif

	nodelist.push_front(InlineNode(filenm, procnm, prettynm, lineno));

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

//***************************************************************************

// Insert one statement range into the map.
//
// Note: we pass the stmt info in 'sinfo', but we don't link sinfo
// itself into the map.  Instead, insert a copy.
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
    info = new StmtInfo(vma, end_vma - vma, file, base, line);
    (*this)[vma] = info;
  }
  else if (left->base_index == base && left->line_num == line) {
    // intervals overlap and match file and line
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
      info = new StmtInfo(vma, end_vma - vma, file, base, line);
      (*this)[vma] = info;
    }
  }

  // compare interval with stmt to the right
  if (info != NULL && right != NULL && end_vma >= right->vma) {

    if (right->base_index == base && right->line_num == line) {
      // intervals overlap and match file and line
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
	      VMA vma, int len, string & filenm, SrcFile::ln line)
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
  StmtInfo info(vma, len, file, base, line);

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
