// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// This file defines the top-level FileInfo, GroupInfo and ProcInfo
// classes for makeSkeleton(), doFunctionList() and the Output
// functions.
//
// The Inline Tree Node classes below ProcInfo: TreeNode, LoopInfo and
// StmtInfo are in Struct-Inline.hpp.

//***************************************************************************

#ifndef BAnal_Struct_Skel_hpp
#define BAnal_Struct_Skel_hpp

#include <list>
#include <map>
#include <string>

#include <CFG.h>
#include <Symtab.h>
#include <Function.h>

#include "VMAInterval.hpp"
#include "Struct-Inline.hpp"

namespace BAnal {
namespace Struct {

using namespace Dyninst;
using namespace Inline;
using namespace std;

class FileInfo;
class GroupInfo;
class ProcInfo;

typedef map <string, FileInfo *> FileMap;
typedef map <VMA, GroupInfo *> GroupMap;
typedef map <VMA, ProcInfo *> ProcMap;


// FileInfo and FileMap are the top-level classes for files and
// procedures.  A FileInfo object contains the procs that belong to
// one file.
//
// FileMap is indexed by file name.
//
class FileInfo {
public:
  string  fileName;
  GroupMap groupMap;

  FileInfo(string nm)
  {
    fileName = nm;
    groupMap.clear();
  }
};


// GroupInfo contains the subset of procs that belong to one binutils
// group as determined by the SymtabAPI::Function.  Normally, this
// includes the unnamed procs (targ4xxxxx) within one Function symbol,
// eg, internal openmp regions.
//
// GroupMap is indexed by Function VMA (symtab if exists, else
// parseapi for plt funcs).
//
// alt_file is for outline funcs whose parseapi file name does not
// match the enclosing symtab file (implies no gap analysis).
//
class GroupInfo {
public:
  SymtabAPI::Function * sym_func;
  VMA  start;
  VMA  end;
  ProcMap procMap;
  VMAIntervalSet gapSet;
  bool  alt_file;

  GroupInfo(SymtabAPI::Function * sf, VMA st, VMA en, bool alt = false)
  {
    sym_func = sf;
    start = st;
    end = en;
    procMap.clear();
    gapSet.clear();
    alt_file = alt;
  }
};


// Info on one ParseAPI Function and Tree Node for one <P> tag as
// determined by the func's entry address.
//
// ProcMap is indexed by the func's entry address.
//
// gap_only is for outline funcs that exist in another file (do the
// full parse in the other file).
//
class ProcInfo {
public:
  ParseAPI::Function * func;
  TreeNode * root;
  string  linkName;
  string  prettyName;
  long  line_num;
  VMA   entry_vma;
  unsigned symbol_index;
  bool  gap_only;

  ProcInfo(ParseAPI::Function * fn, TreeNode * rt, string ln, string pn,
           long l, unsigned symindex = 0, bool gap = false)
  {
    func = fn;
    root = rt;
    linkName = ln;
    prettyName = pn;
    line_num = l;
    entry_vma = (func != NULL) ? func->addr() : 0;
    symbol_index = symindex;
    gap_only = gap;
  }
};

} // namespace Struct
} // namespace BAnal

#endif // BAnal_Struct_Skel_hpp
