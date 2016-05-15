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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

// Now allow using both BANAL_USE_PARSEAPI and BANAL_USE_OA at the
// same time, but must define at least one of them.  Also,
// BANAL_USE_PARSEAPI implies BANAL_USE_SYMTAB.

#if ! defined(BANAL_USE_PARSEAPI) && ! defined(BANAL_USE_OA)
#error must define one of BANAL_USE_PARSEAPI and BANAL_USE_OA
#endif

#if defined(BANAL_USE_PARSEAPI) && ! defined(BANAL_USE_SYMTAB)
#error cannot define BANAL_USE_PARSEAPI without BANAL_USE_SYMTAB
#endif

//************************* System Include Files ****************************

#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <iomanip>
#include <fstream>
#include <sstream>

#include <string>
using std::string;

#include <map>
#include <list>
#include <set>
#include <vector>
#include <typeinfo>
#include <algorithm>
#include <cstring>

//************************ OpenAnalysis Include Files ***********************

#ifdef BANAL_USE_OA
#include <OpenAnalysis/CFG/ManagerCFG.hpp>
#include <OpenAnalysis/Utils/RIFG.hpp>
#include <OpenAnalysis/Utils/NestedSCR.hpp>
#include <OpenAnalysis/Utils/Exception.hpp>
#include "OAInterface.hpp"
#endif

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "Struct.hpp"
#include "Struct-LocationMgr.hpp"

#include <lib/prof/Struct-Tree.hpp>
#include <lib/prof/Struct-TreeIterator.hpp>
using namespace Prof;

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
#include <lib/support/StringTable.hpp>
#include <lib/support/StrUtil.hpp>

#ifdef BANAL_USE_SYMTAB
#include <Symtab.h>
#include <Function.h>
#include "Struct-Inline.hpp"

using namespace Dyninst;
using namespace SymtabAPI;
using namespace Inline;
#endif

#ifdef BANAL_USE_PARSEAPI
#include <CFG.h>
#include <CodeObject.h>
#include <CodeSource.h>
#include <Instruction.h>

using namespace InstructionAPI;
using namespace ParseAPI;
#endif

#define FULL_STRUCT_DEBUG 0


//*************************** Forward Declarations ***************************

namespace BAnal {
namespace Struct {

// ------------------------------------------------------------
// Helpers for building a structure tree
// ------------------------------------------------------------

// The three views of a procedure: scope tree, binutils and parseAPI.
// This is how we combine the open analysis and parseAPI cases to
// simplify passing different arguments.

#ifdef BANAL_USE_PARSEAPI
typedef std::list <ParseAPI::Function *> FuncList;
#endif

class ProcInfo {
public:
  Prof::Struct::Proc * proc;
  BinUtil::Proc * proc_bin;
#ifdef BANAL_USE_PARSEAPI
  FuncList * func_list;
#endif

#ifdef BANAL_USE_PARSEAPI
  ProcInfo(Prof::Struct::Proc *p, BinUtil::Proc *pb, FuncList *fl)
  {
    proc = p;
    proc_bin = pb;
    func_list = fl;
  }
#endif

#ifdef BANAL_USE_OA
  ProcInfo(Prof::Struct::Proc *p, BinUtil::Proc *pb)
  {
    proc = p;
    proc_bin = pb;
  }
#endif
};

typedef std::vector <ProcInfo> ProcInfoVec;

static Prof::Struct::File*
demandFileNode(Prof::Struct::LM* lmStrct, BinUtil::Proc* p);

static Prof::Struct::Proc*
demandProcNode(Prof::Struct::File* fStrct, BinUtil::Proc* p,
	       ProcNameMgr* procNmMgr);

#ifdef BANAL_USE_PARSEAPI
static ProcInfoVec *
buildLMSkeleton(Prof::Struct::LM* lmStrct,
		BinUtil::LM* lm,
		ParseAPI::CodeObject *code_obj,
		ProcNameMgr* procNmMgr);

static void
doFunctionList(Prof::Struct::LM * lm, ProcInfo pinfo,
	       StringTable & strTab, ProcNameMgr * nameMgr);
#endif

#ifdef BANAL_USE_OA
static ProcInfoVec*
buildLMSkeleton(Prof::Struct::LM* lmStrct,
		BinUtil::LM* lm,
		ProcNameMgr* procNmMgr);

static Prof::Struct::Proc*
buildProcStructure(ProcInfo pinfo,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, const std::string& dbgProcGlob);

static int
buildProcLoopNests(Prof::Struct::Proc* pStrct, BinUtil::Proc* p,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, bool isDbg);

static int
buildStmts(Struct::LocationMgr& locMgr,
	   Prof::Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p,
	   OA::OA_ptr<OA::CFG::NodeInterface> bb, ProcNameMgr* procNmMgr,
	   std::list<Prof::Struct::ACodeNode *> &insertions, int targetScopeID);

static void
findLoopBegLineInfo(BinUtil::Proc* p, OA::OA_ptr<OA::NestedSCR> tarj, 
		    OA::OA_ptr<OA::CFG::NodeInterface> headBB,
		    string& begFilenm, string& begProcnm, SrcFile::ln& begLn, VMA &loop_vma);

#endif  // open analysis

// ------------------------------------------------------------
// Debug functions to display input data for loops, blocks and stmts
// ------------------------------------------------------------

#define DEBUG_CFG_SOURCE  0

#if DEBUG_CFG_SOURCE
static void
debugStmt(Prof::Struct::Stmt *, VMA, string &, string &, SrcFile::ln);

#define DEBUG_MESG(expr)  std::cout << expr

#else

#define DEBUG_MESG(expr)

#endif  // DEBUG_CFG_SOURCE

} // namespace Struct
} // namespace BAnal


//*************************** Forward Declarations ***************************

namespace BAnal {
namespace Struct {

// ------------------------------------------------------------
// Helpers for normalizing the structure tree
// ------------------------------------------------------------

static bool
coalesceDuplicateStmts(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe);

static bool
mergePerfectlyNestedLoops(Prof::Struct::ANode* node);

static bool
removeEmptyNodes(Prof::Struct::ANode* node);

  
// ------------------------------------------------------------
// Helpers for normalizing the structure tree
// ------------------------------------------------------------

class StmtKey { // cf. AlienStrctMapKey
public:
  StmtKey(int sortId = 0)
    : m_filenm(StmtKey::DefaultFileNm), m_sortId(sortId)
  { }

  StmtKey(const std::string& filenm, int sortId = 0)
    : m_filenm(filenm), m_sortId(sortId)
  { }

  ~StmtKey()
  { /* owns no data */ }

  bool
  operator<(const StmtKey& y) const
  {
    const StmtKey& x = *this;

    if ((x.sortId() < y.sortId())
	|| (x.sortId() == y.sortId() && (x.filenm() < y.filenm()))) {
      return true;
    }

    return false;
  }

  const std::string&
  filenm() const
  { return m_filenm; }

  int
  sortId() const
  { return m_sortId; }

  std::string
  toString() const
  {
    std::string str = StrUtil::toStr(sortId()) + ":[" + filenm() + "]";
    return str;
  }

  
private:
  const std::string m_filenm; // should not be a pointer or reference
  int m_sortId;

public:
  static std::string DefaultFileNm;
};

std::string StmtKey::DefaultFileNm;


class StmtData {
public:
  StmtData(Prof::Struct::Stmt* stmt = NULL, int level = 0)
    : m_stmt(stmt), m_level(level)
  { }

  ~StmtData()
  { /* owns no data */ }


  Prof::Struct::Stmt*
  stmt() const
  { return m_stmt; }

  void
  stmt(Prof::Struct::Stmt* x)
  { m_stmt = x; }


  int
  level() const
  { return m_level; }

  void
  level(int x)
  { m_level = x; }
  
private:
  Prof::Struct::Stmt* m_stmt; // does not own
  int m_level;
};


class StmtKeyToStmtMap
{
public:
  typedef StmtKey                                           key_type;
  typedef StmtData*                                         mapped_type;
  
  typedef std::map<key_type, mapped_type>                   My_t;
  typedef std::pair<const key_type, mapped_type>            value_type;
  typedef My_t::iterator                                    iterator;

public:
  StmtKeyToStmtMap()
  { }

  ~StmtKeyToStmtMap()
  { clear(); }

  std::pair<iterator,bool>
  insert(const value_type& val)
  { return m_map.insert(val); }

  iterator
  find(const key_type& x)
  { return m_map.find(x); }

  iterator
  end()
  { return m_map.end(); }

  void
  clear()
  {
    for (iterator it = m_map.begin(); it != m_map.end(); ++it) {
      delete it->second;
    }
    m_map.clear();
  }

private:
  My_t m_map;
};


// ------------------------------------------------------------
// Make a dot (graphviz) file for the Control-Flow Graph
// ------------------------------------------------------------

// Write the Control-Flow Graph for each procedure to the ostream
// dotFile.

#ifdef BANAL_USE_PARSEAPI
static void
makeDotFile(std::ostream * dotFile, CodeObject * code_obj)
{
  const CodeObject::funclist & funcList = code_obj->funcs();

  for (auto fit = funcList.begin(); fit != funcList.end(); ++fit)
  {
    ParseAPI::Function * func = *fit;
    map <Block *, int> blockNum;
    map <Block *, int>::iterator mit;
    int num;

    *dotFile << "--------------------------------------------------\n"
	     << "Procedure: '" << func->name() << "'\n\n"
	     << "digraph " << func->name() << " {\n"
	     << "  1 [ label=\"start\" shape=\"diamond\" ];\n";

    const ParseAPI::Function::blocklist & blist = func->blocks();

    // write the list of nodes (blocks)
    num = 1;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      num++;

      blockNum[block] = num;
      *dotFile << "  " << num << " [ label=\"0x" << hex << block->start()
	       << dec << "\" ];\n";
    }
    int endNum = num + 1;
    *dotFile << "  " << endNum << " [ label=\"end\" shape=\"diamond\" ];\n";

    // in parseAPI, functions have a unique entry point
    mit = blockNum.find(func->entry());
    if (mit != blockNum.end()) {
      *dotFile << "  1 -> " << mit->second << ";\n";
    }

    // write the list of internal edges
    num = 1;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      const ParseAPI::Block::edgelist & elist = block->targets();
      num++;

      for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
	mit = blockNum.find((*eit)->trg());
	if (mit != blockNum.end()) {
	  *dotFile << "  " << num << " -> " << mit->second << ";\n";
	}
      }
    }

    // add any exit edges
    const ParseAPI::Function::const_blocklist & eblist = func->exitBlocks();
    for (auto bit = eblist.begin(); bit != eblist.end(); ++bit) {
      Block * block = *bit;
      mit = blockNum.find(block);
      if (mit != blockNum.end()) {
	*dotFile << "  " << mit->second << " -> " << endNum << ";\n";
      }
    }

    *dotFile << "}\n" << endl;
  }
}
#endif  // parseAPI


#ifdef BANAL_USE_OA
static void
makeDotFile(std::ostream * dotFile, BinUtil::LM * lm)
{
  BinUtil::LM::ProcMap & pmap = lm->procs();

  for (BinUtil::LM::ProcMap::iterator it = pmap.begin();
       it != pmap.end(); ++it)
  {
    BinUtil::Proc * proc = it->second;

    OA::OA_ptr <OAInterface> irIF;
    irIF = new OAInterface(proc);

    OA::OA_ptr <OA::CFG::ManagerCFGStandard> cfgmanstd;
    cfgmanstd = new OA::CFG::ManagerCFGStandard(irIF);

    OA::OA_ptr <OA::CFG::CFG> cfg =
      cfgmanstd->performAnalysis(TY_TO_IRHNDL(proc, OA::ProcHandle));

    OA::OA_ptr <OA::RIFG> rifg;
    rifg = new OA::RIFG(cfg, cfg->getEntry(), cfg->getExit());

    *dotFile << "--------------------------------------------------\n"
	     << "Procedure: '" << proc->name() << "'\n\n";
    rifg->dump(*dotFile);
    *dotFile << endl;
  }
}
#endif  // open analysis

} // namespace Struct
} // namespace BAnal


//*************************** Forward Declarations ***************************

#define DBG_CDS  0 /* debug coalesceDuplicateStmts */

static const string& OrphanedProcedureFile = Prof::Struct::Tree::UnknownFileNm;


//****************************************************************************
// Set of routines to build a scope tree
//****************************************************************************

// makeStructure: Builds a scope tree -- with a focus on loop nest
//   recovery -- representing program source code from the load module
//   'lm'.
// A load module represents binary code, which is essentially
//   a collection of procedures along with some extra information like
//   symbol and debugging tables.  This routine relies on the debugging
//   information to associate procedures with thier source code files
//   and to recover procedure loop nests, correlating them to source
//   code line numbers.  A normalization pass is typically run in order
//   to 'clean up' the resulting scope tree.  Among other things, the
//   normalizer employs heuristics to reverse certain compiler
//   optimizations such as loop unrolling.

namespace BAnal {
namespace Struct {

#ifdef BANAL_USE_OA

static Prof::Struct::LM *
makeStructure_OA(BinUtil::LM * lm,
		 std::ostream * dotFile,
		 NormTy doNormalizeTy,
		 bool isIrrIvalLoop,
		 bool isFwdSubst,
		 ProcNameMgr* procNmMgr,
		 const std::string& dbgProcGlob)
{
  Prof::Struct::LM* lmStruct = new Prof::Struct::LM(lm->name(), NULL);

#ifdef BANAL_USE_SYMTAB
  Symtab * symtab = Inline::openSymtab(lm->name());
#endif

  // 1. Build Struct::File/Struct::Proc skeletal structure
  ProcInfoVec * pvec = buildLMSkeleton(lmStruct, lm, procNmMgr);

  // 2. For each [Struct::Proc, BinUtil::Proc] pair, complete the build.
  // Note that a Struct::Proc may be associated with more than one
  // BinUtil::Proc.
  for (auto it = pvec->begin(); it != pvec->end(); ++it) {
    BinUtil::Proc* p = it->proc_bin;

    DIAG_Msg(2, "Building scope tree for [" << p->name()  << "] ... ");
    buildProcStructure(*it, isIrrIvalLoop, isFwdSubst,
		       procNmMgr, dbgProcGlob);
  }
  delete pvec;

  // 3. Normalize
  if (doNormalizeTy != NormTy_None) {
    bool doNormalizeUnsafe = (doNormalizeTy == NormTy_All);
    normalize(lmStruct, doNormalizeUnsafe);
  }

  // 4. Write CFG in dot (graphviz) format to file.
  if (dotFile != NULL) {
    makeDotFile(dotFile, lm);
  }

#ifdef BANAL_USE_SYMTAB
  Inline::closeSymtab();
#endif

  return lmStruct;
}
#endif  // open analysis


#ifdef BANAL_USE_PARSEAPI

static Prof::Struct::LM *
makeStructure_ParseAPI(BinUtil::LM * lm,
		       std::ostream * dotFile,
		       NormTy doNormalizeTy,
		       bool isIrrIvalLoop,
		       bool isFwdSubst,
		       ProcNameMgr* procNmMgr,
		       const std::string& dbgProcGlob)
{
  Prof::Struct::LM* lmStruct = new Prof::Struct::LM(lm->name(), NULL);
  StringTable strTab;

  Symtab * symtab = Inline::openSymtab(lm->name());

  SymtabCodeSource * code_src;
  CodeObject * code_obj;

  if (symtab != NULL) {
    code_src = new SymtabCodeSource(symtab);
    code_obj = new CodeObject(code_src);
    code_obj->parse();
  }

  // 1. Build Struct::File/Struct::Proc skeletal structure
  ProcInfoVec * pvec = buildLMSkeleton(lmStruct, lm, code_obj, procNmMgr);

  // 2. For each [Struct::Proc, BinUtil::Proc] pair, complete the build.
  // Note that a Struct::Proc may be associated with more than one
  // BinUtil::Proc.
  for (auto it = pvec->begin(); it != pvec->end(); ++it) {
    doFunctionList(lmStruct, *it, strTab, procNmMgr);
  }

  // delete pvec and the func lists
  for (auto it = pvec->begin(); it != pvec->end(); ++it) {
    delete it->func_list;
  }
  delete pvec;

#if 0
  // 3. Normalize
  if (doNormalizeTy != NormTy_None) {
    bool doNormalizeUnsafe = (doNormalizeTy == NormTy_All);
    normalize(lmStruct, doNormalizeUnsafe);
  }
#endif

  // FIXME: write this directly in StmtInfo and TreeNode format.
  coalesceDuplicateStmts(lmStruct, false);

  // 4. Write CFG in dot (graphviz) format to file.
  if (dotFile != NULL) {
    makeDotFile(dotFile, code_obj);
  }

  Inline::closeSymtab();

  return lmStruct;
}
#endif  // parseapi

}  // namespace Struct
}  // namespace BAnal


// The top-level split for makeStructure() between OpenAnalysis and
// ParseAPI based on --cfg.  This is where we transition from variable
// (cfgRequest) to #ifdef.  Clumsy, but anything would be awkward for
// this case.
//
Prof::Struct::LM*
BAnal::Struct::makeStructure(BinUtil::LM* lm,
			     std::ostream * dotFile,
			     int cfgRequest,
			     NormTy doNormalizeTy,
			     bool isIrrIvalLoop,
			     bool isFwdSubst,
			     ProcNameMgr* procNmMgr,
			     const std::string& dbgProcGlob)
{
  // Assume lm->Read() has been performed
  DIAG_Assert(lm, DIAG_UnexpectedInput);

  // Select default if --cfg was not specified.
  if (cfgRequest == BAnal::Struct::CFG_DEFAULT) {
#ifdef BANAL_USE_PARSEAPI
    cfgRequest = BAnal::Struct::CFG_PARSEAPI;
#else
    cfgRequest = BAnal::Struct::CFG_OA;
#endif
  }

  // Verify that the requested CFG support was compiled in and then
  // call the OA or ParseAPI version.
  Prof::Struct::LM * lmStruct;

  switch (cfgRequest) {

  case BAnal::Struct::CFG_OA:
#ifdef BANAL_USE_OA
    lmStruct = makeStructure_OA(lm, dotFile, doNormalizeTy, isIrrIvalLoop,
				isFwdSubst, procNmMgr, dbgProcGlob);
#else
    DIAG_EMsg("hpcstruct was not compiled with OpenAnalysis");
    exit(1);
#endif
    break;

  case BAnal::Struct::CFG_PARSEAPI:
#ifdef BANAL_USE_PARSEAPI
    lmStruct = makeStructure_ParseAPI(lm, dotFile, doNormalizeTy, isIrrIvalLoop,
				      isFwdSubst, procNmMgr, dbgProcGlob);
#else
    DIAG_EMsg("hpcstruct was not compiled with ParseAPI");
    exit(1);
#endif
    break;

  default:
    DIAG_Die("internal error: unknown value for cfgRequest: " << cfgRequest);
    break;
  }

  return lmStruct;
}


// normalize: Because of compiler optimizations and other things, it
// is almost always desirable normalize a scope tree.  For example,
// almost all unnormalized scope tree contain duplicate statement
// instances.  See each normalizing routine for more information.
bool
BAnal::Struct::normalize(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe)
{
  bool changed = false;
  
  // Cleanup procedure/alien scopes
  changed |= coalesceDuplicateStmts(lmStrct, doNormalizeUnsafe);
  changed |= mergePerfectlyNestedLoops(lmStrct);
  changed |= removeEmptyNodes(lmStrct);
  
  return changed;
}


//****************************************************************************
// Helpers for building a scope tree
//****************************************************************************

namespace BAnal {
namespace Struct {

// buildLMSkeleton: Build skeletal file-procedure structure.  This
// will be useful later when detecting alien lines.  Also, the
// nesting structure allows inference of accurate boundaries on
// procedure end lines.
//
// A Struct::Proc can be associated with multiple BinUtil::Procs
//
// Struct::Procs will be sorted by begLn (cf. Struct::ACodeNode::Reorder)
//
#ifdef BANAL_USE_OA
static ProcInfoVec *
buildLMSkeleton(Prof::Struct::LM* lmStrct,
		BinUtil::LM* lm,
		ProcNameMgr* procNmMgr)
{
  ProcInfoVec * pvec = new ProcInfoVec;
  
  // -------------------------------------------------------
  // 1. Create basic structure for each procedure
  // -------------------------------------------------------

  for (auto it = lm->procs().begin(); it != lm->procs().end(); ++it) {
    BinUtil::Proc* p = it->second;

    if (p->size() != 0) {
      Prof::Struct::File* fStrct = demandFileNode(lmStrct, p);
      Prof::Struct::Proc* pStrct = demandProcNode(fStrct, p, procNmMgr);
      pvec->push_back(ProcInfo(pStrct, p));
    }
  }

  // -------------------------------------------------------
  // 2. Establish nesting information:
  // -------------------------------------------------------
  // FIXME: disable until we are sure we can handle this (cf. MOAB)
#if 0
  for (auto it = pvec->begin(); it != pvec->end(); ++it) {
    Prof::Struct::Proc* pStrct = it->proc;
    BinUtil::Proc* p = it->proc_bin;
    BinUtil::Proc* parent = p->parent();

    if (parent) {
      Prof::Struct::Proc* parentScope = lmStrct->findProc(parent->begVMA());
      pStrct->unlink();
      pStrct->link(parentScope);
    }
  }
#endif

  // FIXME (minor): The following order is appropriate when we have
  // symbolic information. I.e. we, 1) establish nesting information
  // and then attempt to refine procedure bounds using nesting
  // information.  If such nesting information is not available,
  // assume correct bounds and attempt to establish nesting.
  
  // 3. Establish procedure bounds: nesting + first line ... [FIXME]

  // 4. Establish procedure groups: [FIXME: more stuff from DWARF]
  //      template instantiations, class member functions

  return pvec;
}
#endif  // open analysis


// In the ParseAPI version, we iterate over the ParseAPI list of
// functions and match them up with the BinUtil procedures by entry
// address.  We still use BinUtil::Proc for the line map info, even in
// the ParseAPI case.
//
// Note: several parseapi functions (targ410aa7) may map to the same
// proc scope node and/or binutils proc, so we make a func list.
//
#ifdef BANAL_USE_PARSEAPI
static ProcInfoVec *
buildLMSkeleton(Prof::Struct::LM* lmStruct,
		BinUtil::LM* lm,
		ParseAPI::CodeObject *code_obj,
		ProcNameMgr* procNmMgr)
{
  std::map <Prof::Struct::Proc *, FuncList *> smap;
  ProcInfoVec * pvec = new ProcInfoVec;

  // iterate over the ParseAPI Functions
  const CodeObject::funclist & funcList = code_obj->funcs();

  for (auto fit = funcList.begin(); fit != funcList.end(); ++fit) {
    ParseAPI::Function *func = *fit;
    BinUtil::Proc *p = lm->findProc((VMA) func->addr());

    if (p != NULL) {
      Prof::Struct::File * fStruct = demandFileNode(lmStruct, p);
      Prof::Struct::Proc * pStruct = demandProcNode(fStruct, p, procNmMgr);
      auto sit = smap.find(pStruct);

      if (sit != smap.end()) {
	// two or more parseapi functions map to the same proc scope
	// node -- add to func_list for this proc
	sit->second->push_back(func);
      }
      else {
	// new proc scope node -- create ProcInfo object for this
	// scope, add to pvec and smap
	FuncList * flist = new FuncList;

	flist->push_back(func);
	pvec->push_back(ProcInfo(pStruct, p, flist));
	smap[pStruct] = flist;
      }
    }
    else {
      // these are mostly from plt headers on ppc64
#if 0
      DIAG_WMsgIf(1, "no matching BinUtil::Proc for function '" << func->name()
		  << "' at address 0x" << std::hex << func->addr() << std::dec);
#endif
    }
  }

  return pvec;
}
#endif  // parseAPI


// demandFileNode:
static Prof::Struct::File*
demandFileNode(Prof::Struct::LM* lmStrct, BinUtil::Proc* p)
{
  // Attempt to find filename for procedure
  string filenm = p->filename();
  
  if (filenm.empty()) {
    string procnm;
    SrcFile::ln line;
    p->findSrcCodeInfo(p->begVMA(), 0, procnm, filenm, line);
  }
  if (filenm.empty()) {
    filenm = OrphanedProcedureFile;
  }
  
  // Obtain corresponding Struct::File
  return Prof::Struct::File::demand(lmStrct, filenm);
}


// demandProcNode: Build skeletal Struct::Proc.  We can assume that
// the parent is always a Struct::File.
static Prof::Struct::Proc*
demandProcNode(Prof::Struct::File* fStrct, BinUtil::Proc* p,
	       ProcNameMgr* procNmMgr)
{
  // Find VMA boundaries: [beg, end)
  VMA endVMA = p->begVMA() + p->size();
  VMAInterval bounds(p->begVMA(), endVMA);
  DIAG_Assert(!bounds.empty(), "Attempting to add empty procedure "
	      << bounds.toString());
  
  // don't demangle the link name. because demangling is a many-->one mapping,
  // we need to keep the link name (the mangled name) for use as a unique key
  string procLnNm = p->linkName(); 

  // Binutils often uses simpler procedure names than what we would get from 
  // demangling the procedure's linkName. We need to compute the procedure
  // name from the demangled linkName to match what we get from symtabAPI. 
  const char *linkName = p->linkName().c_str();

  // On powerpc, linkNames for procedures have a leading period. We need to clip 
  // that off before trying to demangle. This approach will work whether
  // the leading character is a period or not.
  if (linkName && linkName[0] == '.') linkName++;

  // Compute the procedure name by demangling the link name.
  string procNm   = BinUtil::canonicalizeProcName(linkName, procNmMgr);
  
  // Find preliminary source line bounds
  string file, proc;
  SrcFile::ln begLn1, endLn1;
  BinUtil::Insn* eInsn = p->endInsn();
  ushort endOp = (eInsn) ? eInsn->opIndex() : (ushort)0;
  p->findSrcCodeInfo(p->begVMA(), 0, p->endVMA(), endOp,
		     proc, file, begLn1, endLn1, 0 /*no swapping*/);
  
  // Compute source line bounds to uphold invariant:
  //   (begLn == 0) <==> (endLn == 0)
  SrcFile::ln begLn, endLn;
  if (p->hasSymbolic()) {
    begLn = p->begLine();
    endLn = std::max(begLn, endLn1);
  }
  else {
    // for now, always assume begLn to be more accurate
    begLn = begLn1;
    endLn = std::max(begLn1, endLn1);
  }

  // Obtain Struct::Proc by name.  This has the effect of fusing with
  // the existing Struct::Proc and accounts for procedure splitting or
  // specialization.
  bool didCreate = false;
  Prof::Struct::Proc* pStrct =
    Prof::Struct::Proc::demand(fStrct, procNm, procLnNm,
			       begLn, endLn, &didCreate);
  if (!didCreate) {
    DIAG_DevMsg(3, "Merging multiple instances of procedure ["
		<< pStrct->toStringXML() << "] with " << procNm << " "
		<< procLnNm << " " << bounds.toString());
    if (SrcFile::isValid(begLn)) {
      pStrct->expandLineRange(begLn, endLn);
    }
  }
  if (p->hasSymbolic()) {
    pStrct->hasSymbolic(p->hasSymbolic()); // optimistically set
  }
  pStrct->vmaSet().insert(bounds);
  
  return pStrct;
}

} // namespace Struct
} // namespace BAnal


//****************************************************************************
// Helpers for Alien nodes
//****************************************************************************

namespace BAnal {
namespace Struct {

#define DEBUG_COMPARE  0

#if DEBUG_COMPARE
static int debug_compare = 0;
#define retval_line 1
#define retval_proc 2
#define retval_file 3
#define MAINTAIN_REASON(x) x
#else
#define MAINTAIN_REASON(x) 
#endif


static bool
AlienScopeFilter(const Prof::Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (x.type() == Prof::Struct::ANode::TyAlien);
}


class AlienCompare {
public:
  bool operator()(const Prof::Struct::Alien* a1,  const Prof::Struct::Alien* a2) const {
    bool retval = false;
    MAINTAIN_REASON(int reason = 0);
    if (a1->begLine() == a2->begLine()) {
      int result_dn = strcmp(a1->displayName().c_str(), a2->displayName().c_str());
      if (result_dn == 0) {
        int result_fn = strcmp(a1->fileName().c_str(), a2->fileName().c_str());
        if (result_fn == 0) {
          retval = false;
        } else {
          retval = (result_fn < 0) ? true : false;
          MAINTAIN_REASON(reason = retval_file);
        }
      } else {
        retval = (result_dn < 0) ? true : false;
        MAINTAIN_REASON(reason = retval_proc);
      }
    } else {
      retval = (a1->begLine() < a2->begLine()) ? true : false;
      MAINTAIN_REASON(reason = retval_line);
    } 
#if DEBUG_COMPARE
    if (debug_compare) {
      std::cout << "compare(" << a1 << ", " << a2 << ") = " 
                << retval << " reason =" << reason << std::endl;
      std::cout << "  " << a1 << "=[" << a1->begLine() << "," 
                << a1->displayName() << "," << a1->fileName() << "]" << std::endl;
      std::cout << "  " << a2 << "=[" << a2->begLine() << "," 
                << a2->displayName() << "," << a2->fileName() << "]" << std::endl;
    }
#endif
    return retval;
  } 
};


static bool
inlinedProceduresFilter(const Prof::Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  if (x.type() != Prof::Struct::ANode::TyAlien) return false;
  Prof::Struct::Alien* alien = (Prof::Struct::Alien*) &x;
  return (alien->begLine() == 0);
}


// iterate over all nodes that represent inlined procedure scopes. compute an approximate
// beginning line based on the minimum line number in scopes directly within.
static void
renumberAlienScopes(Prof::Struct::ANode* node)
{
  // use a filter that selects scopes that represent inlined procedures (not call sites)
  Prof::Struct::ANodeFilter inlinesOnly(inlinedProceduresFilter, "inlinedProceduresFilter", 0);
  
  // iterate over scopes that represent inlined procedures
  for(Prof::Struct::ANodeIterator inlines(node, &inlinesOnly); inlines.Current(); inlines++) {
    Prof::Struct::Alien* inlinedProcedure = dynamic_cast<Prof::Struct::Alien*>(inlines.Current());
    int minLine = INT_MAX;
    int maxLine = 0;

    //---------------------------------------------------------------------
    // iterate over the immediate child scopes of an inlined procedure
    //---------------------------------------------------------------------
    for(NonUniformDegreeTreeNodeChildIterator kids(inlinedProcedure); 
        kids.Current(); kids++) {
      Prof::Struct::ACodeNode* child = 
         dynamic_cast<Prof::Struct::ACodeNode*>(kids.Current());
      int childLine = child->begLine();
      if (childLine > 0) {
        if (childLine < minLine) {
          minLine = childLine;
        }
        if (childLine > maxLine) {
          maxLine = childLine;
        }
      }
    }
    if (maxLine != 0) {
	inlinedProcedure->thawLine();
	inlinedProcedure->setLineRange(minLine,maxLine);
	inlinedProcedure->freezeLine();
    }
  }
}


//----------------------------------------------------------------------------
// because of how alien contexts are entered into the tree, there may be many
// duplicates. if there are duplicates among the immediate children of node, 
// coalesce them. then, apply this coalescing to the subtree rooted at 
// each of the remaining (unique) children of node.
//----------------------------------------------------------------------------
typedef std::list<Prof::Struct::Alien*> AlienList;
typedef std::map<Prof::Struct::Alien*, AlienList *, AlienCompare> AlienMap;

static void
dumpMap(AlienMap &alienMap)
{
    std::cout << "map " << &alienMap << 
                 " (" << alienMap.size() << " items)" << std::endl;

    for(AlienMap::iterator pairs = alienMap.begin(); 
        pairs != alienMap.end(); pairs++) {
        std::cout << "  [" << pairs->first << "] = {"; 
        AlienList *alienList = pairs->second;
        for(AlienList::iterator duplicates = alienList->begin(); 
            duplicates != alienList->end(); duplicates++) {
            std::cout << " " << *duplicates << " "; 
	}
        std::cout << "}" << std::endl; 
    }
}


static void
coalesceAlienChildren(Prof::Struct::ANode* node)
{
  AlienMap alienMap; 
  bool duplicates = false;

  //----------------------------------------------------------------------------
  // enter all alien children into a map. values in the map will be lists of 
  // equivalent alien children.
  //----------------------------------------------------------------------------
  Prof::Struct::ANodeFilter aliensOnly(AlienScopeFilter, "AlienScopeFilter", 0);
  for(Prof::Struct::ANodeChildIterator it(node, &aliensOnly); it.Current(); it++) {
    Prof::Struct::Alien* alien = dynamic_cast<Prof::Struct::Alien*>(it.Current());
    AlienMap::iterator found = alienMap.find(alien);
    if (found == alienMap.end()) {
      alienMap[alien] = new AlienList;
    } else {
      found->second->push_back(alien);
      duplicates = true;
    }
  }

  //----------------------------------------------------------------------------
  // coalesce equivalent alien children by merging all alien children in a list 
  // into the first element. free duplicate aliens after unlinking them from the 
  // tree.
  //----------------------------------------------------------------------------
  if (duplicates) {
    for (AlienMap::iterator sets = alienMap.begin(); 
       sets != alienMap.end(); sets++) {

      //------------------------------------------------------------------------
      // for each list of equivalent aliens, have the first alien adopt children 
      // of all the rest unlink and delete all but the first of the list of 
      // aliens.
      //------------------------------------------------------------------------
      AlienList *alienList =  sets->second;
      Prof::Struct::Alien* first = sets->first;

      // for each of the elements in the list of duplicate aliens
      for (AlienList::iterator duplicates = alienList->begin();
           duplicates != alienList->end(); duplicates++)  {
         Prof::Struct::Alien* duplicate = *duplicates;

         //---------------------------------------------------------------------
	 // accumulate a list of the children of the duplicate alien 
         //---------------------------------------------------------------------
	 std::list<Prof::Struct::ANode*> kidsList;
         for(NonUniformDegreeTreeNodeChildIterator kids(duplicate); 
             kids.Current(); kids++) {
           Prof::Struct::ANode* kid = 
             dynamic_cast<Prof::Struct::ANode*>(kids.Current());
	   kidsList.push_back(kid);
	 }

         //---------------------------------------------------------------------
	 // walk through the list of the children of duplicate and reparent them
         // under first - an equivalent alien that will adopt all children of 
         // duplicate.
         //---------------------------------------------------------------------
         for (std::list<Prof::Struct::ANode*>::iterator kids = kidsList.begin();
           kids != kidsList.end(); kids++)  {
           Prof::Struct::ANode* kid = *kids;
           kid->unlink();
           kid->link(first);
         }
         duplicate->unlink();
         delete duplicate;
      }
      alienList->clear(); // done with this list. discard its contents.
    }
  }

  alienMap.clear(); // done with the map. discard its contents.

  //----------------------------------------------------------------------------
  // recursively apply coalescing to the subtree rooted at each child of node
  //----------------------------------------------------------------------------
  for(NonUniformDegreeTreeNodeChildIterator kids(node); kids.Current(); kids++) {
    Prof::Struct::ANode* kid = dynamic_cast<Prof::Struct::ANode*>(kids.Current());
    if (typeid(*kid) != typeid(Prof::Struct::Stmt)) {
      coalesceAlienChildren(kid);
    }
  }
}


static Prof::Struct::ANode * 
getVisibleAncestor(Prof::Struct::ANode *node)
{
  for (;;) {
    node = node->parent();
    if (node == 0 || node->isVisible()) return node;
  }
}


#if FULL_STRUCT_DEBUG
static int
willBeCycle()
{
  return 1;
}
 

static int
checkCycle(Prof::Struct::ANode *node, Prof::Struct::ANode *loop)
{
  Prof::Struct::ANode *n = loop;
  while (n) {
    if (n == node) {
      return willBeCycle();
    }
    n = n->parent();
  }
  return 0;
}


static int
ancestorIsLoop(Prof::Struct::ANode *node)
{
  Prof::Struct::ANode *n = node;
  while ((n = n->parent())) {
    if (n->type() == Prof::Struct::ANode::TyLoop) 
      return 1;
  }
  return 0;
}
#endif  // full_struct_debug


static bool
matchAliens(Prof::Struct::Alien *n1, Prof::Struct::Alien *n2)
{
  return 
    n1->fileName() == n2->fileName() &&
    n1->begLine() == n2->begLine() &&
    n1->displayName() == n2->displayName();
}


static bool
matchPaths(Prof::Struct::ANode *n1, Prof::Struct::ANode *n2, Prof::Struct::ANode *child, Prof::Struct::ANode **result)
{
  if (n1 == n2) { 
    // n1 and n2 are identical; they trivially match
    *result = child;
    return true;
  }

  // n1 and n2 are not identical; check to see if they are equivalent
  bool parentsMatch = matchPaths(n1->parent(), n2->parent(), n1, result);
  if (parentsMatch) {
    if ((typeid(*n1) == typeid(Prof::Struct::Alien)) &&
	(typeid(*n2) == typeid(Prof::Struct::Alien))) {
      if (matchAliens((Prof::Struct::Alien *)n1, (Prof::Struct::Alien *)n2)) {
	// parents match, n1 and n2 are equivalent, so the paths match to this point
	*result = child; 
	return true;
      }
    }
  }
  return false;
} 


static void
reparentNode(Prof::Struct::ANode *kid, Prof::Struct::ANode *loop, 
	      Prof::Struct::ANode *loopParent, Struct::LocationMgr& locMgr,
	      int nodeDepth, int loopParentDepth)
{
  Prof::Struct::ANode *node = kid;

  if (nodeDepth <= loopParentDepth) {
    // simple case: fall through to reparent node into loop.
  } else {
    Prof::Struct::ANode *child;
    // find node at loopParentDepth
    // always >= 1 trip through
    while (nodeDepth > loopParentDepth) {
      child = node;
      node = node->parent();
      nodeDepth--;
    }

    if (child == loop) return;
    else {
      Prof::Struct::ANode *result = kid;
      matchPaths(node, loopParent, child, &result);
      node = result;
    }
  }

#if FULL_STRUCT_DEBUG
  // heavyhanded debugging
  if (checkCycle(node, loop) == 0) 
#endif
  {
    if (typeid(*node) == typeid(Prof::Struct::Alien)) {
      // if reparenting an alien node, make sure that we are not 
      // caching its old parent in the alien cache
      locMgr.evictAlien((Prof::Struct::ACodeNode *)node->parent(), 
			(Prof::Struct::Alien *)node);
    }
    node->unlink();
    node->link(loop);
  }
}

} // namespace Struct
} // namespace BAnal


//****************************************************************************
// Open Analysis code for procedures, loops and blocks
//****************************************************************************

#ifdef BANAL_USE_OA

namespace BAnal {
namespace Struct {

static int
buildProcLoopNests(Prof::Struct::Proc* enclosingProc, BinUtil::Proc* p,
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::CFGInterface> cfg,
		   OA::RIFG::NodeId fgRoot,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, bool isDbg);


static Prof::Struct::ACodeNode*
buildLoopAndStmts(Struct::LocationMgr& locMgr,
		  Prof::Struct::ACodeNode* topScope, Prof::Struct::ACodeNode* enclosingScope, 
	  	  BinUtil::Proc* p, OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::CFGInterface> cfg,
		  OA::RIFG::NodeId fgNode,
		  bool isIrrIvalLoop, ProcNameMgr* procNmMgr);


// buildProcStructure: Complete the representation for 'pStrct' given the
// BinUtil::Proc 'p'.  Note that pStrcts parent may itself be a Struct::Proc.
//
static Prof::Struct::Proc*
buildProcStructure(ProcInfo pinfo,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, const std::string& dbgProcGlob)
{
  Prof::Struct::Proc * pStrct = pinfo.proc;
  BinUtil::Proc *p = pinfo.proc_bin;

#if DEBUG_CFG_SOURCE
  cout << "\n------------------------------------------------------------\n"
       << "func:  0x" << hex << p->begVMA() << dec << "  '" << p->name() << "'\n";
#endif

  DIAG_Msg(3, "==> Proc `" << p->name() << "' (" << p->id() << ") <==");
  
  bool isDbg = false;
  if (!dbgProcGlob.empty()) {
    //uint dbgId = p->id();
    isDbg = FileUtil::fnmatch(dbgProcGlob, p->name().c_str());
  }
#if FULL_STRUCT_DEBUG
  // heavy handed knob for full debug output, left in place for convenience
  isDbg = true;
#endif
  
  buildProcLoopNests(pStrct, p, isIrrIvalLoop, isFwdSubst, procNmMgr, isDbg);
  coalesceAlienChildren(pStrct);
  renumberAlienScopes(pStrct);
  
  return pStrct;
}


static void
debugCFGInfo(BinUtil::Proc* p)
{
  static const int sepWidth = 77;

  using std::setfill;
  using std::setw;
  using BAnal::OAInterface;
    
  OA::OA_ptr<OAInterface> irIF; irIF = new OAInterface(p);
    
  OA::OA_ptr<OA::CFG::ManagerCFGStandard> cfgmanstd;
  cfgmanstd = new OA::CFG::ManagerCFGStandard(irIF);

  OA::OA_ptr<OA::CFG::CFG> cfg =
    cfgmanstd->performAnalysis(TY_TO_IRHNDL(p, OA::ProcHandle));
    
  OA::OA_ptr<OA::RIFG> rifg; 
  rifg = new OA::RIFG(cfg, cfg->getEntry(), cfg->getExit());

  OA::OA_ptr<OA::NestedSCR> tarj;
  tarj = new OA::NestedSCR(rifg);
    
  cerr << setfill('=') << setw(sepWidth) << "=" << endl;
  cerr << "Procedure: " << p->name() << endl << endl;

  OA::OA_ptr<OA::OutputBuilder> ob1, ob2;
  ob1 = new OA::OutputBuilderText(cerr);
  ob2 = new OA::OutputBuilderDOT(cerr);

  cerr << setfill('-') << setw(sepWidth) << "-" << endl;
  cerr << "*** CFG Text: [nodes, edges: "
       << cfg->getNumNodes() << ", " << cfg->getNumEdges() << "]\n";
  cfg->configOutput(ob1);
  cfg->output(*irIF);

  cerr << setfill('-') << setw(sepWidth) << "-" << endl;
  cerr << "*** CFG DOT:\n";
  cfg->configOutput(ob2);
  cfg->output(*irIF);
  cerr << endl;

  cerr << setfill('-') << setw(sepWidth) << "-" << endl;
  cerr << "*** Nested SCR (Tarjan) Tree\n";
  tarj->dump(cerr);
  cerr << endl;

  cerr << setfill('-') << setw(sepWidth) << "-" << endl;
  cerr << endl << flush;
}


// buildProcLoopNests: Build procedure structure by traversing
// the Nested SCR (Tarjan tree) to create loop nests and statement
// scopes.
static int
buildProcLoopNests(Prof::Struct::Proc* pStrct, BinUtil::Proc* p,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, bool isDbg)
{
  static const int sepWidth = 77;
  using std::setfill;
  using std::setw;

  try {
    using BAnal::OAInterface;
    
    OA::OA_ptr<OAInterface> irIF; irIF = new OAInterface(p);
    
    OA::OA_ptr<OA::CFG::ManagerCFGStandard> cfgmanstd;
    cfgmanstd = new OA::CFG::ManagerCFGStandard(irIF);
    OA::OA_ptr<OA::CFG::CFG> cfg =
      cfgmanstd->performAnalysis(TY_TO_IRHNDL(p, OA::ProcHandle));
    
    OA::OA_ptr<OA::RIFG> rifg;
    rifg = new OA::RIFG(cfg, cfg->getEntry(), cfg->getExit());
    OA::OA_ptr<OA::NestedSCR> tarj; tarj = new OA::NestedSCR(rifg);
    
    OA::RIFG::NodeId fgRoot = rifg->getSource();

    if (isDbg) {
      debugCFGInfo(p);
    }

    int r = buildProcLoopNests(pStrct, p, tarj, cfg, fgRoot,
			       isIrrIvalLoop, isFwdSubst, procNmMgr, isDbg);

    if (isDbg) {
      cerr << setfill('-') << setw(sepWidth) << "-" << endl;
    }

    return r;
  }
  catch (const OA::Exception& x) {
    std::ostringstream os;
    x.report(os);
    DIAG_Throw("[OpenAnalysis] " << os.str());
  }
}


static void
processInterval(BinUtil::Proc* p,
		Prof::Struct::ACodeNode* topScope,
		Prof::Struct::ACodeNode* enclosingScope,
		OA::OA_ptr<OA::CFG::CFGInterface> cfg,
		OA::OA_ptr<OA::NestedSCR> tarj,
		OA::RIFG::NodeId flowGraphNodeId,
		std::vector<bool> &isNodeProcessed,
		Struct::LocationMgr &locMgr,
		bool isIrrIvalLoop, bool isFwdSubst,
		ProcNameMgr* procNmMgr, int &nLoops, bool isDbg)
{
  Prof::Struct::ACodeNode* currentScope;

  // --------------------------------------------------------------
  // build the scope tree representation for statements in the current
  // interval in the flowgraph. if the interval is a loop, the scope
  // returned from buildLoopAndStmts will be a loop scope. this loop
  // scope will be correctly positioned in the scope tree relative to
  // alien contexts from inlined code; however it will not yet be
  // nested inside the loops created by an outer call to
  // processInterval. we correct that at the end after processing
  // nested scopes.
  // --------------------------------------------------------------
  currentScope = 
    buildLoopAndStmts(locMgr, topScope, enclosingScope, p, tarj, cfg, flowGraphNodeId, 
		      isIrrIvalLoop, procNmMgr);

  isNodeProcessed[flowGraphNodeId] = true;
    
  // --------------------------------------------------------------
  // build the scope tree representation for nested intervals in the
  // flowgraph
  // --------------------------------------------------------------
  for(OA::RIFG::NodeId flowGraphKidId = tarj->getInners(flowGraphNodeId); 
      flowGraphKidId != OA::RIFG::NIL; 
      flowGraphKidId = tarj->getNext(flowGraphKidId)) { 

    processInterval(p, topScope, currentScope, cfg, tarj, flowGraphKidId,
		    isNodeProcessed, locMgr, isIrrIvalLoop, isFwdSubst,
		    procNmMgr, nLoops, isDbg);
  }

  if (currentScope != enclosingScope) {
    // --------------------------------------------------------------
    // if my enclosing scope is a loop ...
    // --------------------------------------------------------------
    if (enclosingScope->type() == Prof::Struct::ANode::TyLoop) {
      // --------------------------------------------------------------
      // find a visible ancestor of enclosingScope. this should be the
      // least common ancestor (lca) of currentScope and
      // enclosingScope. all of the nodes along the path from
      // currentScope to the lca belong nested enclosingScope.
      // --------------------------------------------------------------
      Prof::Struct::ANode* lca = getVisibleAncestor(enclosingScope);

      // --------------------------------------------------------------
      // adjust my location in the scope tree so that it is inside 
      // my enclosing scope. 
      // --------------------------------------------------------------
      reparentNode(currentScope, enclosingScope, lca, locMgr, 
		   currentScope->ancestorCount(), lca->ancestorCount()); 
    }
    nLoops++;
  }
}


// buildProcLoopNests: Visit the object code loops using DFS and create 
// source code representations. The resulting loops are UNNORMALIZED.
static int
buildProcLoopNests(Prof::Struct::Proc* enclosingProc, BinUtil::Proc* p,
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::CFGInterface> cfg,
		   OA::RIFG::NodeId fgRoot,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, bool isDbg)
{
  int nLoops = 0;

  std::vector<bool> isNodeProcessed(tarj->getRIFG()->getHighWaterMarkNodeId() + 1);
  
  Struct::LocationMgr locMgr(enclosingProc->ancestorLM());
  if (isDbg) {
    locMgr.debug(1);
  }

  locMgr.begSeq(enclosingProc, isFwdSubst);
  
  // ----------------------------------------------------------
  // Process the Nested SCR (Tarjan tree) in depth first order
  // ----------------------------------------------------------
  processInterval(p, enclosingProc, enclosingProc, cfg,
		  tarj, fgRoot, isNodeProcessed, locMgr,
		  isIrrIvalLoop, isFwdSubst, procNmMgr, nLoops, isDbg);

  // -------------------------------------------------------
  // Process any nodes that we have not already visited.
  //
  // This may occur if the CFG is disconnected.  E.g., we do not
  // correctly handle jump tables.  Note that we cannot be sure of the
  // location of these statements within procedure structure.
  // -------------------------------------------------------
  for (uint i = 1; i < isNodeProcessed.size(); ++i) {
    if (!isNodeProcessed[i]) {
      // INVARIANT: return value is never a Struct::Loop
      buildLoopAndStmts(locMgr, enclosingProc, enclosingProc, p, tarj, cfg, i, isIrrIvalLoop,
			procNmMgr);
    }
  }

  locMgr.endSeq();

  return nLoops;
}


// buildLoopAndStmts: Returns the expected (or 'original') enclosing
// scope for children of 'fgNode', e.g. loop or proc.
static Prof::Struct::ACodeNode*
buildLoopAndStmts(Struct::LocationMgr& locMgr,
		  Prof::Struct::ACodeNode* topScope, Prof::Struct::ACodeNode* enclosingScope,
		  BinUtil::Proc* p, OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::CFGInterface> GCC_ATTR_UNUSED cfg,
		  OA::RIFG::NodeId fgNode,
		  bool isIrrIvalLoop, ProcNameMgr* procNmMgr)
{
  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::OA_ptr<OA::CFG::NodeInterface> bb =
    rifg->getNode(fgNode).convert<OA::CFG::NodeInterface>();
  BinUtil::Insn* insn = BAnal::OA_CFG_getBegInsn(bb);
  VMA begVMA = (insn) ? insn->opVMA() : 0;
  Prof::Struct::Loop* loop = NULL;

  string fnm, pnm;
  SrcFile::ln line;
  VMA loop_vma;
  
  DIAG_DevMsg(10, "buildLoopAndStmts: " << bb << " [id: " << bb->getId() << "] " 
	      << hex << begVMA << " --> " << enclosingScope << dec 
	      << " " << enclosingScope->toString_id());

  Prof::Struct::ACodeNode* targetScope = enclosingScope;
  OA::NestedSCR::Node_t ity = tarj->getNodeType(fgNode);

  if (ity == OA::NestedSCR::NODE_ACYCLIC
      || ity == OA::NestedSCR::NODE_NOTHING) {
    // -----------------------------------------------------
    // ACYCLIC: No loops
    // -----------------------------------------------------
  } else if (ity == OA::NestedSCR::NODE_INTERVAL ||
	   (isIrrIvalLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE)) {
    // -----------------------------------------------------
    // INTERVAL or IRREDUCIBLE as a loop: Loop head
    // -----------------------------------------------------
    findLoopBegLineInfo(p, tarj, bb, fnm, pnm, line, loop_vma);
    pnm = BinUtil::canonicalizeProcName(pnm, procNmMgr);
    
    loop = new Prof::Struct::Loop(NULL, fnm, line, line);
    loop->vmaSet().insert(loop_vma, loop_vma + 1); // a loop id
    targetScope = loop;

#if DEBUG_CFG_SOURCE
    cout << "\nloop:  " << ((ity == OA::NestedSCR::NODE_INTERVAL) ?
			    "(reducible)" : "(irreducible)") << "\n";
    cout << "loop node: (n=" << loop->id() << ")"
	 << "  0x" << hex << loop_vma << dec<< "  l=" << line
	 << "  f='" << fnm << "'  p='" << pnm << "'\n"
	 << "entry block:  0x" << hex << begVMA << dec << "\n";
#endif
  } else if (!isIrrIvalLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE) {
    // -----------------------------------------------------
    // IRREDUCIBLE as no loop: May contain loops
    // -----------------------------------------------------
  } else {
    DIAG_Die("Should never reach!");
  }

  // -----------------------------------------------------
  // Process instructions within BB
  // -----------------------------------------------------
  std::list<Prof::Struct::ACodeNode *> kids;
  buildStmts(locMgr, topScope, p, bb, procNmMgr, kids, targetScope->id());
  
  if (loop) { 
    locMgr.locate(loop, topScope, fnm, pnm, line, targetScope->id(), procNmMgr);
#if FULL_STRUCT_DEBUG
    if (ancestorIsLoop(loop)) willBeCycle();
#endif
  }

  if (typeid(*targetScope) == typeid(Prof::Struct::Loop)) {
    // -----------------------------------------------------
    // reparent statements into the target loop 
    // -----------------------------------------------------
    Prof::Struct::ANode *targetScopeParent = getVisibleAncestor(targetScope); 
    int targetScopeParentDepth = targetScopeParent->ancestorCount(); 
    for (std::list<Prof::Struct::ACodeNode *>::iterator kid = kids.begin(); 
	 kid != kids.end(); ++kid) {
      Prof::Struct::ANode *node = *kid;
      reparentNode(node, targetScope, targetScopeParent, locMgr, 
		   node->ancestorCount(), targetScopeParentDepth);
    }
  }

  return targetScope;
}


// buildStmts: Form loop structure by setting bounds and adding
// statements from the current basic block to 'enclosingStrct'.  Does
// not add duplicates.
static int
buildStmts(Struct::LocationMgr& locMgr,
	   Prof::Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p,
	   OA::OA_ptr<OA::CFG::NodeInterface> bb, ProcNameMgr* procNmMgr, 
	   std::list<Prof::Struct::ACodeNode *> &insertions, int targetScopeID)
{
  static int call_sortId = 0;

#if DEBUG_CFG_SOURCE
  cout << "\nblock:\n";
#endif

  OA::OA_ptr<OA::CFG::NodeStatementsIteratorInterface> it =
    bb->getNodeStatementsIterator();
  for ( ; it->isValid(); ) {
    BinUtil::Insn* insn = IRHNDL_TO_TY(it->current(), BinUtil::Insn*);
    VMA vma = insn->vma();
    ushort opIdx = insn->opIndex();
    VMA opvma = insn->opVMA();

    // advance iterator [needed when creating VMA interval]
    ++(*it);

    // 1. gather source code info
    string filenm, procnm;
    SrcFile::ln line;
    p->findSrcCodeInfo(vma, opIdx, procnm, filenm, line);
    procnm = BinUtil::canonicalizeProcName(procnm, procNmMgr);

    // 2. create a VMA interval
    // the next (or hypothetically next) insn begins no earlier than:
    BinUtil::Insn* n_insn = (it->isValid()) ?
      IRHNDL_TO_TY(it->current(), BinUtil::Insn*) : NULL;
    VMA n_opvma = (n_insn) ? n_insn->opVMA() : insn->endVMA();
    DIAG_Assert(opvma < n_opvma, "Invalid VMAInterval: [" << opvma << ", "
		<< n_opvma << ")");

    VMAInterval vmaint(opvma, n_opvma);

    // 3. Get statement type.  Use a special sort key for calls as a
    // way to protect against bad debugging information which would
    // later cause a call to treated as loop-invariant-code motion and
    // hoisted into a different loop.
    ISA::InsnDesc idesc = insn->desc();
    
    // 4. locate stmt
    Prof::Struct::Stmt* stmt =
      new Prof::Struct::Stmt(NULL, line, line, vmaint.beg(), vmaint.end());

#if DEBUG_CFG_SOURCE
    debugStmt(stmt, vma, filenm, procnm, line);
#endif

    if (idesc.isSubr()) {
      stmt->sortId(--call_sortId);
    }
    insertions.push_back(stmt);
    locMgr.locate(stmt, enclosingStrct, filenm, procnm, line, targetScopeID, procNmMgr);
  }
  return 0;
}


// findLoopBegLineInfo: Given the head basic block node of the loop,
// find loop begin source line information.
//
// The routine first attempts to use the source line information for
// the backward branch, if one exists.  Then, it consults the 'head'
// instruction.
//
// Note: It is possible to have multiple or no backward branches
// (e.g. an 'unstructured' loop written with IFs and GOTOs).  In the
// former case, we take the smallest source line of them all; in the
// latter we use headVMA.
static void
findLoopBegLineInfo(BinUtil::Proc* p, OA::OA_ptr<OA::NestedSCR> tarj, 
		    OA::OA_ptr<OA::CFG::NodeInterface> headBB,
		    string& begFileNm, string& begProcNm, SrcFile::ln& begLn, VMA &loop_vma)
{
  using namespace OA::CFG;

  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::RIFG::NodeId loopHeadNodeId = rifg->getNodeId(headBB);

  begLn = SrcFile::ln_NULL;

  int savedPriority = 0;

  // Find the head vma
  BinUtil::Insn* head = BAnal::OA_CFG_getBegInsn(headBB);
  VMA headVMA = head->vma();
  ushort headOpIdx = head->opIndex();
  DIAG_Assert(headOpIdx == 0, "Target of a branch at " << headVMA
	      << " has op-index of: " << headOpIdx);

  
  // ------------------------------------------------------------
  // Attempt to use backward branch to find loop begin line (see note above)
  // ------------------------------------------------------------
  OA::OA_ptr<EdgesIteratorInterface> it
    = headBB->getCFGIncomingEdgesIterator();
  for ( ; it->isValid(); ++(*it)) {
    OA::OA_ptr<EdgeInterface> e = it->currentCFGEdge();

    OA::OA_ptr<NodeInterface> bb = e->getCFGSource();

    BinUtil::Insn* insn = BAnal::OA_CFG_getEndInsn(bb);

    int currentPriority = 0;


    if (!insn) {
      // we failed to get the instruction. ignore it.
      continue;
    }
    
    VMA vma = insn->vma();

    EdgeType etype = e->getType();

    // compute priority score based on branch type.
    // backward branches are most reliable.
    // apparent backward branches are second best.
    // forward branch seems to be better than nothing.
    switch (etype) {
    case BACK_EDGE:
      currentPriority = 4;
      break;
    case TRUE_EDGE:
    case FALSE_EDGE:
      if (tarj->getOuter(rifg->getNodeId(bb)) != loopHeadNodeId) {
	// if the source of a true or false edge is not within 
	// the tarjan interval, ignore it.
	continue; 
      }
      if (vma >= headVMA) {
        // apparent backward branch
        currentPriority = 3;
      } else currentPriority = 2;
      break;
    case FALLTHROUGH_EDGE:
      // if the source of a fall through edge is within the same loop, 
      // consider it, but at low priority.
      if (tarj->getOuter(rifg->getNodeId(bb)) == loopHeadNodeId) {
        currentPriority = 1;
        break;
      }
    default:
      continue;
    }

    // If we have a backward edge, find the source line of the
    // backward branch.  Note: back edges are not always labeled as such!
    // if (etype == BACK_EDGE || vma >= headVMA) {
    if (currentPriority >= savedPriority) {
      ushort opIdx = insn->opIndex();
      SrcFile::ln line;
      string filenm, procnm;
      p->findSrcCodeInfo(vma, opIdx, procnm, filenm, line);
      if (SrcFile::isValid(line)) {
	if (!SrcFile::isValid(begLn) || currentPriority > savedPriority || 
	    begFileNm.compare(filenm) != 0 || line < begLn) {
	  // NOTE: if inlining information is available, in the case where the priority is the same, 
	  //       we might be able to improve the decision to change the source mapping by comparing
	  //       the length of the inlined sequences and keep the shorter one. leave this adjustment
	  //       for another time, if it seems warranted.
	  begLn = line;
	  begFileNm = filenm;
	  begProcNm = procnm;
	  loop_vma = vma;
	  savedPriority = currentPriority;
	}
      }
    }
  }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  if (!SrcFile::isValid(begLn)) {
    // if no candidate source mapping for the loop has been identified so far, 
    // default to the mapping for the first instruction in the loop header
    p->findSrcCodeInfo(headVMA, headOpIdx, begProcNm, begFileNm, begLn);
    loop_vma = headVMA;
  }

#ifdef BANAL_USE_SYMTAB
  if (loop_vma != headVMA) {
    // Compilers today are not perfect. Our experience is that neither
    // the source code location associated with the source of a flow 
    // graph edge entering the loop nor the source location of the loop 
    // head is always the best answer for 
    // where to locate a loop in the presence of inlined code. Here,
    // we consider both as candidates and pick the one that has a 
    // shallower depth of inlining (if any). If there is no difference,
    // favor the location of the backward branch.
    Inline::InlineSeqn loop_vma_nodelist;
    Inline::InlineSeqn head_vma_nodelist;
    Inline::analyzeAddr(head_vma_nodelist, headVMA);
    Inline::analyzeAddr(loop_vma_nodelist, loop_vma);

    if (head_vma_nodelist.size() < loop_vma_nodelist.size()) {
      p->findSrcCodeInfo(headVMA, headOpIdx, begProcNm, begFileNm, begLn);
      loop_vma = headVMA;
    }
  }
#endif
}

} // namespace Struct
} // namespace BAnal

#endif  // open analysis


//****************************************************************************
// ParseAPI code for functions, loops and blocks
//****************************************************************************

// Mostly done items:
//
// 1. There is a major regression in the pretty_names_begin() iterator
// between Dyninst 8.2-release and git head in analyzeAddr() in
// Struct-Inline.cpp.  The iterator is always empty, so our inlined
// procedure names are always "unknown-proc".  Wisconsin is working on
// it, but until it is fixed, the inline part of the struct files will
// be badly broken.
// ---> now fixed in dyninst git master
//
// 2. Figure out interaction between reparentNode() and location
// manager and make sure they don't work at cross purposes.
// ---> don't use the location manager
//
// 3. findLoopHeader() -- look at entry blocks, back edges and line
// numbers and make an intelligent decision for where a loop begins in
// the source code.
// ---> mostly done, need to fine-tune heuristics, reparenting
//
// 8. Demangle proc names for the hpcstruct file.
// ---> added the binutils version, may need parseapi names
//
// 9. Improve scope info and alien map in makeScopeTree().
// ---> partially done, open ended on how to handle cases where
// ---> inline info does match parseapi tree
//
// 11. Add address ranges to buildLMSkeleton().
// ---> use LM->findProc() to lookup BinUtils proc
//
// 12. Add call_sortId from buildStmts() to doBlock().
// ---> probably don't need this
//
// 13. Demangle names in the Open Analysis case.
// ---> now using __cxa_demangle() in BinUtils.cpp.
//
// --------------------------------------------------
//
// Remaining TO-DO items:
//
// 4. Irreducible loops -- study entry blocks, loop header, adjacent
// nested loops.  Some nested irreducible loops share entry blocks and
// maybe should be merged.
//
// 5. Compute line ranges for loops and procs to help decide what is
// alien code when the symtab inline info is not available.
//
// 6. Handle code movement wrt loops: loop fusion, fission, moving
// code in/out of loops.
//
// 7. Maybe write our own functions for coalescing statements and
// aliens and normalizing source code transforms.
//
// 10. Decide how to handle basic blocks that belong to multiple
// functions.
//
// 14. Import fix for duplicate proc names (pretty vs. typed/mangled).
//
// 15. Some missing file names are "~unknown-file~", some are "", they
// string match to not equal, causing a spurious alien.
//

#ifdef BANAL_USE_PARSEAPI

// The ParseAPI version of traversing functions, CFGs, loops, basic
// blocks and instructions.  This is the code between makeStructure()
// and creating Prof::Struct nodes for loops, stmts, etc.

namespace BAnal {
namespace Struct {

class HeaderInfo;
class ScopeInfo;

typedef map <Block *, bool> BlockSet;
typedef map <VMA, HeaderInfo> HeaderList;
typedef map <long, Prof::Struct::ACodeNode *> AlienScopeMap;

static Prof::Struct::Proc *
makeProcScope(Prof::Struct::LM *, ProcInfo, ParseAPI::Function *,
	      TreeNode *, const ParseAPI::Function::blocklist &, StringTable &);

static TreeNode *
deleteInlinePrefix(TreeNode *, Inline::InlineSeqn, StringTable &);

static LoopList *
doLoopTree(ProcInfo, ParseAPI::Function *, BlockSet &, LoopTreeNode *,
	   StringTable &, ProcNameMgr *);

static TreeNode *
doLoopLate(ProcInfo, ParseAPI::Function *, BlockSet &, Loop *, const string &,
	   StringTable &, ProcNameMgr *);

static void
doBlock(ProcInfo, ParseAPI::Function *, BlockSet &, Block *, TreeNode *,
	StringTable &, ProcNameMgr *);

static LoopInfo *
findLoopHeader(ProcInfo, ParseAPI::Function *, TreeNode *, Loop *, const string &,
	       StringTable &, ProcNameMgr *);

static void
makeScopeTree(Prof::Struct::ACodeNode *, ScopeInfo, TreeNode *,
	      StringTable &, ProcNameMgr *);

#if DEBUG_CFG_SOURCE
static string
debugPrettyName(const string &);

static void
debugLoop(ProcInfo, ParseAPI::Function *, Loop *, const string &,
	  vector <Edge *> &, HeaderList &);

static void
debugInlineTree(TreeNode *, LoopInfo *, StringTable &, int, bool);
#endif

// Info on candidates for loop header.
class HeaderInfo {
public:
  Block * block;
  bool  is_src;
  bool  is_targ;
  bool  is_cond;
  bool  in_incl;
  bool  in_excl;
  int   depth;
  int   score;

  HeaderInfo(Block * blk = NULL)
  {
    block = blk;
    is_src = false;
    is_targ = false;
    is_cond = false;
    in_incl = false;
    in_excl = false;
    depth = 0;
    score = 0;
  }
};

// Info on enclosing node in scope tree.
class ScopeInfo {
public:
  long  file_index;
  SrcFile::ln line;
  bool  is_alien;
  long  proc_index;

  ScopeInfo(long file, SrcFile::ln ln, bool alien = false, long proc = 0)
  {
    file_index = file;
    line = ln;
    is_alien = alien;
    proc_index = proc;
  }
};

//****************************************************************************

// One binutils proc may contain multiple embedded parseapi functions.
// In that case, we create new proc/file scope nodes for each function
// and strip the inline prefix at the call source from the embed
// function.  This often happens with openmp parallel pragmas.
// 
static void
doFunctionList(Prof::Struct::LM * lm, ProcInfo pinfo,
	       StringTable & strTab, ProcNameMgr * nameMgr)
{
  Prof::Struct::Proc * procScope = pinfo.proc;
  FuncList * func_list = pinfo.func_list;
  long num_funcs = func_list->size();

  // make a map of internal call edges (from target to source) across
  // all funcs in this group.  we use this to strip the inline seqn at
  // the call source from the target func.
  //
  std::map <VMA, VMA> callMap;

  if (num_funcs > 1) {
    for (auto fit = func_list->begin(); fit != func_list->end(); ++fit) {
      ParseAPI::Function * func = *fit;
      const ParseAPI::Function::edgelist & elist = func->callEdges();

      for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
	VMA src = (*eit)->src()->last();
	VMA targ = (*eit)->trg()->start();
	callMap[targ] = src;
      }
    }
  }

  // one binutils proc may contain several parseapi funcs
  long num = 0;
  for (auto fit = func_list->begin(); fit != func_list->end(); ++fit)
  {
    ParseAPI::Function * func = *fit;
    TreeNode * root = new TreeNode;
    num++;

    // compute the inline seqn for the call site for this func, if
    // there is one.
    Inline::InlineSeqn prefix;
    auto call_it = callMap.find(func->addr());

    if (call_it != callMap.end()) {
      Inline::analyzeAddr(prefix, call_it->second);
    }

#if DEBUG_CFG_SOURCE
    Prof::Struct::File * file = dynamic_cast <Prof::Struct::File *> (pinfo.proc->parent());

    cout << "\n------------------------------------------------------------\n"
	 << "func:  0x" << hex << func->addr() << dec
	 << "  (" << num << "/" << num_funcs << ")"
	 << "  bin='" << pinfo.proc_bin->name()
	 << "'  parse='" << func->name() << "'\n"
	 << "file:  '" << file->name() << "'\n";

    if (call_it != callMap.end()) {
      cout << "\ncall site prefix:  0x" << hex << call_it->second
	   << " -> 0x" << call_it->first << dec << "\n";
      for (auto pit = prefix.begin(); pit != prefix.end(); ++pit) {
	cout << "inline:  l=" << pit->getLineNum()
	     << "  f='" << pit->getFileName()
	     << "'  p='" << debugPrettyName(pit->getProcName()) << "'\n";
      }
    }
#endif

    // basic blocks for this function
    const ParseAPI::Function::blocklist & blist = func->blocks();
    BlockSet visited;

    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      visited[block] = false;
    }

    // traverse the loop (Tarjan) tree
    LoopList *llist = doLoopTree(pinfo, func, visited, func->getLoopTree(), strTab, nameMgr);

#if DEBUG_CFG_SOURCE
    cout << "\nnon-loop blocks:\n";
#endif

    // process any blocks not in a loop
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      if (! visited[block]) {
	doBlock(pinfo, func, visited, block, root, strTab, nameMgr);
      }
    }

    // merge the loops into the proc's inline tree
    FLPSeqn empty;

    for (auto it = llist->begin(); it != llist->end(); ++it) {
      mergeInlineLoop(root, empty, *it);
    }

    // delete the inline prefix from this func, if non-empty
    if (! prefix.empty()) {
      root = deleteInlinePrefix(root, prefix, strTab);
    }

#if DEBUG_CFG_SOURCE
    cout << "\nfinal inline tree:  (" << num << "/" << num_funcs << ")"
	 << "  bin='" << pinfo.proc_bin->name()
	 << "'  parse='" << func->name() << "'\n";

    if (call_it != callMap.end()) {
      cout << "\ncall site prefix:  0x" << hex << call_it->second
	   << " -> 0x" << call_it->first << dec << "\n";
      for (auto pit = prefix.begin(); pit != prefix.end(); ++pit) {
	cout << "inline:  l=" << pit->getLineNum()
	     << "  f='" << pit->getFileName()
	     << "'  p='" << debugPrettyName(pit->getProcName()) << "'\n";
      }
    }
    cout << "\n";
    debugInlineTree(root, NULL, strTab, 0, true);
    cout << "\nend proc:  (" << num << "/" << num_funcs << ")"
	 << "  bin='" << pinfo.proc_bin->name()
	 << "'  parse='" << func->name() << "'\n";
#endif

    // multiple funcs get their own proc/file scope nodes
    if (num > 1) {
      procScope = makeProcScope(lm, pinfo, func, root, blist, strTab);
    }
    Prof::Struct::File * fileScope = dynamic_cast <Prof::Struct::File *> (procScope->parent());
    long file_index = strTab.str2index(fileScope->name());

    // convert inline tree to Prof::Struct scope tree.  need to preserve
    // the original procScope begin line, or else it gets reset too low.
    SrcFile::ln beg_line = procScope->begLine();

    makeScopeTree(procScope, ScopeInfo(file_index, beg_line), root, strTab, nameMgr);
    if (beg_line > 0) {
      procScope->begLine(beg_line);
    }

    // fixme: may or may not use these
#if 0
    coalesceAlienChildren(procScope);
    renumberAlienScopes(procScope);
#endif

    delete root;
  }
}


// Locate the beginning line number and VMA range for the inline tree
// 'root' and make new Proc and File scope nodes for this function.
// This is only for multiple parseAPI funcs within one binutils proc.
//
// Returns: proc scope node
//
static Prof::Struct::Proc *
makeProcScope(Prof::Struct::LM * lm, ProcInfo pinfo, ParseAPI::Function * func,
	      TreeNode * root, const ParseAPI::Function::blocklist & blist,
	      StringTable & strTab)
{
  long empty_index = strTab.str2index("");
  long file_index = empty_index;
  SrcFile::ln beg_line = 0;

  // locate the func at the file/line of the lowest VMA among root's
  // stmts (no inline steps) that has a non-empty file.  fall back on
  // the original binutils proc name (eg, no debug info).
  //
  for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
    StmtInfo *sinfo = sit->second;

    if (sinfo->file_index != empty_index && sinfo->line_num != 0) {
      file_index = sinfo->file_index;
      beg_line = sinfo->line_num;
      break;
    }
  }

  string filenm = (file_index != empty_index) ?
      strTab.index2str(file_index) : pinfo.proc_bin->filename();
  string basenm = FileUtil::basename(filenm.c_str());
  stringstream buf;

  buf << "outline " << basenm << ":" << beg_line
      << " (0x" << hex << func->addr() << dec << ")";

  Prof::Struct::File * fileScope = Prof::Struct::File::demand(lm, filenm);
  Prof::Struct::Proc * procScope =
      Prof::Struct::Proc::demand(fileScope, buf.str(), func->name(),
				 beg_line, beg_line, NULL);

  // scan the func's basic blocks and set min/max VMA
  VMA beg_vma = 0;
  VMA end_vma = 0;

  auto bit = blist.begin();
  if (bit != blist.end()) {
    beg_vma = (*bit)->start();
    end_vma = (*bit)->end();
  }
  for (; bit != blist.end(); ++bit) {
    beg_vma = std::min(beg_vma, (VMA) (*bit)->start());
    end_vma = std::max(end_vma, (VMA) (*bit)->end());
  }
  procScope->vmaSet().insert(beg_vma, end_vma);

  return procScope;
}


// Delete the inline sequence 'prefix' from root's tree and reparent
// any statements or loops.  We expect there to be no statements,
// loops or subtrees along the deleted spine, but if there are, move
// them to the subtree.
//
// Returns: the subtree at the end of prefix.
//
static TreeNode *
deleteInlinePrefix(TreeNode * root, Inline::InlineSeqn prefix, StringTable & strTab)
{
  StmtMap  stmts;
  LoopList loops;

  // walk the prefix and collect any stmts or loops
  for (auto pit = prefix.begin(); pit != prefix.end(); ++pit)
  {
    FLPIndex flp(strTab, *pit);
    auto nit = root->nodeMap.find(flp);

    if (nit != root->nodeMap.end()) {
      TreeNode * subtree = nit->second;

      // statements
      for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
	stmts[sit->first] = sit->second;
      }
      root->stmtMap.clear();

      // loops
      for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
	loops.push_back(*lit);
      }
      root->loopList.clear();

      // subtrees
      for (auto it = root->nodeMap.begin(); it != root->nodeMap.end(); ++it) {
	TreeNode * node = it->second;
	if (node != subtree) {
	  mergeInlineEdge(subtree, it->first, node);
	}
      }
      root->nodeMap.clear();
      delete root;

      root = subtree;
    }
  }

  // reattach the stmts and loops
  for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
    root->stmtMap[sit->first] = sit->second;
  }
  stmts.clear();

  for (auto lit = loops.begin(); lit != loops.end(); ++lit) {
    root->loopList.push_back(*lit);
  }
  loops.clear();

  return root;
}


// Returns: list of LoopInfo objects.
//
// If the loop at this node is non-null (internal node), then the list
// contains one element for that loop.  If the loop is null (root),
// then the list contains one element for each subtree.
//
static LoopList *
doLoopTree(ProcInfo pinfo, ParseAPI::Function * func,
	   BlockSet & visited, LoopTreeNode * ltnode,
	   StringTable & strTab, ProcNameMgr * nameMgr)
{
  LoopList * myList = new LoopList;

  if (ltnode == NULL) {
    return myList;
  }
  Loop *loop = ltnode->loop;

  // process the children of the loop tree
  vector <LoopTreeNode *> clist = ltnode->children;

  for (uint i = 0; i < clist.size(); i++) {
    LoopList *subList = doLoopTree(pinfo, func, visited, clist[i], strTab, nameMgr);

    for (auto sit = subList->begin(); sit != subList->end(); ++sit) {
      myList->push_back(*sit);
    }
    delete subList;
  }

  // if no loop at this node (root node), then return the list of
  // children.
  if (loop == NULL) {
    return myList;
  }

  // otherwise, finish this loop, put into LoopInfo format, insert the
  // subloops and return a list of one.
  string loopName = ltnode->name();
  FLPSeqn empty;

  TreeNode * myLoop = doLoopLate(pinfo, func, visited, loop, loopName, strTab, nameMgr);

  for (auto it = myList->begin(); it != myList->end(); ++it) {
    mergeInlineLoop(myLoop, empty, *it);
  }

  // reparent the tree and put into LoopInfo format
  LoopInfo * myInfo = findLoopHeader(pinfo, func, myLoop, loop, loopName, strTab, nameMgr);

  myList->clear();
  myList->push_back(myInfo);

  return myList;
}


// Post-order process for one loop, after the subloops.  Add any
// leftover inclusive blocks, select the loop header, reparent as
// needed, remove the top inline spine, and put into the LoopInfo
// format.
//
// Returns: the raw inline tree for this loop.
//
static TreeNode *
doLoopLate(ProcInfo pinfo, ParseAPI::Function * func,
	   BlockSet & visited, Loop * loop, const string & loopName,
	   StringTable & strTab, ProcNameMgr * nameMgr)
{
  TreeNode * root = new TreeNode;

  DEBUG_MESG("\nbegin loop:  " << loopName << "  '"
	     << func->name() << "'\n");

  // add the inclusive blocks not contained in a subloop
  vector <Block *> blist;
  loop->getLoopBasicBlocks(blist);

  for (uint i = 0; i < blist.size(); i++) {
    if (! visited[blist[i]]) {
      doBlock(pinfo, func, visited, blist[i], root, strTab, nameMgr);
    }
  }

  return root;
}


// Process one basic block.
//
static void
doBlock(ProcInfo pinfo, ParseAPI::Function * func,
	BlockSet & visited, Block * block, TreeNode * root,
	StringTable & strTab, ProcNameMgr * nameMgr)
{
  if (block == NULL || visited[block]) {
    return;
  }
  visited[block] = true;

#if DEBUG_CFG_SOURCE
  cout << "\nblock:\n";
#endif

  // iterate through the instructions in this block
  map <Offset, InstructionAPI::Instruction::Ptr> imap;
  block->getInsns(imap);

  for (auto iit = imap.begin(); iit != imap.end(); ++iit) {
    Offset vma = iit->first;
    int    len = iit->second->size();
    string procnm;
    string filenm;
    SrcFile::ln line;

    pinfo.proc_bin->findSrcCodeInfo(vma, 0, procnm, filenm, line);
    procnm = BinUtil::canonicalizeProcName(procnm, nameMgr);

#if DEBUG_CFG_SOURCE
    debugStmt(NULL, vma, filenm, procnm, line);
#endif

    addStmtToTree(root, strTab, vma, len, filenm, line, procnm);
  }
}


// New heuristic for identifying loop header inside inline tree.
// Start at the root, descend the inline tree and try to find where
// the loop begins.  This is the central problem of all hpcstruct:
// where to locate a loop inside an inline sequence.
//
// Note: loop "headers" are no longer tied to a specific VMA and
// machine instruction.  They are strictly file and line number.
// (For some loops, there is no right VMA.)
//
// Returns: detached LoopInfo object.
//
static LoopInfo *
findLoopHeader(ProcInfo pinfo, ParseAPI::Function * func, TreeNode * root,
	       Loop * loop, const string & loopName,
	       StringTable & strTab, ProcNameMgr * nameMgr)
{
  Prof::Struct::File * file = dynamic_cast <Prof::Struct::File *> (pinfo.proc->parent());
  long base_index = strTab.str2index(FileUtil::basename(file->name()));
  string procName = func->name();

  //------------------------------------------------------------
  // Step 1 -- build the list of loop exit conditions
  //------------------------------------------------------------

  vector <Block *> inclBlocks;
  set <Block *> bset;
  HeaderList clist;

  loop->getLoopBasicBlocks(inclBlocks);
  for (auto bit = inclBlocks.begin(); bit != inclBlocks.end(); ++bit) {
    bset.insert(*bit);
  }

  // a stmt is a loop exit condition if it has outgoing edges to
  // blocks both inside and outside the loop.
  //
  for (auto bit = inclBlocks.begin(); bit != inclBlocks.end(); ++bit) {
    const Block::edgelist & outEdges = (*bit)->targets();
    VMA src_vma = (*bit)->last();
    bool in_loop = false, out_loop = false;

    for (auto eit = outEdges.begin(); eit != outEdges.end(); ++eit) {
      Block *dest = (*eit)->trg();

      if (bset.find(dest) != bset.end()) { in_loop = true; }
      else { out_loop = true; }
    }

    if (in_loop && out_loop) {
      clist[src_vma] = HeaderInfo(*bit);
      clist[src_vma].is_cond = true;
      clist[src_vma].score = 2;
    }
  }

  // add bonus points if the stmt is also a back edge source
  vector <Edge *> backEdges;
  loop->getBackEdges(backEdges);

  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    VMA src_vma = (*eit)->src()->last();

    auto it = clist.find(src_vma);
    if (it != clist.end()) {
      it->second.is_src = true;
      it->second.score += 1;
    }
  }

#if DEBUG_CFG_SOURCE
  cout << "\nraw inline tree:  " << loopName
       << "  '" << func->name() << "'\n"
       << "file:  '" << file->name() << "'\n\n";
  debugInlineTree(root, NULL, strTab, 0, false);
  debugLoop(pinfo, func, loop, loopName, backEdges, clist);
  cout << "\nsearching inline tree:\n";
#endif

  //------------------------------------------------------------
  // Step 2 -- find the right inline depth
  //------------------------------------------------------------

  // start at the root, descend the inline tree and try to find the
  // right level for the loop.  an inline branch or subloop is an
  // absolute stopping point.  the hard case is one inline subtree
  // plus statements.  we stop if there is a loop condition, else
  // continue and reparent the stmts.  always reparent any stmt in a
  // different file from the inline callsite.

  FLPSeqn  path;
  StmtMap  stmts;

  while (root->nodeMap.size() == 1 && root->loopList.size() == 0) {
    FLPIndex flp = root->nodeMap.begin()->first;

    // look for loop cond at this level
    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      if (sit->second->base_index == flp.base_index) {
	auto it = clist.find(sit->first);

	if (it != clist.end() && it->second.is_cond) {
	  goto found_level;
	}
      }
    }

    for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
      if (sit->second->base_index == flp.base_index) {
	auto it = clist.find(sit->first);

	if (it != clist.end() && it->second.is_cond) {
	  goto found_level;
	}
      }
    }

    // reparent the stmts and proceed to the next level
    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      stmts[sit->first] = sit->second;
    }
    root->stmtMap.clear();

    TreeNode *subtree = root->nodeMap.begin()->second;
    root->nodeMap.clear();
    delete root;
    root = subtree;
    path.push_back(flp);
    base_index = -1;

    DEBUG_MESG("inline:  l=" << flp.line_num
	       << "  f='" << strTab.index2str(flp.file_index)
	       << "'  p='" << debugPrettyName(strTab.index2str(flp.proc_index))
	       << "'\n");
  }
found_level:

  //------------------------------------------------------------
  // Step 3 -- reattach stmts into this level
  //------------------------------------------------------------

  // fixme: want to attach some stmts below this level

  for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
    root->stmtMap[sit->first] = sit->second;
  }
  stmts.clear();

  //------------------------------------------------------------
  // Step 4 -- choose a loop header file/line at this level
  //------------------------------------------------------------

  long file_ans = 0;
  long base_ans = 0;
  long line_ans = 0;

  if (root->nodeMap.size() > 0 || root->loopList.size() > 0) {
    //
    // if there is an inline callsite or subloop, then use that file
    // name and the minimum line number among all callsites, subloops
    // and loop conditions with the same file.
    //
    // if there is an inconsistent choice of file name, prefer the
    // file matching the function, but do this only at top-level (no
    // inline steps).  inline call sites show only where called from,
    // not where defined.
    //
    for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
      FLPIndex flp = nit->first;
      file_ans = flp.file_index;
      base_ans = flp.base_index;
      line_ans = flp.line_num;

      if (base_index < 0 || flp.base_index == base_index) {
	goto found_file;
      }
    }

    for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
      LoopInfo *info = *lit;
      file_ans = info->file_index;
      base_ans = info->base_index;
      line_ans = info->line_num;

      if (base_index < 0 || info->base_index == base_index) {
	goto found_file;
      }
    }
found_file:

    // min of inline callsites
    for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
      FLPIndex flp = nit->first;

      if (flp.base_index == base_ans && flp.line_num < line_ans) {
	line_ans = flp.line_num;
      }
    }

    // min of subloops
    for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
      LoopInfo *info = *lit;

      if (info->base_index == base_ans && info->line_num < line_ans) {
	line_ans = info->line_num;
      }
    }

    // min of loop cond stmts
    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      VMA vma = sit->first;
      StmtInfo *info = sit->second;

      if (info->base_index == base_ans && info->line_num < line_ans
	  && clist.find(vma) != clist.end()) {
	line_ans = info->line_num;
      }
    }
  }
  else {
    //
    // if there are only terminal stmts, then select the file name of
    // the best candidate (loop cond + back edge source, loop cond,
    // then any stmt) and then the min line number.
    //
    int max_score = -1;

    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      VMA vma = sit->first;
      StmtInfo *info = sit->second;
      auto it = clist.find(vma);
      int score = (it != clist.end()) ? it->second.score : 0;

      if (score > max_score) {
	max_score = score;
	file_ans = info->file_index;
	base_ans = info->base_index;
	line_ans = info->line_num;
      }
      else if (score == max_score && info->base_index == base_ans
	       && info->line_num < line_ans) {
	line_ans = info->line_num;
      }
    }
  }

  DEBUG_MESG("header:  l=" << line_ans << "  f='"
	     << strTab.index2str(file_ans) << "'\n");

  vector <Block *> entryBlocks;
  loop->getLoopEntries(entryBlocks);
  VMA entry_vma = (*(entryBlocks.begin()))->start();

  LoopInfo *info = new LoopInfo(root, path, loopName, entry_vma,
				file_ans, base_ans, line_ans);

#if DEBUG_CFG_SOURCE
  cout << "\nreparented inline tree:  " << loopName
       << "  '" << func->name() << "'\n\n";
  debugInlineTree(root, info, strTab, 0, false);
#endif

  return info;
}


// Find the alien node in 'alienMap' that matches 'file_index'.
// Create new node and link into map and 'enclScope' if needed.
// Expand the line range of an earlier node if needed.
//
static Prof::Struct::ACodeNode *
findAlienScope(Prof::Struct::ACodeNode * enclScope, AlienScopeMap & alienMap,
	       long file_index, SrcFile::ln line, long proc_index,
	       StringTable & strTab, ProcNameMgr * nameMgr)
{
  Prof::Struct::ACodeNode * alien;
  const string & filenm = strTab.index2str(file_index);
  long base_index = strTab.str2index(FileUtil::basename(filenm.c_str()));
  auto ait = alienMap.find(base_index);

  if (ait != alienMap.end()) {
    // alien node exists, expand the line range
    alien = ait->second;

    if (line < alien->begLine()) {
      alien->begLine(line);
    }
    if (line > alien->endLine()) {
      alien->endLine(line);
    }
  }
  else {
    // create new alien, link into map and enclScope
    const string & procnm = strTab.index2str(proc_index);
    const string & display = BinUtil::canonicalizeProcName(procnm, nameMgr);

    alien = new Prof::Struct::Alien(enclScope, filenm, procnm, display, line, line);
    alienMap[base_index] = alien;
  }

  return alien;
}


// Convert inline TreeNode 'tree' to Prof::Struct scope tree format
// and merge into 'enclScope'.
//
#define GUARD_ALIEN  "<inline>"

static void
makeScopeTree(Prof::Struct::ACodeNode * enclScope, ScopeInfo scinfo,
	      TreeNode * tree, StringTable & strTab, ProcNameMgr * nameMgr)
{
  if (tree == NULL) {
    return;
  }

  long guard_index = strTab.str2index(GUARD_ALIEN);
  long empty_index = strTab.str2index("");

  // FIXME: this is a primitive alien map based only on file name.
  // We should also consider if the stmt's line num is outside the
  // enclosing scope's line range, whether the stmt's inline seqn
  // exists, and whether the stmt was reparented.
  //
  // FIXME: need a better algorithm for matching absolute and relative
  // file names with '..' (this just uses basename).
  //
  AlienScopeMap alienMap;
  Prof::Struct::ACodeNode * scope;
  const string & sc_filenm = strTab.index2str(scinfo.file_index);
  long sc_base = strTab.str2index(FileUtil::basename(sc_filenm.c_str()));

  // add terminal statements.  if the file name does not match the
  // enclosing scope, or if the stmt's line number comes before its
  // scope, then add a single guard alien.
  //
  for (auto sit = tree->stmtMap.begin(); sit != tree->stmtMap.end(); ++sit) {
    StmtInfo *info = sit->second;
    VMA vma = info->vma;
    long myfile = info->file_index;
    long mybase = info->base_index;
    SrcFile::ln myline = info->line_num;
    scope = enclScope;

    if (scinfo.is_alien) {
      // target alien, second half of callsite alien
      scope = findAlienScope(scope, alienMap, myfile, myline, scinfo.proc_index,
			     strTab, nameMgr);
    }
    else if (myfile != empty_index && (mybase != sc_base || myline < scinfo.line)) {
      // guard alien
      scope = findAlienScope(scope, alienMap, myfile, myline, guard_index,
			     strTab, nameMgr);
    }

    new Prof::Struct::Stmt(scope, myline, myline, vma, vma + info->len);
  }

  // add loops, located by their header.  again, if the file name does
  // not match the enclosing scope, then add a single guard alien.
  //
  for (auto lit = tree->loopList.begin(); lit != tree->loopList.end(); ++lit) {
    VMA vma = (*lit)->entry_vma;
    long myfile = (*lit)->file_index;
    long mybase = (*lit)->base_index;
    string filenm = strTab.index2str(myfile);
    SrcFile::ln myline = (*lit)->line_num;
    scope = enclScope;

    if (scinfo.is_alien) {
      // target alien, second half of callsite alien
      scope = findAlienScope(scope, alienMap, myfile, myline, scinfo.proc_index,
			     strTab, nameMgr);
    }
    else if (myfile != empty_index && (mybase != sc_base || myline < scinfo.line)) {
      // guard alien
      scope = findAlienScope(scope, alienMap, myfile, myline, guard_index,
			     strTab, nameMgr);
    }

    Prof::Struct::Loop *loop = new Prof::Struct::Loop(scope, filenm, myline, myline);
    loop->vmaSet().insert(vma, vma + 1);

    makeScopeTree(loop, ScopeInfo(myfile, myline), (*lit)->node, strTab, nameMgr);
  }

  // add inline subtrees.  always add a call site alien above the
  // subtree.  if the parent is also an alien, then add a target alien
  // between the two call site aliens.
  //
  for (auto nit = tree->nodeMap.begin(); nit != tree->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;
    long myfile = flp.file_index;
    long line = flp.line_num;
    long proc = scinfo.proc_index;
    const string & filenm = strTab.index2str(myfile);
    scope = enclScope;

    // add a target alien between two call site aliens.
    if (scinfo.is_alien) {
      scope = findAlienScope(scope, alienMap, myfile, line, proc, strTab, nameMgr);
    }

    // add the call site alien with no proc name.
    scope = new Prof::Struct::Alien(scope, filenm, "", "", line, line);

    makeScopeTree(scope, ScopeInfo(myfile, 0, true, flp.proc_index),
		  nit->second, strTab, nameMgr);
  }
}

}  // namespace Struct
}  // namespace BAnal

#endif  // parseAPI


//****************************************************************************
// Debug functions
//****************************************************************************

#if DEBUG_CFG_SOURCE

// Debug functions to display the raw input data for loops, blocks,
// stmts, file names, proc names and line numbers.  We compute the
// loop and alien nesting based on this input.  This applies to both
// Open Analysis and ParseAPI.

#define INDENT   "   "

namespace BAnal {
namespace Struct {

// Cleanup and shorten the proc name: demangle plus collapse nested
// <...> and (...) to just <> and ().  For example:
//
//   std::map<int,long>::myfunc(int) --> std::map<>::myfunc()
//
// This is only for more compact debug output.  Internal decisions are
// always made on the full string.
//
static string
debugPrettyName(const string & procnm)
{
  string str = BinUtil::demangleProcName(procnm);
  string ans = "";
  size_t str_len = str.size();
  size_t pos = 0;

  while (pos < str_len) {
    size_t next = str.find_first_of("<(", pos);
    char open, close;

    if (next == string::npos) {
      ans += str.substr(pos);
      break;
    }
    if (str[next] == '<') { open = '<';  close = '>'; }
    else { open = '(';  close = ')'; }

    ans += str.substr(pos, next - pos) + open + close;

    int depth = 1;
    for (pos = next + 1; pos < str_len && depth > 0; pos++) {
      if (str[pos] == open) { depth++; }
      else if (str[pos] == close) { depth--; }
    }
  }

  return ans;
}


static void
debugStmt(Prof::Struct::Stmt * stmt, VMA vma,
	  string & filenm, string & procnm, SrcFile::ln line)
{
  cout << INDENT << "stmt:";

  if (stmt != NULL) {
    cout << " (n=" << stmt->id() << ")";
  }
  cout << "  0x" << hex << vma << dec
       << "  l=" << line << "  f='" << filenm << "'\n";

#ifdef BANAL_USE_SYMTAB
  Inline::InlineSeqn nodeList;
  Inline::analyzeAddr(nodeList, vma);

  // list is outermost to innermost
  for (auto nit = nodeList.begin(); nit != nodeList.end(); ++nit) {
    cout << INDENT << INDENT << "inline:  l=" << nit->getLineNum()
	 << "  f='" << nit->getFileName()
	 << "'  p='" << debugPrettyName(nit->getProcName()) << "'\n";
  }
#endif
}


#ifdef BANAL_USE_PARSEAPI
static void
debugLoop(ProcInfo pinfo, ParseAPI::Function * func,
	  Loop * loop, const string & loopName,
	  vector <Edge *> & backEdges, HeaderList & clist)
{
  vector <Block *> entBlocks;
  int num_ents = loop->getLoopEntries(entBlocks);

  cout << "\nheader info:  " << loopName
       << ((num_ents == 1) ? "  (reducible)" : "  (irreducible)")
       << "  '" << func->name() << "'\n\n";

  cout << "entry blocks:" << hex;
  for (auto bit = entBlocks.begin(); bit != entBlocks.end(); ++bit) {
    cout << "  0x" << (*bit)->start();
  }

  cout << "\nback edge sources:";
  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    cout << "  0x" << (*eit)->src()->last();
  }

  cout << "\nback edge targets:";
  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    cout << "  0x" << (*eit)->trg()->start();
  }

  cout << "\n\nexit conditions:\n";
  for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
    VMA vma = cit->first;
    HeaderInfo * info = &(cit->second);
    SrcFile::ln line;
    string filenm, procnm, label;
    InlineSeqn seqn;

    pinfo.proc_bin->findSrcCodeInfo(vma, 0, procnm, filenm, line);
    analyzeAddr(seqn, vma);

    if (info->is_src) { label = "src "; }
    else if (info->is_targ) { label = "targ"; }
    else if (info->is_cond) { label = "cond"; }
    else { label = "??? "; }

    cout << "0x" << hex << vma << dec
	 << "  " << label
	 << "  excl: " << loop->hasBlockExclusive(info->block)
	 << "  cond: " << info->is_cond
	 << "  depth: " << seqn.size()
	 << "  l=" << line
	 << "  f='" << filenm << "'\n";
  }
}


// If LoopInfo is non-null, then treat 'node' as a detached loop and
// prepend the FLP seqn from 'info' above the tree.  Else, 'node' is
// the tree.
//
void
debugInlineTree(TreeNode * node, LoopInfo * info, StringTable & strTab,
		int depth, bool expand_loops)
{
  // treat node as a detached loop with FLP seqn above it.
  if (info != NULL) {
    depth = 0;
    for (auto pit = info->path.begin(); pit != info->path.end(); ++pit) {
      for (int i = 1; i <= depth; i++) {
	cout << INDENT;
      }
      FLPIndex flp = *pit;

      cout << "inline:  l=" << flp.line_num
	   << "  f='" << strTab.index2str(flp.file_index)
	   << "'  p='" << debugPrettyName(strTab.index2str(flp.proc_index))
	   << "'\n";
      depth++;
    }

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "loop:  " << info->name
	 << "  l=" << info->line_num
	 << "  f='" << strTab.index2str(info->file_index) << "'\n";
    depth++;
  }

  // print the terminal statements
  for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
    StmtInfo *sinfo = sit->second;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "stmt:  0x" << hex << sinfo->vma << dec
	 << "  l=" << sinfo->line_num
	 << "  f='" << strTab.index2str(sinfo->file_index) << "'\n";
  }

  // recur on the subtrees
  for (auto nit = node->nodeMap.begin(); nit != node->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "inline:  l=" << flp.line_num
	 << "  f='"  << strTab.index2str(flp.file_index)
	 << "'  p='" << debugPrettyName(strTab.index2str(flp.proc_index))
	 << "'\n";

    debugInlineTree(nit->second, NULL, strTab, depth + 1, expand_loops);
  }

  // recur on the loops
  for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
    LoopInfo *info = *lit;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }

    cout << "loop:  " << info->name
	 << "  l=" << info->line_num
	 << "  f='" << strTab.index2str(info->file_index) << "'\n";

    if (expand_loops) {
      debugInlineTree(info->node, NULL, strTab, depth + 1, expand_loops);
    }
  }
}
#endif  // parseAPI

}  // namespace Struct
}  // namespace BAnal

#endif  // DEBUG_CFG_SOURCE


//****************************************************************************
// Helpers for normalizing a scope tree
//****************************************************************************

namespace BAnal {
namespace Struct {

// coalesceDuplicateStmts: Coalesce duplicate statement instances that
// may appear in the scope tree.  There are two basic cases:
//
// Case 1: Multiple instances of the same statement, where one
//   statement is a child of the least common ancestor. Discard all
//   but the innermost instance.  Consider the following examples:
//
//   lca --- ... --- S
//      \--- S'
//
//   Comment: Compiler optimizations may hoist loop-invariant
//   operations out of inner loops.  Note however that in rare cases,
//   code may be moved deeper into a nest (e.g. to free registers).
//   Even so, we want to associate statements within loop nests to the
//   deepest level in which they appear.
//
//   lca --- S
//      \--- S'
//
//   Comment: Multiple statements exist at the same level because of
//   multiple basic blocks containing the same statement,
//   cf. buildStmts().
//
// Case 2: Multiple instances of the same statement where no instance
//   is a child of the least common ancestor.  Merge the distinct
//   paths to each statement instance into one.
//
//   lca ---...--- S
//      \---...--- S'
//
//   Comment: Compiler optimizations such as loop unrolling (start-up,
//   steady-state, wind-down), e.g., can produce multiple statement
//   instances.
//
// Observation: the merging of Case 2 may result in instances of Case
//   1 (but not vice versa).  Therefore, we are guaranteed to reach a
//   fixed point if we apply coalescing in two phases, considering all
//   instances of Case 2 before any instances of Case 1.
//
// In the presence of inlining (Struct::Alien scopes), one can think
// of multiple Struct::Alien (instead of Struct::Stmt) instances.
// 
// - Apply Case 1 only in the context of a single file (Struct::File,
//   Struct::Alien).  That is, do not attempt to delete the shallower
//   Alien in the following example (since it could represent two
//   inlined instances).
//
//   alien --- L --- S
//        \--- S'
//
// - Apply Case 2 to merge overlapping sibling Aliens scopes:
//   lca --- A1 --- S
//      \--- A2 --- S'
//
//   E.g., do not merge something like this, as it is unlikely that L1
//   and L2 are the same.
//
//   lca --- L1 --- A1 --- S
//      \--- L2 --- A2 --- S'

static bool
cds_main(Prof::Struct::ACodeNode* node, int level,
	 Prof::Struct::ACodeNode* fileCtxt, StmtKeyToStmtMap* stmtMap,
	 bool mayCase1, bool mayCase2);

static bool
cds_processStmt(Prof::Struct::Stmt* stmt1, int level1,
		Prof::Struct::ACodeNode* fileCtxt, StmtKeyToStmtMap* stmtMap,
		bool mayCase1, bool mayCase2);

static bool
cds_mayCase2(Prof::Struct::ACodeNode* fileCtxt, Prof::Struct::ANode* lca,
	     Prof::Struct::Stmt* stmt1, Prof::Struct::Stmt* stmt2);

static bool
cds_nodeFilter(const Prof::Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (typeid(x) == typeid(Prof::Struct::Proc));
}


static bool
coalesceDuplicateStmts(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe)
{
  bool changed = false;

  StmtKeyToStmtMap stmtMap; // tracks deepest statement instance

  const bool mayCase1 = true;
  bool mayCase2 = doNormalizeUnsafe;
  
  // Apply the normalization routine to each Struct::Proc

  Prof::Struct::ANodeFilter filter(cds_nodeFilter, "cds_nodeFilter", 0);
  for (Prof::Struct::ANodeIterator it(lmStrct, &filter); it.Current(); ++it) {
    Prof::Struct::ACodeNode* x =
      dynamic_cast<Prof::Struct::ACodeNode*>(it.Current());
    DIAG_Assert(x, DIAG_UnexpectedInput);

    Prof::Struct::ACodeNode* fileCtxt = x->ancestorFile();
    DIAG_Assert(fileCtxt, DIAG_UnexpectedInput);

    // Perform Case 2 coalescing
    if (mayCase2) {
      changed |= cds_main(x, 1, fileCtxt, &stmtMap, false, mayCase2);
      stmtMap.clear(); // deletes contents
    }

    // Perform Case 1 coalescing
    changed |= cds_main(x, 1, fileCtxt, &stmtMap, mayCase1, false);
    stmtMap.clear(); // deletes contents
  }

  return changed;
}


static bool
cds_main(Prof::Struct::ACodeNode* node, int level,
	 Prof::Struct::ACodeNode* fileCtxt, StmtKeyToStmtMap* stmtMap,
	 bool mayCase1, bool mayCase2)
{
  bool changed = false;

  if (!node) { return changed; }

  DIAG_DevMsgIf(DBG_CDS, "CDS: " << node);

  // A post-order traversal of this node (visit children before parent).
  // N.B.: Iteration must support tree modification.
  for (Prof::Struct::ACodeNodeChildIterator it(node); it.Current(); /* */) {
    Prof::Struct::ACodeNode* x = it.current();
    it++; // advance iterator so that 'x' may be deleted
    
    Prof::Struct::ACodeNode* fileCtxt1 = fileCtxt;
    StmtKeyToStmtMap* stmtMap1 = stmtMap, *stmtMap_new = NULL;
    if (typeid(*x) == typeid(Prof::Struct::Alien)) {
      fileCtxt1 = x;
      if (mayCase1) {
	stmtMap1 = stmtMap_new = new StmtKeyToStmtMap; // push new 'stmtMap'
      }
    }
    
    // 1. Process the children of 'x'
    changed |= cds_main(x, level + 1, fileCtxt1, stmtMap1, mayCase1, mayCase2);

    // 2. Process 'x'
    if (typeid(*x) == typeid(Prof::Struct::Stmt)) {
      Prof::Struct::Stmt* stmt = static_cast<Prof::Struct::Stmt*>(x);
      changed |= cds_processStmt(stmt, level, fileCtxt, stmtMap,
				 mayCase1, mayCase2);
    }

    delete stmtMap_new; // pop 'stmtMap'
  }
  
  return changed;
}


// cds_processStmt: Applies case 1 or 2, as described above.
//   'fileCtxt' is the file context/name for 'stmt1'.
// 
// N.B.: Assume that we are always within the context of exactly one
//   Struct::File node but possibly several Struct::Alien nodes.
static bool
cds_processStmt(Prof::Struct::Stmt* stmt1, int level1,
		Prof::Struct::ACodeNode* fileCtxt, StmtKeyToStmtMap* stmtMap,
		bool mayCase1, bool mayCase2)
{
  bool changed = false;
  
  std::string filenm = StmtKey::DefaultFileNm; // see assumption above
  if (typeid(*fileCtxt) == typeid(Prof::Struct::Alien)) {
    Prof::Struct::Alien* alien = static_cast<Prof::Struct::Alien*>(fileCtxt);
    filenm = alien->name() + alien->fileName(); // cf. AlienStrctMapKey
  }

  // N.B.: 'filenm' should be copied b/c a Struct::Alien can be deleted
  StmtKey stmtkey(filenm, stmt1->sortId());
  StmtKeyToStmtMap::iterator it = stmtMap->find(stmtkey);

  if (it != stmtMap->end()) {
    StmtData* stmtdata = it->second;

    Prof::Struct::Stmt* stmt2 = stmtdata->stmt();
    DIAG_Assert(stmt1 != stmt2, DIAG_UnexpectedInput);
    DIAG_DevMsgIf(DBG_CDS, " Find: " << stmt1 << " " << stmt2);
    
    // --------------------------------------------------------
    // Determine which case to consider
    // --------------------------------------------------------
    
    // Find least common ancestor.  N.B.: the enclosing Struct::Proc
    // is always a common ancestor.
    Prof::Struct::ANode* lca =
      Prof::Struct::ANode::leastCommonAncestor(stmt1, stmt2);
    DIAG_Assert(lca, "");
    
    bool isCase1 = (stmt1->parent() == lca || stmt2->parent() == lca);
    bool isCase2 = !isCase1;

    // --------------------------------------------------------
    // Determine deepest Stmt instance
    // --------------------------------------------------------

    Prof::Struct::Stmt* deep_stmt = stmt1;
    int deep_lvl = level1;
    if (stmtdata->level() > level1) { // stmt2.level > stmt1.level
      deep_stmt = stmt2;
      deep_lvl = stmtdata->level();
    }

    // --------------------------------------------------------
    // Case 1: Duplicate Stmts, where at least one Stmt is a child of
    //   'lca'.  Delete the most shallow instance.
    // --------------------------------------------------------
    if (isCase1 && mayCase1) {
      Prof::Struct::Stmt* shallow = (deep_stmt == stmt1) ? stmt2 : stmt1;

      DIAG_DevMsgIf(DBG_CDS, "  Delete: " << shallow);
      deep_stmt->vmaSet().merge(shallow->vmaSet()); // merge VMAs

      shallow->unlink();
      delete shallow;
      changed = true;
    }

    // --------------------------------------------------------
    // Case 2: Merge the path lca->...->stmt2 into the path lca->...->stmt1
    // --------------------------------------------------------
    if (isCase2 && mayCase2) {
      mayCase2 = cds_mayCase2(fileCtxt, lca, stmt1, stmt2);
    }

    if (isCase2 && mayCase2) {
      DIAG_DevMsgIf(DBG_CDS, "  Merge: " << stmt1 << " <- " << stmt2);
      DIAG_Assert(Logic::implies(level1 == stmtdata->level(),
				 deep_stmt == stmt1),
		  "If stmt2 is merged into stmt1, ensure we keep stmt1");

      // N.B.: the iterator stack is still valid because
      //   Struct::ANode::merge() only relocates a Struct::Proc
      changed = Prof::Struct::ANode::mergePaths(lca, stmt1, stmt2);
    }

    // --------------------------------------------------------
    // Update 'stmtMap' with deepest statement (which was not deleted)
    // --------------------------------------------------------
    stmtdata->stmt(deep_stmt);
    stmtdata->level(deep_lvl);
  }
  else {
    // Add the statement instance to the map
    StmtData* stmtdata = new StmtData(stmt1, level1);
    stmtMap->insert(make_pair(stmtkey, stmtdata));
    DIAG_DevMsgIf(DBG_CDS, " Map: " << stmt1);
  }

  return changed;
}


static bool
cds_mayCase2(Prof::Struct::ACodeNode* GCC_ATTR_UNUSED fileCtxt,
	     Prof::Struct::ANode* lca,
	     Prof::Struct::Stmt* stmt1, Prof::Struct::Stmt* stmt2)
{
  // INVARIANT:  Case 2 means that 'stmt1' and 'stmt2' match w.r.t. 'fileCtxt'

  const std::type_info& alien_ty = typeid(Prof::Struct::Alien);

  Prof::Struct::ANode* stmt1_parent = stmt1->parent();
  Prof::Struct::ANode* stmt2_parent = stmt2->parent();

  if (typeid(*stmt1_parent) == alien_ty && typeid(*stmt2_parent) == alien_ty) {
    if (stmt1_parent->parent() == lca && stmt2_parent->parent() == lca) {
      return true;
    }
    else {
      return false;
    }
  }
  else {
    return true;
  }
}


//****************************************************************************

// mergePerfectlyNestedLoops: Fuse together a child with a parent when
// the child is perfectly nested in the parent.
static bool
mergePerfectlyNestedLoops(Prof::Struct::ANode* node)
{
  bool changed = false;
  
  if (!node) { return changed; }
  
  // A post-order traversal of this node (visit children before parent)...
  for (Prof::Struct::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::Struct::ACodeNode* x =
      dynamic_cast<Prof::Struct::ACodeNode*>(it.Current()); // always true
    DIAG_Assert(x, "");
    it++; // advance iterator -- it is pointing at 'x'
    
    // 1. Process children of 'x'
    changed |= mergePerfectlyNestedLoops(x);
    
    // 2. If perfectly nested, merge.  Perfectly nested: 'x' is a
    //   loop; parent is a loop; and 'x' is parent's only child.
    if (typeid(*x) == typeid(Prof::Struct::Loop)
	&& typeid(*node) == typeid(Prof::Struct::Loop)
	&& node->childCount() == 1) {
      Prof::Struct::Loop* node_lp = static_cast<Prof::Struct::Loop*>(node);
      if (SrcFile::isValid(x->begLine(), x->endLine())
	  && x->begLine() == node_lp->begLine()
	  && x->endLine() == node_lp->endLine()) {
	// Move all children of 'x' so that they are children of 'node'
	changed = Prof::Struct::ANode::merge(node, x);
	DIAG_Assert(changed, "mergePerfectlyNestedLoops: merge failed.");
      }
    }
  }
  
  return changed;
}


//****************************************************************************

// removeEmptyNodes: Removes certain 'empty' scopes from the tree,
// always maintaining the top level Struct::Root (PGM) scope.  See the
// predicate 'removeEmptyNodes_isEmpty' for details.
static bool
removeEmptyNodes_isEmpty(const Prof::Struct::ANode* node);

static bool
removeEmptyNodes(Prof::Struct::ANode* node)
{
  bool changed = false;
  
  if (!node) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (Prof::Struct::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::Struct::ANode* x = dynamic_cast<Prof::Struct::ANode*>(it.Current());
    it++; // advance iterator -- it is pointing at 'x'
    
    // 1. Recursively do any trimming for this tree's children
    changed |= removeEmptyNodes(x);

    // 2. Trim this node if necessary
    if (removeEmptyNodes_isEmpty(x)) {
      x->unlink(); // unlink 'x' from tree
      delete x;
      changed = true;
    }
  }
  
  return changed;
}


// removeEmptyNodes_isEmpty: determines whether a scope is 'empty':
//   true, if a Struct::File has no children.
//   true, if a Struct::Loop or Struct::Proc has no children *and* an empty
//     VMAIntervalSet.
//   false, otherwise
static bool
removeEmptyNodes_isEmpty(const Prof::Struct::ANode* node)
{
  if ((typeid(*node) == typeid(Prof::Struct::File)
       || typeid(*node) == typeid(Prof::Struct::Alien))
      && node->isLeaf()) {
    return true;
  }
  if ((typeid(*node) == typeid(Prof::Struct::Proc)
       || typeid(*node) == typeid(Prof::Struct::Loop))
      && node->isLeaf()) {
    const Prof::Struct::ACodeNode* n =
      dynamic_cast<const Prof::Struct::ACodeNode*>(node);
    return n->vmaSet().empty();
  }
  return false;
}


} // namespace Struct
} // namespace BAnal

