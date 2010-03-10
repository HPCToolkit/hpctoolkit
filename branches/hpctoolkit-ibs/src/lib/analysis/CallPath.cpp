// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <typeinfo>


//*************************** User Include Files ****************************

#include <include/uint.h>

#include "CallPath.hpp"
#include "CallPath-OverheadMetricFact.hpp"
#include "Util.hpp"

#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/PGMReader.hpp>

#include <lib/prof-lean/hpcrun-metric.h>

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>


//*************************** Forward Declarations ***************************


//****************************************************************************
// 
//****************************************************************************

static void
coalesceStmts(Prof::Struct::Tree& structure);


namespace Analysis {

namespace CallPath {


Prof::CallPath::Profile*
read(const Util::StringVec& profileFiles, const Util::UIntVec* groupMap,
     int mergeTy, uint rFlags)
{
  // Special case
  if (profileFiles.empty()) {
    Prof::CallPath::Profile* prof = Prof::CallPath::Profile::make(rFlags);
    return prof;
  }
  
  // General case
  uint groupId = (groupMap) ? (*groupMap)[0] : 0;
  Prof::CallPath::Profile* prof = read(profileFiles[0], groupId, rFlags);

  for (uint i = 1; i < profileFiles.size(); ++i) {
    groupId = (groupMap) ? (*groupMap)[i] : 0;
    Prof::CallPath::Profile* p = read(profileFiles[i], groupId, rFlags);
    prof->merge(*p, mergeTy);
    delete p;
  }
  
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
    prof = Prof::CallPath::Profile::make(prof_fnm, rFlags, /*outfs*/ NULL);
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
  DocHandlerArgs docargs; // NOTE: override for replacePath()

  Prof::Struct::readStructure(*structure, args.structureFiles,
			      PGMDocHandler::Doc_STRUCT, docargs);

  // TODO:tallent: An elegant idea but somewhat pointless unless
  // Analysis::Util::demandStructure() enforces Non-Overlapping
  // Principle and overlayStaticStructure() both creates frames and
  // merges nodes that map to the same structure.
  coalesceStmts(*structure);
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
// callsites from statements, even if they are within the same scope
// and on the same source line.
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
      LineToStmtMap::iterator it = stmtMap.find(line);
      if (it != stmtMap.end()) {
	// found -- we have a duplicate
	Prof::Struct::Stmt* n_stmtOrig = (*it).second;
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
// Overlaying static structure on a CCT
//****************************************************************************

typedef std::map<Prof::Struct::ANode*, Prof::CCT::ANode*> StructToCCTMap;

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


//****************************************************************************

void
Analysis::CallPath::
overlayStaticStructureMain(Prof::CallPath::Profile& prof,
			   string agent, bool doNormalizeTy)
{
  const Prof::LoadMapMgr* loadmap = prof.loadMapMgr();
  Prof::Struct::Root* rootStrct = prof.structure()->root();
  
  for (Prof::LoadMapMgr::LMSet_nm::const_iterator it = loadmap->lm_begin_nm();
       it != loadmap->lm_end_nm(); ++it) {
    Prof::ALoadMap::LM* loadmap_lm = *it;
    
    // tallent:TODO: The call to LoadMap::compute_relocAmt() in
    // Profile::hpcrun_fmt_epoch_fread has emitted a warning if a
    // load module is unavailable.  Probably should be here...
    if (loadmap_lm->isAvail() && loadmap_lm->isUsed()) {
      const string& lm_nm = loadmap_lm->name();
      
      Prof::Struct::LM* lmStrct = Prof::Struct::LM::demand(rootStrct, lm_nm);
      Analysis::CallPath::overlayStaticStructureMain(prof, loadmap_lm, 
						     lmStrct);
    }
  }

  Analysis::CallPath::normalize(prof, agent, doNormalizeTy);
  
  // Note: Use StructMetricIdFlg to flag that static structure is used
  rootStrct->aggregateMetrics(Prof::CallPath::Profile::StructMetricIdFlg);
  rootStrct->pruneByMetrics();
}


void
Analysis::CallPath::
overlayStaticStructureMain(Prof::CallPath::Profile& prof, 
			   Prof::LoadMap::LM* loadmap_lm,
			   Prof::Struct::LM* lmStrct)
{
  const string& lm_nm = loadmap_lm->name();
  BinUtil::LM* lm = NULL;

  bool useStruct = (lmStrct->childCount() > 0);

  if (useStruct) {
    DIAG_Msg(1, "STRUCTURE: " << lm_nm);
  }
  else {
    DIAG_Msg(1, "Line map : " << lm_nm);

    try {
      lm = new BinUtil::LM();
      lm->open(lm_nm.c_str());
      lm->read(BinUtil::LM::ReadFlg_Proc);
    }
    catch (...) {
      DIAG_EMsg("While reading '" << lm_nm << "'...");
      throw;
    }
  }

  Analysis::CallPath::overlayStaticStructure(prof, loadmap_lm, lmStrct, lm);
  
  // account for new structure inserted by banal::bloop::makeStructureSimple()
  lmStrct->computeVMAMaps();

  delete lm;
}


// overlayStaticStructure: Create frames for CCT::Call and CCT::Stmt
// nodes using a preorder walk over the CCT.
void
Analysis::CallPath::
overlayStaticStructure(Prof::CallPath::Profile& prof,
		       Prof::LoadMap::LM* loadmap_lm,
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  overlayStaticStructure(prof.cct()->root(), loadmap_lm, lmStrct, lm);
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
      Prof::LoadMap::LM* loadmap_lm = prof.loadMapMgr()->lm(n_dyn->lmId());
      const string& lm_nm = loadmap_lm->name();

      const Prof::Struct::LM* lmStrct = rootStrct->findLM(lm_nm);
      DIAG_Assert(lmStrct, "failed to find Struct::LM: " << lm_nm);

      VMA ip_ur = n_dyn->ip();
      const Prof::Struct::ACodeNode* strct = lmStrct->findByVMA(ip_ur);
      DIAG_Assert(strct, "failed to find structure: (" << n_dyn->lmId() << ", " << hex << ip_ur << dec << ")");
      
      n->structure(strct);
    }
  }
}


//****************************************************************************

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

  StructToCCTMap strctToCCTMap;

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

      // 1. Add symbolic information to 'n_dyn'
      VMA ip_ur = n_dyn->ip();
      Struct::ACodeNode* strct = 
	Analysis::Util::demandStructure(ip_ur, lmStrct, lm, useStruct);
      n->structure(strct);
      strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;

      DIAG_MsgIf(0, "overlayStaticStructure: dyn (" << n_dyn->lmId() << ", " << hex << ip_ur << ") --> struct " << strct << dec << " " << strct->toStringMe());

      // 2. Demand a procedure frame for 'n_dyn' and its scope within it
      Struct::ANode* scope_strct = strct->ancestor(Struct::ANode::TyLoop,
						   Struct::ANode::TyAlien,
						   Struct::ANode::TyProc);
      scope_strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;

      Prof::CCT::ANode* scope_frame = 
	demandScopeInFrame(n_dyn, scope_strct, strctToCCTMap);

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
// pruned by pruneByMetrics().  We could be a little smarter, but on
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
      n_frame = new Prof::CCT::ProcFrm(node_frame, n_strct);
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
// Normaling the CCT
//***************************************************************************

static void
pruneByMetrics(Prof::CallPath::Profile& prof);

static void
coalesceStmts(Prof::CallPath::Profile& prof);

static void
makeReturnCountMetric(Prof::CallPath::Profile& prof);

static void
mergeCilkMain(Prof::CallPath::Profile& prof);


void
Analysis::CallPath::normalize(Prof::CallPath::Profile& prof,
			      string agent, bool doNormalizeTy)
{
  // N.B.: sets CallPath::Profile::StructMetricIdFlg
  pruneByMetrics(prof);

  if (doNormalizeTy) {
    coalesceStmts(prof);
  }

  makeReturnCountMetric(prof);

  if (!agent.empty()) {
    OverheadMetricFact* overheadMetricFact = NULL;
    if (agent == "agent-cilk") {
      overheadMetricFact = new CilkOverheadMetricFact;
    }
    else if (agent == "agent-pthread") {
      overheadMetricFact = new PthreadOverheadMetricFact;
    }
    else {
      DIAG_Die("Bad value for 'agent': " << agent);
    }
    overheadMetricFact->make(prof);
    delete overheadMetricFact;
  }

  if (agent == "agent-cilk") {
    mergeCilkMain(prof);
  }
}


//***************************************************************************

// pruneByMetrics: 
// 
// Background: This function name should be surprising since we have
// not computed metrics for interior nodes yet.  
//
// Observe that the fully dynamic CCT is sparse in the sense that *every*
// node must have some non-zero inclusive metric value.  This is true
// because every leaf node represents a sample point.  However, when
// static structure is added, the CCT may contain 'spurious' static
// scopes.  Since such scopes will not have STATEMENT nodes as
// children, we can prune them by removing empty scopes rather than
// computing inclusive values and pruning nodes whose metric values
// are all zero.

static void
pruneByMetrics(Prof::CCT::ANode* node);

static void
pruneByMetrics(Prof::CallPath::Profile& prof)
{
  pruneByMetrics(prof.cct()->root());
}


static void
pruneByMetrics(Prof::CCT::ANode* node)
{
  using namespace Prof;

  if (!node) { return; }

  for (CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    CCT::ANode* x = it.current();
    it++; // advance iterator -- it is pointing at 'x'

    // 1. Recursively do any trimming for this tree's children
    pruneByMetrics(x);

    // 2. Trim this node if necessary
    bool isTy = (typeid(*x) == typeid(CCT::ProcFrm) || 
		 typeid(*x) == typeid(CCT::Loop));
    if (x->isLeaf() && isTy) {
      x->unlink(); // unlink 'x' from tree
      DIAG_DevMsgIf(0, "pruneByMetrics: " << hex << x << dec << " (sid: " << x->structureId() << ")");
      delete x;
    }
    else {
      // We are keeping the node -- set the static structure flag
      Struct::ACodeNode* strct = x->structure();
      if (strct) {
	strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;
      }
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
  typedef std::multimap<SrcFile::ln, Prof::CCT::Stmt*> LineToStmtMap;//changed by Xu Liu using multimap instead of map
  typedef std::multimap<string, string>::size_type sz_type; //add by Xu Liu

  int ret;

  if (!node) {
    return;
  }

  // A <line> -> <stmt> is sufficient because procedure frames
  // identify a unique load module and source file.
  LineToStmtMap stmtMap;
  
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
    
    if ( n->isLeaf() && (typeid(*n) == typeid(Prof::CCT::Stmt)) ) {
      // Test for duplicate source line info.
      Prof::CCT::Stmt* n_stmt = static_cast<Prof::CCT::Stmt*>(n);
      SrcFile::ln line = n_stmt->begLine();//find the key
      sz_type entries = stmtMap.count(line);
      LineToStmtMap::iterator it1 = stmtMap.find(line);
      if (entries != 0) {
      int indicator = 0; //indicate whether the incoming node is merged
      Prof::CCT::Stmt* stmtHost = NULL; //the incoming node is merged into stmtHost
      for(sz_type cnt=0; cnt<entries; ++cnt, ++it1){
      if(stmtHost == NULL){//no node is coalesced
	// found -- we have a duplicate
	Prof::CCT::Stmt* n_stmtOrig = (*it1).second;
        //add by Xu Liu begin
        uint i;
        for(i=0; i<n_stmtOrig->numMetrics(); i++)
          if(n_stmtOrig->fmt(i)==1)
            break;
        if(i<n_stmtOrig->numMetrics())//we have address metrics
        {
          ret = n_stmtOrig->mergeMe_IBS(*n_stmt);
          if(ret > 0)//merged
          {
            n_stmt->unlink();
            delete n_stmt;
            indicator = 1;//this node is merged
            stmtHost = n_stmtOrig; //record the host node and check whether other nodes can be merged	
            continue;
//            break;
          }
        }
        else//no address metrics
        // add by Xu Liu end
        {
	  n_stmtOrig->mergeMe(*n_stmt);
	
	  // remove 'n_stmt' from tree
	  n_stmt->unlink();
	  delete n_stmt; // NOTE: could clear corresponding StructMetricIdFlg
          indicator = 1;
        }
      }
      else //the node is merged and check whether other nodes in stmtMap can be merged
      {
         Prof::CCT::Stmt* stmtGuest = (*it1).second;
         ret = stmtHost->mergeMe_IBS(*stmtGuest);
         if(ret > 0) //merged
         {
           stmtGuest->unlink();
           //fix the problem that it1 cannot be erased
//           stmtGuest = NULL;
//           delete stmtGuest;
//           stmtMap.erase(it1);
         }
      }
      }
        if(indicator == 0)//no merge
          stmtMap.insert(std::make_pair(line, n_stmt));
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
      m->isComputed(true);
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
    bool isFrame = (typeid(*n) == typeid(Prof::CCT::ProcFrm)
		    && !(static_cast<Prof::CCT::ProcFrm*>(n)->isAlien()));
    if (!isFrame && n != cct_root) {
      Prof::CCT::ANode* n_parent = n->parent();
      for (uint i = 0; i < retCntId.size(); ++i) {
	uint mId = retCntId[i];
	n_parent->demandMetric(mId) += n->demandMetric(mId);
	n->metric(mId) = 0.0;
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


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {


// makeDatabase: assumes Analysis::Args::makeDatabaseDir() has been called
void
makeDatabase(Prof::CallPath::Profile& prof, const Analysis::Args& args)
{
  const string& db_dir = args.db_dir;
  DIAG_Msg(1, "Copying source files reached by PATH option to " << db_dir);
  // NOTE: makes file names in structure relative to database
  Analysis::Util::copySourceFiles(prof.structure()->root(), 
				  args.searchPathTpls, db_dir);
  
  string experiment_fnm = db_dir + "/" + args.out_db_experiment;
  std::ostream* os = IOUtil::OpenOStream(experiment_fnm.c_str());
  bool prettyPrint = (Diagnostics_GetDiagnosticFilterLevel() >= 5);
  Analysis::CallPath::write(prof, *os, args.title, prettyPrint);
  IOUtil::CloseStream(os);
}


void
write(Prof::CallPath::Profile& prof, std::ostream& os, 
      const string& title, bool prettyPrint)
{
  static const char* experimentDTD =
#include <lib/xml/hpc-experiment.dtd.h>

  using namespace Prof;

  int oFlags = 0; // CCT::Tree::OFlg_LeafMetricsOnly;
  if (!prettyPrint) {
    oFlags |= CCT::Tree::OFlg_Compressed;
  }
  DIAG_If(5) {
    oFlags |= CCT::Tree::OFlg_Debug;
  }

  uint metricBegId = 0;
  uint metricEndId = prof.metricMgr()->size();

  if (true /* CCT::Tree::OFlg_VisibleMetricsOnly*/) {
    Metric::ADesc* mBeg = prof.metricMgr()->findFirstVisible();
    Metric::ADesc* mEnd = prof.metricMgr()->findLastVisible();
    metricBegId = (mBeg) ? mBeg->id()     : Metric::Mgr::npos;
    metricEndId = (mEnd) ? mEnd->id() + 1 : Metric::Mgr::npos;
  }

  string name = (title.empty()) ? prof.name() : title;

  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE hpc-experiment [\n" << experimentDTD << "]>" << std::endl;
  os << "<HPCToolkitExperiment version=\"2.0\">\n";
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

