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

#include <string>
using std::string;

#include <typeinfo>

#include <cstdio>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>


//*************************** User Include Files ****************************

#include <include/uint.h>

#include "CallPath-Profile.hpp"
#include "Struct-Tree.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/prof-lean/hpcfile_csprof.h>
#include <lib/prof-lean/hpcfile_cstree.h>

#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.hpp>


//*************************** Forward Declarations **************************

#define DBG 0

//***************************************************************************


//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {


Profile::Profile(uint numMetrics)
{
  m_metricdesc.resize(numMetrics);
  for (uint i = 0; i < m_metricdesc.size(); ++i) {
    m_metricdesc[i] = new SampledMetricDesc();
  }
  m_cct = new CCT::Tree(this);
  m_epoch = NULL;
  m_structure = NULL;
}


Profile::~Profile()
{
  for (uint i = 0; i < m_metricdesc.size(); ++i) {
    delete m_metricdesc[i];
  }
  delete m_cct;
  delete m_epoch;
  delete m_structure;
}


void 
Profile::merge(Profile& y)
{
  DIAG_Assert(!m_structure && !y.m_structure, "Profile::merge: profiles should not have structure yet!");

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
writeXML_help(std::ostream& os, const char* entry_nm, 
	      Struct::Tree* structure, const Struct::ANodeFilter* filter,
	      int type)
{
  Struct::ANode* root = structure ? structure->root() : NULL;
  if (!root) {
    return;
  }

  for (Struct::ANodeIterator it(root, filter); it.Current(); ++it) {
    Struct::ANode* strct = it.CurNode();
    
    uint id = strct->id();
    const char* nm = NULL;
    
    if (type == 1) { // LoadModule
      nm = strct->name().c_str();
    }
    else if (type == 2) { // File
      nm = ((strct->type() == Struct::ANode::TyALIEN) ? 
	    dynamic_cast<Struct::Alien*>(strct)->fileName().c_str() :
	    dynamic_cast<Struct::File*>(strct)->name().c_str());
    }
    else if (type == 3) { // Proc
      nm = strct->name().c_str();
    }
    else {
      DIAG_Die(DIAG_UnexpectedInput);
    }

    os << "    <" << entry_nm << " i" << MakeAttrNum(id) 
       << " n" << MakeAttrStr(nm) << "/>\n";
  }
}


static bool 
writeXML_FileFilter(const Struct::ANode& x, long type)
{
  return (x.type() == Struct::ANode::TyFILE || x.type() == Struct::ANode::TyALIEN);
}


static bool 
writeXML_ProcFilter(const Struct::ANode& x, long type)
{
  return (x.type() == Struct::ANode::TyPROC || x.type() == Struct::ANode::TyALIEN);
}


std::ostream& 
Profile::writeXML_hdr(std::ostream& os, int oFlags, const char* pre) const
{
  os << "  <MetricTable>\n";
  uint n_metrics = numMetrics();
  for (uint i = 0; i < n_metrics; i++) {
    const SampledMetricDesc* m = metric(i);
    os << "    <Metric i" << MakeAttrNum(i) 
       << " n" << MakeAttrStr(m->name()) << ">\n";
    os << "      <Info>" 
       << "<NV n=\"period\" v" << MakeAttrNum(m->period()) << "/>"
       << "<NV n=\"flags\" v" << MakeAttrNum(m->flags(), 16) << "/>"
       << "</Info>\n";
    os << "    </Metric>\n";
  }
  os << "  </MetricTable>\n";

  os << "  <LoadModuleTable>\n";
  writeXML_help(os, "LoadModule", m_structure, &Struct::ANodeTyFilter[Struct::ANode::TyLM], 1);
  os << "  </LoadModuleTable>\n";

  os << "  <FileTable>\n";
  Struct::ANodeFilter filt1(writeXML_FileFilter, "FileTable", 0);
  writeXML_help(os, "File", m_structure, &filt1, 2);
  os << "  </FileTable>\n";

  if ( !(oFlags & CCT::Tree::OFlg_Debug) ) {
    os << "  <ProcedureTable>\n";
    Struct::ANodeFilter filt2(writeXML_ProcFilter, "ProcTable", 0);
    writeXML_help(os, "Procedure", m_structure, &filt2, 3);
    os << "  </ProcedureTable>\n";
  }

  return os;
}


std::ostream&
Profile::dump(std::ostream& os) const
{
  os << m_name << std::endl;

  //m_metricdesc.dump(os);

  if (m_epoch) {
    m_epoch->dump(os);
  }

  if (m_cct) {
    m_cct->dump(os);
  }
  return os;
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

// ---------------------------------------------------------
// hpcfile_cstree_read() and helpers
// ---------------------------------------------------------

// Allocates space for a new tree node 'node' for the tree 'tree' and
// returns a pointer to 'node'.  The node should be initialized with
// data from 'data', but it should not be linked with any other node
// of the tree.  Note: The first call to this function creates the
// root node of the call stack tree; the user should record a pointer
// to this node (e.g. in 'tree') for subsequent access to the call
// stack tree.
typedef void* 
(*hpcfile_cstree_cb__create_node_fn_t)(void*, 
				       hpcfile_cstree_nodedata_t*);

// Given two tree nodes 'node' and 'parent' allocated with the above
// function and with 'parent' already linked into the tree, links
// 'node' into 'tree' such that 'parent' is the parent of 'node'.
// Note that while nodes have only one parent node, several nodes may
// have the same parent, indicating that the parent has multiple
// children.
typedef void (*hpcfile_cstree_cb__link_parent_fn_t)(void*, void*, void*);

static void* cstree_create_node_CB(void* tree, 
				   hpcfile_cstree_nodedata_t* data);
static void  cstree_link_parent_CB(void* tree, void* node, void* parent);

static void* hpcfile_alloc_CB(size_t sz);
static void  hpcfile_free_CB(void* mem);

int
hpcfile_cstree_read(FILE* fs, void* tree, 
		    int num_metrics,
		    hpcfile_cstree_cb__create_node_fn_t create_node_fn,
		    hpcfile_cstree_cb__link_parent_fn_t link_parent_fn,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn,
		    char* errbuf,
		    int errSz);


static void 
convertOpIPToIP(VMA opIP, VMA& ip, ushort& opIdx);

static void
cct_fixRoot(Prof::CCT::Tree* tree, const char* progName);

static void
cct_fixLeaves(Prof::CCT::ANode* node);


//***************************************************************************

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
    //return NULL;
  }
  
  uint num_metrics = metadata.num_metrics;
  uint num_ccts = metadata.num_ccts;
  
  DIAG_Msg(3, fnm << ": metrics found: " << num_metrics);
  DIAG_Msg(3, fnm << ": ccts found: " << num_ccts);

  CallPath::Profile* prof = new CallPath::Profile(num_metrics);
  if (num_ccts > 0) {
    const int errSz = 128;
    char errbuf[errSz];

    ret = hpcfile_cstree_read(fs, prof->cct(), num_metrics,
			      cstree_create_node_CB, cstree_link_parent_CB,
			      hpcfile_alloc_CB, hpcfile_free_CB,
			      errbuf, errSz);
    
    if (ret != HPCFILE_OK) {
      delete prof;
      DIAG_Throw(fnm << ": error reading calling context tree (HPC_CSTREE). [" 
		 << errbuf << "]");
      //return NULL;
    }
  }

  hpcfile_close(fs);


  // Extract profiling info
  prof->name("[Profile Name]"); 
  
  // ------------------------------------------------------------
  // Epoch
  // ------------------------------------------------------------
  DIAG_WMsgIf(epochtbl.num_epoch > 1, fnm << ": only processing last epoch!");
  const uint epoch_id  = (epochtbl.num_epoch - 1);

  uint num_lm = epochtbl.epoch_modlist[epoch_id].num_loadmodule;

  Epoch* epoch = new Epoch(num_lm);

  for (int i = num_lm - 1; i >= 0; --i) { 
    string nm = epochtbl.epoch_modlist[epoch_id].loadmodule[i].name;
    RealPathMgr::singleton().realpath(nm);
    VMA loadAddr = epochtbl.epoch_modlist[epoch_id].loadmodule[i].mapaddr;
    size_t sz = 0; //epochtbl.epoch_modlist[epoch_id].loadmodule[i].size;

    Epoch::LM* lm = new Epoch::LM(nm, loadAddr, sz);
    epoch->lm_insert(lm);
  }
  epoch_table__free_data(&epochtbl, hpcfile_free_CB);

  DIAG_MsgIf(DBG, epoch->toString());

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
  for (uint i = 0; i < num_metrics; i++) {
    SampledMetricDesc* metric = prof->metric(i);
    metric->name(metadata.metrics[i].metric_name);
    metric->flags(metadata.metrics[i].flags);
    metric->period(metadata.metrics[i].sample_period);
  }

  // ------------------------------------------------------------
  // Cleanup
  // ------------------------------------------------------------
  for (uint i = 0; i < num_metrics; i++) {
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
  CCT::ANode* root = cct()->root();
  
  for (CCT::ANodeIterator it(root); it.CurNode(); ++it) {
    CCT::ANode* n = it.CurNode();

    CCT::ADynNode* n_dyn = dynamic_cast<CCT::ADynNode*>(n);
    if (n_dyn) { // n_dyn->lm_id() == Epoch::LM_id_NULL
      VMA ip = n_dyn->CCT::ADynNode::ip();
      Epoch::LM* lm = epoch()->lm_find(ip);
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
  CCT::ANode* root = cct()->root();
  
  for (CCT::ANodeIterator it(root); it.CurNode(); ++it) {
    CCT::ANode* n = it.CurNode();
    
    CCT::ADynNode* n_dyn = dynamic_cast<CCT::ADynNode*>(n);
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


//***************************************************************************
// 
//***************************************************************************

// hpcfile_cstree_read: Given an empty (not non-NULL!) tree 'tree',
// reads tree nodes from the file stream 'fs' and constructs the tree
// using the user supplied callback functions.  See documentation
// below for interfaces for callback interfaces.
//
// The tree data is thoroughly checked for errors. Returns HPCFILE_OK
// upon success; HPCFILE_ERR on error.
// The errbuf is optional; upon an error it is filled.
int
hpcfile_cstree_read(FILE* fs, void* tree, 
		    int num_metrics,
		    hpcfile_cstree_cb__create_node_fn_t create_node_fn,
		    hpcfile_cstree_cb__link_parent_fn_t link_parent_fn,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn,
		    char* errbuf, int errSz)
{
  hpcfile_cstree_hdr_t fhdr;
  hpcfile_cstree_node_t tmp_node;
  int ret = HPCFILE_ERR;
  uint32_t tag;

  if (errbuf) { errbuf[0] = '\0'; }
  
  // A vector storing created tree nodes.  The vector indices correspond to
  // the nodes' persistent ids.
  void**       node_vec = NULL;
  lush_lip_t** lip_vec  = NULL;

  if (!fs) { return HPCFILE_ERR; }
  
  // Open file for reading; read and sanity check header
  ret = hpcfile_cstree_read_hdr(fs, &fhdr);
  if (ret != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  // node id upper bound (exclusive)
  int id_ub = fhdr.num_nodes + HPCFILE_CSTREE_ID_ROOT;

  // Allocate space for 'node_vec'
  if (fhdr.num_nodes != 0) {
    node_vec = (void**)alloc_fn(sizeof(void*) * id_ub);
    lip_vec  = (lush_lip_t**)alloc_fn(sizeof(void*) * id_ub);
    for (int i = 0; i < HPCFILE_CSTREE_ID_ROOT; ++i) {
      node_vec[i] = NULL;
      lip_vec[i]  = NULL;
    }
  }
  
  // Read each node, creating it and linking it to its parent 
  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = (hpcfile_metric_data_t*)alloc_fn(num_metrics * sizeof(hpcfile_uint_t));
  
  for (int i = HPCFILE_CSTREE_ID_ROOT; i < id_ub; ++i) {

    ret = hpcfile_tag__fread(&tag, fs);
    if (ret != HPCFILE_OK) { 
      return HPCFILE_ERR; 
    }

    // ----------------------------------------------------------
    // Read the LIP
    // ----------------------------------------------------------
    lush_lip_t* lip = NULL;
    hpcfile_uint_t lip_idx = i;
    
    if (tag == HPCFILE_TAG__CSTREE_LIP) {
      lip = (lush_lip_t*)alloc_fn(sizeof(lush_lip_t));
      ret = hpcfile_cstree_lip__fread(lip, fs);
      if (ret != HPCFILE_OK) { 
	return HPCFILE_ERR; 
      }

      ret = hpcfile_tag__fread(&tag, fs);
      if (ret != HPCFILE_OK) { 
	return HPCFILE_ERR; 
      }
    }

    lip_vec[lip_idx] = lip;

    // ----------------------------------------------------------
    // Read the node
    // ----------------------------------------------------------
    
    if ( !(tag == HPCFILE_TAG__CSTREE_NODE) ) {
      ret = HPCFILE_ERR;
      goto cleanup;
    }

    ret = hpcfile_cstree_node__fread(&tmp_node, fs);
    if (ret != HPCFILE_OK) {
      if (errbuf) { snprintf(errbuf, errSz, "Error after node %"PRIu64, tmp_node.id); }
      goto cleanup; // ret = HPCFILE_ERR
    }

    lip_idx = tmp_node.data.lip.id;
    tmp_node.data.lip.ptr = lip_vec[lip_idx];

    if (tmp_node.id_parent >= id_ub) { 
      if (errbuf) { snprintf(errbuf, errSz, "Invalid parent for node %"PRIu64, tmp_node.id); }
      ret = HPCFILE_ERR;
      goto cleanup;
    } 
    void* parent = node_vec[tmp_node.id_parent];

    // parent should already exist unless id == HPCFILE_CSTREE_ID_ROOT
    if (!parent && tmp_node.id != HPCFILE_CSTREE_ID_ROOT) {
      if (errbuf) { snprintf(errbuf, errSz, "Cannot find parent for node %"PRIu64, tmp_node.id); }
      ret = HPCFILE_ERR;
      goto cleanup;
    }

    // Create node and link to parent
    void* node = create_node_fn(tree, &tmp_node.data);
    node_vec[tmp_node.id] = node;

    if (parent) {
      link_parent_fn(tree, node, parent);
    }
  }

  free_fn(tmp_node.data.metrics);


  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  ret = HPCFILE_OK;
  
  // Close file and cleanup
 cleanup:
  if (node_vec) { free_fn(node_vec); }
  if (lip_vec)  { free_fn(lip_vec); }
  
  return ret;
}


static void* 
cstree_create_node_CB(void* a_tree, hpcfile_cstree_nodedata_t* data)
{
  using namespace Prof;

  CCT::Tree* tree = (CCT::Tree*)a_tree; 
  
  VMA ip;
  ushort opIdx;
  convertOpIPToIP((VMA)data->ip, ip, opIdx);

  std::vector<hpcfile_metric_data_t> metricVec;
  metricVec.clear();
  for (uint i = 0; i < data->num_metrics; i++) {
    metricVec.push_back(data->metrics[i]);
  }

  DIAG_DevMsgIf(0, "cstree_create_node_CB: " << hex << data->ip << dec);
  CCT::Call* n = new CCT::Call(NULL, data->as_info, ip, opIdx, data->lip.ptr,
			       data->cpid, &tree->metadata()->metricDesc(), 
			       metricVec);
  
  // Initialize the tree, if necessary
  if (tree->empty()) {
    tree->root(n);
  }
  
  return n;
}


static void  
cstree_link_parent_CB(void* tree, void* node, void* parent)
{
  using namespace Prof;

  CCT::Call* p = (CCT::Call*)parent;
  CCT::Call* n = (CCT::Call*)node;
  n->Link(p);
  
  DIAG_DevMsgIf(0, "cstree_link_parent_CB: parent(" << hex << p << ") child(" 
		<< n << ")" << dec);
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


//***************************************************************************
// 
//***************************************************************************

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
  using namespace Prof;

  // Add PGM node
  CCT::ANode* oldroot = tree->root();
  if (!oldroot || oldroot->type() != CCT::ANode::TyRoot) {
    CCT::ANode* newroot = new CCT::Root(progName);

    if (oldroot) { 
      //oldroot->Link(newroot); // 'newroot' is parent of 'n'

      // Move all children of 'oldroot' to 'newroot'
      for (CCT::ANodeChildIterator it(oldroot); it.Current(); /* */) {
	CCT::ANode* n = it.CurNode();
	it++; // advance iterator -- it is pointing at 'n'
	n->Unlink();
	n->Link(newroot);
      }
      
      delete oldroot;
    }

    tree->root(newroot);
  }
}


// Convert leaves (CCT::Call) to CCT::Stmt
//
// FIXME: There should be a better way of doing this.  Can it be
// avoided altogether, by reading the tree in correctly?
static void
cct_fixLeaves(Prof::CCT::ANode* node)
{
  using namespace Prof;

  if (!node) { return; }

  // For each immediate child of this node...
  for (CCT::ANodeChildIterator it(node); it.Current(); /* */) {
    CCT::ADynNode* x = dynamic_cast<CCT::ADynNode*>(it.CurNode());
    DIAG_Assert(x, "");
    it++; // advance iterator -- it is pointing at 'x'

    DIAG_DevMsgIf(0, "cct_fixLeaves: parent(" << hex << node << ") child(" << x
		  << "): " << x->ip() << dec);
    if (x->isLeaf() && typeid(*x) == typeid(CCT::Call)) {
      // This x is a leaf. Convert.
      CCT::Stmt* x_new = new CCT::Stmt(NULL, x->cpid(), x->metricdesc());
      *x_new = *(dynamic_cast<CCT::Call*>(x));
      
      x_new->Link(node);
      x->Unlink();
      delete x;
    }
    else if (!x->isLeaf()) {
      cct_fixLeaves(x);
    }
  }
}

