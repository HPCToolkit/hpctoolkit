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

#include "CallPathProfile.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/prof-lean/hpcfile_csproflib.h>

#include <lib/support/Trace.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************


//***************************************************************************
// CSProfile
//***************************************************************************

namespace Prof {

CSProfile::CSProfile(uint i)
{
  m_metricdesc.resize(i);
  for (int i = 0; i < m_metricdesc.size(); ++i) {
    m_metricdesc[i] = new SampledMetricDesc();
  }
  m_cct  = new CSProfTree(this);
  m_epoch = NULL;
}


CSProfile::~CSProfile()
{
  for (int i = 0; i < m_metricdesc.size(); ++i) {
    delete m_metricdesc[i];
  }
  delete m_cct;
  delete m_epoch;
}


void 
CSProfile::merge(const CSProfile& y)
{
  uint x_numMetrics = numMetrics();

  // merge metrics
  for (int i = 0; i < y.numMetrics(); ++i) {
    const SampledMetricDesc* m = y.metric(i);
    m_metricdesc.push_back(new SampledMetricDesc(*m));
  }
  
  // merge epochs... [FIXME]
  
  m_cct->merge(y.cct(), &m_metricdesc, x_numMetrics, y.numMetrics());
}


void 
CSProfile::dump(std::ostream& os) const
{
  // FIXME
}


void 
CSProfile::ddump() const
{
  dump();
}

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

static bool
addPGMToCSProfTree(Prof::CSProfTree* tree, const char* progName);

static bool 
fixLeaves(Prof::CSProfNode* node);

static void
unrelocateIPs(Prof::CSProfile* prof);



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


  // Extract profiling info
  prof->name("[Profile Name]"); 
  
  // ------------------------------------------------------------
  // Epoch
  // ------------------------------------------------------------
  DIAG_WMsgIf(epochtbl.num_epoch > 1, fnm << ": only processing first epoch!");

  uint num_lm = epochtbl.epoch_modlist[0].num_loadmodule;

  Epoch* epoch = new Epoch(num_lm);

  for (int i = 0; i < num_lm; i++) { 
    const char* nm = epochtbl.epoch_modlist[0].loadmodule[i].name;
    VMA loadAddr = epochtbl.epoch_modlist[0].loadmodule[i].mapaddr;
    Epoch::LM* lm = new Epoch::LM(nm, loadAddr);
    epoch->lm_insert(lm);
  }
  epoch_table__free_data(&epochtbl, hpcfile_free_CB);

  epoch->compute_relocAmt();
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

  // Add PGM node to tree
  addPGMToCSProfTree(prof->cct(), prof->name().c_str());
  
  // Convert leaves (CSProfCallSiteNode) to Prof::CSProfStatementNodes
  // FIXME: There should be a better way of doing this.  We could
  // merge it with a normalization step...
  fixLeaves(prof->cct()->root());

  unrelocateIPs(prof);
  
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
  std::vector<hpcfile_metric_data_t> metricVec;
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
  using namespace Prof;

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
      
      CSProfStatementNode* newc = new CSProfStatementNode(NULL, c->metricdesc());
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


static void
unrelocateIPs(Prof::CSProfile* prof)
{
  Prof::CSProfNode* root = prof->cct()->root();
  const Prof::Epoch* epoch = prof->epoch();
  
  for (Prof::CSProfNodeIterator it(root); it.CurNode(); ++it) {
    Prof::CSProfNode* n = it.CurNode();

    Prof::IDynNode* n_dyn = dynamic_cast<Prof::IDynNode*>(n);
    if (n_dyn) {
      VMA ip = n_dyn->ip_real();
      Prof::Epoch::LM* lm = epoch->lm_find(ip);
      VMA ip_ur = ip - lm->relocAmt();

      n_dyn->lm_id(lm->id());
      n_dyn->ip(ip_ur, 0);
      lm->isUsed(true); // FIXME:
    }
  }
}

