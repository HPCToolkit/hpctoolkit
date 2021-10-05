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
// Copyright ((c)) 2002-2021, Rice University
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

#include <iostream>
using std::hex;
using std::dec;

#include <fstream>

#include <string>
using std::string;

#include <climits>
#include <cstring>
#include <map>
#include <vector>

#include <typeinfo>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>

#include "CallPath.hpp"
#include "CallPath-MetricComponentsFact.hpp"
#include "Util.hpp"

#include <lib/banal/StructSimple.hpp>

#include <lib/prof/CCT-Tree.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Metric-ADesc.hpp>

#include <lib/profxml/XercesUtil.hpp>
#include <lib/profxml/PGMReader.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>


//********************************** Macros **********************************

#define DEBUG_COALESCING 0

#define DATABASE_VERSION "2.2"

//*************************** Forward Declarations ***************************

std::ostream* Analysis::CallPath::dbgOs = NULL; // for parallel debugging


//****************************************************************************
// 
//****************************************************************************

static void
coalesceStmts(Prof::Struct::Tree& structure);

namespace Analysis {

namespace CallPath {


Prof::CallPath::Profile*
read(const Util::StringVec& profileFiles, const Util::UIntVec* groupMap,
     int mergeTy, uint rFlags, uint mrgFlags)
{
  // Special case
  if (profileFiles.empty()) {
    Prof::CallPath::Profile* prof = Prof::CallPath::Profile::make(rFlags);
    return prof;
  }
  
  // General case
  uint groupId = (groupMap) ? (*groupMap)[0] : 0;
  Prof::CallPath::Profile* prof = read(profileFiles[0], groupId, rFlags);

  // add the directory into the set of directories
  prof->addDirectory(profileFiles[0]);

  for (uint i = 1; i < profileFiles.size(); ++i) {
    groupId = (groupMap) ? (*groupMap)[i] : 0;
    Prof::CallPath::Profile* p = read(profileFiles[i], groupId, rFlags);
    prof->merge(*p, mergeTy, mrgFlags);

    prof->metricMgr()->mergePerfEventStatistics(p->metricMgr());
    delete p;

    // add the directory into the set of directories
    prof->addDirectory(profileFiles[i]);
  }
  prof->metricMgr()->mergePerfEventStatistics_finalize(profileFiles.size());
  
  return prof;
}


Prof::CallPath::Profile*
read(const char* prof_fnm, uint groupId, uint rFlags)
{
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  Prof::CallPath::Profile* prof = NULL;
  try {
    DIAG_MsgIf(0, "Reading: '" << prof_fnm << "'");
    prof = Prof::CallPath::Profile::make(prof_fnm, rFlags, /*outfs*/ NULL, false);
  }
  catch (...) {
    DIAG_EMsg("While reading profile '" << prof_fnm << "'...");
    throw;
  }

  // -------------------------------------------------------
  // Potentially update the profile's metrics
  // -------------------------------------------------------

  if (groupId > 0) {
    Prof::Metric::Mgr* metricMgr = prof->metricMgr();
    for (uint i = 0; i < metricMgr->size(); ++i) {
      Prof::Metric::ADesc* m = metricMgr->metric(i);
      m->namePfx(StrUtil::toStr(groupId));
    }
    metricMgr->recomputeMaps();
  }

  return prof;
}


void
readStructure(Prof::Struct::Tree* structure, const Analysis::Args& args)
{
  DocHandlerArgs docargs(&RealPathMgr::singleton());

  Prof::Struct::readStructure(*structure, args.structureFiles,
			      PGMDocHandler::Doc_STRUCT, docargs);

  // BAnal::Struct::makeStructure() creates a Struct::Tree that
  // distinguishes between non-call-site statements and call site
  // statements mapping to the same line.
  // 
  // To maintain the Non-Overlapping Principle with respect to the
  // Struct::Tree, we used to coalesce non-call-site statements and
  // call site statements mapping to the same source line.  However,
  // this is problematic on two counts.
  // 
  // First, it is somewhat pointless unless (1)
  // Analysis::Util::demandStructure() enforces the Non-Overlapping
  // Principle; and (2) overlayStaticStructureMain(), in addition to
  // creating frames, merges nodes (Stmts and Calls) that map to the
  // same structure.
  // 
  // Second, for constructing hpcviewer's Callers view, we need
  // static-structure ids to maintain call site distinctions, even if
  // several call sites map to the same line.
  //
  // (Cf. Analysis::CallPath::noteStaticStructureOnLeaves() and
  // Prof::CCT::ADynNode::isMergable().)
  if (0) {
    coalesceStmts(*structure);
  }
}


} // namespace CallPath

} // namespace Analysis


//****************************************************************************


static void
coalesceStmts(Prof::Struct::ANode* node);


static void
coalesceStmts(Prof::Struct::Tree& structure)
{
  coalesceStmts(structure.root());
}


// coalesceStmts: Maintain Non-Overlapping Principle of source code
// for static struture.  Structure information currently distinguishes
// non-callsite statements from callsite statements, even if they are
// within the same scope and on the same source line.
void
coalesceStmts(Prof::Struct::ANode* node)
{
  typedef std::map<SrcFile::ln, Prof::Struct::Stmt*> LineToStmtMap;

  if (!node) {
    return;
  }

  // A <line> -> <stmt> is sufficient for statements within the same scope
  LineToStmtMap stmtMap;
  
  // ---------------------------------------------------
  // For each immediate child of this node...
  //
  // Use cmpById()-ordering so that results are deterministic
  // (cf. hpcprof-mpi)
  // ---------------------------------------------------
  for (Prof::Struct::ANodeSortedChildIterator it(node, Prof::Struct::ANodeSortedIterator::cmpById);
       it.current(); /* */) {
    Prof::Struct::ANode* n = it.current();
    it++; // advance iterator -- it is pointing at 'n'
    
    if ( n->isLeaf() && (typeid(*n) == typeid(Prof::Struct::Stmt)) ) {
      // Test for duplicate source line info.
      Prof::Struct::Stmt* n_stmt = static_cast<Prof::Struct::Stmt*>(n);
      SrcFile::ln line = n_stmt->begLine();
      LineToStmtMap::iterator it_stmt = stmtMap.find(line);
      if (it_stmt != stmtMap.end()) {
	// found -- we have a duplicate
	Prof::Struct::Stmt* n_stmtOrig = (*it_stmt).second;
	DIAG_MsgIf(0, "coalesceStmts: deleting " << n_stmt->toStringMe());
	Prof::Struct::ANode::merge(n_stmtOrig, n_stmt); // deletes n_stmt
      }
      else {
	// no entry found -- add
	stmtMap.insert(std::make_pair(line, n_stmt));
      }
    }
    else if (!n->isLeaf()) {
      // Recur
      coalesceStmts(n);
    }
  }
}


//****************************************************************************
// Precompute Struct Simple
//****************************************************************************

//
// Use use binutils to pre-compute the struct simple tree for all load
// modules that don't have a full structure file.  Overlay static
// structure has trouble with aliens if we compute the stmt nodes
// incrementally.  (why?)
//
// Note: adding inline sequences from a struct file can blow up the
// size of the CCT by 50x, so we gather all the vma's per load module
// early in one pass.
//

typedef std::vector <VMA> VmaVec;
typedef std::map <int, VmaVec *> VmaVecMap;

//
// Traverse CCT Tree, collect (VMA, LM) pairs for each dyn node, and
// make a vector of VMA's per load module.
//
static void
makeVMAmap(VmaVecMap & vmaMap, Prof::CCT::ANode * node)
{
  using namespace Prof;

  // traverse CCT tree, same order as overlayStaticStructure()
  for (CCT::ANodeSortedChildIterator nit(node, CCT::ANodeSortedIterator::cmpByDynInfo);
       nit.current(); )
  {
    CCT::ANode * n2 = nit.current();
    nit++;

    // only dynamic nodes have vma address
    CCT::ADynNode * n_dyn = dynamic_cast <CCT::ADynNode *> (n2);
    if ( n_dyn ) {
      Prof::LoadMap::LMId_t lmid = n_dyn->lmId();
      VMA vma = n_dyn->lmIP();
      VmaVec * vec = NULL;

      auto it = vmaMap.find(lmid);
      if (it != vmaMap.end()) {
	vec = it->second;
      }
      else {
	vec = new VmaVec;
	vmaMap[lmid] = vec;
      }
      vec->push_back(vma);
    }

    if (! n2->isLeaf()) {
      makeVMAmap(vmaMap, n2);
    }
  }
}


//
// Precompute struct simple for one struct tree (lmStruct) from the
// binutils load module (lm) and vma vector (vmaVec).
//
static void
precomputeStructSimple(Prof::Struct::LM * lmStruct,
		       BinUtil::LM * lm,
		       VmaVec * vmaVec)
{
  if (lm == NULL || vmaVec == NULL) {
    return;
  }

  for (uint i = 0; i < vmaVec->size(); i++) {
    VMA vma = (*vmaVec)[i];

    if (lmStruct->findStmt(vma) == NULL) {
      BAnal::Struct::makeStructureSimple(lmStruct, lm, vma);
    }
  }

  lmStruct->computeVMAMaps();
}


//****************************************************************************
// Overlaying static structure on a CCT
//****************************************************************************

typedef std::map<Prof::Struct::ANode*, Prof::CCT::ANode*> StructToCCTMap;

static void
overlayStaticStructureMain(Prof::CallPath::Profile& prof,
			   Prof::LoadMap::LM* loadmap_lm,
			   Prof::Struct::LM* lmStrct,
			   VmaVec * vmaVec,
                           bool printProgress);

static void
overlayStaticStructure(Prof::CCT::ANode* node,
		       Prof::LoadMap::LM* loadmap_lm,
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm);

static Prof::CCT::ANode*
demandScopeInFrame(Prof::CCT::ADynNode* node, Prof::Struct::ANode* strct,
		   StructToCCTMap& strctToCCTMap);

static Prof::CCT::ProcFrm*
makeFrame(Prof::CCT::ADynNode* node, Prof::Struct::Proc* procStrct,
	  StructToCCTMap& strctToCCTMap);

static void
makeFrameStructure(Prof::CCT::ANode* node_frame,
		   Prof::Struct::ACodeNode* node_strct,
		   StructToCCTMap& strctToCCTMap);

static void
coalesceStmts(Prof::CallPath::Profile& prof);


//****************************************************************************

//
// The main entry point for hpcprof and prof-mpi.  Iterate over load
// modules and overlay structure one LM at a time.
//
void
Analysis::CallPath::
overlayStaticStructureMain(Prof::CallPath::Profile& prof,
			   string agent, bool doNormalizeTy,
                           bool printProgress)
{
  const Prof::LoadMap* loadmap = prof.loadmap();
  Prof::Struct::Root* rootStrct = prof.structure()->root();
  VmaVecMap vmaMap;

  makeVMAmap(vmaMap, prof.cct()->root());

  std::string errors;

  // -------------------------------------------------------
  // Overlay static structure. N.B. To process spurious samples,
  // iteration includes LoadMap::LMId_NULL
  // -------------------------------------------------------
  for (Prof::LoadMap::LMId_t i = Prof::LoadMap::LMId_NULL;
      i <= loadmap->size(); ++i) {
    Prof::LoadMap::LM* lm = loadmap->lm(i);

    if (lm->isUsed()) {
      try {
        const string& lm_nm = lm->name();
        Prof::Struct::LM* lmStrct = Prof::Struct::LM::demand(rootStrct, lm_nm);

	VmaVec * vmaVec = NULL;
	auto it = vmaMap.find(i);
	if (it != vmaMap.end()) {
	  vmaVec = it->second;
	}

	overlayStaticStructureMain(prof, lm, lmStrct, vmaVec, printProgress);
      }
      catch (const Diagnostics::Exception& x) {
        errors += "  " + x.what() + "\n";
      }
    }
  }

  if (!errors.empty()) {
    DIAG_WMsgIf(1, "Cannot fully process samples because of errors reading load modules:\n" << errors);
  }

  // delete VMA vectors
  for (auto it = vmaMap.begin(); it != vmaMap.end(); ++it) {
    delete it->second;
  }

  // -------------------------------------------------------
  // Basic normalization
  // -------------------------------------------------------
  if (doNormalizeTy) {
    coalesceStmts(prof);
  }
  
  applyThreadMetricAgents(prof, agent);

  normalize(prof, "none", true);
}


//
// Overlay for one load module.
//
static void
overlayStaticStructureMain(Prof::CallPath::Profile& prof,
			   Prof::LoadMap::LM* loadmap_lm,
			   Prof::Struct::LM* lmStrct,
			   VmaVec * vmaVec,
                           bool printProgress)
{
  const string& lm_nm = loadmap_lm->name();
  const string& lm_pretty_name = Prof::LoadMap::LM::pretty_name(lm_nm);

  BinUtil::LM* lm = NULL;

  bool useStruct = (lmStrct->childCount() > 0);

  if (useStruct) {
    DIAG_MsgIf(printProgress, "STRUCTURE: " << lm_pretty_name);
  } else if (loadmap_lm->id() == Prof::LoadMap::LMId_NULL) {
    // no-op for this case
  } else {
    try {
      lm = new BinUtil::LM();
      lm->open(lm_nm.c_str());
      lm->read(prof.directorySet(), BinUtil::LM::ReadFlg_Proc);
      if (vmaVec == NULL) {
	DIAG_WMsgIf(printProgress, "Unable to compute struct simple for " << lm_nm);
      }
      else {
	precomputeStructSimple(lmStrct, lm, vmaVec);
      }
    }
    catch (const Diagnostics::Exception& x) {
      delete lm;
      lm = NULL;
      DIAG_WMsgIf(printProgress, "Cannot fully process samples for load module " << 
                  lm_pretty_name << ": " << x.what());
    }
    if (lm) DIAG_MsgIf(printProgress, "Line map : " << lm_pretty_name);
  }

  if (lm) {
    lmStrct->pretty_name(lm->name());
  }

  overlayStaticStructure(prof.cct()->root(), loadmap_lm, lmStrct, NULL);
  
  // account for new structure inserted by BAnal::Struct::makeStructureSimple()
  lmStrct->computeVMAMaps();

  delete lm;
}


void
Analysis::CallPath::
noteStaticStructureOnLeaves(Prof::CallPath::Profile& prof)
{
  const Prof::Struct::Root* rootStrct = prof.structure()->root();

  Prof::CCT::ANodeIterator it(prof.cct()->root(), NULL/*filter*/,
			      true/*leavesOnly*/, IteratorStack::PreOrder);
  for (Prof::CCT::ANode* n = NULL; (n = it.current()); ++it) {
    Prof::CCT::ADynNode* n_dyn = dynamic_cast<Prof::CCT::ADynNode*>(n);
    if (n_dyn) {
      Prof::LoadMap::LMId_t lmId = n_dyn->lmId(); // ok if LoadMap::LMId_NULL
      Prof::LoadMap::LM* loadmap_lm = prof.loadmap()->lm(lmId);
      const string& lm_nm = loadmap_lm->name();

      const Prof::Struct::LM* lmStrct = rootStrct->findLM(lm_nm);
      DIAG_Assert(lmStrct, "failed to find Struct::LM: " << lm_nm);

      VMA lm_ip = n_dyn->lmIP();
      const Prof::Struct::ACodeNode* strct = lmStrct->findByVMA(lm_ip);

      // Laks: I don't think an empty strct is critical. We can just send a warning
      //  and then continue. (like the serial version of hpcprof)
      if (!strct) {
        DIAG_EMsg("Analysis::CallPath::noteStaticStructureOnLeaves: failed to find structure for: "
            << n_dyn->toStringMe(Prof::CCT::Tree::OFlg_DebugAll));
        continue;
      }

      n->structure(strct);
    }
  }
}


//
// overlayStaticStructure: Create frames for CCT::Call and CCT::Stmt
// nodes using a preorder walk over the CCT.
//
// Technically an entry point for one load module, but nothing
// actually uses this.
//
void
Analysis::CallPath::
overlayStaticStructure(Prof::CallPath::Profile& prof,
		       Prof::LoadMap::LM* loadmap_lm,
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  overlayStaticStructure(prof.cct()->root(), loadmap_lm, lmStrct, lm);
}


//****************************************************************************

//
// Where the overlay actually happens.  This is per load module.
//
static void
overlayStaticStructure(Prof::CCT::ANode* node,
		       Prof::LoadMap::LM* loadmap_lm,
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  // INVARIANT: The parent of 'node' has been fully processed
  // w.r.t. the given load module and lives within a correctly located
  // procedure frame.
  
  if (!node) { return; }

  bool useStruct = (!lm);

  // N.B.: dynamically allocate to better handle the deep recursion
  // required for very deep CCTs.
  StructToCCTMap* strctToCCTMap = new StructToCCTMap;

  if (0 && Analysis::CallPath::dbgOs) {
    (*Analysis::CallPath::dbgOs) << "overlayStaticStructure: node (";
    Prof::CCT::ADynNode* node_dyn = dynamic_cast<Prof::CCT::ADynNode*>(node);
    if (node_dyn) {
      (*Analysis::CallPath::dbgOs) << node_dyn->lmId() << ", " << hex << node_dyn->lmIP() << dec;
    }
    (*Analysis::CallPath::dbgOs) << "): " << node->toStringMe() << std::endl;
  }

  // ---------------------------------------------------
  // For each immediate child of this node...
  //
  // Use cmpByDynInfo()-ordering so that results are deterministic
  // (cf. hpcprof-mpi)
  // ---------------------------------------------------
  for (Prof::CCT::ANodeSortedChildIterator it(node, Prof::CCT::ANodeSortedIterator::cmpByDynInfo);
       it.current(); /* */) {
    Prof::CCT::ANode* n = it.current();
    it++; // advance iterator -- it is pointing at 'n'
    
    // ---------------------------------------------------
    // process Prof::CCT::ADynNode nodes
    // 
    // N.B.: Since we process w.r.t. one load module at a time, we may
    //   see non-ADynNode nodes!
    // ---------------------------------------------------
    Prof::CCT::ADynNode* n_dyn = dynamic_cast<Prof::CCT::ADynNode*>(n);
    if (n_dyn && (n_dyn->lmId() == loadmap_lm->id())) {
      using namespace Prof;

      const string* unkProcNm = NULL;
      if (n_dyn->isSecondarySynthRoot()) {
	unkProcNm = &Struct::Tree::PartialUnwindProcNm;
      }

      // 1. Add symbolic information to 'n_dyn'
      VMA lm_ip = n_dyn->lmIP();
      Struct::ACodeNode* strct =
        Analysis::Util::demandStructure(lm_ip, lmStrct, lm, useStruct,
				unkProcNm);
      
      n->structure(strct);

      //strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;

      DIAG_MsgIf(0, "overlayStaticStructure: dyn (" << n_dyn->lmId() << ", " << hex << lm_ip << ") --> struct " << strct << dec << " " << strct->toStringMe());
      if (0 && Analysis::CallPath::dbgOs) {
	(*Analysis::CallPath::dbgOs) << "dyn (" << n_dyn->lmId() << ", " << hex << lm_ip << dec << ") --> struct " << strct->toStringMe() << std::endl;
      }

      // 2. Demand a procedure frame for 'n_dyn' and its scope within it
      Struct::ANode* scope_strct = strct->ancestor(Struct::ANode::TyLoop,
						   Struct::ANode::TyAlien,
						   Struct::ANode::TyProc);
      //scope_strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;

      Prof::CCT::ANode* scope_frame =
	demandScopeInFrame(n_dyn, scope_strct, *strctToCCTMap);

      // 3. Link 'n' to its parent
      n->unlink();
      n->link(scope_frame);
    }
    
    // ---------------------------------------------------
    // recur
    // ---------------------------------------------------
    if (!n->isLeaf()) {
      overlayStaticStructure(n, loadmap_lm, lmStrct, lm);
    }
  }

  delete strctToCCTMap;
}


// demandScopeInFrame: Return the scope in the CCT frame that
// corresponds to 'strct'.  Creates a procedure frame and adds
// structure to it, if necessary.
//
// INVARIANT: symbolic information has been added to 'node'.
static Prof::CCT::ANode*
demandScopeInFrame(Prof::CCT::ADynNode* node,
		   Prof::Struct::ANode* strct,
		   StructToCCTMap& strctToCCTMap)
{
  Prof::CCT::ANode* frameScope = NULL;
  
  StructToCCTMap::iterator it = strctToCCTMap.find(strct);
  if (it != strctToCCTMap.end()) {
    frameScope = (*it).second;
  }
  else {
    Prof::Struct::Proc* procStrct = strct->ancestorProc();
    makeFrame(node, procStrct, strctToCCTMap);

    it = strctToCCTMap.find(strct);
    DIAG_Assert(it != strctToCCTMap.end(), "");
    frameScope = (*it).second;
  }
  
  return frameScope;
}


// makeFrame: Create a CCT::ProcFrm 'frame' corresponding to 'procStrct'
//   - make 'frame' a sibling of 'node'
//   - populate 'strctToCCTMap' with the frame's static structure
static Prof::CCT::ProcFrm*
makeFrame(Prof::CCT::ADynNode* node, Prof::Struct::Proc* procStrct,
	  StructToCCTMap& strctToCCTMap)
{
  Prof::CCT::ProcFrm* frame = new Prof::CCT::ProcFrm(NULL, procStrct);
  frame->link(node->parent());
  strctToCCTMap.insert(std::make_pair(procStrct, frame));

  makeFrameStructure(frame, procStrct, strctToCCTMap);

  return frame;
}


// makeFrameStructure: Given a procedure frame 'frame' and its
// associated procedure structure 'procStrct', mirror procStrct's loop
// and alien structure within 'frame'.  Populate 'strctToCCTMap'.
// 
// NOTE: this *eagerly* adds structure to a frame that may later be
// pruned by pruneTrivialNodes().  We could be a little smarter, but on
// another day.
static void
makeFrameStructure(Prof::CCT::ANode* node_frame,
		   Prof::Struct::ACodeNode* node_strct,
		   StructToCCTMap& strctToCCTMap)
{
  for (Prof::Struct::ACodeNodeChildIterator it(node_strct);
       it.Current(); ++it) {
    Prof::Struct::ACodeNode* n_strct = it.current();

    // Done: if we reach the natural base case or embedded procedure
    if (n_strct->isLeaf() || typeid(*n_strct) == typeid(Prof::Struct::Proc)) {
      continue;
    }

    // Create n_frame, the frame node corresponding to n_strct
    Prof::CCT::ANode* n_frame = NULL;
    if (typeid(*n_strct) == typeid(Prof::Struct::Loop)) {
      n_frame = new Prof::CCT::Loop(node_frame, n_strct);
    }
    else if (typeid(*n_strct) == typeid(Prof::Struct::Alien)) {
      n_frame = new Prof::CCT::Proc(node_frame, n_strct);
    }
    
    if (n_frame) {
      strctToCCTMap.insert(std::make_pair(n_strct, n_frame));
      DIAG_DevMsgIf(0, "makeFrameStructure: " << hex << " [" << n_strct << " -> " << n_frame << "]" << dec);

      // Recur
      makeFrameStructure(n_frame, n_strct, strctToCCTMap);
    }
  }
}


//***************************************************************************

static void
coalesceStmts(Prof::CCT::ANode* node);

static void
coalesceStmts(Prof::CallPath::Profile& prof)
{
  coalesceStmts(prof.cct()->root());
}


// coalesceStmts: After static structure has been overlayed,
// CCT::Stmt's live within a procedure frame (alien or native).
// However, leaf nodes (CCT::Stmt) are still distinct according to
// instruction pointer.  Group CCT::Stmts within the same scope by
// line (or structure).
void
coalesceStmts(Prof::CCT::ANode* node)
{
  typedef std::map<SrcFile::ln, Prof::CCT::Stmt*> LineToStmtMap;

  if (!node) {
    return;
  }

  // A <line> -> <stmt> is sufficient because procedure frames
  // identify a unique load module and source file.
  // 
  // N.B.: dynamically allocate to better handle the deep recursion
  // required for very deep CCTs.
  LineToStmtMap* stmtMap = new LineToStmtMap;
  
  // ---------------------------------------------------
  // For each immediate child of this node...
  //
  // cmpByDynInfo()-ordering should be good enough
  // ---------------------------------------------------
  for (Prof::CCT::ANodeSortedChildIterator it(node, Prof::CCT::ANodeSortedIterator::cmpByDynInfo);
       it.current(); /* */) {
    Prof::CCT::ANode* n = it.current();
    it++; // advance iterator -- it is pointing at 'n'
    
    if ( n->isLeaf() && (typeid(*n) == typeid(Prof::CCT::Stmt)) ) {
      // Test for duplicate source line info.
      Prof::CCT::Stmt* n_stmt = static_cast<Prof::CCT::Stmt*>(n);
      auto *strct = dynamic_cast<Prof::Struct::Stmt *>(n_stmt->structure());

      // Filter out call stmts
      if (strct->stmtType() == Prof::Struct::Stmt::STMT_STMT) {
        SrcFile::ln line = n_stmt->begLine();
        LineToStmtMap::iterator it_stmt = stmtMap->find(line);
        if (it_stmt != stmtMap->end()) {
          // found -- we have a duplicate
          Prof::CCT::Stmt* n_stmtOrig = (*it_stmt).second;

          DIAG_MsgIf(DEBUG_COALESCING, "Coalescing:\n" 
            << "\tx: " 
            << n_stmtOrig->toStringMe(Prof::CCT::Tree::OFlg_Debug) 
            << "\n\ty: " 
            << n_stmt->toStringMe(Prof::CCT::Tree::OFlg_Debug));

          // N.B.: Because (a) trace records contain a function's
          // representative IP and (b) two traces that contain samples
          // from the same function should have their conflict resolved
          // in Prof::CallPath::Profile::merge(), we would expect that
          // merge effects are impossible.  That is, we expect that it
          // is impossible that a CCT::ProcFrm has multiple CCT::Stmts
          // with distinct trace ids.
          //
          // However, merge effects are possible *after* static
          // structure is added to the CCT.  The reason is that multiple
          // object-level procedures can map to one source-level
          // procedure (e.g., multiple template instantiations mapping
          // to the same source template or multiple stripped functions
          // mapping to UnknownProcNm).
          if (! Prof::CCT::ADynNode::hasMergeEffects(*n_stmtOrig, *n_stmt)) {
            Prof::CCT::MergeEffect effct = n_stmtOrig->mergeMe(*n_stmt, /*MergeContext=*/ NULL,/*metricBegIdx=*/ 0, /*mayConflict=*/ false);
            DIAG_Assert(effct.isNoop(), "Analysis::CallPath::coalesceStmts: trace ids lost (" << effct.toString() << ") when merging y into x:\n"
              << "\tx: " << n_stmtOrig->toStringMe(Prof::CCT::Tree::OFlg_Debug) << "\n"
              << "\ty: " << n_stmt->toStringMe(Prof::CCT::Tree::OFlg_Debug));

            // remove 'n_stmt' from tree
            n_stmt->unlink();
            delete n_stmt; // NOTE: could clear corresponding StructMetricIdFlg
          }
        }
        else {
          // no entry found -- add
          stmtMap->insert(std::make_pair(line, n_stmt));
        }
      }
    }
    else if (!n->isLeaf()) {
      // Recur
      coalesceStmts(n);
    }
  }

  delete stmtMap;
}


//***************************************************************************
// Normalizing the CCT
//***************************************************************************

static void
pruneTrivialNodes(Prof::CallPath::Profile& prof);

static void
mergeCilkMain(Prof::CallPath::Profile& prof);

static void
noteStaticStructure(Prof::CallPath::Profile& prof);


void
Analysis::CallPath::pruneBySummaryMetrics(Prof::CallPath::Profile& prof,
					  uint8_t* prunedNodes)
{
  VMAIntervalSet ivalset;
  
  const Prof::Metric::Mgr& mMgrGbl = *(prof.metricMgr());
  for (uint mId = 0; mId < mMgrGbl.size(); ++mId) {
    const Prof::Metric::ADesc* m = mMgrGbl.metric(mId);
    if (m->isVisible() && !m->isTemporary()
	&& m->type() == Prof::Metric::ADesc::TyIncl
	&& (m->nameBase().find("Sum") != string::npos)) {
      ivalset.insert(VMAInterval(mId, mId + 1)); // [ )
    }
  }
  
  prof.cct()->root()->pruneByMetrics(*prof.metricMgr(), ivalset,
				     prof.cct()->root(), 0.001,
				     prunedNodes);
}


void
Analysis::CallPath::normalize(Prof::CallPath::Profile& prof,
			      string agent, bool GCC_ATTR_UNUSED doNormalizeTy)
{
  pruneTrivialNodes(prof);

  if (agent == "agent-cilk") {
    mergeCilkMain(prof); // may delete CCT:ProcFrm
  }
}


void
Analysis::CallPath::pruneStructTree(Prof::CallPath::Profile& prof)
{
  // -------------------------------------------------------
  // Prune Struct::Tree based on CallPath::Profile::StructMetricIdFlg
  //
  // Defer this until the end so that we keep the minimal
  // Struct::Tree.  Note that this should almost certainly come after
  // all uses of noteStaticStructureOnLeaves().
  // -------------------------------------------------------
  
  noteStaticStructure(prof);

  Prof::Struct::Root* rootStrct = prof.structure()->root();
  rootStrct->aggregateMetrics(Prof::CallPath::Profile::StructMetricIdFlg);
  rootStrct->pruneByMetrics();
}


//***************************************************************************

// pruneTrivialNodes:
// 
// Without static structure, the CCT is sparse in the sense that
// *every* node must have some non-zero inclusive metric value.  To
// see this, note that every leaf node represents a sample point;
// therefore all interior metric values must be greater than zero.
//
//  However, when static structure is added, the CCT may contain
// 'spurious' static scopes in the sense that their metric values are
// zero.  Since such scopes will not have CCT::Stmt nodes as children,
// we can prune them by removing empty scopes rather than computing
// inclusive values and pruning nodes whose metric values are all
// zero.

static void
pruneTrivialNodes(Prof::CCT::ANode* node);

static void
pruneTrivialNodes(Prof::CallPath::Profile& prof)
{
  pruneTrivialNodes(prof.cct()->root());
}


static void
pruneTrivialNodes(Prof::CCT::ANode* node)
{
  using namespace Prof;

  if (!node) { return; }

  for (CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    CCT::ANode* x = it.current();
    it++; // advance iterator -- it is pointing at 'x'

    // 1. Recursively do any trimming for this tree's children
    pruneTrivialNodes(x);

    // 2. Trim this node if necessary
    if (x->isLeaf()) {
      bool isPrunableTy = (typeid(*x) == typeid(CCT::ProcFrm) ||
			   typeid(*x) == typeid(CCT::Proc) ||
			   typeid(*x) == typeid(CCT::Loop));
      bool isSynthetic =
	(dynamic_cast<Prof::CCT::ADynNode*>(x)
	 && static_cast<Prof::CCT::ADynNode*>(x)->isSecondarySynthRoot());
	
      if (isPrunableTy || isSynthetic) {
	x->unlink(); // unlink 'x' from tree
	DIAG_DevMsgIf(0, "pruneTrivialNodes: " << hex << x << dec << " (sid: " << x->structureId() << ")");
	delete x;
      }
    }
  }
}


//***************************************************************************

// mergeCilkMain: cilk_main is called from two distinct call sites
// within the runtime, resulting in an undesirable bifurcation within
// the CCT.  The easiest way to fix this is to use a normalization
// step.
static void
mergeCilkMain(Prof::CallPath::Profile& prof)
{
  using namespace Prof;

  CCT::ProcFrm* mainFrm = NULL;

  // 1. attempt to find 'CilkNameMgr::cilkmain'
  for (CCT::ANodeIterator it(prof.cct()->root(),
			     &CCT::ANodeTyFilter[CCT::ANode::TyProcFrm]);
       it.Current(); ++it) {
    CCT::ProcFrm* x = static_cast<CCT::ProcFrm*>(it.current());
    if (x->procName() == CilkNameMgr::cilkmain) {
      mainFrm = x;
      break;
    }
  }

  // 2. merge any sibling 'CilkNameMgr::cilkmain'
  if (mainFrm) {
    CCT::ANodeChildIterator it(mainFrm->parent(),
			       &CCT::ANodeTyFilter[CCT::ANode::TyProcFrm]);
    for ( ; it.Current(); /* */) {
      CCT::ProcFrm* x = static_cast<CCT::ProcFrm*>(it.current());
      it++; // advance iterator -- it is pointing at 'x'
      
      if (x->procName() == CilkNameMgr::cilkmain) {
	mainFrm->merge(x); // deletes 'x'
      }
    }
  }
}


//***************************************************************************

static void
noteStaticStructure(Prof::CallPath::Profile& prof)
{
  using namespace Prof;

  const Prof::CCT::ANode* cct_root = prof.cct()->root();

  for (CCT::ANodeIterator it(cct_root); it.Current(); ++it) {
    CCT::ANode* x = it.current();

    Prof::Struct::ACodeNode* strct = x->structure();
    if (strct) {
      strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;
    }
  }
}


//***************************************************************************
// Making special CCT metrics
//***************************************************************************

static void
makeReturnCountMetric(Prof::CallPath::Profile& prof);

// N.B.: Expects that thread-level metrics are available.
void
Analysis::CallPath::applyThreadMetricAgents(Prof::CallPath::Profile& prof,
					    string agent)
{
  makeReturnCountMetric(prof);

  if (!agent.empty()) {
    MetricComponentsFact* metricComponentsFact = NULL;
    if (agent == "agent-cilk") {
      metricComponentsFact = new CilkOverheadMetricFact;
    }
    else if (agent == "agent-mpi") {
      // *** applySummaryMetricAgents() ***
    }
    else if (agent == "agent-pthread") {
      metricComponentsFact = new PthreadOverheadMetricFact;
    }
    else {
      DIAG_Die("Bad value for 'agent': " << agent);
    }

    if (metricComponentsFact) {
      metricComponentsFact->make(prof);
      delete metricComponentsFact;
    }
  }
}


// N.B.: Expects that summary metrics have been computed.
void
Analysis::CallPath::applySummaryMetricAgents(Prof::CallPath::Profile& prof,
					     string agent)
{
  if (!agent.empty()) {
    MetricComponentsFact* metricComponentsFact = NULL;
    if (agent == "agent-mpi") {
      metricComponentsFact = new MPIBlameShiftIdlenessFact;
    }

    if (metricComponentsFact) {
      metricComponentsFact->make(prof);
      delete metricComponentsFact;
    }
  }
}


//***************************************************************************

// makeReturnCountMetric: A return count refers to the number of times
// a given CCT node is called by its parent context.  However, when
// hpcrun records return counts, there is no structure (e.g. procedure
// frames) in the CCT.  An an example, in the CCT fragment below, the
// return count [3] at 0xc means that 0xc returned to 0xbeef 3 times.
// Simlarly, 0xbeef returned to its caller 5 times.
//
//              |               |
//       ip: 0xbeef [5]         |
//       /      |      \        |
//   0xa [1]  0xb [2]  0xc [3]  |
//      |       |       |       |
//
// To be able to say procedure F is called by procedure G x times
// within this context, it is necessary to aggregate these counts at
// the newly added procedure frames (Struct::ProcFrm).
static void
makeReturnCountMetric(Prof::CallPath::Profile& prof)
{
  std::vector<uint> retCntId;

  // -------------------------------------------------------
  // find return count metrics, if any
  // -------------------------------------------------------
  Prof::Metric::Mgr* metricMgr = prof.metricMgr();
  for (uint i = 0; i < metricMgr->size(); ++i) {
    Prof::Metric::ADesc* m = metricMgr->metric(i);
    if (m->nameBase().find(HPCRUN_METRIC_RetCnt) != string::npos) {
      retCntId.push_back(m->id());
      m->computedType(Prof::Metric::ADesc::ComputedTy_Final);
      m->type(Prof::Metric::ADesc::TyExcl);
    }
  }

  if (retCntId.empty()) {
    return;
  }

  // -------------------------------------------------------
  // propagate and aggregate return counts
  // -------------------------------------------------------
  Prof::CCT::ANode* cct_root = prof.cct()->root();
  Prof::CCT::ANodeIterator it(cct_root, NULL/*filter*/, false/*leavesOnly*/,
			      IteratorStack::PostOrder);
  for (Prof::CCT::ANode* n = NULL; (n = it.current()); ++it) {
    if (typeid(*n) != typeid(Prof::CCT::ProcFrm) && n != cct_root) {
      Prof::CCT::ANode* n_parent = n->parent();
      for (uint i = 0; i < retCntId.size(); ++i) {
	uint mId = retCntId[i];
	n_parent->demandMetric(mId) += n->demandMetric(mId);
	n->metric(mId) = 0.0;
      }
    }
  }
}


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {

static void
write(Prof::CallPath::Profile& prof, std::ostream& os,
      const Analysis::Args& args);


// makeDatabase: assumes Analysis::Args::makeDatabaseDir() has been called
void
makeDatabase(Prof::CallPath::Profile& prof, const Analysis::Args& args)
{
  const string& db_dir = args.db_dir;

  DIAG_Msg(1, "Populating Experiment database: " << db_dir);
  
  // 1. Copy source files.  
  //    NOTE: makes file names in 'prof.structure' relative to database
  Analysis::Util::copySourceFiles(prof.structure()->root(),
				  args.searchPathTpls, db_dir);

  // 2. Copy trace files (if necessary)
  Analysis::Util::copyTraceFiles(db_dir, prof.traceFileNameSet());

  // 3. Create 'experiment.xml' file
  string experiment_fnm = db_dir + "/" + args.out_db_experiment;
  std::ostream* os = IOUtil::OpenOStream(experiment_fnm.c_str());

  char* outBuf = new char[HPCIO_RWBufferSz];

  std::streambuf* os_buf = os->rdbuf();
  os_buf->pubsetbuf(outBuf, HPCIO_RWBufferSz);

  // 4. Write data for 'experiment.xml'
  Analysis::CallPath::write(prof, *os, args);
  IOUtil::CloseStream(os);

  // 5. Create 'experiment.dtd' file
  string dtd_fnm = db_dir + "/" + Analysis_OUT_DB_DTD;
  os = IOUtil::OpenOStream(dtd_fnm.c_str());
  os_buf = os->rdbuf();
  os_buf->pubsetbuf(outBuf, HPCIO_RWBufferSz);

  static const char* experimentDTD =
#include <lib/xml/hpc-experiment.dtd.h>
  *os << experimentDTD;
  
  IOUtil::CloseStream(os);
  
  delete[] outBuf;
}


static void
write(Prof::CallPath::Profile& prof, std::ostream& os,
      const Analysis::Args& args)
{
  using namespace Prof;

  int oFlags = 0; // CCT::Tree::OFlg_LeafMetricsOnly;
  if (Diagnostics_GetDiagnosticFilterLevel() < 5) {
    oFlags |= CCT::Tree::OFlg_Compressed;
  }
  DIAG_If(5) {
    oFlags |= CCT::Tree::OFlg_Debug;
  }

  if (args.db_addStructId) {
    oFlags |= CCT::Tree::OFlg_StructId;
  }

  uint metricBegId = 0;
  uint metricEndId = prof.metricMgr()->size();

 {
    Metric::ADesc* mBeg = prof.metricMgr()->findFirstVisible();
    Metric::ADesc* mEnd = prof.metricMgr()->findLastVisible();
    metricBegId = (mBeg) ? mBeg->id()     : Metric::Mgr::npos;
    metricEndId = (mEnd) ? mEnd->id() + 1 : Metric::Mgr::npos;
  }

  string name = (args.title.empty()) ? prof.name() : args.title;

  os << "<?xml version=\"1.0\"?>\n";

  os << "<HPCToolkitExperiment version=\"" DATABASE_VERSION "\">\n";
  os << "<!--DTD file=\"" Analysis_OUT_DB_DTD "\"-->\n";
  os << "<Header n" << MakeAttrStr(name) << ">\n";
  os << "  <Info/>\n";
  os << "</Header>\n";

  os << "<SecCallPathProfile i=\"0\" n" << MakeAttrStr(name) << ">\n";

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << "<SecHeader>\n";
  prof.writeXML_hdr(os, metricBegId, metricEndId, oFlags);
  os << "  <Info/>\n";
  os << "</SecHeader>\n";
  os.flush();

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << "<SecCallPathProfileData>\n";
  prof.cct()->writeXML(os, metricBegId, metricEndId, oFlags);
  os << "</SecCallPathProfileData>\n";

  os << "</SecCallPathProfile>\n";
  os << "</HPCToolkitExperiment>\n";
  os.flush();
}

} // namespace CallPath

} // namespace Analysis

//***************************************************************************

