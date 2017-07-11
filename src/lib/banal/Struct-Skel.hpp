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
#include "Struct-Inline.hpp"

namespace BAnal {
namespace Struct {

using namespace Dyninst;
using namespace Inline;
using namespace std;

class FileInfo;
class GroupInfo;
class ProcInfo;
class GapInfo;

typedef map <string, FileInfo *> FileMap;
typedef map <SymtabAPI::Function *, GroupInfo *> GroupMap;
typedef map <VMA, ProcInfo *> ProcMap;
typedef list <GapInfo> GapList;


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
// GroupMap is indexed by the SymtabAPI Function.
//
class GroupInfo {
public:
  SymtabAPI::Function * sym_func;
  string linkName;
  string prettyName;
  VMA  start;
  VMA  end;
  ProcMap procMap;
  GapList gapList;

  GroupInfo(SymtabAPI::Function * sf, string ln, string pn)
  {
    sym_func = sf;
    linkName = ln;
    prettyName = pn;
    start = (sf != NULL) ? sf->getOffset() : 0;
    end = (sf != NULL) ? (start + sf->getSize()) : 0;
    procMap.clear();
    gapList.clear();
  }
};


// Info on one ParseAPI Function and Tree Node for one <P> tag as
// determined by the func's entry address.
//
// ProcMap is indexed by the func's entry address.
//
class ProcInfo {
public:
  ParseAPI::Function * func;
  TreeNode * root;
  long  line_num;
  VMA   entry_vma;
  bool  leader;

  ProcInfo(ParseAPI::Function * fn, TreeNode * rt, long ln)
  {
    func = fn;
    root = rt;
    line_num = ln;
    entry_vma = (func != NULL) ? func->addr() : 0;
    leader = false;
  }
};


// GapInfo represents one vma range within a Function Group that is
// not covered by any ParseAPI Function within that group.
//
class GapInfo {
public:
  VMA  start;
  VMA  end;
  long line;

  GapInfo(VMA st, VMA en, long ln)
  {
    start = st;
    end = en;
    line = ln;
  }
};

} // namespace Struct
} // namespace BAnal

#endif // BAnal_Struct_Skel_hpp
