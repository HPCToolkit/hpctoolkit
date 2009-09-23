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
// Copyright ((c)) 2002-2009, Rice University 
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

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/realpath.h>


//*************************** Forward Declarations ***************************


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {


Prof::CallPath::Profile*
read(const Util::StringVec& profileFiles, const Util::UIntVec* groupMap,
     int mergeTy, uint rFlags)
{
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

  if (groupId) {
    Prof::Metric::Mgr* metricMgr = prof->metricMgr();
    for (uint i = 0; i < metricMgr->size(); ++i) {
      Prof::Metric::ADesc* m = metricMgr->metric(i);
      m->namePfx(StrUtil::toStr(groupId));
    }
    //metricMgr->recomputeMaps(); // not strictly necessary
  }

  return prof;
}


void
readStructure(Prof::Struct::Tree* structure, const Analysis::Args& args)
{
  DocHandlerArgs docargs; // NOTE: override for replacePath()

  Prof::Struct::readStructure(*structure, args.structureFiles,
			      PGMDocHandler::Doc_STRUCT, docargs);
}


} // namespace CallPath

} // namespace Analysis


//****************************************************************************
// Overlaying static structure on a CCT
//****************************************************************************

void
Analysis::CallPath::
overlayStaticStructureMain(Prof::CallPath::Profile& prof, string agent)
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
  
  Analysis::CallPath::normalize(prof, agent);
  
  // Note: Use StructMetricIdFlg to flag that static structure is used
  rootStrct->accumulateMetrics(Prof::CallPath::Profile::StructMetricIdFlg);
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

  delete lm;
}


//****************************************************************************

typedef std::map<Prof::Struct::ANode*, Prof::CCT::ANode*> StructToCCTMap;

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
overlayStaticStructure(Prof::CallPath::Profile& prof, Prof::CCT::ANode* node,
		       Prof::LoadMap::LM* loadmap_lm, 
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm);


// overlayStaticStructure: Create frames for CCT::Call and CCT::Stmt
// using a preorder walk over the CCT.
void
Analysis::CallPath::
overlayStaticStructure(Prof::CallPath::Profile& prof, 
		       Prof::LoadMap::LM* loadmap_lm, 
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  overlayStaticStructure(prof, prof.cct()->root(), loadmap_lm, lmStrct, lm);
}


static void 
overlayStaticStructure(Prof::CallPath::Profile& prof, Prof::CCT::ANode* node, 
		       Prof::LoadMap::LM* loadmap_lm, 
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  // INVARIANT: The parent of 'node' has been fully processed
  // w.r.t. the given load module and lives within a correctly located
  // procedure frame.
  
  if (!node) { return; }

  bool useStruct = (!lm);

  StructToCCTMap strctToCCTMap;

  // For each immediate child of this node...
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); /* */) {
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
      VMA ip_ur = n_dyn->ip();
      DIAG_DevIf(50) {
	Prof::CCT::Call* p = node->ancestorCall();
	DIAG_DevMsg(0, "overlayStaticStructure: " << hex << ((p) ? p->ip() : 0) << " --> " << ip_ur << dec);
      }

      using namespace Prof;

      // 1. Add symbolic information to 'n_dyn'
      Struct::ACodeNode* strct = 
	Analysis::Util::demandStructure(ip_ur, lmStrct, lm, useStruct);
      n->structure(strct);
      strct->demandMetric(CallPath::Profile::StructMetricIdFlg) += 1.0;

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
      overlayStaticStructure(prof, n, loadmap_lm, lmStrct, lm);
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

    // Done: if we reach the natural base case or embedded proceedure
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
coalesceStmts(Prof::CallPath::Profile& prof);

static void 
pruneByMetrics(Prof::CallPath::Profile& prof);

void 
Analysis::CallPath::normalize(Prof::CallPath::Profile& prof, 
			      string lush_agent)
{
  pruneByMetrics(prof);

  coalesceStmts(prof);

  if (!lush_agent.empty()) {
    OverheadMetricFact* overheadMetricFact = NULL;
    if (lush_agent == "agent-cilk") {
      overheadMetricFact = new CilkOverheadMetricFact;
    }
    else if (lush_agent == "agent-pthread") {
      overheadMetricFact = new PthreadOverheadMetricFact;
    }
    else {
      DIAG_Die("Bad value for 'lush_agent': " << lush_agent);
    }
    overheadMetricFact->make(prof);
    delete overheadMetricFact;
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


// coalesceStmts: In the CCT collected by hpcrun, leaf nodes
// (CCT::Stmt) are distinct according to instruction pointer.  After
// static structure has been overlayed, they have been coalesced by
// Struct::Stmt.  However, because structure information distinguishes
// callsites from statements, we still want to coalesce CCT::Stmts by
// line.
//
// NOTE: After static structure has been overlayed on the CCT, a
// node's child statement nodes obey the non-overlapping principle of
// a source code.
static void 
coalesceStmts(Prof::CCT::ANode* node)
{
  typedef std::map<SrcFile::ln, Prof::CCT::Stmt*> LineToStmtMap;

  if (!node) { 
    return; 
  }

  LineToStmtMap stmtMap;
  
  // For each immediate child of this node...
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::CCT::ANode* child = it.current();
    it++; // advance iterator -- it is pointing at 'child'
    
    bool inspect = (child->isLeaf() 
		    && (typeid(*child) == typeid(Prof::CCT::Stmt)));

    if (inspect) {
      // This child is a leaf. Test for duplicate source line info.
      Prof::CCT::Stmt* c = dynamic_cast<Prof::CCT::Stmt*>(child);
      SrcFile::ln line = c->begLine();
      LineToStmtMap::iterator it = stmtMap.find(line);
      if (it != stmtMap.end()) {
	// found -- we have a duplicate
	Prof::CCT::Stmt* c1 = (*it).second;
	c1->merge_me(*c);
	
	// remove 'child' from tree
	child->unlink();
	delete child;
	// NOTE: could clear Prof::CallPath::Profile::StructMetricIdFlg
      }
      else { 
	// no entry found -- add
	stmtMap.insert(std::make_pair(line, c));
      }
    }
    else if (!child->isLeaf()) {
      // Recur
      coalesceStmts(child);
    }
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


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {

void
makeDatabase(Prof::CallPath::Profile& prof, const Analysis::Args& args)
{
  // prepare output directory (N.B.: chooses a unique name!)
  string db_dir = args.db_dir; // make copy
  std::pair<string, bool> ret = FileUtil::mkdirUnique(db_dir);
  db_dir = RealPath(ret.first.c_str());  // exits on failure...
    
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

  int oFlags = CCT::Tree::OFlg_LeafMetricsOnly;
  if (!prettyPrint) { 
    oFlags |= CCT::Tree::OFlg_Compressed;
  }
  DIAG_If(5) {
    oFlags |= CCT::Tree::OFlg_Debug;
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
  prof.writeXML_hdr(os, oFlags);
  os << "  <Info/>\n";
  os << "</SecHeader>\n";
  os.flush();

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << "<SecCallPathProfileData>\n";
  prof.cct()->writeXML(os, oFlags);
  os << "</SecCallPathProfileData>\n";

  os << "</SecCallPathProfile>\n";
  os << "</HPCToolkitExperiment>\n";
  os.flush();

}

} // namespace CallPath

} // namespace Analysis

//***************************************************************************

