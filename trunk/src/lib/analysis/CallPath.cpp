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

#define DBG_NORM_PROC_FRAME 0
#define DBG_LUSH_PROC_FRAME 0


typedef std::set<Prof::CSProfCodeNode*> CSProfCodeNodeSet;

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
  os << "  <MetricTable>\n";
  uint n_metrics = prof->numMetrics();
  for (uint i = 0; i < n_metrics; i++) {
    const SampledMetricDesc* metric = prof->metric(i);
    os << "    <Metric i" << MakeAttrNum(i) 
       << " n" << MakeAttrStr(metric->name()) << ">\n";
    os << "      <Info>" 
       << "<NV n=\"period\" v" << MakeAttrNum(metric->period()) << "/>"
       << "<NV n=\"flags\" v" << MakeAttrNum(metric->flags(), 16) << "/>"
       << "</Info>\n";
    os << "    </Metric>\n";
  }
  os << "  </MetricTable>\n";
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

typedef std::map<Prof::Struct::ACodeNode*, 
		 Prof::CSProfProcedureFrameNode*> ACodeNodeToProcFrameMap;


typedef std::pair<Prof::CSProfProcedureFrameNode*, 
		  Prof::Struct::ACodeNode*> ProcFrameAndLoop;
inline bool 
operator<(const ProcFrameAndLoop& x, const ProcFrameAndLoop& y) 
{
  return ((x.first < y.first) || 
	  ((x.first == y.first) && (x.second < y.second)));
}

typedef std::map<ProcFrameAndLoop, Prof::CSProfLoopNode*> ProcFrameAndLoopToCSLoopMap;




void
addSymbolicInfo(Prof::CSProfCodeNode* n, Prof::IDynNode* n_dyn,
		Prof::Struct::LM* lmStrct, 
		Prof::Struct::ACodeNode* callingCtxt, 
		Prof::Struct::ACodeNode* scope);

Prof::CSProfProcedureFrameNode*
demandProcFrame(Prof::IDynNode* node,
		Prof::Struct::LM* lmStrct, 
		Prof::Struct::ACodeNode* callingCtxt,
		Prof::Struct::Loop* loop,
		VMA curr_ip,
		ACodeNodeToProcFrameMap& frameMap,
		ProcFrameAndLoopToCSLoopMap& loopMap);

void
makeProcFrame(Prof::Struct::Proc* proc, Prof::IDynNode* dyn_node,
	      ACodeNodeToProcFrameMap& frameMap,
	      ProcFrameAndLoopToCSLoopMap& loopMap);

void
loopifyFrame(Prof::CSProfProcedureFrameNode* frame, 
	     Prof::Struct::ACodeNode* ctxtScope,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, 
		Prof::Struct::LM* lmStrct, binutils::LM* lm);


// inferCallFrames: Effectively create equivalence classes of frames
// for all the return addresses found under.
//
void
Analysis::CallPath::
inferCallFrames(Prof::CallPath::Profile* prof, Prof::Epoch::LM* epoch_lm, 
		Prof::Struct::LM* lmStrct, binutils::LM* lm)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  inferCallFrames(prof, cct->root(), epoch_lm, lmStrct, lm);
}


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
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
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* n = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 
    
    // ---------------------------------------------------
    // process this node
    // ---------------------------------------------------
    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn && (n_dyn->lm_id() == epoch_lm->id())) {
      VMA ip_ur = n_dyn->ip();
      DIAG_DevIf(50) {
	Prof::CSProfCallSiteNode* p = node->AncestorCallSite();
	DIAG_DevMsg(0, "inferCallFrames: " << hex << ((p) ? p->ip() : 0) << " --> " << ip_ur << dec);
      }

      // 1. Find associated structure
      using namespace Prof;
      
      Struct::ACodeNode* strct = 
	Analysis::Util::demandStructure(ip_ur, lmStrct, lm, useStruct);

      Struct::ACodeNode* ctxt = strct->AncCallingCtxt();
      
       // FIXME: should include PROC (for nested procs)
      Struct::ANode* t = strct->Ancestor(Struct::ANode::TyLOOP, 
					 Struct::ANode::TyALIEN);
      Struct::Loop* loop = dynamic_cast<Struct::Loop*>(t);

      // 2. Add symbolic information to 'n'
      addSymbolicInfo(n, n_dyn, lmStrct, ctxt, strct);

      // 3. Demand a procedure frame for 'n'.
      Prof::CSProfProcedureFrameNode* frame = 
	demandProcFrame(n_dyn, lmStrct, ctxt, loop, ip_ur, frameMap, loopMap);
      
      // 4. Find new parent context for 'n': the frame itself or a
      // loop within the frame
      Prof::CSProfCodeNode* newParent = frame;
      if (loop) {
	ProcFrameAndLoop toFind(frame, loop);
	ProcFrameAndLoopToCSLoopMap::iterator it = loopMap.find(toFind);
	DIAG_Assert(it != loopMap.end(), "Cannot find corresponding loop structure:\n" << loop->toStringXML());
	newParent = (*it).second;
      }
      
      // 5. Link 'n' to its frame
      n->Unlink();
      n->Link(newParent);
    }
    
    // recur 
    if (!n->IsLeaf()) {
      inferCallFrames(prof, n, epoch_lm, lmStrct, lm);
    }
  }
}


// demandProcFrame: Find or create a procedure frame for 'node'
// given it's corresponding load module structure (lmStrct), calling
// context structure (callingCtxt, a Struct::Proc or Struct::Alien) and
// statement-type scope.
// 
// Assumes that symbolic information has been added to node.
Prof::CSProfProcedureFrameNode*
demandProcFrame(Prof::IDynNode* node,
		Prof::Struct::LM* lmStrct, 
		Prof::Struct::ACodeNode* callingCtxt,
		Prof::Struct::Loop* loop,
		VMA ip_ur,
		ACodeNodeToProcFrameMap& frameMap,
		ProcFrameAndLoopToCSLoopMap& loopMap)
{
  Prof::CSProfProcedureFrameNode* frame = NULL;

  Prof::Struct::ACodeNode* toFind = (callingCtxt) ? callingCtxt : lmStrct;
  
  ACodeNodeToProcFrameMap::iterator it = frameMap.find(toFind);
  if (it != frameMap.end()) {
    frame = (*it).second;
  }
  else {
    // INVARIANT: 'node' has symbolic information

    // Find and create the frame.
    if (callingCtxt) {
      Prof::Struct::Proc* proc = callingCtxt->AncProc();
      makeProcFrame(proc, node, frameMap, loopMap);

      // frame should now be in map
      ACodeNodeToProcFrameMap::iterator it = frameMap.find(toFind);
      DIAG_Assert(it != frameMap.end(), "");
      frame = (*it).second;
    }
    else {
      frame = new Prof::CSProfProcedureFrameNode(NULL);
      addSymbolicInfo(frame, node, lmStrct, NULL, NULL);
      frame->Link(node->proxy()->Parent());

      string nm = "<unknown(struct)>:contains" + StrUtil::toStr(ip_ur, 16);
      frame->SetProc(nm);
      
      frameMap.insert(std::make_pair(toFind, frame));
    }
  }
  
  return frame;
}


void 
makeProcFrame(Prof::Struct::Proc* proc, Prof::IDynNode* node_dyn,
	      ACodeNodeToProcFrameMap& frameMap,
	      ProcFrameAndLoopToCSLoopMap& loopMap)
{
  Prof::CSProfProcedureFrameNode* frame;
  
  frame = new Prof::CSProfProcedureFrameNode(NULL);
  addSymbolicInfo(frame, node_dyn, NULL, proc, proc);
  frame->Link(node_dyn->proxy()->Parent());
  frameMap.insert(std::make_pair(proc, frame));

  loopifyFrame(frame, proc, frameMap, loopMap);
}



void
loopifyFrame(Prof::CSProfCodeNode* mirrorNode, Prof::Struct::ACodeNode* node,
	     Prof::CSProfProcedureFrameNode* frame,
	     Prof::CSProfLoopNode* enclLoop,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


// Given a procedure frame 'frame' and its associated context scope
// 'ctxtScope' (Struct::Proc or Struct::Alien), mirror ctxtScope's loop and
// context structure and add entries to 'frameMap' and 'loopMap.'
void
loopifyFrame(Prof::CSProfProcedureFrameNode* frame, 
	     Prof::Struct::ACodeNode* ctxtScope,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap)
{
  loopifyFrame(frame, ctxtScope, frame, NULL, frameMap, loopMap);
}


// 'frame' is the enclosing frame
// 'loop' is the enclosing loop
void
loopifyFrame(Prof::CSProfCodeNode* mirrorNode, 
	     Prof::Struct::ACodeNode* node,
	     Prof::CSProfProcedureFrameNode* frame,
	     Prof::CSProfLoopNode* enclLoop,
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

    Prof::CSProfCodeNode* mirrorRoot = mirrorNode;
    Prof::CSProfProcedureFrameNode* nxt_frame = frame;
    Prof::CSProfLoopNode* nxt_enclLoop = enclLoop;

    if (n->Type() == Prof::Struct::ANode::TyLOOP) {
      // loops are always children of the current root (loop or frame)
      Prof::CSProfLoopNode* lp = 
	new Prof::CSProfLoopNode(mirrorNode, n->begLine(), n->endLine(), 
				 n->id());
      loopMap.insert(std::make_pair(ProcFrameAndLoop(frame, n), lp));
      DIAG_DevMsgIf(0, hex << "(" << frame << " " << n << ") -> (" << lp << ")" << dec);

      mirrorRoot = lp;
      nxt_enclLoop = lp;
    }
    else if (n->Type() == Prof::Struct::ANode::TyALIEN) {
      Prof::CSProfProcedureFrameNode* fr = new Prof::CSProfProcedureFrameNode(NULL);
      addSymbolicInfo(fr, NULL, NULL, n, n);
      fr->isAlien() = true;
      frameMap.insert(std::make_pair(n, fr));
      DIAG_DevMsgIf(0, hex << fr->GetProc() << " [" << n << " -> " << fr << "]" << dec);
      
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

void 
addSymbolicInfo(Prof::CSProfCodeNode* n, Prof::IDynNode* n_dyn,
		Prof::Struct::LM* lmStrct, 
		Prof::Struct::ACodeNode* callingCtxt, 
		Prof::Struct::ACodeNode* scope)
{
  if (!n) {
    return;
  }

  //Diag_Assert(logic::equiv(callingCtxt == NULL, scope == NULL));
  if (scope) {
    bool isAlien = (callingCtxt->Type() == Prof::Struct::ANode::TyALIEN);
    const std::string& fnm = (isAlien) ? 
      dynamic_cast<Prof::Struct::Alien*>(callingCtxt)->fileName() :
      callingCtxt->AncFile()->name();

    n->SetFile(fnm);
#if (DBG_LUSH_PROC_FRAME)
    std::string nm = callingCtxt->name();
    if (n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
      nm += " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
	+ ", " + n_dyn->lip_str() + ") [" + n_dyn->assocInfo_str() + "]";
    }
    n->SetProc(nm);
#else
    n->SetProc(callingCtxt->name());
#endif
    n->SetLine(scope->begLine());
    n->SetFileIsText(true);
    n->structureId(scope->id());
  }
  else {
    n->SetFile(lmStrct->name());
    n->SetLine(0);
    n->SetFileIsText(false);
    n->structureId(lmStrct->id());
  }
}


//***************************************************************************
// Routines for Inferring Call Frames (without STRUCTURE, based on LM)
//***************************************************************************

typedef std::map<string, Prof::CSProfProcedureFrameNode*> StringToProcFrameMap;
typedef std::map<uint, Prof::CSProfProcedureFrameNode*> UintToProcFrameMap;


// StaticStructureMgr: Manages (on demand) creation of static
// structure.  
// 
// NOTE: hpcstruct's scope ids begin at 0 and increase.
// StaticStructureMgr's scope ids begin at s_uniqueIdMax and decrease.
class StaticStructureMgr
{
public:
  StaticStructureMgr(binutils::LM* lm) : m_lm(lm) { }
  
  ~StaticStructureMgr() 
  {
    for (StringToProcFrameMap::iterator it = m_frameMap.begin(); 
	 it != m_frameMap.end(); ++it) {
      delete it->second;
    }
  }
  
  binutils::LM* 
  lm() const { return m_lm; }
  
  const Prof::CSProfProcedureFrameNode* 
  getFrame(Prof::IDynNode* n_dyn)
  {
    Prof::CSProfCodeNode* n = n_dyn->proxy();
    VMA ip_ur = n_dyn->ip();

    string key = n->GetFile() + n->GetProc();

    StringToProcFrameMap::iterator it = m_frameMap.find(key);
    Prof::CSProfProcedureFrameNode* frame;
    if (it != m_frameMap.end()) { 
      frame = (*it).second; // found
    }
    else {
      // no entry found -- add
      frame = new Prof::CSProfProcedureFrameNode(NULL);

      string func = n->GetProc();
      if (func.empty()) {
	func = "<unknown(~struct)>:contains-" + StrUtil::toStr(ip_ur, 16);
      } 
      DIAG_MsgIf(DBG_NORM_PROC_FRAME, "frame name: " << n->GetProc() 
		 << " --> " << func);
	
      SrcFile::ln begLn = 0;
      if (n->FileIsText()) {
	// determine the first line of the enclosing procedure
	m_lm->GetProcFirstLineInfo(ip_ur, n_dyn->opIndex(), begLn);
      }

      frame->SetFile(n->GetFile());
      frame->SetProc(func);
      frame->SetFileIsText(n->FileIsText());
      frame->SetLine(begLn);
      frame->structureId(getNextStructureId());

      m_frameMap.insert(std::make_pair(key, frame));
    }

    return frame;
  }

  static uint 
  minId()
  { return s_nextUniqueId + 1; }

  static uint 
  maxId()
  { return s_uniqueIdMax; }

  static uint 
  getNextStructureId() 
  { return s_nextUniqueId--; }
  
private:
  binutils::LM* m_lm; // does not own
  StringToProcFrameMap m_frameMap;

  static uint s_nextUniqueId;

  // tallent: hpcviewer (Java) requires that this be INT_MAX or LONG_MAX
  static const uint s_uniqueIdMax = INT_MAX;
};

uint StaticStructureMgr::s_nextUniqueId = StaticStructureMgr::s_uniqueIdMax;


void 
addSymbolicInfo(Prof::IDynNode* n, binutils::LM* lm);


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, StaticStructureMgr& frameMgr);

// Without STRUCTURE information, procedure frames must be naively
// inferred from the line map.
//   
//          main                     main
//         /    \        =====>        |  
//        /      \                   foo:<start line foo>
//       foo:1  foo:2                 /   \
//                                   /     \
//                                foo:1   foo:2 
void 
Analysis::CallPath::
inferCallFrames(Prof::CallPath::Profile* prof, Prof::Epoch::LM* epoch_lm, 
		binutils::LM* lm)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }

  StaticStructureMgr frameMgr(lm);

  DIAG_MsgIf(DBG_NORM_PROC_FRAME, "start normalizing same procedure children");
  inferCallFrames(prof, cct->root(), epoch_lm, frameMgr);

  DIAG_Assert(Prof::Struct::ANode::maxId() < StaticStructureMgr::minId(), 
	      "There are more than " << StaticStructureMgr::maxId() << " structure nodes!");
}


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, StaticStructureMgr& frameMgr)
{
  if (!node) { return; }

  // To determine when a callsite and leaves share the *same* frame
  UintToProcFrameMap frameMap;

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* n = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 

    // ------------------------------------------------------------
    // recur 
    // ------------------------------------------------------------
    if (!n->IsLeaf()) {
      inferCallFrames(prof, n, epoch_lm, frameMgr);
    }

    // ------------------------------------------------------------
    // process this node if within the current load module
    //   (and of the correct type: Prof::IDynNode!)
    // ------------------------------------------------------------
    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn && (n_dyn->lm_id() == epoch_lm->id())) {
      VMA ip_ur = n_dyn->ip();
      DIAG_MsgIf(DBG_NORM_PROC_FRAME, "analyzing node " << n->GetProc()
		 << hex << " " << ip_ur);
      
      addSymbolicInfo(n_dyn, frameMgr.lm());
      
      const Prof::CSProfProcedureFrameNode* uniq_frame = 
	frameMgr.getFrame(n_dyn);

      uint key = uniq_frame->structureId();
      UintToProcFrameMap::iterator it = frameMap.find(key);

      Prof::CSProfProcedureFrameNode* frame;
      if (it != frameMap.end()) { 
	frame = it->second; // found
      } 
      else {
	// no entry found -- add
	frame = new Prof::CSProfProcedureFrameNode(*uniq_frame);
	frame->Link(node);
	frameMap.insert(std::make_pair(key, frame));
      }
      
      n->Unlink();
      n->Link(frame);
    }
  }
}


//***************************************************************************

void 
addSymbolicInfo(Prof::IDynNode* n_dyn, binutils::LM* lm)
{
  if (!n_dyn) {
    return;
  }

  Prof::CSProfCodeNode* n = n_dyn->proxy();

  string func, file;
  SrcFile::ln srcLn;
  lm->GetSourceFileInfo(n_dyn->ip(), n_dyn->opIndex(), func, file, srcLn);
  func = GetBestFuncName(func);
#if (DBG_LUSH_PROC_FRAME)
  if (!func.empty() && n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
    func +=  " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
      + ", " + n_dyn->lip_str() + ") [" + n_dyn->assocInfo_str() + "]";
  }
#endif

  n->SetFile(file);
  n->SetProc(func);
  n->SetLine(srcLn);
  n->SetFileIsText(true);
  n->structureId(0); // FIXME
  
  // if file name is missing then using load module name. 
  if (file.empty() || func.empty()) {
    n->SetFile(lm->name());
    n->SetLine(0); //don't need to have line number for loadmodule
    n->SetFileIsText(false);
  }
}


//***************************************************************************
// Routines for normalizing the ScopeTree
//***************************************************************************

static void 
coalesceCallsiteLeaves(Prof::CallPath::Profile* prof);

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
  coalesceCallsiteLeaves(prof);

  if (!lush_agent.empty()) {
    lush_cilkNormalize(prof);
    lush_makeParallelOverhead(prof);
  }

  pruneByMetrics(prof);

  return (true);
}


// FIXME
// If pc values from the leaves map to the same source file info,
// coalese these leaves into one.
static void 
coalesceCallsiteLeaves(Prof::CSProfNode* node);

static void 
coalesceCallsiteLeaves(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  coalesceCallsiteLeaves(cct->root());
}


// FIXME
typedef std::map<string, Prof::CSProfStatementNode*> StringToCallSiteMap;

static void 
coalesceCallsiteLeaves(Prof::CSProfNode* node)
{
  if (!node) { return; }

  // FIXME: Use this set to determine if we have a duplicate source line
  StringToCallSiteMap sourceInfoMap;
  
  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* child = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(child, ""); // always true (FIXME)
    it++; // advance iterator -- it is pointing at 'child'
    
    bool inspect = (child->IsLeaf() 
		    && (child->GetType() == Prof::CSProfNode::STATEMENT));
    
    if (inspect) {
      // This child is a leaf. Test for duplicate source line info.
      Prof::CSProfStatementNode* c = dynamic_cast<Prof::CSProfStatementNode*>(child);
      string mykey = string(c->GetFile()) + string(c->GetProc()) 
	+ StrUtil::toStr(c->GetLine());
      
      StringToCallSiteMap::iterator it = sourceInfoMap.find(mykey);
      if (it != sourceInfoMap.end()) {
	// found -- we have a duplicate
	Prof::CSProfStatementNode* c1 = (*it).second;
	c1->mergeMetrics(*c);
	
	// remove 'child' from tree
	child->Unlink();
	delete child;
      } 
      else { 
	// no entry found -- add
	sourceInfoMap.insert(std::make_pair(mykey, c));
      }
    } 
    else if (!child->IsLeaf()) {
      // Recur:
      coalesceCallsiteLeaves(child);
    }
  } 
}


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
pruneByMetrics(Prof::CSProfNode* node);

static void 
pruneByMetrics(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  pruneByMetrics(cct->root());
}


static void 
pruneByMetrics(Prof::CSProfNode* node)
{
  if (!node) { return; }

  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfNode* x = it.CurNode();
    it++; // advance iterator -- it is pointing at 'x'

    // 1. Recursively do any trimming for this tree's children
    pruneByMetrics(x);

    // 2. Trim this node if necessary
    bool isTy = (x->GetType() == Prof::CSProfNode::PROCEDURE_FRAME || 
		 x->GetType() == Prof::CSProfNode::LOOP);
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
  is_overhead(Prof::CSProfCodeNode* x)
  {
    if (x->GetType() == Prof::CSProfNode::PROCEDURE_FRAME) {
      const string& x_fnm = x->GetFile();
      if (x_fnm.length() >= s_tag.length()) {
	size_t tag_beg = x_fnm.length() - s_tag.length();
	return (x_fnm.compare(tag_beg, s_tag.length(), s_tag) == 0);
      }
      // fall through
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
  static const string s_proc_pfx;
  static const string s_slow_pfx;
  static const string s_slow_sfx;

  static string 
  normalizeName(Prof::CSProfCodeNode* x) 
  {
    string x_nm = x->codeName();
    
    // if '_cilk' is a prefix of x_nm, normalize
    if (x->GetType() == Prof::CSProfNode::PROCEDURE_FRAME) {
      //int pfx = s_proc_pfx.length();
      //size_t mrk_x = x_nm.find_first_of('@');
      //string x_procnm = x.substr(pfx, mrk_x - 1 - pfx);
      const string& x_procnm = x->GetProc();

      string xtra = "";
      if (ParallelOverhead::is_overhead(x)) {
	xtra = "@" + ParallelOverhead::s_tag;
      }

      // 1. _cilk_cilk_main_import --> invoke_main_slow
      if (x_procnm == "_cilk_cilk_main_import") {
	return "invoke_main_slow" + xtra;
      }

      // 2. _cilk_x_slow --> x [_cilk_cilk_main_slow --> cilk_main]
      if (is_slow_pfx(x_procnm)) {
	int len = x_procnm.length() - s_slow_pfx.length() - s_slow_sfx.length();
	string x_basenm = 
	  x_procnm.substr(s_slow_pfx.length(), len);
	return x_basenm + xtra;
      }

      return x_procnm + xtra;
    }
    return x_nm;
  }

  static inline bool 
  is_proc_pfx(const string& x)
  {
    return (x.compare(0, s_proc_pfx.length(), s_proc_pfx) == 0);
  }

  static inline bool 
  is_slow_pfx(const string& x)
  {
    return (x.compare(0, s_slow_pfx.length(), s_slow_pfx) == 0);
  }

  static inline bool 
  is_slow_proc(Prof::CSProfCodeNode* x)
  {
    const string& x_procnm = x->GetProc();
    return is_slow_pfx(x_procnm);
  }

};

const string CilkCanonicalizer::s_proc_pfx = "PROCEDURE_FRAME ";
const string CilkCanonicalizer::s_slow_pfx = "_cilk_";
const string CilkCanonicalizer::s_slow_sfx = "_slow";



//***************************************************************************
// lush_cilkNormalize
//***************************************************************************


static void 
lush_cilkNormalize(Prof::CSProfNode* node);

static void 
lush_cilkNormalizeByFrame(Prof::CSProfNode* node);


// Notes: Match frames, and use those matches to merge callsites.

static void 
lush_cilkNormalize(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }

  lush_cilkNormalize(cct->root());
}


typedef std::map<string, Prof::CSProfCodeNode*> CilkMergeMap;


// - Preorder Visit
static void 
lush_cilkNormalize(Prof::CSProfNode* node)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit node
  // ------------------------------------------------------------
  if (node->GetType() == Prof::CSProfNode::PROCEDURE_FRAME) {
    lush_cilkNormalizeByFrame(node);

    // normalize routines not normalized by merging...
    Prof::CSProfCodeNode* x = dynamic_cast<Prof::CSProfCodeNode*>(node);
    x->SetProc(CilkCanonicalizer::normalizeName(x));
  }

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); ++it) {
    Prof::CSProfNode* x = it.CurNode();
    lush_cilkNormalize(x);
  }
}


static void 
lush_cilkNormalizeByFrame(Prof::CSProfNode* node)
{
  DIAG_MsgIf(0, "====> (" << node << ") " << node->codeName());
  
  // ------------------------------------------------------------
  // Gather (grand) children frames (skip one level)
  // ------------------------------------------------------------
  CSProfCodeNodeSet frameSet;
  
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); ++it) {
    Prof::CSProfNode* child = it.CurNode();

    for (Prof::CSProfNodeChildIterator it(child); it.Current(); ++it) {
      Prof::CSProfCodeNode* x = 
	dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
      
      if (x->GetType() == Prof::CSProfNode::PROCEDURE_FRAME) {
	frameSet.insert(x);
      }
    }
  }


  // ------------------------------------------------------------
  // Merge against (grand) children frames
  // ------------------------------------------------------------

  CilkMergeMap mergeMap;

  for (CSProfCodeNodeSet::iterator it = frameSet.begin(); 
       it != frameSet.end(); ++it) {
    
    Prof::CSProfCodeNode* x = *it;

    string x_nm = CilkCanonicalizer::normalizeName(x);
    DIAG_MsgIf(0, "\tins: " << x->codeName() << "\n" 
	       << "\tas : " << x_nm << " --> " << x);
    CilkMergeMap::iterator it = mergeMap.find(x_nm);
    if (it != mergeMap.end()) {
      // found -- we have a duplicate
      Prof::CSProfCodeNode* y = (*it).second;

      // keep the version without the "_cilk_" prefix
      Prof::CSProfCodeNode* tokeep = y, *todel = x;
      if (y->GetType() == Prof::CSProfNode::PROCEDURE_FRAME 
	  && CilkCanonicalizer::is_slow_proc(y)) {
        tokeep = x;
	todel = y;
	mergeMap[x_nm] = x;
      } 
      
      Prof::CSProfNode* par_tokeep = tokeep->Parent();
      Prof::CSProfNode* par_todel  = todel->Parent();

      DIAG_MsgIf(0, "\tkeep (" << tokeep << "): " << tokeep->codeName() << "\n"
		 << "\tdel  (" << todel  << "): " << todel->codeName());

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
lush_makeParallelOverhead(Prof::CSProfNode* node, 
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

  for (Prof::CSProfNodeIterator it(cct->root()); it.Current(); ++it) {
    Prof::CSProfNode* x = it.CurNode();
    Prof::IDynNode* x_dyn = dynamic_cast<Prof::IDynNode*>(x);
    if (x_dyn) {
      x_dyn->expandMetrics_after(n_new_metrics);
    }
  }

  lush_makeParallelOverhead(cct->root(), metric_src, metric_dst, false);
}


static void 
lush_makeParallelOverhead(Prof::CSProfNode* node, 
			  const std::vector<uint>& m_src, 
			  const std::vector<uint>& m_dst, 
			  bool is_overhead_ctxt)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit node
  // ------------------------------------------------------------
  if (is_overhead_ctxt) {
    for (Prof::CSProfNodeChildIterator it(node); it.Current(); ++it) {
      Prof::CSProfNode* x = it.CurNode();
      Prof::IDynNode* x_dyn = dynamic_cast<Prof::IDynNode*>(x);
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

  if (node->GetType() == Prof::CSProfNode::PROCEDURE_FRAME) {
    Prof::CSProfCodeNode* x = dynamic_cast<Prof::CSProfCodeNode*>(node);
    is_overhead_ctxt = ParallelOverhead::is_overhead(x);
  }

  for (Prof::CSProfNodeChildIterator it(node); it.Current(); ++it) {
    Prof::CSProfNode* x = it.CurNode();
    lush_makeParallelOverhead(x, m_src, m_dst, is_overhead_ctxt);
  }
}

//***************************************************************************
