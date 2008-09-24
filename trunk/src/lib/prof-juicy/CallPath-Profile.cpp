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

//*************************** User Include Files ****************************

#include <include/general.h>

#include "CallPath-Profile.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/prof-lean/hpcfile_csproflib.h>

#include <lib/support/Trace.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************


//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {


Profile::Profile(uint numMetrics)
{
  m_metricdesc.resize(numMetrics);
  for (int i = 0; i < m_metricdesc.size(); ++i) {
    m_metricdesc[i] = new SampledMetricDesc();
  }
  m_cct = new CCT::Tree(this);
  m_epoch = NULL;
}


Profile::~Profile()
{
  for (int i = 0; i < m_metricdesc.size(); ++i) {
    delete m_metricdesc[i];
  }
  delete m_cct;
  delete m_epoch;
}


void 
Profile::merge(Profile& y)
{
  // -------------------------------------------------------
  // merge metrics
  // -------------------------------------------------------
  uint x_numMetrics = numMetrics();
  for (uint i = 0; i < y.numMetrics(); ++i) {
    const SampledMetricDesc* m = y.metric(i);
    addMetric(new SampledMetricDesc(*m));
  }
  
  // -------------------------------------------------------
  // merge epochs
  // -------------------------------------------------------
  std::vector<Epoch::MergeChange> mergeChg = m_epoch->merge(*y.epoch());
  y.cct_applyEpochMergeChanges(mergeChg);
  // INVARIANT: y's cct now refers to x's epoch

  // -------------------------------------------------------
  // merge cct
  // -------------------------------------------------------
  m_cct->merge(y.cct(), &m_metricdesc, x_numMetrics, y.numMetrics());
}


void 
Profile::dump(std::ostream& os) const
{
  // FIXME
}


void 
Profile::ddump() const
{
  dump();
}


} // namespace CallPath

} // namespace Prof


//***************************************************************************
//
//***************************************************************************

extern "C" {
  static void* cstree_create_node_CB(void* tree, 
				     hpcfile_cstree_nodedata_t* data);
  static void  cstree_link_parent_CB(void* tree, void* node, void* parent);

  static void* hpcfile_alloc_CB(size_t sz);
  static void  hpcfile_free_CB(void* mem);
}

static void 
convertOpIPToIP(VMA opIP, VMA& ip, ushort& opIdx);

static void
cct_fixRoot(Prof::CCT::Tree* tree, const char* progName);

static void
cct_fixLeaves(Prof::CSProfNode* node);



namespace Prof {

namespace CallPath {


Profile* 
Profile::make(const char* fnm) 
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
  
  DIAG_Msg(3, fnm << ": metrics found: " << num_metrics);

  CallPath::Profile* prof = new CallPath::Profile(num_metrics);
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


  // Extract profiling info
  prof->name("[Profile Name]"); 
  
  // ------------------------------------------------------------
  // Epoch
  // ------------------------------------------------------------
  DIAG_WMsgIf(epochtbl.num_epoch > 1, fnm << ": only processing first epoch!");

  uint num_lm = epochtbl.epoch_modlist[0].num_loadmodule;

  Epoch* epoch = new Epoch(num_lm);

  for (int i = num_lm - 1; i >= 0; --i) { 
    const char* nm = epochtbl.epoch_modlist[0].loadmodule[i].name;
    VMA loadAddr = epochtbl.epoch_modlist[0].loadmodule[i].mapaddr;
    size_t sz = 0; //epochtbl.epoch_modlist[0].loadmodule[i].size;
    Epoch::LM* lm = new Epoch::LM(nm, loadAddr, sz);
    epoch->lm_insert(lm);
  }
  epoch_table__free_data(&epochtbl, hpcfile_free_CB);

  try {
    epoch->compute_relocAmt();
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg("While reading profile '" << fnm << "': Cannot fully process samples from unavailable load modules:\n" << x.what());
  }
  prof->epoch(epoch);

  // ------------------------------------------------------------
  // Extract metrics
  // ------------------------------------------------------------
  for (int i = 0; i < num_metrics; i++) {
    SampledMetricDesc* metric = prof->metric(i);
    metric->name(metadata.metrics[i].metric_name);
    metric->flags(metadata.metrics[i].flags);
    metric->period(metadata.metrics[i].sample_period);
  }

  // ------------------------------------------------------------
  // Cleanup
  // ------------------------------------------------------------
  for (int i = 0; i < num_metrics; i++) {
    hpcfile_free_CB(metadata.metrics[i].metric_name);
  }
  hpcfile_free_CB(metadata.target);
  hpcfile_free_CB(metadata.metrics);


  // ------------------------------------------------------------
  // Postprocess
  // ------------------------------------------------------------
  cct_fixRoot(prof->cct(), prof->name().c_str());  
  cct_fixLeaves(prof->cct()->root());

  prof->cct_canonicalize();
  
  return prof;
}


void
Profile::cct_canonicalize()
{
  Prof::CSProfNode* root = cct()->root();
  
  for (Prof::CSProfNodeIterator it(root); it.CurNode(); ++it) {
    Prof::CSProfNode* n = it.CurNode();

    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn) {
      VMA ip = n_dyn->IDynNode::ip();
      Prof::Epoch::LM* lm = epoch()->lm_find(ip);
      VMA ip_ur = ip - lm->relocAmt();
      DIAG_MsgIf(0, "cct_canonicalize: " << hex << ip << dec << " -> " << lm->id());

      n_dyn->lm_id(lm->id());
      n_dyn->ip(ip_ur, n_dyn->opIndex());
      lm->isUsed(true); // FIXME:
    }
  }
}


void 
Profile::cct_applyEpochMergeChanges(std::vector<Epoch::MergeChange>& mergeChg)
{
  Prof::CSProfNode* root = cct()->root();
  
  for (Prof::CSProfNodeIterator it(root); it.CurNode(); ++it) {
    Prof::CSProfNode* n = it.CurNode();
    
    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn) {

      Epoch::LM_id_t y_lm_id = n_dyn->lm_id();
      for (uint i = 0; i < mergeChg.size(); ++i) {
	const Epoch::MergeChange& chg = mergeChg[i];
	if (chg.old_id == y_lm_id) {
	  n_dyn->lm_id(chg.new_id);
	  break;
	}
      }

    }
  }
}


} // namespace CallPath

} // namespace Prof



static void* 
cstree_create_node_CB(void* tree, 
		      hpcfile_cstree_nodedata_t* data)
{
  Prof::CCT::Tree* my_tree = (Prof::CCT::Tree*)tree; 
  
  VMA ip;
  ushort opIdx;
  convertOpIPToIP((VMA)data->ip, ip, opIdx);
  std::vector<hpcfile_metric_data_t> metricVec;
  metricVec.clear();
  for (uint i = 0; i < data->num_metrics; i++) {
    metricVec.push_back(data->metrics[i]);
  }

  DIAG_DevMsgIf(0, "cstree_create_node_CB: " << hex << data->ip << dec);
  Prof::CSProfCallSiteNode* n = 
    new Prof::CSProfCallSiteNode(NULL, data->as_info, ip, opIdx, data->lip.ptr, data->cpid,
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


// 1. Create a (PGM) root for the CCT
// 2. Remove the 'monitor_main' placeholder root 
static void
cct_fixRoot(Prof::CCT::Tree* tree, const char* progName)
{
  // Add PGM node
  Prof::CSProfNode* oldroot = tree->root();
  if (!oldroot || oldroot->GetType() != Prof::CSProfNode::PGM) {
    Prof::CSProfNode* newroot = new Prof::CSProfPgmNode(progName);

    if (oldroot) { 
      //oldroot->Link(newroot); // 'newroot' is parent of 'n'

      // Move all children of 'oldroot' to 'newroot'
      for (Prof::CSProfNodeChildIterator it(oldroot); it.Current(); /* */) {
	Prof::CSProfNode* n = it.CurNode();
	it++; // advance iterator -- it is pointing at 'n'
	n->Unlink();
	n->Link(newroot);
      }
      
      delete oldroot;
    }

    tree->root(newroot);
  }
}


// Convert leaves (CSProfCallSiteNode) to Prof::CSProfStatementNodes
//
// FIXME: There should be a better way of doing this.  Can it be
// avoided altogether, by reading the tree in correctly?
static void
cct_fixLeaves(Prof::CSProfNode* node)
{
  using namespace Prof;

  if (!node) { return; }

  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfCodeNode* child = dynamic_cast<CSProfCodeNode*>(it.CurNode());
    IDynNode* child_dyn = dynamic_cast<IDynNode*>(child);
    DIAG_Assert(child && child_dyn, "");
    it++; // advance iterator -- it is pointing at 'child'

    DIAG_DevMsgIf(0, "cct_fixLeaves: " << hex << child_dyn->ip() << dec);
    if (child->IsLeaf() && child->GetType() == CSProfNode::CALLSITE) {
      // This child is a leaf. Convert.
      CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(child);
      
      CSProfStatementNode* newc = new CSProfStatementNode(NULL, c->cpid(), c->metricdesc());
      *newc = *c;
      
      newc->LinkBefore(node->FirstChild()); // do not break iteration!
      c->Unlink();
      delete c;
    } 
    else if (!child->IsLeaf()) {
      cct_fixLeaves(child);
    }
  }
}

