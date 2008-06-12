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
using std::cerr;
using std::hex;
using std::dec;

#include <fstream>
#include <stack>

#include <string>
using std::string;

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <cstring>

//*************************** User Include Files ****************************

#include "CallPath.hpp"

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations ***************************

#define FIXME_ADD_ASSOC_TAGS 0
#define DBG_NORM_PROC_FRAME 0

// FIXME: why is the output different without c_str()?
#define C_STR .c_str() 
//#define C_STR 

#define DEB_NORM_SEARCH_PATH  0
#define DEB_MKDIR_SRC_DIR 0


//****************************************************************************
// Dump a CSProfTree 
//****************************************************************************

const char *CSPROFILEdtd =
#include <lib/xml/CSPROFILE.dtd.h>


namespace Analysis {

namespace CallPath {

void 
writeInDatabase(Prof::CallPath::Profile* prof, const string& filenm) 
{
  std::filebuf fb;
  fb.open(filenm.c_str(), std::ios::out);
  std::ostream os(&fb);
  write(prof, os, true);
  fb.close();
}


void
write(Prof::CallPath::Profile* prof, std::ostream& os, bool prettyPrint)
{
  using namespace Prof;

  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE CSPROFILE [\n" << CSPROFILEdtd << "]>" << std::endl;
  os.flush();
  os << "<CSPROFILE version=\"1.0.2\">\n";
  os << "<CSPROFILEHDR>\n</CSPROFILEHDR>\n"; 
  os << "<CSPROFILEPARAMS>\n";
  os << "<TARGET name"; WriteAttrStr(os, prof->name()); os << "/>\n";

  // write out metrics
  uint n_metrics = prof->numMetrics();
  for (uint i = 0; i < n_metrics; i++) {
    const SampledMetricDesc* metric = prof->metric(i);
    os << "<METRIC shortName" << MakeAttrNum(i)
       << " nativeName" << MakeAttrStr(metric->name())
       << " period" << MakeAttrNum(metric->period())
       << " flags" << MakeAttrNum(metric->flags(), 16)
       << "/>\n";
  }
  os << "</CSPROFILEPARAMS>\n";

  os.flush();
  
  int dumpFlags = (CCT::Tree::XML_TRUE); // CCT::Tree::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= CCT::Tree::COMPRESSED_OUTPUT; }
  
  prof->cct()->dump(os, dumpFlags);

  os << "</CSPROFILE>\n";
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
findOrCreateProcFrame(Prof::IDynNode* node,
		      Prof::Struct::LM* lmStrct, 
		      Prof::Struct::ACodeNode* callingCtxt,
		      Prof::Struct::Loop* loop,
		      VMA curr_ip,
		      ACodeNodeToProcFrameMap& frameMap,
		      ProcFrameAndLoopToCSLoopMap& loopMap);

void
createFramesForProc(Prof::Struct::Proc* proc, Prof::IDynNode* dyn_node,
		    ACodeNodeToProcFrameMap& frameMap,
		    ProcFrameAndLoopToCSLoopMap& loopMap);

void
loopifyFrame(Prof::CSProfProcedureFrameNode* frame, 
	     Prof::Struct::ACodeNode* ctxtScope,
	     ACodeNodeToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, Prof::Struct::LM* lmStrct);


// inferCallFrames: Effectively create equivalence classes of frames
// for all the return addresses found under.
//
void
Analysis::CallPath::
inferCallFrames(Prof::CallPath::Profile* prof, Prof::Epoch::LM* epoch_lm, 
		Prof::Struct::LM* lmStrct)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  inferCallFrames(prof, cct->root(), epoch_lm, lmStrct);
}


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, Prof::Struct::LM* lmStrct)
{
  // INVARIANT: The parent of 'node' has been fully processed and
  // lives within a correctly located procedure frame.
  
  if (!node) { return; }

  ACodeNodeToProcFrameMap frameMap;
  ProcFrameAndLoopToCSLoopMap loopMap;

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* n = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 
    
    // process this node
    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn && (n_dyn->lm_id() == epoch_lm->id())) {
      VMA ip_ur = n_dyn->ip();
      DIAG_DevIf(50) {
	Prof::CSProfCallSiteNode* p = node->AncestorCallSite();
	DIAG_DevMsg(0, "inferCallFrames: " << hex << ((p) ? p->ip() : 0) << " --> " << ip_ur << dec);
      }
      
      Prof::Struct::ACodeNode* scope = lmStrct->findByVMA(ip_ur);
      Prof::Struct::ACodeNode* ctxt = NULL;
      Prof::Struct::Loop* loop = NULL;
      if (scope) {
	using namespace Prof::Struct;
	ctxt = scope->AncCallingCtxt();
	// FIXME: should include PROC (for nested procs)
	ANode* x = scope->Ancestor(ANode::TyLOOP, ANode::TyALIEN);
	loop = dynamic_cast<Prof::Struct::Loop*>(x);
      }

      // Add symbolic information to 'n'
      addSymbolicInfo(n, n_dyn, lmStrct, ctxt, scope);

      // Find (or create) a procedure frame for 'n'.
      Prof::CSProfProcedureFrameNode* frame = 
	findOrCreateProcFrame(n_dyn, lmStrct, ctxt, loop, ip_ur, 
			      frameMap, loopMap);
      
      // Find appropriate (new) parent context for 'node': the frame
      // or a loop within the frame
      Prof::CSProfCodeNode* newParent = frame;
      if (loop) {
	ProcFrameAndLoop toFind(frame, loop);
	ProcFrameAndLoopToCSLoopMap::iterator it = loopMap.find(toFind);
	DIAG_Assert(it != loopMap.end(), "Cannot find corresponding loop scope:\n" << loop->toStringXML());
	newParent = (*it).second;
      }
      
      // Link 'n' to its frame
      n->Unlink();
      n->Link(newParent);
    }
    
    // recur 
    if (!n->IsLeaf()) {
      inferCallFrames(prof, n, epoch_lm, lmStrct);
    }
  }
}


// findOrCreateProcFrame: Find or create a procedure frame for 'node'
// given it's corresponding load module scope (lmStrct), calling
// context scope (callingCtxt, a Struct::Proc or Struct::Alien) and
// statement-type scope.
// 
// Assumes that symbolic information has been added to node.
Prof::CSProfProcedureFrameNode*
findOrCreateProcFrame(Prof::IDynNode* node,
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
      createFramesForProc(proc, node, frameMap, loopMap);

      // frame should now be in map
      ACodeNodeToProcFrameMap::iterator it = frameMap.find(toFind);
      DIAG_Assert(it != frameMap.end(), "");
      frame = (*it).second;
    }
    else {
      frame = new Prof::CSProfProcedureFrameNode(NULL);
      addSymbolicInfo(frame, node, lmStrct, NULL, NULL);
      frame->Link(node->proxy()->Parent());

      string nm = string("unknown(s)@") + StrUtil::toStr(ip_ur, 16); // FIXME
      //string nm = "unknown(s)@" + StrUtil::toStr(ip, 16);
      frame->SetProc(nm C_STR);
      
      frameMap.insert(std::make_pair(toFind, frame));
    }
  }
  
  return frame;
}


void 
createFramesForProc(Prof::Struct::Proc* proc, Prof::IDynNode* node_dyn,
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
				 n->UniqueId());
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

    n->SetFile(fnm C_STR);
#if (FIXME_ADD_ASSOC_TAGS)
    std::string nm = callingCtxt->name() C_STR;
    if (n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
      nm += " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
	+ ", " + n_dyn->lip_str() + ") [" + n_dyn->assocInfo_str() + "]";
    }
    n->SetProc(nm);
#else
    n->SetProc(callingCtxt->name() C_STR);
#endif
    n->SetLine(scope->begLine());
    n->SetFileIsText(true);
    n->structureId() = scope->UniqueId();
  }
  else {
    n->SetFile(lmStrct->name() C_STR);
    n->SetLine(0);
    n->SetFileIsText(false);
    n->structureId() = lmStrct->UniqueId();
  }
}


//***************************************************************************
// Routines for Inferring Call Frames (based on LM)
//***************************************************************************

typedef std::map<string, Prof::CSProfProcedureFrameNode*> StringToProcFrameMap;

void addSymbolicInfo(Prof::IDynNode* n, binutils::LM* lm);

void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, binutils::LM* lm);

// create an extended profile representation
// normalize call sites 
//   
//          main                     main
//         /    \        =====>        |  
//        /      \                   foo:<start line foo>
//       foo:1  foo:2                 /   \
//                                   /     \
//                                foo:1   foo:2 
//  
// FIXME
// If pc values from the leaves map to the same source file info,
// coalese these leaves into one.

void 
Analysis::CallPath::
inferCallFrames(Prof::CallPath::Profile* prof, Prof::Epoch::LM* epoch_lm, 
		binutils::LM* lm)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  DIAG_MsgIf(DBG_NORM_PROC_FRAME, "start normalizing same procedure children");
  inferCallFrames(prof, cct->root(), epoch_lm, lm);
}


void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::CSProfNode* node, 
		Prof::Epoch::LM* epoch_lm, binutils::LM* lm)
{
  if (!node) { return; }

  // Use this set to determine if we have a duplicate source line
  StringToProcFrameMap frameMap;

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* n = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 

    // ------------------------------------------------------------
    // recur 
    // ------------------------------------------------------------
    if (!n->IsLeaf()) {
      inferCallFrames(prof, n, epoch_lm, lm);
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
      
      addSymbolicInfo(n_dyn, lm);
      
      string myid = n->GetFile() + n->GetProc();
      
      StringToProcFrameMap::iterator it = frameMap.find(myid);
      Prof::CSProfProcedureFrameNode* frame;
      if (it != frameMap.end()) { 
	// found 
	frame = (*it).second;
      } 
      else {
	// no entry found -- add
	frame = new Prof::CSProfProcedureFrameNode(NULL);
	frame->SetFile(n->GetFile() C_STR);
	
	string frameNm = n->GetProc();
	if (frameNm.empty()) {
	  frameNm = string("unknown(~s)@") + StrUtil::toStr(ip_ur, 16);
	} 
	DIAG_MsgIf(DBG_NORM_PROC_FRAME, "frame name: " << n->GetProc() 
		   << " --> " << frameNm);
	
	
	frame->SetProc(frameNm C_STR);
	frame->SetFileIsText(n->FileIsText());
	if (!frame->FileIsText()) {
	  frame->SetLine(0);
	} 
	else {
	  // determine the first line of the enclosing procedure
	  SrcFile::ln begLn;
	  lm->GetProcFirstLineInfo(ip_ur, n_dyn->opIndex(), begLn);
	  frame->SetLine(begLn);
	}
	
	frame->Link(node);
	frameMap.insert(std::make_pair(myid, frame));
      }
      
      n->Unlink();
      n->Link(frame);
    }
  }
}


//***************************************************************************

#if 0
void
addSymbolicInfo(Prof::CallPath::Profile* prof, VMA begVMA, VMA endVMA, binutils::LM* lm)
{
  VMA curr_ip; 

  /* point to the first load module in the Epoch table */
  Prof::CCT::Tree* tree = prof->Getcct();
  Prof::CSProfNode* root = tree->GetRoot();
  
  for (Prof::CSProfNodeIterator it(root); it.CurNode(); ++it) {
    Prof::CSProfNode* n = it.CurNode();
    Prof::CSProfCodeNode* nn = dynamic_cast<Prof::CSProfCodeNode*>(n);
    
    if (nn && (nn->GetType() == Prof::CSProfNode::STATEMENT 
	       || nn->GetType() == Prof::CSProfNode::CALLSITE)
	&& !nn->GotSrcInfo()) {
      curr_ip = nn->ip();
      if (begVMA <= curr_ip && curr_ip <= endVMA) { 
	// in the current load module
	addSymbolicInfo(nn,lm,true);
	nn->SetSrcInfoDone(true);
      }
    }  
  }
}
#endif


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
#if (FIXME_ADD_ASSOC_TAGS)
  if (!func.empty() && n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
    func +=  " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
      + ", " + n_dyn->lip_str() + ") [" + n_dyn->assocInfo_str() + "]";
  }
#endif

  n->SetFile(file C_STR);
  n->SetProc(func C_STR);
  n->SetLine(srcLn);
  n->SetFileIsText(true);
  
  // if file name is missing then using load module name. 
  if (file.empty() || func.empty()) {
    n->SetFile(lm->name() C_STR);
    n->SetLine(0); //don't need to have line number for loadmodule
    n->SetFileIsText(false);
  }
}


//***************************************************************************
// Routines for normalizing the ScopeTree
//***************************************************************************

bool coalesceCallsiteLeaves(Prof::CallPath::Profile* prof);
void removeEmptyScopes(Prof::CallPath::Profile* prof);

bool 
Analysis::CallPath::normalize(Prof::CallPath::Profile* prof)
{
  // Remove duplicate/inplied file and procedure information from tree
  coalesceCallsiteLeaves(prof);
  removeEmptyScopes(prof);
  
  return (true);
}


// FIXME
// If pc values from the leaves map to the same source file info,
// coalese these leaves into one.
bool coalesceCallsiteLeaves(Prof::CSProfNode* node);

bool 
coalesceCallsiteLeaves(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return true; }
  
  return coalesceCallsiteLeaves(cct->root());
}


// FIXME
typedef std::map<string, Prof::CSProfStatementNode*> StringToCallSiteMap;

bool 
coalesceCallsiteLeaves(Prof::CSProfNode* node)
{
  bool noError = true;
  
  if (!node) { return noError; }

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
      string myid = string(c->GetFile()) + string(c->GetProc()) 
	+ StrUtil::toStr(c->GetLine());
      
      StringToCallSiteMap::iterator it = sourceInfoMap.find(myid);
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
	sourceInfoMap.insert(std::make_pair(myid, c));
      }
    } 
    else if (!child->IsLeaf()) {
      // Recur:
      noError = noError && coalesceCallsiteLeaves(child);
    }
  } 
  
  return noError;
}



void 
removeEmptyScopes(Prof::CSProfNode* node);

void 
removeEmptyScopes(Prof::CallPath::Profile* prof)
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return; }
  
  removeEmptyScopes(cct->root());
}


void 
removeEmptyScopes(Prof::CSProfNode* node)
{
  if (!node) { return; }

  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfNode* child = it.CurNode();
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any trimming for this tree's children
    removeEmptyScopes(child);

    // 2. Trim this node if necessary
    bool remove = (child->IsLeaf() 
		   && child->GetType() == Prof::CSProfNode::PROCEDURE_FRAME);
    if (remove) {
      child->Unlink(); // unlink 'child' from tree
      delete child;
    }
  }
}


//***************************************************************************
// 
//***************************************************************************

void copySourceFiles(Prof::CSProfNode* node, 
		     std::vector<string>& searchPaths,
		     const string& dest_dir);

void innerCopySourceFiles(Prof::CSProfNode* node, 
			  std::vector<string>& searchPaths,
			  const string& dest_dir);


/** Copies the source files for the executable into the database 
    directory. */
void 
Analysis::CallPath::copySourceFiles(Prof::CallPath::Profile* prof, 
				    std::vector<string>& searchPaths,
				    const string& dest_dir) 
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return ; }

  copySourceFiles(cct->root(), searchPaths, dest_dir);
}


/** Perform DFS traversal of the tree nodes and copy
    the source files for the executable into the database
    directory. */
void 
copySourceFiles(Prof::CSProfNode* node, 
		std::vector<string>& searchPaths,
		const string& dest_dir) 
{
  xDEBUG(DEB_MKDIR_SRC_DIR, 
	 cerr << "descend into node" << std::endl;);

  if (!node) {
    return;
  }

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.CurNode(); it++) {
    // recur 
    copySourceFiles(it.CurNode(), searchPaths, dest_dir);
  }

  innerCopySourceFiles(node, searchPaths, dest_dir);
}


void 
innerCopySourceFiles(Prof::CSProfNode* node, 
		     std::vector<string>& searchPaths,
		     const string& dest_dir)
{
  bool inspect; 
  string nodeSourceFile;
  string relativeSourceFile;
  string procedureFrame;
  bool sourceFileWasCopied = false;
  bool fileIsText = false; 

  switch(node->GetType()) {
  case Prof::CSProfNode::CALLSITE:
    {
      Prof::CSProfCallSiteNode* c = dynamic_cast<Prof::CSProfCallSiteNode*>(node);
      nodeSourceFile = c->GetFile();
      fileIsText = c->FileIsText();
      inspect = true;
      procedureFrame = c->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze CALLSITE for proc" << procedureFrame
	     << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;

  case Prof::CSProfNode::STATEMENT:
    {
      Prof::CSProfStatementNode* st = dynamic_cast<Prof::CSProfStatementNode*>(node);
      nodeSourceFile = st->GetFile();
      fileIsText = st->FileIsText();
      inspect = true;
      procedureFrame = st->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze STATEMENT for proc" << procedureFrame
	     << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;

  case Prof::CSProfNode::PROCEDURE_FRAME:
    {
      Prof::CSProfProcedureFrameNode* pf = 
	dynamic_cast<Prof::CSProfProcedureFrameNode*>(node);
      nodeSourceFile = pf->GetFile();
      fileIsText = pf->FileIsText();
      inspect = true;
      procedureFrame = pf->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze PROCEDURE_FRAME for proc" << 
	     procedureFrame << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;

  default:
    inspect = false;
    break;
  } 
  
  if (inspect) {
    // copy source file for current node
    xDEBUG(DEB_MKDIR_SRC_DIR,
	   cerr << "attempt to copy " << nodeSourceFile << std::endl;);
// FMZ     if (! nodeSourceFile.empty()) {
     if (fileIsText && !nodeSourceFile.empty()) {
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "attempt to copy text, nonnull " << nodeSourceFile << std::endl;);
      
      // foreach  search paths
      //    normalize  searchPath + file
      //    if (file can be opened) 
      //    break into components 
      //    duplicate directory structure (mkdir segment by segment)
      //    copy the file (use system syscall)
      int ii;
      bool searchDone = false;
      for (ii=0; !searchDone && ii<searchPaths.size(); ii++) {
	string testPath;
	if ( nodeSourceFile[0] == '/') {
	  testPath = nodeSourceFile;
	  searchDone = true;
	}  else {
	  testPath = searchPaths[ii]+"/"+nodeSourceFile;
	}
	xDEBUG(DEB_MKDIR_SRC_DIR,
	       cerr << "search test path " << testPath << std::endl;);
	string normTestPath = normalizeFilePath(testPath);
	xDEBUG(DEB_MKDIR_SRC_DIR,
	       cerr << "normalized test path " << normTestPath << std::endl;);
	if (normTestPath == "") {
	  xDEBUG(DEB_MKDIR_SRC_DIR,
		 cerr << "null test path " << std::endl;);
	} else {
	  xDEBUG(DEB_MKDIR_SRC_DIR,
		 cerr << "attempt to text open" << normTestPath << std::endl;);
	  FILE *sourceFileHandle = fopen(normTestPath.c_str(), "rt");
	  if (sourceFileHandle != NULL) {
	    searchDone = true;
	    char normTestPathChr[MAX_PATH_SIZE+1];
	    strcpy(normTestPathChr, normTestPath.c_str());
	    relativeSourceFile = normTestPathChr+1;
	    sourceFileWasCopied = true;
	    xDEBUG(DEB_MKDIR_SRC_DIR,
		   cerr << "text open succeeded; path changed to " <<
		   relativeSourceFile << std::endl;);
	    fclose (sourceFileHandle);

	    // check if the file already exists (we've copied it for a previous sample)
	    string testFilePath = dest_dir + normTestPath;
	    FILE *testFileHandle = fopen(testFilePath.c_str(), "rt");
	    if (testFileHandle != NULL) {
	      fclose(testFileHandle);
	    } else {
	      // we've found the source file and we need to copy it into the database
	      std::stack<string> pathSegmentsStack;
	      breakPathIntoSegments (normTestPath,
				     pathSegmentsStack);
	      std::vector<string> pathSegmentsVector;
	      for (; !pathSegmentsStack.empty();) {
		string crtSegment = pathSegmentsStack.top();
		pathSegmentsStack.pop(); 
		pathSegmentsVector.insert(pathSegmentsVector.begin(),
					  crtSegment);
		xDEBUG(DEB_MKDIR_SRC_DIR,
		       cerr << "inserted path segment " << 
		       pathSegmentsVector[0] << std::endl;);
	      }

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "converted stack to vector" << std::endl;);

	      char filePathChr[MAX_PATH_SIZE +1];
	      strcpy(filePathChr, dest_dir.c_str());
	      chdir(filePathChr);

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "after chdir " << std::endl;);

	      string subPath = dest_dir;
	      int pathSegmentIndex;
	      for (pathSegmentIndex=0; 
		   pathSegmentIndex<pathSegmentsVector.size()-1;
		   pathSegmentIndex++) {
		subPath = subPath+"/"+pathSegmentsVector[pathSegmentIndex];
		xDEBUG(DEB_MKDIR_SRC_DIR,
		       cerr << "about to mkdir " 
		       << subPath << std::endl;);
		int mkdirResult =  
		  mkdir (subPath.c_str(), 
			 S_IRWXU | S_IRGRP | S_IXGRP 
			 | S_IROTH | S_IXOTH); 
		if (mkdirResult == -1) {
		  switch (errno) {
		  case EEXIST:  
		    xDEBUG(DEB_MKDIR_SRC_DIR,
			   cerr << "EEXIST " << std::endl;); 
		    // we created the same directory 
		    // for a different source file
		    break;
		  default:
		    cerr << "mkdir failed for " << subPath << std::endl;
		    perror("mkdir failed:");
		    exit (1);
		  }
		}
	      }
	      strcpy(filePathChr, subPath.c_str());
	      chdir(filePathChr);
	      string cpCommand = "cp -f "+normTestPath+" .";
	      system (cpCommand.c_str());
	      // fix the file name  so it points to the one in the source directory
	    }
	  }
	}
      }
    }
  }
  
  if (inspect && sourceFileWasCopied) {
    switch( node->GetType()) {
    case Prof::CSProfNode::CALLSITE:
      {
	Prof::CSProfCallSiteNode* c = dynamic_cast<Prof::CSProfCallSiteNode*>(node);
	c->SetFile(relativeSourceFile);
      }
      break;
    case Prof::CSProfNode::STATEMENT:
      {
	Prof::CSProfStatementNode* st = dynamic_cast<Prof::CSProfStatementNode*>(node);
	st->SetFile(relativeSourceFile);
      }
      break;
    case Prof::CSProfNode::PROCEDURE_FRAME:
      {
	Prof::CSProfProcedureFrameNode* pf = 
	  dynamic_cast<Prof::CSProfProcedureFrameNode*>(node);
	pf->SetFile(relativeSourceFile);
      }
      break;
    default:
      cerr << "Invalid node type" << std::endl;
      exit(0);
      break;
    }
  }
}


//***************************************************************************
// 
//***************************************************************************

/** Normalizes a file path. Handle construncts such as :
 ~/...
 ./...
 .../dir1/../...
 .../dir1/./.... 
 ............./
*/
string 
normalizeFilePath(const string& filePath, 
		  std::stack<string>& pathSegmentsStack)
{
  char cwdName[MAX_PATH_SIZE +1];
  getcwd(cwdName, MAX_PATH_SIZE);
  string crtDir=cwdName; 
  
  string filePathString = filePath;
  char filePathChr[MAX_PATH_SIZE +1];
  strcpy(filePathChr, filePathString.c_str());

  // ~/...
  if ( filePathChr[0] == '~' ) {
    if ( filePathChr[1]=='\0' ||
	 filePathChr[1]=='/' ) {
      // reference to our home directory
      string homeDir = getenv("HOME"); 
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "home dir:" << homeDir << std::endl;);
      filePathString = homeDir + (filePathChr+1); 
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "new filePathString=" << filePathString << std::endl;);
    } else {
      // parse the beginning of the filePath to determine the other username
      int filePos = 0;
      char userHome[MAX_PATH_SIZE+1]; 
      for ( ;
	    ( filePathChr[filePos] != '\0' &&
	      filePathChr[filePos] != '/');
	    filePos++) {
	userHome[filePos]=filePathChr[filePos];
      }
      userHome[filePos]='\0';
      if (chdir (userHome) == 0 ) {
	char userHomeDir[MAX_PATH_SIZE +1];
	getcwd(userHomeDir, MAX_PATH_SIZE);
	filePathString = userHomeDir;
	filePathString = filePathString + (filePathChr + filePos);
	xDEBUG(DEB_NORM_SEARCH_PATH, 
	       cerr << "new filePathString=" << 
	       filePathString << std::endl;);
      }
    }
  }

 
  // ./...
  if ( filePathChr[0] == '.') {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "crt dir:" << crtDir << std::endl;);
    filePathString = crtDir + "/"+string(filePathChr+1); 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "updated filePathString=" << filePathString << std::endl;);
  }

  if ( filePathChr[0] != '/') {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "crt dir:" << crtDir << std::endl;);
    filePathString = crtDir + "/"+ string(filePathChr); 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "applied crtDir: " << crtDir << " and got updated filePathString=" << filePathString << std::endl;);
  }

  // ............./
  if ( filePathChr[ strlen(filePathChr) -1] == '/') {
    strcpy(filePathChr, filePathString.c_str());
    filePathChr [strlen(filePathChr)-1]='\0';
    filePathString = filePathChr; 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "after removing trailing / got filePathString=" << filePathString << std::endl;);
  }

  // parse the path and determine segments separated by '/' 
  strcpy(filePathChr, filePathString.c_str());

  int crtPos=0;
  char crtSegment[MAX_PATH_SIZE+1];
  int crtSegmentPos=0;
  crtSegment[crtSegmentPos]='\0';

  for (; crtPos <= strlen(filePathChr); crtPos ++) {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "parsing " << filePathChr[crtPos] << std::endl;);
    if (filePathChr[crtPos] ==  '/' || 
	filePathChr[crtPos] ==  '\0') {
      crtSegment[crtSegmentPos]='\0';
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "parsed segment " << crtSegment << std::endl;);
      if (strcmp(crtSegment,".") == 0) {
	//  .../dir1/./.... 
      } 
      else if (strcmp(crtSegment,"..") == 0) {
	// .../dir1/../...
	if (pathSegmentsStack.empty()) {
	  DIAG_EMsg("Invalid path: " << filePath);
	  return "";
	} 
	else {
	  pathSegmentsStack.pop();
	}
      } 
      else {
	if (crtSegmentPos>0) {
	  pathSegmentsStack.push(string(crtSegment));
	  xDEBUG(DEB_NORM_SEARCH_PATH, 
		 cerr << "pushed segment " << crtSegment << std::endl;);
	}
      }
      crtSegment[0]='\0';
      crtSegmentPos=0;
    } 
    else {
      crtSegment[crtSegmentPos] = filePathChr[crtPos];
      crtSegmentPos++;
    }
  }

  if (pathSegmentsStack.empty()) {
    DIAG_EMsg("Invalid path: " << filePath);
    return "";
  }

  std::stack<string> stackCopy = pathSegmentsStack;
  string resultPath = stackCopy.top();
  stackCopy.pop();
  for (;! stackCopy.empty(); ) {
    resultPath = stackCopy.top() + "/"+resultPath;
    stackCopy.pop();
  }
  resultPath = "/"+resultPath;
  xDEBUG(DEB_NORM_SEARCH_PATH, 
	 cerr << "normalized path: " << resultPath << std::endl;);
  return resultPath;
}


string 
normalizeFilePath(const string& filePath)
{
  std::stack<string> pathSegmentsStack;
  string resultPath = normalizeFilePath(filePath, pathSegmentsStack);
  return resultPath;
}


/** Decompose a normalized path into path segments.*/
void 
breakPathIntoSegments(const string& normFilePath, 
			   std::stack<string>& pathSegmentsStack)
{
  string resultPath = normalizeFilePath(normFilePath, pathSegmentsStack);
}

