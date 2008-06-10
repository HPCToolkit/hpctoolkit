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

#include "CallPath.hpp"

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
addPGMToCSProfTree(Prof::CSProfTree* tree, const char* progName);

static bool 
fixLeaves(Prof::CSProfNode* node);


//****************************************************************************
// Dump a CSProfTree 
//****************************************************************************

const char *CSPROFILEdtd =
#include <lib/xml/CSPROFILE.dtd.h>


void 
Analysis::CallPath::writeInDatabase(Prof::CSProfile* prof, const string& filenm) 
{
  filebuf fb;
  fb.open(filenm.c_str(), ios::out);
  std::ostream os(&fb);
  write(prof, os, true);
  fb.close();
}


/* version=1.0.2 for alpha memory profile---FMZ */
void
Analysis::CallPath::write(Prof::CSProfile* prof, std::ostream& os, bool prettyPrint)
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
    const Prof::SampledMetricDesc* metric = prof->metric(i);
    os << "<METRIC shortName" << MakeAttrNum(i)
       << " nativeName" << MakeAttrStr(metric->name())
       << " period" << MakeAttrNum(metric->period())
       << " flags" << MakeAttrNum(metric->flags(), 16)
       << "/>\n";
  }
  os << "</CSPROFILEPARAMS>\n";

  os.flush();
  
  int dumpFlags = (Prof::CSProfTree::XML_TRUE); // CSProfTree::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= Prof::CSProfTree::COMPRESSED_OUTPUT; }
  
  prof->cct()->dump(os, dumpFlags);

  os << "</CSPROFILE>\n";
  os.flush();
}


//****************************************************************************
// Set of routines to build a scope tree while reading data file
//****************************************************************************

Prof::CSProfile* 
ReadProfile_CSPROF(const char* fnm) 
{
  using namespace Prof;

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

  Prof::CSProfile* prof = new Prof::CSProfile(num_metrics);
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

  Epoch* epoch = new Epoch(num_lm);

  for (int i = 0; i < num_lm; i++) { 
    Epoch::LM* lm = new Epoch::LM();
    lm->name(epochtbl.epoch_modlist[0].loadmodule[i].name);
    lm->loadAddr(epochtbl.epoch_modlist[0].loadmodule[i].mapaddr);
    
    //lm->loadAddrPref(epochtbl.epoch_modlist[0].loadmodule[i].vaddr);
    lm->isUsed(false);
    epoch->lm(i, lm);
  }
  epoch_table__free_data(&epochtbl, hpcfile_free_CB);
  
  epoch->SortLoadmoduleByVMA(); 

  // Extract profiling info
  prof->name("[Profile Name]"); 
  
  // Extract metrics
  for (int i = 0; i < num_metrics; i++) {
    SampledMetricDesc* metric = prof->metric(i);
    metric->name(metadata.metrics[i].metric_name);
    metric->flags(metadata.metrics[i].flags);
    metric->period(metadata.metrics[i].sample_period);
  }

  prof->epoch(epoch);

  // We must deallocate pointer-data
  for (int i = 0; i < num_metrics; i++) {
    hpcfile_free_CB(metadata.metrics[i].metric_name);
  }
  hpcfile_free_CB(metadata.target);
  hpcfile_free_CB(metadata.metrics);

  // Add PGM node to tree
  addPGMToCSProfTree(prof->cct(), prof->name().c_str());
  
  // Convert leaves (CSProfCallSiteNode) to Prof::CSProfStatementNodes
  // FIXME: There should be a better way of doing this.  We could
  // merge it with a normalization step...
  fixLeaves(prof->cct()->root());
  
  return prof;
}


static void* 
cstree_create_node_CB(void* tree, 
		      hpcfile_cstree_nodedata_t* data)
{
  Prof::CSProfTree* my_tree = (Prof::CSProfTree*)tree; 
  
  VMA ip;
  ushort opIdx;
  convertOpIPToIP((VMA)data->ip, ip, opIdx);
  vector<hpcfile_metric_data_t> metricVec;
  metricVec.clear();
  for (uint i = 0; i < data->num_metrics; i++) {
    metricVec.push_back(data->metrics[i]);
  }

  DIAG_DevMsgIf(0, "cstree_create_node_CB: " << hex << data->ip << dec);
  Prof::CSProfCallSiteNode* n = 
    new Prof::CSProfCallSiteNode(NULL, data->as_info, ip, opIdx, data->lip.ptr,
				 &my_tree->metadata()->metricDesc(), metricVec);
  n->SetSrcInfoDone(false);
  
  // Initialize the tree, if necessary
  if (my_tree->empty()) {
    my_tree->root(n);
  }
  
  return n;
}


static void  
cstree_link_parent_CB(void* tree, void* node, void* parent)
{
  Prof::CSProfCallSiteNode* p = (Prof::CSProfCallSiteNode*)parent;
  Prof::CSProfCallSiteNode* n = (Prof::CSProfCallSiteNode*)node;
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
#if 0  
  opIdx = (ushort)(opIP & 0x3); // the mask ...00011 covers 0, 1 and 2
  ip = opIP - opIdx;
#else
  // FIXME: tallent: Sigh! The above is only true for IA64! Replace
  // with ISA::ConvertVMAToOpVMA, probably accessed through LM::isa
  ip = opIP;
  opIdx = 0;
#endif
}


bool
addPGMToCSProfTree(Prof::CSProfTree* tree, const char* progName)
{
  bool noError = true;

  // Add PGM node
  Prof::CSProfNode* n = tree->root();
  if (!n || n->GetType() != Prof::CSProfNode::PGM) {
    Prof::CSProfNode* root = new Prof::CSProfPgmNode(progName);
    if (n) { 
      n->Link(root); // 'root' is parent of 'n'
    }
    tree->root(root);
  }
  
  return noError;
}


static bool 
fixLeaves(Prof::CSProfNode* node)
{
  bool noError = true;
  
  if (!node) { return noError; }

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* child = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    Prof::IDynNode* child_dyn = dynamic_cast<Prof::IDynNode*>(child);
    DIAG_Assert(child && child_dyn, "");
    it++; // advance iterator -- it is pointing at 'child'

    DIAG_DevMsgIf(0, "fixLeaves: " << hex << child_dyn->ip() << dec);
    if (child->IsLeaf() && child->GetType() == Prof::CSProfNode::CALLSITE) {
      // This child is a leaf. Convert.
      Prof::CSProfCallSiteNode* c = dynamic_cast<Prof::CSProfCallSiteNode*>(child);
      
      Prof::CSProfStatementNode* newc = new Prof::CSProfStatementNode(NULL, c->metricdesc());
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


void
Epoch_SetLMUsed(Prof::CSProfile* prof)
{
  VMA curr_ip;  
  
  Prof::CSProfTree* tree = prof->cct();
  Prof::CSProfNode* root = tree->root();
  
  for (Prof::CSProfNodeIterator it(root); it.CurNode(); ++it) {
    Prof::CSProfNode* n = it.CurNode();
    Prof::CSProfCallSiteNode* nn = dynamic_cast<Prof::CSProfCallSiteNode*>(n);
    
    if (nn)  {
      curr_ip = nn->ip();
      Prof::Epoch::LM* csploadmd = prof->epoch()->lm_find(curr_ip);
      if (!(csploadmd->isUsed())) {
	csploadmd->isUsed(true);
      }
    }
  }
}



//****************************************************************************
// Routines for Inferring Call Frames (based on STRUCTURE information)
//****************************************************************************

typedef std::map<CodeInfo*, Prof::CSProfProcedureFrameNode*> CodeInfoToProcFrameMap;


typedef std::pair<Prof::CSProfProcedureFrameNode*, CodeInfo*> ProcFrameAndLoop;
inline bool 
operator<(const ProcFrameAndLoop& x, const ProcFrameAndLoop& y) 
{
  return ((x.first < y.first) || 
	  ((x.first == y.first) && (x.second < y.second)));
}

typedef std::map<ProcFrameAndLoop, Prof::CSProfLoopNode*> ProcFrameAndLoopToCSLoopMap;




void
addSymbolicInfo(Prof::CSProfCodeNode* n, Prof::IDynNode* n_dyn,
		LoadModScope* lmScope, CodeInfo* callingCtxt, CodeInfo* scope);

Prof::CSProfProcedureFrameNode*
findOrCreateProcFrame(Prof::IDynNode* node,
		      LoadModScope* lmScope, CodeInfo* callingCtxt,
		      LoopScope* loop,
		      VMA curr_ip,
		      CodeInfoToProcFrameMap& frameMap,
		      ProcFrameAndLoopToCSLoopMap& loopMap);

void
createFramesForProc(ProcScope* proc, Prof::IDynNode* dyn_node,
		    CodeInfoToProcFrameMap& frameMap,
		    ProcFrameAndLoopToCSLoopMap& loopMap);

void
loopifyFrame(Prof::CSProfProcedureFrameNode* frame, CodeInfo* ctxtScope,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


void 
inferCallFrames(Prof::CSProfile* prof, Prof::CSProfNode* node, 
		VMA begVMA, VMA endVMA, LoadModScope* lmScope, VMA relocVMA);


// inferCallFrames: Effectively create equivalence classes of frames
// for all the return addresses found under.
//
void
Analysis::CallPath::inferCallFrames(Prof::CSProfile* prof, VMA begVMA, VMA endVMA, 
		LoadModScope* lmScope, VMA relocVMA)
{
  Prof::CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return; }
  
  inferCallFrames(prof, csproftree->root(), begVMA, endVMA, 
		  lmScope, relocVMA);
}


void 
inferCallFrames(Prof::CSProfile* prof, Prof::CSProfNode* node, 
		VMA begVMA, VMA endVMA, LoadModScope* lmScope, VMA relocVMA)
{
  // INVARIANT: The parent of 'node' has been fully processed and
  // lives within a correctly located procedure frame.
  
  if (!node) { return; }

  CodeInfoToProcFrameMap frameMap;
  ProcFrameAndLoopToCSLoopMap loopMap;

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.Current(); /* */) {
    Prof::CSProfCodeNode* n = dynamic_cast<Prof::CSProfCodeNode*>(it.CurNode());
    DIAG_Assert(n, "Unexpected node type");
    
    it++; // advance iterator -- it is pointing at 'n' 
    
    // process this node
    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn && (begVMA <= n_dyn->ip() && n_dyn->ip() <= endVMA)) {
      VMA ip = n_dyn->ip(); // for debuggers
      VMA curr_ip = ip - relocVMA;
      
      DIAG_DevIf(50) {
	Prof::CSProfCallSiteNode* p = node->AncestorCallSite();
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
      Prof::CSProfProcedureFrameNode* frame = 
	findOrCreateProcFrame(n_dyn, lmScope, ctxt, loop, curr_ip, 
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
Prof::CSProfProcedureFrameNode*
findOrCreateProcFrame(Prof::IDynNode* node,
		      LoadModScope* lmScope, CodeInfo* callingCtxt,
		      LoopScope* loop,
		      VMA curr_ip,
		      CodeInfoToProcFrameMap& frameMap,
		      ProcFrameAndLoopToCSLoopMap& loopMap)
{
  Prof::CSProfProcedureFrameNode* frame = NULL;

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
      frame = new Prof::CSProfProcedureFrameNode(NULL);
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
createFramesForProc(ProcScope* proc, Prof::IDynNode* node_dyn,
		    CodeInfoToProcFrameMap& frameMap,
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
loopifyFrame(Prof::CSProfCodeNode* mirrorNode, CodeInfo* node,
	     Prof::CSProfProcedureFrameNode* frame,
	     Prof::CSProfLoopNode* enclLoop,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap);


// Given a procedure frame 'frame' and its associated context scope
// 'ctxtScope' (ProcScope or AlienScope), mirror ctxtScope's loop and
// context structure and add entries to 'frameMap' and 'loopMap.'
void
loopifyFrame(Prof::CSProfProcedureFrameNode* frame, CodeInfo* ctxtScope,
	     CodeInfoToProcFrameMap& frameMap,
	     ProcFrameAndLoopToCSLoopMap& loopMap)
{
  loopifyFrame(frame, ctxtScope, frame, NULL, frameMap, loopMap);
}


// 'frame' is the enclosing frame
// 'loop' is the enclosing loop
void
loopifyFrame(Prof::CSProfCodeNode* mirrorNode, CodeInfo* node,
	     Prof::CSProfProcedureFrameNode* frame,
	     Prof::CSProfLoopNode* enclLoop,
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

    Prof::CSProfCodeNode* mirrorRoot = mirrorNode;
    Prof::CSProfProcedureFrameNode* nxt_frame = frame;
    Prof::CSProfLoopNode* nxt_enclLoop = enclLoop;

    if (n->Type() == ScopeInfo::LOOP) {
      // loops are always children of the current root (loop or frame)
      Prof::CSProfLoopNode* lp = 
	new Prof::CSProfLoopNode(mirrorNode, n->begLine(), n->endLine(), 
				 n->UniqueId());
      loopMap.insert(std::make_pair(ProcFrameAndLoop(frame, n), lp));
      DIAG_DevMsgIf(0, hex << "(" << frame << " " << n << ") -> (" << lp << ")" << dec);

      mirrorRoot = lp;
      nxt_enclLoop = lp;
    }
    else if (n->Type() == ScopeInfo::ALIEN) {
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
    n->SetFile(lmScope->name() C_STR);
    n->SetLine(0);
    n->SetFileIsText(false);
    n->structureId() = lmScope->UniqueId();
  }
}


//***************************************************************************
// Routines for Inferring Call Frames (based on LM)
//***************************************************************************

typedef std::map<string, Prof::CSProfProcedureFrameNode*> StringToProcFrameMap;

void addSymbolicInfo(Prof::IDynNode* n, binutils::LM* lm);

void 
inferCallFrames(Prof::CSProfile* prof, Prof::CSProfNode* node, 
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
Analysis::CallPath::inferCallFrames(Prof::CSProfile* prof, VMA begVMA, VMA endVMA, 
				    binutils::LM *lm)
{
  Prof::CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return; }
  
  DIAG_MsgIf(DBG_NORM_PROC_FRAME, "start normalizing same procedure children");
  inferCallFrames(prof, csproftree->root(), begVMA, endVMA, lm);
}


void 
inferCallFrames(Prof::CSProfile* prof, Prof::CSProfNode* node, 
		VMA begVMA, VMA endVMA, binutils::LM* lm)
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
      inferCallFrames(prof, n, begVMA, endVMA, lm);
    }

    // ------------------------------------------------------------
    // process this node if within the current load module
    //   (and of the correct type: Prof::IDynNode!)
    // ------------------------------------------------------------
    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn && (begVMA <= n_dyn->ip() && n_dyn->ip() <= endVMA)) {
      VMA curr_ip = n_dyn->ip();
      DIAG_MsgIf(DBG_NORM_PROC_FRAME, "analyzing node " << n->GetProc()
		 << hex << " " << curr_ip);
      
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
addSymbolicInfo(Prof::CSProfile* prof, VMA begVMA, VMA endVMA, binutils::LM* lm)
{
  VMA curr_ip; 

  /* point to the first load module in the Epoch table */
  Prof::CSProfTree* tree = prof->Getcct();
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

bool coalesceCallsiteLeaves(Prof::CSProfile* prof);
void removeEmptyScopes(Prof::CSProfile* prof);

bool 
Analysis::CallPath::normalize(Prof::CSProfile* prof)
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
coalesceCallsiteLeaves(Prof::CSProfile* prof)
{
  Prof::CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return true; }
  
  return coalesceCallsiteLeaves(csproftree->root());
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
removeEmptyScopes(Prof::CSProfile* prof)
{
  Prof::CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return; }
  
  removeEmptyScopes(csproftree->root());
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
Analysis::CallPath::copySourceFiles(Prof::CSProfile *prof, 
				    std::vector<string>& searchPaths,
				    const string& dest_dir) 
{
  Prof::CSProfTree* csproftree = prof->cct();
  if (!csproftree) { return ; }

  copySourceFiles(csproftree->root(), searchPaths, dest_dir);
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

