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
#include <fstream>
#include <stack>
#include <string>
using std::string;

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>

#ifdef NO_STD_CHEADERS  // FIXME
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "CSProfileUtils.hpp"

#include <lib/prof-lean/hpcfile_csproflib.h>

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations ***************************

#define FIXME_ADD_ASSOC_TAGS 0
#define DBG_NORM_PROC_FRAME 0

// FIXME: why is the output different without c_str()?
#define C_STR .c_str() 
//#define C_STR 

//*************************** Forward Declarations ***************************

using namespace xml;

extern "C" {
  static void* cstree_create_node_CB(void* tree, 
				     hpcfile_cstree_nodedata_t* data);
  static void  cstree_link_parent_CB(void* tree, void* node, void* parent);

  static void* hpcfile_alloc_CB(size_t sz);
  static void  hpcfile_free_CB(void* mem);
}

static void 
convertOpIPToIP(VMA opIP, VMA& ip, ushort& opIdx);

static bool
addPGMToCSProfTree(CSProfTree* tree, const char* progName);

static bool 
fixLeaves(CSProfNode* node);


//****************************************************************************
// Dump a CSProfTree 
//****************************************************************************

const char *CSPROFILEdtd =
#include <lib/xml/CSPROFILE.dtd.h>


void 
writeCSProfileInDatabase(CSProfile* prof, const string& fnm) 
{
  filebuf fb;
  fb.open(fnm.c_str(), ios::out);
  std::ostream os(&fb);
  writeCSProfile(prof, os, true);
  fb.close();
}


/* version=1.0.2 for alpha memory profile---FMZ */
void
writeCSProfile(CSProfile* prof, std::ostream& os, bool prettyPrint)
{
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
    const CSProfileMetric* metric = prof->metric(i);
    os << "<METRIC shortName"; WriteAttrNum(os, i);
    os << " nativeName";       WriteAttrNum(os, metric->name());
    os << " period";           WriteAttrNum(os, metric->period());
    os << " flags";            WriteAttrNum(os, metric->flags());
    os << "/>\n";
  }
  os << "</CSPROFILEPARAMS>\n";

  os.flush();
  
  int dumpFlags = (CSProfTree::XML_TRUE); // CSProfTree::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= CSProfTree::COMPRESSED_OUTPUT; }
  
  prof->cct()->dump(os, dumpFlags);

  os << "</CSPROFILE>\n";
  os.flush();
}


//****************************************************************************
// Set of routines to build a scope tree while reading data file
//****************************************************************************

CSProfile* 
ReadProfile_CSPROF(const char* fnm, const char *execnm) 
{
  hpcfile_csprof_data_t metadata;
  int ret;

  // ------------------------------------------------------------
  // Read profile
  // ------------------------------------------------------------

  FILE* fs = hpcfile_open_for_read(fnm);
  if (!fs) { 
    DIAG_Throw(fnm << ": could not open");
  }

  epoch_table_t epochtbl;
  ret = hpcfile_csprof_read(fs, &metadata, &epochtbl, hpcfile_alloc_CB, 
			     hpcfile_free_CB);
  if (ret != HPCFILE_OK) {
    DIAG_Throw(fnm << ": error reading header (HPC_CSPROF)");
    return NULL;
  }
  
  uint num_metrics = metadata.num_metrics;
  
  DIAG_Msg(2, "Metrics found: " << num_metrics);

  CSProfile* prof = new CSProfile(num_metrics);
  ret = hpcfile_cstree_read(fs, prof->cct(), num_metrics,
			    cstree_create_node_CB, cstree_link_parent_CB,
			    hpcfile_alloc_CB, hpcfile_free_CB);
  if (ret != HPCFILE_OK) {
    DIAG_Throw(fnm << ": error reading calling context tree (HPC_CSTREE)."
	       << " (Or no samples were taken.) [FIXME: should not have been lumped together!]");
    delete prof;
    return NULL;
  }

  hpcfile_close(fs);


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  DIAG_WMsgIf(epochtbl.num_epoch > 1, fnm << ": only processing first epoch!");
  
  uint num_lm = epochtbl.epoch_modlist[0].num_loadmodule;

  CSProfEpoch* epochmdlist = new CSProfEpoch(num_lm);

  for (int i = 0; i < num_lm; i++) { 
    CSProfLDmodule* lm = new CSProfLDmodule();
    lm->SetName(epochtbl.epoch_modlist[0].loadmodule[i].name);
    lm->SetVaddr(epochtbl.epoch_modlist[0].loadmodule[i].vaddr);
    lm->SetMapaddr(epochtbl.epoch_modlist[0].loadmodule[i].mapaddr);  
    lm->SetUsedFlag(false);
    epochmdlist->SetLoadmodule(i,lm);
  }
  epoch_table__free_data(&epochtbl, hpcfile_free_CB);
  
  epochmdlist->SortLoadmoduleByVMA(); 

  // Extract profiling info
  prof->name(execnm); 
  
  // Extract metrics
  for (int i = 0; i < num_metrics; i++) {
    CSProfileMetric* metric = prof->metric(i);
    metric->name(metadata.metrics[i].metric_name);
    metric->flags(metadata.metrics[i].flags);
    metric->period(metadata.metrics[i].sample_period);
  }

  prof->epoch(epochmdlist);

  // We must deallocate pointer-data
  for (int i = 0; i < num_metrics; i++) {
    hpcfile_free_CB(metadata.metrics[i].metric_name);
  }
  hpcfile_free_CB(metadata.target);
  hpcfile_free_CB(metadata.metrics);

  // Add PGM node to tree
  addPGMToCSProfTree(prof->cct(), prof->name().c_str());
  
  // Convert leaves (CSProfCallSiteNode) to CSProfStatementNodes
  // FIXME: There should be a better way of doing this.  We could
  // merge it with a normalization step...
  fixLeaves(prof->cct()->root());
  
  return prof;
}


void
DumpProfile_CSPROF(const char* fnm) 
{
  hpcfile_csprof_data_t metadata;
  int ret;

  FILE* fs = hpcfile_open_for_read(fnm);
  if (!fs) { 
    DIAG_Throw(fnm << ": could not open");
  }

  ret = hpcfile_csprof_fprint(fs, stdout, &metadata);
  if (ret != HPCFILE_OK) {
    DIAG_Throw(fnm << ": error reading HPC_CSPROF");
  }
  
  uint num_metrics = metadata.num_metrics;
  
  ret = hpcfile_cstree_fprint(fs, num_metrics, stdout);
  if (ret != HPCFILE_OK) { 
    DIAG_Throw(fnm << ": error reading HPC_CSTREE.");
  }

  hpcfile_close(fs);
}


static void* 
cstree_create_node_CB(void* tree, 
		      hpcfile_cstree_nodedata_t* data)
{
  CSProfTree* t = (CSProfTree*)tree; 
  
  VMA ip;
  ushort opIdx;
  convertOpIPToIP((VMA)data->ip, ip, opIdx);
  vector<uint> metricVec;
  metricVec.clear();
  for (uint i = 0; i < data->num_metrics; i++) {
    metricVec.push_back((uint)data->metrics[i]);
  }

  DIAG_DevMsgIf(0, "cstree_create_node_CB: " << hex << data->ip << dec);
  CSProfCallSiteNode* n = 
    new CSProfCallSiteNode(NULL, data->as_info, ip, opIdx, data->lip.ptr,
			   metricVec);
  n->SetSrcInfoDone(false);
  
  // Initialize the tree, if necessary
  if (t->empty()) {
    t->root(n);
  }
  
  return n;
}


static void  
cstree_link_parent_CB(void* tree, void* node, void* parent)
{
  CSProfCallSiteNode* p = (CSProfCallSiteNode*)parent;
  CSProfCallSiteNode* n = (CSProfCallSiteNode*)node;
  n->Link(p);
}


static void* 
hpcfile_alloc_CB(size_t sz)
{
  return (new char[sz]);
}


static void  
hpcfile_free_CB(void* mem)
{
  delete[] (char*)mem;
}


// convertOpIPToIP: Find the instruction pointer 'ip' and operation
// index 'opIdx' from the operation pointer 'opIP'.  The operation
// pointer is represented by adding 0, 1, or 2 to the instruction
// pointer for the first, second and third operation, respectively.
void 
convertOpIPToIP(VMA opIP, VMA& ip, ushort& opIdx)
{
  opIdx = (ushort)(opIP & 0x3); // the mask ...00011 covers 0, 1 and 2
  ip = opIP - opIdx;
}


bool
addPGMToCSProfTree(CSProfTree* tree, const char* progName)
{
  bool noError = true;

  // Add PGM node
  CSProfNode* n = tree->root();
  if (!n || n->GetType() != CSProfNode::PGM) {
    CSProfNode* root = new CSProfPgmNode(progName);
    if (n) { 
      n->Link(root); // 'root' is parent of 'n'
    }
    tree->root(root);
  }
  
  return noError;
}


static bool 
fixLeaves(CSProfNode* node)
{
  bool noError = true;
  
  if (!node) { return noError; }

  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfCodeNode* child = dynamic_cast<CSProfCodeNode*>(it.CurNode());
    IDynNode* child_dyn = dynamic_cast<IDynNode*>(child);
    DIAG_Assert(child && child_dyn, "");
    it++; // advance iterator -- it is pointing at 'child'

    DIAG_DevMsgIf(0, "fixLeaves: " << hex << child_dyn->ip() << dec);
    if (child->IsLeaf() && child->GetType() == CSProfNode::CALLSITE) {
      // This child is a leaf. Convert.
      CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(child);
      
      CSProfStatementNode* newc = new CSProfStatementNode(NULL);
      *newc = *c;
      
      newc->LinkBefore(node->FirstChild()); // do not break iteration!
      c->Unlink();
      delete c;
    } 
    else if (!child->IsLeaf()) {
      // Recur:
      noError = noError && fixLeaves(child);
    }
  }
  
  return noError;
}


//****************************************************************************
// Routines for Inferring Call Frames (based on STRUCTURE information)
//****************************************************************************

typedef std::map<CodeInfo*, CSProfProcedureFrameNode*> CodeInfoToProcFrameMap;


typedef std::pair<CSProfProcedureFrameNode*, CodeInfo*> ProcFrameAndLoop;
inline bool 
operator<(const ProcFrameAndLoop& x, const ProcFrameAndLoop& y) 
{
  return ((x.first < y.first) || 
	  ((x.first == y.first) && (x.second < y.second)));
}

typedef std::map<ProcFrameAndLoop, CSProfLoopNode*> ProcFrameAndLoopToCSLoopMap;




void
addSymbolicInfo(CSProfCodeNode* n, IDynNode* n_dyn,
		LoadModScope* lmScope, CodeInfo* callingCtxt, CodeInfo* scope);

CSProfProcedureFrameNode*
findOrCreateProcFrame(IDynNode* node,
		      LoadModScope* lmScope, CodeInfo* callingCtxt,
		      LoopScope* loop,
		      VMA curr_ip,
		      CodeInfoToProcFrameMap& frameMap,
		      ProcFrameAndLoopToCSLoopMap& loopMap);

void
createFramesForProc(ProcScope* proc, IDynNode* dyn_node,
		    CodeInfoToProcFrameMap& frameMap,
		    ProcFrameAndLoopToCSLoopMap& loopMap);

void
loopifyFrame(CSProfProcedureFrameNode* frame, CodeInfo* ctxtScope,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


void 
inferCallFrames(CSProfile* prof, CSProfNode* node, 
		VMA begVMA, VMA endVMA, LoadModScope* lmScope, VMA relocVMA);


// inferCallFrames: Effectively create equivalence classes of frames
// for all the return addresses found under.
//
void
inferCallFrames(CSProfile* prof, VMA begVMA, VMA endVMA, 
		LoadModScope* lmScope, VMA relocVMA)
{
  CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return; }
  
  inferCallFrames(prof, csproftree->root(), begVMA, endVMA, 
		  lmScope, relocVMA);
}


void 
inferCallFrames(CSProfile* prof, CSProfNode* node, 
		VMA begVMA, VMA endVMA, LoadModScope* lmScope, VMA relocVMA)
{
  // INVARIANT: The parent of 'node' has been fully processed and
  // lives within a correctly located procedure frame.
  
  if (!node) { return; }

  CodeInfoToProcFrameMap frameMap;
  ProcFrameAndLoopToCSLoopMap loopMap;

  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfCodeNode* n = dynamic_cast<CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 
    
    // process this node
    IDynNode* n_dyn = dynamic_cast<IDynNode*>(n);
    if (n_dyn && (begVMA <= n_dyn->ip() && n_dyn->ip() <= endVMA)) {
      VMA ip = n_dyn->ip(); // for debuggers
      VMA curr_ip = ip - relocVMA;
      
      DIAG_DevIf(50) {
	CSProfCallSiteNode* p = node->AncestorCallSite();
	DIAG_DevMsg(0, "inferCallFrames: " << hex << ((p) ? p->ip() : 0) << " --> " << ip << dec);
      }
      
      CodeInfo* scope = lmScope->findByVMA(curr_ip);
      CodeInfo* ctxt = NULL;
      LoopScope* loop = NULL;
      if (scope) {
	ctxt = scope->CallingCtxt();
	// FIXME: should include PROC (for nested procs)
	ScopeInfo* x = scope->Ancestor(ScopeInfo::LOOP, ScopeInfo::ALIEN);
	loop = dynamic_cast<LoopScope*>(x);
      }

      // Add symbolic information to 'n'
      addSymbolicInfo(n, n_dyn, lmScope, ctxt, scope);

      // Find (or create) a procedure frame for 'n'.
      CSProfProcedureFrameNode* frame = 
	findOrCreateProcFrame(n_dyn, lmScope, ctxt, loop, curr_ip, 
			      frameMap, loopMap);
      
      // Find appropriate (new) parent context for 'node': the frame
      // or a loop within the frame
      CSProfCodeNode* newParent = frame;
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
      inferCallFrames(prof, n, begVMA, endVMA, lmScope, relocVMA);
    }
  }
}


// findOrCreateProcFrame: Find or create a procedure frame for 'node'
// given it's corresponding load module scope (lmScope), calling
// context scope (callingCtxt, a ProcScope or AlienScope) and
// statement-type scope.
// 
// Assumes that symbolic information has been added to node.
CSProfProcedureFrameNode*
findOrCreateProcFrame(IDynNode* node,
		      LoadModScope* lmScope, CodeInfo* callingCtxt,
		      LoopScope* loop,
		      VMA curr_ip,
		      CodeInfoToProcFrameMap& frameMap,
		      ProcFrameAndLoopToCSLoopMap& loopMap)
{
  CSProfProcedureFrameNode* frame = NULL;

  CodeInfo* toFind = (callingCtxt) ? callingCtxt : lmScope;
  
  CodeInfoToProcFrameMap::iterator it = frameMap.find(toFind);
  if (it != frameMap.end()) {
    frame = (*it).second;
  }
  else {
    // INVARIANT: 'node' has symbolic information

    // Find and create the frame.
    if (callingCtxt) {
      ProcScope* proc = callingCtxt->Proc();
      createFramesForProc(proc, node, frameMap, loopMap);

      // frame should now be in map
      CodeInfoToProcFrameMap::iterator it = frameMap.find(toFind);
      DIAG_Assert(it != frameMap.end(), "");
      frame = (*it).second;
    }
    else {
      frame = new CSProfProcedureFrameNode(NULL);
      addSymbolicInfo(frame, node, lmScope, NULL, NULL);
      frame->Link(node->proxy()->Parent());

      string nm = string("unknown(s)@") + StrUtil::toStr(curr_ip, 16); // FIXME
      //string nm = "unknown(s)@" + StrUtil::toStr(ip, 16);
      frame->SetProc(nm C_STR);
      
      frameMap.insert(std::make_pair(toFind, frame));
    }
  }
  
  return frame;
}


void 
createFramesForProc(ProcScope* proc, IDynNode* node_dyn,
		    CodeInfoToProcFrameMap& frameMap,
		    ProcFrameAndLoopToCSLoopMap& loopMap)
{
  CSProfProcedureFrameNode* frame;
  
  frame = new CSProfProcedureFrameNode(NULL);
  addSymbolicInfo(frame, node_dyn, NULL, proc, proc);
  frame->Link(node_dyn->proxy()->Parent());
  frameMap.insert(std::make_pair(proc, frame));

  loopifyFrame(frame, proc, frameMap, loopMap);
}



void
loopifyFrame(CSProfCodeNode* mirrorNode, CodeInfo* node,
	     CSProfProcedureFrameNode* frame,
	     CSProfLoopNode* enclLoop,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


// Given a procedure frame 'frame' and its associated context scope
// 'ctxtScope' (ProcScope or AlienScope), mirror ctxtScope's loop and
// context structure and add entries to 'frameMap' and 'loopMap.'
void
loopifyFrame(CSProfProcedureFrameNode* frame, CodeInfo* ctxtScope,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap)
{
  loopifyFrame(frame, ctxtScope, frame, NULL, frameMap, loopMap);
}


// 'frame' is the enclosing frame
// 'loop' is the enclosing loop
void
loopifyFrame(CSProfCodeNode* mirrorNode, CodeInfo* node,
	     CSProfProcedureFrameNode* frame,
	     CSProfLoopNode* enclLoop,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap)
{
  for (CodeInfoChildIterator it(node); it.Current(); ++it) {
    CodeInfo* n = it.CurCodeInfo();

    // Done: if we reach the natural base case or embedded proceedure
    if (n->IsLeaf() || n->Type() == ScopeInfo::PROC) {
      continue;
    }

    // - Flatten nested alien frames descending from a loop
    // - Presume that alien frames derive from callsites in the parent
    // frame, but flatten any nesting.

    CSProfCodeNode* mirrorRoot = mirrorNode;
    CSProfProcedureFrameNode* nxt_frame = frame;
    CSProfLoopNode* nxt_enclLoop = enclLoop;

    if (n->Type() == ScopeInfo::LOOP) {
      // loops are always children of the current root (loop or frame)
      CSProfLoopNode* lp = 
	new CSProfLoopNode(mirrorNode, n->begLine(), n->endLine(), 
			   n->UniqueId());
      loopMap.insert(std::make_pair(ProcFrameAndLoop(frame, n), lp));
      DIAG_DevMsgIf(0, hex << "(" << frame << " " << n << ") -> (" << lp << ")" << dec);

      mirrorRoot = lp;
      nxt_enclLoop = lp;
    }
    else if (n->Type() == ScopeInfo::ALIEN) {
      CSProfProcedureFrameNode* fr = new CSProfProcedureFrameNode(NULL);
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
addSymbolicInfo(CSProfCodeNode* n, IDynNode* n_dyn,
		LoadModScope* lmScope, CodeInfo* callingCtxt, CodeInfo* scope)
{
  if (!n) {
    return;
  }

  //Diag_Assert(logic::equiv(callingCtxt == NULL, scope == NULL));
  if (scope) {
    bool isAlien = (callingCtxt->Type() == ScopeInfo::ALIEN);
    const std::string& fnm = (isAlien) ? 
      dynamic_cast<AlienScope*>(callingCtxt)->fileName() :
      callingCtxt->File()->name();

    n->SetFile(fnm C_STR);
#if (FIXME_ADD_ASSOC_TAGS)
    std::string nm = callingCtxt->name() C_STR;
    if (n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
      nm += " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
	+ ", " + StrUtil::toStr(n_dyn->ip(), 16) + ") [" 
	+ n_dyn->assocInfo_str() + "]";
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
    n->SetFile(lmScope->name() C_STR);
    n->SetLine(0);
    n->SetFileIsText(false);
    n->structureId() = lmScope->UniqueId();
  }
}


//***************************************************************************
// Routines for Inferring Call Frames (based on LM)
//***************************************************************************

typedef std::map<string, CSProfProcedureFrameNode*> StringToProcFrameMap;

void addSymbolicInfo(IDynNode* n, binutils::LM* lm);

void inferCallFrames(CSProfile* prof, CSProfNode* node, 
		     VMA begVMA, VMA endVMA, binutils::LM* lm);

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
inferCallFrames(CSProfile* prof, VMA begVMA, VMA endVMA, binutils::LM *lm)
{
  CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return; }
  
  DIAG_MsgIf(DBG_NORM_PROC_FRAME, "start normalizing same procedure children");
  inferCallFrames(prof, csproftree->root(), begVMA, endVMA, lm);
}


void 
inferCallFrames(CSProfile* prof, CSProfNode* node, 
		VMA begVMA, VMA endVMA, binutils::LM* lm)
{
  if (!node) { return; }

  // Use this set to determine if we have a duplicate source line
  StringToProcFrameMap frameMap;

  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfCodeNode* n = dynamic_cast<CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 

    // ------------------------------------------------------------
    // recur 
    // ------------------------------------------------------------
    if (!n->IsLeaf()) {
      inferCallFrames(prof, n, begVMA, endVMA, lm);
    }

    // ------------------------------------------------------------
    // process this node if within the current load module
    //   (and of the correct type: IDynNode!)
    // ------------------------------------------------------------
    IDynNode* n_dyn = dynamic_cast<IDynNode*>(n);
    if (n_dyn && (begVMA <= n_dyn->ip() && n_dyn->ip() <= endVMA)) {
      VMA curr_ip = n_dyn->ip();
      DIAG_MsgIf(DBG_NORM_PROC_FRAME, "analyzing node " << n->GetProc()
		 << hex << " " << curr_ip);
      
      addSymbolicInfo(n_dyn, lm);
      
      string myid = n->GetFile() + n->GetProc();
      
      StringToProcFrameMap::iterator it = frameMap.find(myid);
      CSProfProcedureFrameNode* frame;
      if (it != frameMap.end()) { 
	// found 
	frame = (*it).second;
      } 
      else {
	// no entry found -- add
	frame = new CSProfProcedureFrameNode(NULL);
	frame->SetFile(n->GetFile() C_STR);
	
	string frameNm = n->GetProc();
	if (frameNm.empty()) {
	  frameNm = string("unknown(~s)@") + StrUtil::toStr(curr_ip, 16);
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
	  lm->GetProcFirstLineInfo(curr_ip, n_dyn->opIndex(), begLn);
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
addSymbolicInfo(CSProfile* prof, VMA begVMA, VMA endVMA, binutils::LM* lm)
{
  VMA curr_ip; 

  /* point to the first load module in the Epoch table */
  CSProfTree* tree = prof->Getcct();
  CSProfNode* root = tree->GetRoot();
  
  for (CSProfNodeIterator it(root); it.CurNode(); ++it) {
    CSProfNode* n = it.CurNode();
    CSProfCodeNode* nn = dynamic_cast<CSProfCodeNode*>(n);
    
    if (nn && (nn->GetType() == CSProfNode::STATEMENT 
	       || nn->GetType() == CSProfNode::CALLSITE)
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
addSymbolicInfo(IDynNode* n_dyn, binutils::LM* lm)
{
  if (!n_dyn) {
    return;
  }

  CSProfCodeNode* n = n_dyn->proxy();

  string func, file;
  SrcFile::ln srcLn;
  lm->GetSourceFileInfo(n_dyn->ip(), n_dyn->opIndex(), func, file, srcLn);
  func = GetBestFuncName(func);
#if (FIXME_ADD_ASSOC_TAGS)
  if (!func.empty() && n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
    func +=  " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
      + ", " + StrUtil::toStr(n_dyn->ip(), 16) + ") [" 
      + n_dyn->assocInfo_str() + "]";
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

bool coalesceCallsiteLeaves(CSProfile* prof);
void removeEmptyScopes(CSProfile* prof);

bool 
normalizeCSProfile(CSProfile* prof)
{
  // Remove duplicate/inplied file and procedure information from tree
  coalesceCallsiteLeaves(prof);
  removeEmptyScopes(prof);
  
  return (true);
}


// FIXME
// If pc values from the leaves map to the same source file info,
// coalese these leaves into one.
bool coalesceCallsiteLeaves(CSProfNode* node);

bool 
coalesceCallsiteLeaves(CSProfile* prof)
{
  CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return true; }
  
  return coalesceCallsiteLeaves(csproftree->root());
}


// FIXME
typedef std::map<string, CSProfStatementNode*> StringToCallSiteMap;

bool 
coalesceCallsiteLeaves(CSProfNode* node)
{
  bool noError = true;
  
  if (!node) { return noError; }

  // FIXME: Use this set to determine if we have a duplicate source line
  StringToCallSiteMap sourceInfoMap;
  
  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfCodeNode* child = dynamic_cast<CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(child, ""); // always true (FIXME)
    it++; // advance iterator -- it is pointing at 'child'
    
    bool inspect = (child->IsLeaf() 
		    && (child->GetType() == CSProfNode::STATEMENT));
    
    if (inspect) {
      // This child is a leaf. Test for duplicate source line info.
      CSProfStatementNode* c = dynamic_cast<CSProfStatementNode*>(child);
      string myid = string(c->GetFile()) + string(c->GetProc()) 
	+ StrUtil::toStr(c->GetLine());
      
      StringToCallSiteMap::iterator it = sourceInfoMap.find(myid);
      if (it != sourceInfoMap.end()) {
	// found -- we have a duplicate
	CSProfStatementNode* c1 = (*it).second;
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
removeEmptyScopes(CSProfNode* node);

void 
removeEmptyScopes(CSProfile* prof)
{
  CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return; }
  
  removeEmptyScopes(csproftree->root());
}


void 
removeEmptyScopes(CSProfNode* node)
{
  if (!node) { return; }

  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfNode* child = it.CurNode();
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any trimming for this tree's children
    removeEmptyScopes(child);

    // 2. Trim this node if necessary
    bool remove = (child->IsLeaf() 
		   && child->GetType() == CSProfNode::PROCEDURE_FRAME);
    if (remove) {
      child->Unlink(); // unlink 'child' from tree
      delete child;
    }
  }
}


//***************************************************************************
// 
//***************************************************************************

void copySourceFiles(CSProfNode* node, 
		     std::vector<string>& searchPaths,
		     const string& dbSourceDirectory);

void innerCopySourceFiles(CSProfNode* node, 
			  std::vector<string>& searchPaths,
			  const string& dbSourceDirectory);


/** Copies the source files for the executable into the database 
    directory. */
void 
copySourceFiles(CSProfile *prof, 
		std::vector<string>& searchPaths,
		const string& dbSourceDirectory) 
{
  CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return ; }

  copySourceFiles(csproftree->root(), searchPaths, dbSourceDirectory);
}


/** Perform DFS traversal of the tree nodes and copy
    the source files for the executable into the database
    directory. */
void 
copySourceFiles(CSProfNode* node, 
		std::vector<string>& searchPaths,
		const string& dbSourceDirectory) 
{
  xDEBUG(DEB_MKDIR_SRC_DIR, 
	 cerr << "descend into node" << std::endl;);

  if (!node) {
    return;
  }

  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.CurNode(); it++) {
    // recur 
    copySourceFiles(it.CurNode(), searchPaths, dbSourceDirectory);
  }

  innerCopySourceFiles(node, searchPaths, dbSourceDirectory);
}


void 
innerCopySourceFiles(CSProfNode* node, 
		     std::vector<string>& searchPaths,
		     const string& dbSourceDirectory)
{
  bool inspect; 
  string nodeSourceFile;
  string relativeSourceFile;
  string procedureFrame;
  bool sourceFileWasCopied = false;
  bool fileIsText = false; 

  switch(node->GetType()) {
  case CSProfNode::CALLSITE:
    {
      CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(node);
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

  case CSProfNode::STATEMENT:
    {
      CSProfStatementNode* st = dynamic_cast<CSProfStatementNode*>(node);
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

  case CSProfNode::PROCEDURE_FRAME:
    {
      CSProfProcedureFrameNode* pf = 
	dynamic_cast<CSProfProcedureFrameNode*>(node);
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
	    string testFilePath = dbSourceDirectory + normTestPath;
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
	      strcpy(filePathChr, dbSourceDirectory.c_str());
	      chdir(filePathChr);

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "after chdir " << std::endl;);

	      string subPath = dbSourceDirectory;
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
    case CSProfNode::CALLSITE:
      {
	CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(node);
	c->SetFile(relativeSourceFile);
      }
      break;
    case CSProfNode::STATEMENT:
      {
	CSProfStatementNode* st = dynamic_cast<CSProfStatementNode*>(node);
	st->SetFile(relativeSourceFile);
      }
      break;
    case CSProfNode::PROCEDURE_FRAME:
      {
	CSProfProcedureFrameNode* pf = 
	  dynamic_cast<CSProfProcedureFrameNode*>(node);
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


//***************************************************************************
// 
//***************************************************************************

void
ldmdSetUsedFlag(CSProfile* prof)
{
  VMA curr_ip;  
  
  CSProfTree* tree = prof->cct();
  CSProfNode* root = tree->root();
  
  for (CSProfNodeIterator it(root); it.CurNode(); ++it) {
    CSProfNode* n = it.CurNode();
    CSProfCallSiteNode* nn = dynamic_cast<CSProfCallSiteNode*>(n);
    
    if (nn)  {
      curr_ip = nn->ip();
      CSProfLDmodule * csploadmd = prof->epoch()->FindLDmodule(curr_ip);
      if (!(csploadmd->GetUsedFlag())) {
	csploadmd->SetUsedFlag(true);
      }
    }
  }
}

