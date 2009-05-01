// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
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

#include "CallPath.hpp"
#include "Util.hpp"

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations ***************************

#define DBG_LUSH 0

typedef std::set<Prof::CCT::ANode*> CCTANodeSet;

//****************************************************************************
// Dump a CSProfTree 
//****************************************************************************

namespace Analysis {

namespace CallPath {

void
write(Prof::CallPath::Profile* prof, std::ostream& os, 
      string& title, bool prettyPrint)
{
  static const char* experimentDTD =
#include <lib/xml/hpc-experiment.dtd.h>

  using namespace Prof;

  int oFlags = 0;
  if (!prettyPrint) { 
    oFlags |= CCT::Tree::OFlg_Compressed;
  }
  DIAG_If(5) {
    oFlags |= CCT::Tree::OFlg_Debug;
  }

  string name = (title.empty()) ? prof->name() : title;

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
  prof->writeXML_hdr(os, oFlags);
  os << "  <Info/>\n";
  os << "</SecHeader>\n";
  os.flush();

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << "<SecCallPathProfileData>\n";
  prof->cct()->writeXML(os, oFlags);
  os << "</SecCallPathProfileData>\n";

  os << "</SecCallPathProfile>\n";
  os << "</HPCToolkitExperiment>\n";
  os.flush();

}

} // namespace CallPath

} // namespace Analysis


//****************************************************************************
// Routines for Inferring Call Frames (based on STRUCTURE information)
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
overlayStaticStructure(Prof::CallPath::Profile* prof, Prof::CCT::ANode* node,
		       Prof::Epoch::LM* epoch_lm, 
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm);


// overlayStaticStructure: Create frames for CCT::Call and CCT::Stmt
// using a preorder walk over the CCT.
void
Analysis::CallPath::
overlayStaticStructure(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm, 
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  overlayStaticStructure(prof, cct->root(), epoch_lm, lmStrct, lm);
}


static void 
overlayStaticStructure(Prof::CallPath::Profile* prof, Prof::CCT::ANode* node, 
		       Prof::Epoch::LM* epoch_lm, 
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm)
{
  // INVARIANT: The parent of 'node' has been fully processed and
  // lives within a correctly located procedure frame.
  
  if (!node) { return; }

  bool useStruct = (!lm);

  StructToCCTMap strctToCCTMap;

  // For each immediate child of this node...
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::CCT::ANode* n = it.CurNode();
    it++; // advance iterator -- it is pointing at 'n' 
    
    // ---------------------------------------------------
    // process Prof::CCT::ADynNode nodes
    // ---------------------------------------------------
    Prof::CCT::ADynNode* n_dyn = dynamic_cast<Prof::CCT::ADynNode*>(n);
    if (n_dyn && (n_dyn->lm_id() == epoch_lm->id())) {
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
      strct->metricIncr(CallPath::Profile::StructMetricIdFlg, 1.0);

      // 2. Demand a procedure frame for 'n_dyn' and its scope within it
      Struct::ANode* scope_strct = strct->ancestor(Struct::ANode::TyLOOP,
						   Struct::ANode::TyALIEN,
						   Struct::ANode::TyPROC);
      scope_strct->metricIncr(Prof::CallPath::Profile::StructMetricIdFlg, 1.0);

      Prof::CCT::ANode* scope_frame = 
	demandScopeInFrame(n_dyn, scope_strct, strctToCCTMap);

      // 3. Link 'n' to its parent
      n->Unlink();
      n->Link(scope_frame);
    }
    
    // ---------------------------------------------------
    // recur 
    // ---------------------------------------------------
    if (!n->isLeaf()) {
      overlayStaticStructure(prof, n, epoch_lm, lmStrct, lm);
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
    Prof::Struct::Proc* procStrct = strct->AncProc();
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
  frame->Link(node->Parent());
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
    Prof::Struct::ACodeNode* n_strct = it.CurNode();

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
// Routines for normalizing the ScopeTree
//***************************************************************************

static void 
coalesceStmts(Prof::CallPath::Profile* prof);

static void 
pruneByMetrics(Prof::CallPath::Profile* prof);

static void 
lush_makeParallelOverhead(Prof::CallPath::Profile* prof);


void 
Analysis::CallPath::normalize(Prof::CallPath::Profile* prof, 
			      string lush_agent)
{
  pruneByMetrics(prof);

  coalesceStmts(prof);

  if (!lush_agent.empty()) {
    lush_makeParallelOverhead(prof);
  }
}


//***************************************************************************

static void 
coalesceStmts(Prof::CCT::ANode* node);


static void 
coalesceStmts(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  coalesceStmts(cct->root());
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
    Prof::CCT::ANode* child = it.CurNode();
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
	c1->mergeMetrics(*c);
	
	// remove 'child' from tree
	child->Unlink();
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
pruneByMetrics(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  pruneByMetrics(cct->root());
}


static void 
pruneByMetrics(Prof::CCT::ANode* node)
{
  if (!node) { return; }

  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::CCT::ANode* x = it.CurNode();
    it++; // advance iterator -- it is pointing at 'x'

    // 1. Recursively do any trimming for this tree's children
    pruneByMetrics(x);

    // 2. Trim this node if necessary
    bool isTy = (typeid(*x) == typeid(Prof::CCT::ProcFrm) || 
		 typeid(*x) == typeid(Prof::CCT::Loop));
    if (x->isLeaf() && isTy) {
      x->Unlink(); // unlink 'x' from tree
      DIAG_DevMsgIf(0, "pruneByMetrics: " << hex << x << dec << " (sid: " << x->structureId() << ")");
      delete x;
    }
    else {
      // We are keeping the node -- set the static structure flag
      Prof::Struct::ACodeNode* strct = x->structure();
      if (strct) {
	strct->metricIncr(Prof::CallPath::Profile::StructMetricIdFlg, 1.0);
      }
    }
  }
}


//***************************************************************************
// 
//***************************************************************************

class ParallelOverhead
{
public:
  static const string s_tag;

  static inline bool 
  is_overhead(Prof::CCT::ProcFrm* x)
  {
    const string& x_fnm = x->fileName();
    if (x_fnm.length() >= s_tag.length()) {
      size_t tag_beg = x_fnm.length() - s_tag.length();
      return (x_fnm.compare(tag_beg, s_tag.length(), s_tag) == 0);
    }
    return false;
  }

  static inline bool 
  is_metric_src(Prof::SampledMetricDesc* mdesc)
  {
    const string& nm = mdesc->name();
    return ((nm.find("PAPI_TOT_CYC") == 0) || (nm.find("WALLCLOCK") == 0));
  }

  static void
  convertToWorkMetric(Prof::SampledMetricDesc* mdesc)
  {
    const string& nm = mdesc->name();
    if (nm.find("PAPI_TOT_CYC") == 0) {
      mdesc->name("work (cyc)");
    }
    else if (nm.find("WALLCLOCK") == 0) {
      mdesc->name("work (ms)");
    }
    else {
      DIAG_Die(DIAG_Unimplemented);
    }
  }

};

const string ParallelOverhead::s_tag = "lush:parallel-overhead";


//***************************************************************************

static void 
lush_makeParallelOverhead(Prof::CCT::ANode* node, 
			  const std::vector<uint>& m_src, 
			  const std::vector<uint>& m_dst, 
			  bool is_overhead_ctxt);


// Assumes: metrics are still only at leaves (CCT::Stmt)
static void 
lush_makeParallelOverhead(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }

  // ------------------------------------------------------------
  // Create parallel overhead metric descriptor
  // Create mapping from source metrics to overhead metrics
  // ------------------------------------------------------------
  std::vector<uint> metric_src;
  std::vector<uint> metric_dst;
  
  uint numMetrics_orig = prof->numMetrics();
  for (uint m_id = 0; m_id < numMetrics_orig; ++m_id) {
    Prof::SampledMetricDesc* m_desc = prof->metric(m_id);
    if (ParallelOverhead::is_metric_src(m_desc)) {
      ParallelOverhead::convertToWorkMetric(m_desc);
      metric_src.push_back(m_id);

      Prof::SampledMetricDesc* m_new = 
	new Prof::SampledMetricDesc("overhead", "parallel overhead", 
				    m_desc->period());
      uint m_new_id = prof->addMetric(m_new);
      DIAG_Assert(m_new_id >= numMetrics_orig, "Currently, we assume new metrics are added at the end of the metric vector.");
      metric_dst.push_back(m_new_id);
    }
  }

  // ------------------------------------------------------------
  // Create space for metric values
  // ------------------------------------------------------------
  uint n_new_metrics = metric_dst.size();

  for (Prof::CCT::ANodeIterator it(cct->root()); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.CurNode();
    Prof::CCT::ADynNode* x_dyn = dynamic_cast<Prof::CCT::ADynNode*>(x);
    if (x_dyn) {
      x_dyn->expandMetrics_after(n_new_metrics);
    }
  }

  lush_makeParallelOverhead(cct->root(), metric_src, metric_dst, false);
}


static void 
lush_makeParallelOverhead(Prof::CCT::ANode* node, 
			  const std::vector<uint>& m_src, 
			  const std::vector<uint>& m_dst, 
			  bool isOverheadCtxt)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit CCT::Stmt nodes (Assumes metrics are only at leaves)
  // ------------------------------------------------------------
  if (isOverheadCtxt && (typeid(*node) == typeid(Prof::CCT::Stmt))) {
    Prof::CCT::Stmt* stmt = dynamic_cast<Prof::CCT::Stmt*>(node);
    for (uint i = 0; i < m_src.size(); ++i) {
      uint src_idx = m_src[i];
      uint dst_idx = m_dst[i];
      hpcfile_metric_data_t mval = stmt->metric(src_idx);
      
      stmt->metricDecr(src_idx, mval);
      stmt->metricIncr(dst_idx, mval);
    }
  }

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------
  
  // Note: once set, isOverheadCtxt should remain true for all descendents
  bool isOverheadCtxt_nxt = isOverheadCtxt;
  if (!isOverheadCtxt && typeid(*node) == typeid(Prof::CCT::ProcFrm)) {
    Prof::CCT::ProcFrm* x = dynamic_cast<Prof::CCT::ProcFrm*>(node);
    isOverheadCtxt_nxt = ParallelOverhead::is_overhead(x);
  }

  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.CurNode();
    lush_makeParallelOverhead(x, m_src, m_dst, isOverheadCtxt_nxt);
  }
}

//***************************************************************************
