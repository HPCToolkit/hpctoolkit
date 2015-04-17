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
// Copyright ((c)) 2002-2015, Rice University
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

//************************* System Include Files ****************************

#include <limits.h>

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
#include <vector>
#include <typeinfo>
#include <algorithm>
#include <cstring>

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/CFG/ManagerCFG.hpp>
#include <OpenAnalysis/Utils/RIFG.hpp>
#include <OpenAnalysis/Utils/NestedSCR.hpp>
#include <OpenAnalysis/Utils/Exception.hpp>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "Struct.hpp"
#include "Struct-LocationMgr.hpp"
#include "OAInterface.hpp"

#include <lib/prof/Struct-Tree.hpp>
#include <lib/prof/Struct-TreeIterator.hpp>
using namespace Prof;

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>

#ifdef BANAL_USE_SYMTAB
#include <Symtab.h>
#include <Function.h>
#include "Struct-Inline.hpp"

using namespace Dyninst;
using namespace SymtabAPI;

#ifdef BANAL_USE_PARSEAPI
#include <CFG.h>
#include <CodeObject.h>
#include <CodeSource.h>

using namespace ParseAPI;
#endif
#endif

#define FULL_STRUCT_DEBUG 0

//*************************** Forward Declarations ***************************

namespace BAnal {

namespace Struct {

// ------------------------------------------------------------
// Helpers for building a structure tree
// ------------------------------------------------------------

typedef std::multimap<Prof::Struct::Proc*, BinUtil::Proc*> ProcStrctToProcMap;

static ProcStrctToProcMap*
buildLMSkeleton(Prof::Struct::LM* lmStrct, BinUtil::LM* lm,
		ProcNameMgr* procNmMgr);

static Prof::Struct::File*
demandFileNode(Prof::Struct::LM* lmStrct, BinUtil::Proc* p);

static Prof::Struct::Proc*
demandProcNode(Prof::Struct::File* fStrct, BinUtil::Proc* p,
	       ProcNameMgr* procNmMgr);


static Prof::Struct::Proc*
buildProcStructure(Prof::Struct::Proc* pStrct, BinUtil::Proc* p,
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
class StmtData;

class SortIdToStmtMap
  : public std::map<int, StmtData*> {
public:
  typedef std::map<int, StmtData*> My_t;

public:
  SortIdToStmtMap() { }
  virtual ~SortIdToStmtMap() { clear(); }
  virtual void clear();
};

class StmtData {
public:
  StmtData(Prof::Struct::Stmt* stmt = NULL, int level = 0)
    : m_stmt(stmt), m_level(level) { }
  ~StmtData() { /* owns no data */ }

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
  Prof::Struct::Stmt* m_stmt;
  int m_level;
};

static void
deleteContents(Prof::Struct::ANodeSet* s);


// ------------------------------------------------------------
// Helpers for normalizing the structure tree
// ------------------------------------------------------------

class CDS_RestartException {
public:
  CDS_RestartException(Prof::Struct::ACodeNode* n) : m_node(n) { }
  ~CDS_RestartException() { }

  Prof::Struct::ACodeNode* node() const { return m_node; }

private:
  Prof::Struct::ACodeNode* m_node;
};


// ------------------------------------------------------------
// Make a dot (graphviz) file for the Control-Flow Graph
// ------------------------------------------------------------

// Write the Control-Flow Graph for each procedure to the ostream
// dotFile.  This code will likely migrate somewhere down inside
// buildProcStructure at some later time.

#ifdef BANAL_USE_PARSEAPI
static void
makeDotFile_parseAPI(std::ostream * dotFile, CodeObject * code_obj)
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
#endif


#ifdef BANAL_USE_OPEN_ANALYSIS
static void
makeDotFile_OA(std::ostream * dotFile, BinUtil::LM * lm)
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
#endif

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
Prof::Struct::LM*
BAnal::Struct::makeStructure(BinUtil::LM* lm,
			     std::ostream * dotFile,
			     NormTy doNormalizeTy,
			     bool isIrrIvalLoop,
			     bool isFwdSubst,
			     ProcNameMgr* procNmMgr,
			     const std::string& dbgProcGlob)
{
  // Assume lm->Read() has been performed
  DIAG_Assert(lm, DIAG_UnexpectedInput);

  Prof::Struct::LM* lmStrct = new Prof::Struct::LM(lm->name(), NULL);

  // 1. Build Struct::File/Struct::Proc skeletal structure
  ProcStrctToProcMap* mp = buildLMSkeleton(lmStrct, lm, procNmMgr);

#ifdef BANAL_USE_SYMTAB
  Symtab * symtab = Inline::openSymtab(lm->name());

#ifdef BANAL_USE_PARSEAPI
  SymtabCodeSource * code_src;
  CodeObject * code_obj;

  if (symtab != NULL) {
    code_src = new SymtabCodeSource(symtab);
    code_obj = new CodeObject(code_src);
    code_obj->parse();
  }
#endif
#endif
  
  // 2. For each [Struct::Proc, BinUtil::Proc] pair, complete the build.
  // Note that a Struct::Proc may be associated with more than one
  // BinUtil::Proc.
  for (ProcStrctToProcMap::iterator it = mp->begin(); it != mp->end(); ++it) {
    Prof::Struct::Proc* pStrct = it->first;
    BinUtil::Proc* p = it->second;

    DIAG_Msg(2, "Building scope tree for [" << p->name()  << "] ... ");
    buildProcStructure(pStrct, p, isIrrIvalLoop, isFwdSubst,
		       procNmMgr, dbgProcGlob);
  }
  delete mp;

  // 3. Normalize
  if (doNormalizeTy != NormTy_None) {
    bool doNormalizeUnsafe = (doNormalizeTy == NormTy_All);
    normalize(lmStrct, doNormalizeUnsafe);
  }

  // 4. Write CFG in dot (graphviz) format to file.
  if (dotFile != NULL) {
#ifdef BANAL_USE_PARSEAPI
    makeDotFile_parseAPI(dotFile, code_obj);
#endif
#ifdef BANAL_USE_OPEN_ANALYSIS
    makeDotFile_OA(dotFile, lm);
#endif
  }

#ifdef BANAL_USE_SYMTAB
  Inline::closeSymtab();
#endif

  return lmStrct;
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
static ProcStrctToProcMap*
buildLMSkeleton(Prof::Struct::LM* lmStrct, BinUtil::LM* lm,
		ProcNameMgr* procNmMgr)
{
  ProcStrctToProcMap* mp = new ProcStrctToProcMap;
  
  // -------------------------------------------------------
  // 1. Create basic structure for each procedure
  // -------------------------------------------------------

  for (BinUtil::LM::ProcMap::iterator it = lm->procs().begin();
       it != lm->procs().end(); ++it) {
    BinUtil::Proc* p = it->second;
    if (p->size() != 0) {
      Prof::Struct::File* fStrct = demandFileNode(lmStrct, p);
      Prof::Struct::Proc* pStrct = demandProcNode(fStrct, p, procNmMgr);
      mp->insert(make_pair(pStrct, p));
    }
  }

  // -------------------------------------------------------
  // 2. Establish nesting information:
  // -------------------------------------------------------
  // FIXME: disable until we are sure we can handle this (cf. MOAB)
#if 0
  for (ProcStrctToProcMap::iterator it = mp->begin(); it != mp->end(); ++it) {
    Prof::Struct::Proc* pStrct = it->first;
    BinUtil::Proc* p = it->second;
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
  return mp;
}


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
  
  // Find procedure name
  string procNm   = BinUtil::canonicalizeProcName(p->name(), procNmMgr);
  string procLnNm = BinUtil::canonicalizeProcName(p->linkName(), procNmMgr);
  
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
//
//****************************************************************************

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



static bool
AlienScopeFilter(const Prof::Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (x.type() == Prof::Struct::ANode::TyAlien);
}


#if DEBUG_COMPARE
static int debug_compare = 0;
#define retval_line 1
#define retval_proc 2
#define retval_file 3
#define MAINTAIN_REASON(x) x
#else
#define MAINTAIN_REASON(x) 
#endif


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

void dumpMap(AlienMap &alienMap)
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


// buildProcStructure: Complete the representation for 'pStrct' given the
// BinUtil::Proc 'p'.  Note that pStrcts parent may itself be a Struct::Proc.
static Prof::Struct::Proc*
buildProcStructure(Prof::Struct::Proc* pStrct, BinUtil::Proc* p,
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, const std::string& dbgProcGlob)
{
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


void
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
#endif


static bool
aliensMatch(Prof::Struct::Alien *n1, Prof::Struct::Alien *n2)
{
  return 
    n1->fileName() == n2->fileName() &&
    n1->begLine() == n2->begLine() &&
    n1->displayName() == n2->displayName();
}


static bool
pathsMatch(Prof::Struct::ANode *n1, Prof::Struct::ANode *n2)
{
  if (n1 != n2) {
    bool result = pathsMatch(n1->parent(), n2->parent());
    if (result &&
	(typeid(*n1) == typeid(Prof::Struct::Alien)) &&
	(typeid(*n2) == typeid(Prof::Struct::Alien))) {
      return aliensMatch((Prof::Struct::Alien *)n1, 
			 (Prof::Struct::Alien *)n2);
    } else return false;
  } return true;
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
    //  this seems to have an off by one error
    // if (node == loopParent) return;
    if (child == loop) return;
    else if (pathsMatch(node, loopParent)) {
      node = child;
    } else {
      node = kid;
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
    locMgr.locate(loop, topScope, fnm, pnm, line, targetScope->id());
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
    if (idesc.isSubr()) {
      stmt->sortId(--call_sortId);
    }
    insertions.push_back(stmt);
    locMgr.locate(stmt, enclosingStrct, filenm, procnm, line, targetScopeID);
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


//****************************************************************************
// Helpers for normalizing a scope tree
//****************************************************************************

namespace BAnal {

namespace Struct {


//****************************************************************************

// coalesceDuplicateStmts: Coalesce duplicate statement instances that
// may appear in the scope tree.  There are two basic cases:
//
// Case 1a:
// If the same statement exists at multiple levels within a loop nest,
//   discard all but the innermost instance.
// Rationale: Compiler optimizations may hoist loop-invariant
//   operations out of inner loops.  Note however that in rare cases,
//   code may be moved deeper into a nest (e.g. to free registers).
//   Even so, we want to associate statements within loop nests to the
//   deepest level in which they appear.
// E.g.: lca --- ... --- s1
//          \--- s2
//
// Case 1b:
// If the same statement exists within the same loop, discard all but one.
// Rationale: Multiple statements exist at the same level because of
//   multiple basic blocks containing the same statement, cf. buildStmts().
//   [FIXME -- LocationMgr]
//   Also, the merging in case 2 may result in duplicate statements.
// E.g.: lca --- s1
//          \--- s2
//
// Case 2:
// If duplicate statements appear in different loops, find the least
//   common ancestor (deepest nested common ancestor) in the scope tree
//   and merge the corresponding loops along the paths to each of the
//   duplicate statements.  Note that merging can create instances of
//   case 1.
// Rationale: Compiler optimizations such as loop unrolling (start-up,
//   steady-state, wind-down), e.g., can produce multiple statement
//   instances.
// E.g.: lca ---...--- s1
//          \---...--- s2
static bool CDS_doNormalizeUnsafe = true;

static bool
coalesceDuplicateStmts(Prof::Struct::ACodeNode* scope,
		       SortIdToStmtMap* stmtMap,
		       Prof::Struct::ANodeSet* visited,
		       Prof::Struct::ANodeSet* toDelete,
		       int level);

static bool
CDS_Main(Prof::Struct::ACodeNode* scope, SortIdToStmtMap* stmtMap,
	 Prof::Struct::ANodeSet* visited, Prof::Struct::ANodeSet* toDelete,
	 int level);

static bool
CDS_InspectStmt(Prof::Struct::Stmt* stmt1, SortIdToStmtMap* stmtMap,
		Prof::Struct::ANodeSet* toDelete, int level);

static bool
CDS_ScopeFilter(const Prof::Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (x.type() == Prof::Struct::ANode::TyProc
	  || x.type() == Prof::Struct::ANode::TyAlien);
}


static bool
coalesceDuplicateStmts(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe)
{
  bool changed = false;
  CDS_doNormalizeUnsafe = doNormalizeUnsafe;
  SortIdToStmtMap stmtMap;    // line to statement data map
  Prof::Struct::ANodeSet visitedScopes; // all children of a scope have been visited
  Prof::Struct::ANodeSet toDelete;      // nodes to delete

  // Apply the normalization routine to each Struct::Proc and Struct::Alien
  // so that 1) we are guaranteed to only process Struct::ACodeNodes and 2) we
  // can assume that all line numbers encountered are within the same
  // file (keeping the SortIdToStmtMap simple and fast).  (Children
  // Struct::Alien's are skipped.)

  Prof::Struct::ANodeFilter filter(CDS_ScopeFilter, "CDS_ScopeFilter", 0);
  for (Prof::Struct::ANodeIterator it(lmStrct, &filter); it.Current(); ++it) {
    Prof::Struct::ACodeNode* scope = dynamic_cast<Prof::Struct::ACodeNode*>(it.Current());
    changed |= coalesceDuplicateStmts(scope, &stmtMap, &visitedScopes,
				      &toDelete, 1);
    stmtMap.clear();           // Clear statement table
    visitedScopes.clear();     // Clear visited set
    deleteContents(&toDelete); // Clear 'toDelete'
  }

  return changed;
}


// coalesceDuplicateStmts Helper:
//
// Because instances of case 2 can create instances of case 1, we need
// to restart the algorithm at the lca after merging has been applied.
//
// This has some implications:
// - We can delete nodes during the merging. (Deletion of nodes will
//   invalidate the current *and* ancestor iterators out to the
//   lca!)
// - We cannot simply delete nodes in case 1.  The restarting means
//   that nodes that we have already seen are in the statement map and
//   could therefore be deleted out from under us during iteration.
//   Consider this snippet of code:
//
//   <L b=... >  // level 3
//     ...
//     <L b="1484" e="1485">  // level 4
//        <S b="1484" />
//        <S b="1484" />
//        <S b="1485" />
//        <S b="1485" />
//     </L>
//     <S b="1485">
//     ...
//   </L>
//
//   CDS_Main loops on the children of the current scope.  It saves the
//   current value of the iterator to 'child' and then increments it (an
//   attempt to prevet this problem!). After that it recursively calls CDS
//   on 'child'.
//
//   When 'child' points to the 4th level loop in the picture above, the
//   iterator has been incremented and points to the next child which
//   happens to be a statement for line 1485.
//
//   When CDS processes the inner loop and reaches a statement for line
//   1485, it realizes a duplicate was found in the level 3 loop -- the
//   statement to which level 3 loop iterator points to.  Because the
//   current statement is on a deeper level (4 as opposed to 3 in the map),
//   the statement on level 3 is unlinked and deleted.  After the level 4
//   loop is completely processed, it returns back to the level 3 loop
//   where it points to a block of memory that was freed already.
//
// Solution: After application of case 2, unwind the recursion stack
//   and restart the algorithm at the least common ancestor.  Retain a
//   set of visited nodes so that we do not have to revisit fully
//   explored and unchanged nodes.  Since the most recently merged
//   path will not be in the visited set, it will be propertly visited
//
// Notes:
// - We always merge *into* the current path to avoid deleting all nodes
//   on the current recursion stack.
// - Note that the merging of Struct::ACodeNodes can trigger a reordering based
//   on begin/end lines; new children will not simply end up at the
//   end of the child list.
//
// Another solution: Divide into two distinct phases.  Phase 1 collects all
//   statements into a multi-map (that handles sorted iteration and
//   fast local resorts).  Phase 2 iterates over the map, applying
//   case 1 and 2 until all duplicate entries are removed.
static bool
coalesceDuplicateStmts(Prof::Struct::ACodeNode* scope,
		       SortIdToStmtMap* stmtMap,
		       Prof::Struct::ANodeSet* visited,
		       Prof::Struct::ANodeSet* toDelete,
		       int level)
{
  try {
    return CDS_Main(scope, stmtMap, visited, toDelete, level);
  }
  catch (CDS_RestartException& x) {
    // Unwind the recursion stack until we find the node
    if (x.node() == scope) {
      return coalesceDuplicateStmts(x.node(), stmtMap, visited,
				    toDelete, level);
    }
    else {
      throw;
    }
  }
}


// CDS_Main: Helper for the above. Assumes that all statement line
// numbers are within the same file.  We operate on the children of
// 'scope' to support support node deletion (case 1 above).  When we
// have visited all children of 'scope' we place it in 'visited'.
static bool
CDS_Main(Prof::Struct::ACodeNode* scope, SortIdToStmtMap* stmtMap,
	 Prof::Struct::ANodeSet* visited, Prof::Struct::ANodeSet* toDelete,
	 int level)
{
  bool changed = false;
  
  if (!scope) { return changed; }
  if (visited->find(scope) != visited->end()) { return changed; }
  if (toDelete->find(scope) != toDelete->end()) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (Prof::Struct::ANodeChildIterator it(scope); it.Current(); ++it) {
    Prof::Struct::ACodeNode* child =
      dynamic_cast<Prof::Struct::ACodeNode*>(it.Current()); // always true
    DIAG_Assert(child, "");
    
    if (toDelete->find(child) != toDelete->end()) { continue; }
    
    if (typeid(*child) == typeid(Prof::Struct::Alien)) { continue; }
    
    DIAG_DevMsgIf(DBG_CDS, "CDS: " << child);

    // 1. Recursively perform re-nesting on 'child'.
    changed |= coalesceDuplicateStmts(child, stmtMap, visited, toDelete,
				      level + 1);
    
    // 2. Examine 'child'
    if (typeid(*child) == typeid(Prof::Struct::Stmt)) {
      // Note: 'child' may be deleted or a restart exception may be thrown
      Prof::Struct::Stmt* stmt = dynamic_cast<Prof::Struct::Stmt*>(child);
      changed |= CDS_InspectStmt(stmt, stmtMap, toDelete, level);
    }
  }
  
  visited->insert(scope);
  return changed;
}

  
// CDS_InspectStmt: applies case 1 or 2, as described above
static bool
CDS_InspectStmt(Prof::Struct::Stmt* stmt1, SortIdToStmtMap* stmtMap,
		Prof::Struct::ANodeSet* toDelete, int level)
{
  bool changed = false;
  
  int sortid = stmt1->sortId();
  StmtData* stmtdata = (*stmtMap)[sortid];
  if (stmtdata) {
    
    Prof::Struct::Stmt* stmt2 = stmtdata->stmt();
    DIAG_DevMsgIf(DBG_CDS, " Find: " << stmt1 << " " << stmt2);
    
    // Ensure we have two different instances of the same sortid (line)
    if (stmt1 == stmt2) { return false; }
    
    // Find the least common ancestor.  At most it should be a
    // procedure scope.
    Prof::Struct::ANode* lca =
      Prof::Struct::ANode::leastCommonAncestor(stmt1, stmt2);
    DIAG_Assert(lca, "");
    
    // Because we have the lca and know that the descendent nodes are
    // statements (leafs), the test for case 1 is very simple:
    bool case1 = (stmt1->Parent() == lca || stmt2->Parent() == lca);
    if (case1) {
      // Case 1: Duplicate statements. Delete shallower one.
      Prof::Struct::Stmt* toRemove = NULL;
      
      if (stmtdata->level() < level) { // stmt2.level < stmt1.level
	toRemove = stmt2;
	stmtdata->stmt(stmt1);  // replace stmt2 with stmt1
	stmtdata->level(level);
      }
      else {
	toRemove = stmt1;
      }
      
      toDelete->insert(toRemove);
      stmtdata->stmt()->vmaSet().merge(toRemove->vmaSet()); // merge VMAs
      DIAG_DevMsgIf(DBG_CDS, "  Delete: " << toRemove);
      changed = true;
    }
    else if (CDS_doNormalizeUnsafe) {
      // Case 2: Duplicate statements in different loops (or scopes).
      // Merge the nodes from stmt2->lca into those from stmt1->lca.
      DIAG_DevMsgIf(DBG_CDS, "  Merge: " << stmt1 << " <- " << stmt2);
      changed = Prof::Struct::ANode::mergePaths(lca, stmt1, stmt2);
      if (changed) {
	// We may have created instances of case 1.  Furthermore,
	// while neither statement is deleted ('stmtMap' is still
	// valid), iterators between stmt1 and 'lca' are invalidated.
	// Restart at lca.
	Prof::Struct::ACodeNode* lca_CI =
	  dynamic_cast<Prof::Struct::ACodeNode*>(lca);
	DIAG_Assert(lca_CI, "");
	throw CDS_RestartException(lca_CI);
      }
    }
  }
  else {
    // Add the statement instance to the map
    stmtdata = new StmtData(stmt1, level);
    (*stmtMap)[sortid] = stmtdata;
    DIAG_DevMsgIf(DBG_CDS, " Map: " << stmt1);
  }

  return changed;
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
    Prof::Struct::ACodeNode* child =
      dynamic_cast<Prof::Struct::ACodeNode*>(it.Current()); // always true
    DIAG_Assert(child, "");
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any merging for this tree's children
    changed |= mergePerfectlyNestedLoops(child);
    
    // 2. Test if this is a perfectly nested loop with identical loop
    //    bounds and merge if so.

    // The following cast may be illegal but the 'perfect nesting' test
    //   will ensure it doesn't cause a problem (Loops are Struct::ACodeNode's).
    // Perfectly nested test: child is a loop; parent is a loop; and
    //   this is only child.
    Prof::Struct::ACodeNode* n_CI =
      dynamic_cast<Prof::Struct::ACodeNode*>(node);
    bool perfNested = (typeid(*child) == typeid(Prof::Struct::Loop) &&
		       typeid(*node) == typeid(Prof::Struct::Loop) &&
		       node->childCount() == 1);
    if (perfNested && SrcFile::isValid(child->begLine(), child->endLine()) &&
	child->begLine() == n_CI->begLine() &&
	child->endLine() == n_CI->endLine()) {

      // Move all children of 'child' so that they are children of 'node'
      changed = Prof::Struct::ANode::merge(node, child);
      DIAG_Assert(changed, "mergePerfectlyNestedLoops: merge failed.");
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
    Prof::Struct::ANode* child =
      dynamic_cast<Prof::Struct::ANode*>(it.Current());
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any trimming for this tree's children
    changed |= removeEmptyNodes(child);

    // 2. Trim this node if necessary
    if (removeEmptyNodes_isEmpty(child)) {
      child->unlink(); // unlink 'child' from tree
      delete child;
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


//****************************************************************************
// Helpers for traversing the Nested SCR (Tarjan Tree)
//****************************************************************************

void
SortIdToStmtMap::clear()
{
  for (iterator it = begin(); it != end(); ++it) {
    delete (*it).second;
  }
  My_t::clear();
}

//****************************************************************************
// Helper Routines
//****************************************************************************

static void
deleteContents(Prof::Struct::ANodeSet* s)
{
  // Delete nodes in toDelete
  for (Prof::Struct::ANodeSet::iterator it = s->begin(); it != s->end(); ++it) {
    Prof::Struct::ANode* n = (*it);
    n->unlink(); // unlink 'n' from tree
    delete n;
  }
  s->clear();
}


} // namespace Struct

} // namespace BAnal

