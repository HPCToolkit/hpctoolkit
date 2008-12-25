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
write(Prof::CallPath::Profile* prof, std::ostream& os, bool prettyPrint)
{
  static const char* experimentDTD =
#include <lib/xml/hpc-experiment.dtd.h>

  using namespace Prof;

  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE hpc-experiment [\n" << experimentDTD << "]>" << std::endl;
  os << "<HPCToolkitExperiment version=\"2.0\">\n";
  os << "<Header n" << MakeAttrStr(prof->name()) << ">\n";
  os << "  <Info/>\n";
  os << "</Header>\n";

  os << "<SecCallPathProfile i=\"0\" n" << MakeAttrStr(prof->name()) << ">\n";

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  os << "<SecHeader>\n";
  prof->writeXML_hdr(os);
  os << "  <Info/>\n";
  os << "</SecHeader>\n";
  os.flush();

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  int dumpFlags = (CCT::Tree::XML_TRUE); // CCT::Tree::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= CCT::Tree::COMPRESSED_OUTPUT; }

  os << "<SecCallPathProfileData>\n";
  prof->cct()->writeXML(os, dumpFlags);
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

typedef std::map<Prof::Struct::ACodeNode*, Prof::CCT::ProcFrm*> ACodeNodeToProcFrameMap;


typedef std::pair<Prof::CCT::ProcFrm*, 
		  Prof::Struct::ACodeNode*> ProcFrameAndLoop;
inline bool 
operator<(const ProcFrameAndLoop& x, const ProcFrameAndLoop& y) 
{
  return ((x.first < y.first) || 
	  ((x.first == y.first) && (x.second < y.second)));
}

typedef std::map<ProcFrameAndLoop, Prof::CCT::Loop*> ProcFrameAndLoopToCSLoopMap;




static Prof::CCT::ProcFrm*
demandProcFrame(Prof::CCT::IDynNode* node,
		Prof::Struct::ACodeNode* pctxtStrct,
		ACodeNodeToProcFrameMap& frameMap,
		ProcFrameAndLoopToCSLoopMap& loopMap);

static void
makeProcFrame(Prof::CCT::IDynNode* node, Prof::Struct::Proc* proc, 
	      ACodeNodeToProcFrameMap& frameMap,
	      ProcFrameAndLoopToCSLoopMap& loopMap);

static void
loopifyFrame(Prof::CCT::ProcFrm* frame, 
	     Prof::Struct::ACodeNode* ctxtScope,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


static void 
overlayStaticStructure(Prof::CallPath::Profile* prof, Prof::CCT::ANode* node,
		       Prof::Epoch::LM* epoch_lm, 
		       Prof::Struct::LM* lmStrct, binutils::LM* lm);


// overlayStaticStructure: Effectively create equivalence classes of frames
// for all the return addresses found under.
//
void
Analysis::CallPath::
overlayStaticStructure(Prof::CallPath::Profile* prof, 
		       Prof::Epoch::LM* epoch_lm, 
		       Prof::Struct::LM* lmStrct, binutils::LM* lm)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  overlayStaticStructure(prof, cct->root(), epoch_lm, lmStrct, lm);
}


static void 
overlayStaticStructure(Prof::CallPath::Profile* prof, Prof::CCT::ANode* node, 
		       Prof::Epoch::LM* epoch_lm, 
		       Prof::Struct::LM* lmStrct, binutils::LM* lm)
{
  // INVARIANT: The parent of 'node' has been fully processed and
  // lives within a correctly located procedure frame.
  
  if (!node) { return; }

  bool useStruct = (!lm);

  ACodeNodeToProcFrameMap frameMap;
  ProcFrameAndLoopToCSLoopMap loopMap;

  // For each immediate child of this node...
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::CCT::ANode* n = it.CurNode();
    
    it++; // advance iterator -- it is pointing at 'n' 
    
    // ---------------------------------------------------
    // process this node
    // ---------------------------------------------------
    Prof::CCT::IDynNode* n_dyn = dynamic_cast<Prof::CCT::IDynNode*>(n);
    if (n_dyn && (n_dyn->lm_id() == epoch_lm->id())) {
      VMA ip_ur = n_dyn->ip();
      DIAG_DevIf(50) {
	Prof::CCT::Call* p = node->ancestorCall();
	DIAG_DevMsg(0, "overlayStaticStructure: " << hex << ((p) ? p->ip() : 0) << " --> " << ip_ur << dec);
      }

      // 1. Add symbolic information to 'n'
      using namespace Prof;
      
      Struct::ACodeNode* strct = 
	Analysis::Util::demandStructure(ip_ur, lmStrct, lm, useStruct);

      Struct::ACodeNode* pctxtStrct = strct->ancestorProcCtxt();

      Struct::ANode* t = strct->Ancestor(Struct::ANode::TyLOOP, 
					 Struct::ANode::TyALIEN);
                                      // FIXME: include PROC (for nested procs)
      Struct::Loop* loopStrct = dynamic_cast<Struct::Loop*>(t);

      n->structure(strct);
      strct->SetPerfData(CallPath::Profile::StructMetricIdFlg, 1.0);

      // 2. Demand a procedure frame for 'n', complete with loop structure
      Prof::CCT::ProcFrm* frame = demandProcFrame(n_dyn, pctxtStrct, 
						  frameMap, loopMap);
      
      // 3. Determine parent context for 'n': the procedure frame
      // itself or loop within
      Prof::CCT::ANode* newParent = frame;
      if (loopStrct) {
	ProcFrameAndLoop toFind(frame, loopStrct);
	ProcFrameAndLoopToCSLoopMap::iterator it = loopMap.find(toFind);
	DIAG_Assert(it != loopMap.end(), "Cannot find corresponding loop structure:\n" << loopStrct->toStringXML());
	newParent = (*it).second;
      }
      
      // 4. Link 'n' to its parent
      n->Unlink();
      n->Link(newParent);
    }
    
    // recur 
    if (!n->IsLeaf()) {
      overlayStaticStructure(prof, n, epoch_lm, lmStrct, lm);
    }
  }
}


// demandProcFrame: Find or create a procedure frame for 'node' given
// its corresponding procedure context 'pctxtStrct' (Struct::Proc or
// Struct::Alien)
// 
// Assumes that symbolic information has been added to node.
static Prof::CCT::ProcFrm*
demandProcFrame(Prof::CCT::IDynNode* node,
		Prof::Struct::ACodeNode* pctxtStrct,
		ACodeNodeToProcFrameMap& frameMap,
		ProcFrameAndLoopToCSLoopMap& loopMap)
{
  Prof::CCT::ProcFrm* frame = NULL;
  
  ACodeNodeToProcFrameMap::iterator it = frameMap.find(pctxtStrct);
  if (it != frameMap.end()) {
    frame = (*it).second;
  }
  else {
    // Find and create the frame.
    // INVARIANT: 'node' has symbolic information

    Prof::Struct::Proc* procStrct = pctxtStrct->AncProc();
    makeProcFrame(node, procStrct, frameMap, loopMap);

    // frame should now be in map
    ACodeNodeToProcFrameMap::iterator it = frameMap.find(pctxtStrct);
    DIAG_Assert(it != frameMap.end(), "");
    frame = (*it).second;
  }
  
  return frame;
}


static void 
makeProcFrame(Prof::CCT::IDynNode* node, Prof::Struct::Proc* procStrct,
	      ACodeNodeToProcFrameMap& frameMap,
	      ProcFrameAndLoopToCSLoopMap& loopMap)
{
  Prof::CCT::ProcFrm* frame = new Prof::CCT::ProcFrm(NULL);

  frame->structure(procStrct);
  procStrct->SetPerfData(Prof::CallPath::Profile::StructMetricIdFlg, 1.0);

  frame->Link(node->proxy()->Parent());
  frameMap.insert(std::make_pair(procStrct, frame));

  loopifyFrame(frame, procStrct, frameMap, loopMap);
}


static void
loopifyFrame(Prof::CCT::ANode* mirrorNode, Prof::Struct::ACodeNode* node,
	     Prof::CCT::ProcFrm* frame,
	     Prof::CCT::Loop* enclLoop,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


// Given a procedure frame 'frame' and its associated context scope
// 'ctxtScope' (Struct::Proc or Struct::Alien), mirror ctxtScope's loop and
// context structure and add entries to 'frameMap' and 'loopMap.'
static void
loopifyFrame(Prof::CCT::ProcFrm* frame, 
	     Prof::Struct::ACodeNode* ctxtScope,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap)
{
  loopifyFrame(frame, ctxtScope, frame, NULL, frameMap, loopMap);
}


// 'frame' is the enclosing frame
// 'loop' is the enclosing loop
static void
loopifyFrame(Prof::CCT::ANode* mirrorNode, 
	     Prof::Struct::ACodeNode* node,
	     Prof::CCT::ProcFrm* frame,
	     Prof::CCT::Loop* enclLoop,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap)
{
  for (Prof::Struct::ACodeNodeChildIterator it(node); it.Current(); ++it) {
    Prof::Struct::ACodeNode* n = it.CurACodeNode();

    // Done: if we reach the natural base case or embedded proceedure
    if (n->IsLeaf() || n->Type() == Prof::Struct::ANode::TyPROC) {
      continue;
    }

    // - Flatten nested alien frames descending from a loop
    // - Presume that alien frames derive from callsites in the parent
    // frame, but flatten any nesting.

    Prof::CCT::ANode* mirrorRoot = mirrorNode;
    Prof::CCT::ProcFrm* nxt_frame = frame;
    Prof::CCT::Loop* nxt_enclLoop = enclLoop;

    if (n->Type() == Prof::Struct::ANode::TyLOOP) {
      // loops are always children of the current root (loop or frame)
      Prof::CCT::Loop* lp = new Prof::CCT::Loop(mirrorNode, n);
      loopMap.insert(std::make_pair(ProcFrameAndLoop(frame, n), lp));
      DIAG_DevMsgIf(0, hex << "(" << frame << " " << n << ") -> (" << lp << ")" << dec);

      mirrorRoot = lp;
      nxt_enclLoop = lp;
    }
    else if (n->Type() == Prof::Struct::ANode::TyALIEN) {
      Prof::CCT::ProcFrm* fr = new Prof::CCT::ProcFrm(NULL);

      fr->structure(n);
      n->SetPerfData(Prof::CallPath::Profile::StructMetricIdFlg, 1.0);

      frameMap.insert(std::make_pair(n, fr));
      DIAG_DevMsgIf(0, hex << fr->procName() << " [" << n << " -> " << fr << "]" << dec);
      
      if (enclLoop) {
	fr->Link(enclLoop);
      }
      else {
	fr->Link(frame);
      }

      mirrorRoot = fr;
      nxt_frame = fr;
    }
    
    loopifyFrame(mirrorRoot, n, nxt_frame, nxt_enclLoop, frameMap, loopMap);
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
lush_cilkNormalize(Prof::CallPath::Profile* prof);

static void 
lush_makeParallelOverhead(Prof::CallPath::Profile* prof);


bool 
Analysis::CallPath::normalize(Prof::CallPath::Profile* prof, 
			      string lush_agent)
{
  coalesceStmts(prof);

  if (!lush_agent.empty()) {
    lush_cilkNormalize(prof);
    lush_makeParallelOverhead(prof);
  }

  pruneByMetrics(prof);

  return (true);
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


// coalesceStmts: In the CCT collected by hpcrun, leaf nodes are
// distinct according to instruction pointer.  We wish to coalesce all
// nodes belonging to the same line.  One way of doing this 
//
// NOTE: We assume that prior normalizations have been applied.  This
// means that statement nodes have already been separated by procedure
// frame (including Alien frames).  This means that all of a node's
// child statement nodes obey the non-overlapping principle of a
// source code.
static void 
coalesceStmts(Prof::CCT::ANode* node)
{
  // Since hpcstruct distinguishes callsites from statements, use
  // SrcFile::ln instead of Prof::Struct::ACodeNode*.
  typedef std::map<SrcFile::ln, Prof::CCT::Stmt*> StructToStmtMap;

  if (!node) { 
    return; 
  }

  StructToStmtMap stmtMap;
  
  // For each immediate child of this node...
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    Prof::CCT::ANode* child = it.CurNode();
    it++; // advance iterator -- it is pointing at 'child'
    
    bool inspect = (child->IsLeaf() && (child->type() == Prof::CCT::ANode::TyStmt));

    if (inspect) {
      // This child is a leaf. Test for duplicate source line info.
      Prof::CCT::Stmt* c = dynamic_cast<Prof::CCT::Stmt*>(child);
      SrcFile::ln line = c->begLine();
      StructToStmtMap::iterator it = stmtMap.find(line);
      if (it != stmtMap.end()) {
	// found -- we have a duplicate
	Prof::CCT::Stmt* c1 = (*it).second;
	c1->mergeMetrics(*c);
	
	// remove 'child' from tree
	child->Unlink();
	delete child;
      } 
      else { 
	// no entry found -- add
	stmtMap.insert(std::make_pair(line, c));
      }
    }
    else if (!child->IsLeaf()) {
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
    bool isTy = (x->type() == Prof::CCT::ANode::TyProcFrm || 
		 x->type() == Prof::CCT::ANode::TyLoop);
    if (x->IsLeaf() && isTy) {
      x->Unlink(); // unlink 'x' from tree
      delete x;
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


class CilkCanonicalizer
{
public:
  static const string s_slow_pfx;
  static const string s_slow_sfx;

  static string 
  normalizeName(Prof::CCT::ProcFrm* x) 
  {
    //string x_nm = x.substr(pfx, mrk_x - 1 - pfx);
    const string& x_nm = x->procName();
    
    string xtra = "";

    size_t mrk_x = x_nm.find(ParallelOverhead::s_tag);
    if (mrk_x == string::npos && ParallelOverhead::is_overhead(x)) {
      xtra = "@" + ParallelOverhead::s_tag;
    }
    
    // if '_cilk' is a prefix of x_nm, normalize
    if (x_nm == "_cilk_cilk_main_import") {
      // 1. _cilk_cilk_main_import --> invoke_main_slow
      return "invoke_main_slow" + xtra;
    }
    else if (is_slow_pfx(x_nm)) {
      // 2. _cilk_x_slow --> x [_cilk_cilk_main_slow --> cilk_main]
      int len = x_nm.length() - s_slow_pfx.length() - s_slow_sfx.length();
      string x_basenm = x_nm.substr(s_slow_pfx.length(), len);
      return x_basenm + xtra;
    }
    else {
      return x_nm + xtra;
    }
  }

  static inline bool 
  is_slow_pfx(const string& x)
  {
    return (x.compare(0, s_slow_pfx.length(), s_slow_pfx) == 0);
  }

  static inline bool 
  is_slow_proc(Prof::CCT::ProcFrm* x)
  {
    const string& x_procnm = x->procName();
    return is_slow_pfx(x_procnm);
  }

};

const string CilkCanonicalizer::s_slow_pfx = "_cilk_";
const string CilkCanonicalizer::s_slow_sfx = "_slow";



//***************************************************************************
// lush_cilkNormalize
//***************************************************************************


static void 
lush_cilkNormalize(Prof::CCT::ANode* node);

static void 
lush_cilkNormalizeByFrame(Prof::CCT::ANode* node);


// Notes: Match frames, and use those matches to merge callsites.

static void 
lush_cilkNormalize(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }

  lush_cilkNormalize(cct->root());
}


typedef std::map<string, Prof::CCT::ANode*> CilkMergeMap;


// - Preorder Visit
static void 
lush_cilkNormalize(Prof::CCT::ANode* node)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit node
  // ------------------------------------------------------------
  Prof::CCT::ProcFrm* proc = dynamic_cast<Prof::CCT::ProcFrm*>(node);
  if (proc) {
    lush_cilkNormalizeByFrame(node);

    // normalize routines not normalized by merging...
    proc->procNameXXX(CilkCanonicalizer::normalizeName(proc));
  }

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.CurNode();
    lush_cilkNormalize(x);
  }
}


static void 
lush_cilkNormalizeByFrame(Prof::CCT::ANode* node)
{
  DIAG_MsgIf(DBG_LUSH, "====> (" << node << ") " << node->codeName());
  
  // ------------------------------------------------------------
  // Gather (grand) children frames (skip one level)
  // ------------------------------------------------------------
  CCTANodeSet frameSet;
  
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* child = it.CurNode();

    for (Prof::CCT::ANodeChildIterator it(child); it.Current(); ++it) {
      Prof::CCT::ProcFrm* x = dynamic_cast<Prof::CCT::ProcFrm*>(it.CurNode());
      if (x) {
	frameSet.insert(x);
      }
    }
  }


  // ------------------------------------------------------------
  // Merge against (grand) children frames
  // ------------------------------------------------------------

  CilkMergeMap mergeMap;

  for (CCTANodeSet::iterator it = frameSet.begin(); it != frameSet.end(); ++it) {
    
    Prof::CCT::ProcFrm* x = dynamic_cast<Prof::CCT::ProcFrm*>(*it);

    string x_nm = CilkCanonicalizer::normalizeName(x);
    DIAG_MsgIf(DBG_LUSH, "\tins: " << x->codeName() << "\n" 
	       << "\tas : " << x_nm << " --> " << x);
    CilkMergeMap::iterator it = mergeMap.find(x_nm);
    if (it != mergeMap.end()) {
      // found -- we have a duplicate
      Prof::CCT::ProcFrm* y = dynamic_cast<Prof::CCT::ProcFrm*>((*it).second);

      // keep the version without the "_cilk_" prefix
      Prof::CCT::ANode* tokeep = y, *todel = x;
      
      if (CilkCanonicalizer::is_slow_proc(y)) {
        tokeep = x;
	todel = y;
	mergeMap[x_nm] = x;
      }
      
      Prof::CCT::ANode* par_tokeep = tokeep->Parent();
      Prof::CCT::ANode* par_todel  = todel->Parent();

      DIAG_MsgIf(DBG_LUSH, "\tkeep (" << tokeep << "): " << tokeep->codeName() 
		 << "\n" << "\tdel  (" << todel  << "): " << todel->codeName());

      // merge parents, if necessary
      if (par_tokeep != par_todel) {
	par_tokeep->merge_node(par_todel); // unlinks and deletes par_todel
      }

      // merge parents, if necessary
      tokeep->merge_node(todel); // unlinks and deletes todel
    }
    else { 
      mergeMap.insert(std::make_pair(x_nm, x));
    }
  }
}


//***************************************************************************

static void 
lush_makeParallelOverhead(Prof::CCT::ANode* node, 
			  const std::vector<uint>& m_src, 
			  const std::vector<uint>& m_dst, 
			  bool is_overhead_ctxt);



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
    Prof::CCT::IDynNode* x_dyn = dynamic_cast<Prof::CCT::IDynNode*>(x);
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
			  bool is_overhead_ctxt)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit node
  // ------------------------------------------------------------
  if (is_overhead_ctxt) {
    for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
      Prof::CCT::ANode* x = it.CurNode();
      Prof::CCT::IDynNode* x_dyn = dynamic_cast<Prof::CCT::IDynNode*>(x);
      if (x_dyn) {
	for (uint i = 0; i < m_src.size(); ++i) {
	  uint src_idx = m_src[i];
	  uint dst_idx = m_dst[i];
	  hpcfile_metric_data_t mval = x_dyn->metric(src_idx);

	  x_dyn->metricDecr(src_idx, mval);
	  x_dyn->metricIncr(dst_idx, mval);
	}
      }
    }
  }

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------

  if (node->type() == Prof::CCT::ANode::TyProcFrm) {
    Prof::CCT::ProcFrm* x = dynamic_cast<Prof::CCT::ProcFrm*>(node);
    is_overhead_ctxt = ParallelOverhead::is_overhead(x);
  }

  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.CurNode();
    lush_makeParallelOverhead(x, m_src, m_dst, is_overhead_ctxt);
  }
}

//***************************************************************************
