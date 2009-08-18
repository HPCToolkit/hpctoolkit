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

#include <map>

#include <typeinfo>

#include <cstdio>

#include <alloca.h>


//*************************** User Include Files ****************************

#include <include/uint.h>

#include "CallPath-Profile.hpp"
#include "Struct-Tree.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

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
  m_loadmapMgr = new LoadMapMgr;
  m_cct = new CCT::Tree(this);
  m_structure = NULL;
}


Profile::~Profile()
{
  for (uint i = 0; i < m_metricdesc.size(); ++i) {
    delete m_metricdesc[i];
  }
  delete m_loadmapMgr;
  delete m_cct;
  delete m_structure;
}


void 
Profile::merge(Profile& y, bool isSameThread)
{
  DIAG_Assert(!m_structure && !y.m_structure, "Profile::merge: profiles should not have structure yet!");

  // -------------------------------------------------------
  // merge metrics
  // -------------------------------------------------------
  uint x_numMetrics = numMetrics();
  uint x_newMetricBegIdx = 0;
  uint y_newMetrics   = 0;

  if (!isSameThread) {
    // new metrics columns
    x_newMetricBegIdx = x_numMetrics;
    y_newMetrics   = y.numMetrics();

    for (uint i = 0; i < y.numMetrics(); ++i) {
      const SampledMetricDesc* m = y.metric(i);
      addMetric(new SampledMetricDesc(*m));
    }
  }
  
  // -------------------------------------------------------
  // merge LoadMaps
  //
  // Post-INVARIANT: y's cct refers to x's LoadMapMgr
  // -------------------------------------------------------
  std::vector<LoadMap::MergeChange> mergeChg = 
    m_loadmapMgr->merge(*y.loadMapMgr());
  y.cct_canonicalizePostMerge(mergeChg);

  // -------------------------------------------------------
  // merge CCTs
  // -------------------------------------------------------
  m_cct->merge(y.cct(), &m_metricdesc, x_newMetricBegIdx, y_newMetrics);
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

  if (m_loadmapMgr) {
    m_loadmapMgr->dump(os);
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

static Prof::CCT::ANode* 
cct_makeNode(Prof::CCT::Tree* cct, uint32_t id,
	     hpcfile_cstree_nodedata_t* data);

static void
cct_fixRoot(Prof::CCT::Tree* tree, const char* progName);

static void
cct_fixLeaves(Prof::CCT::ANode* node);

static void*
hpcfmt_alloc(size_t sz);

static void
hpcfmt_free(void* mem);

//***************************************************************************

namespace Prof {

namespace CallPath {


// TODO: Analysis::Raw::writeAsText_callpath() should use this
// routine for textual dumping when the format is sanitized.

Profile* 
Profile::make(const char* fnm, FILE* outfs) 
{
  int ret;

  FILE* fs = hpcio_open_r(fnm);
  if (!fs) { 
    DIAG_Throw("error opening file");
  }

  // ------------------------------------------------------------
  // Read header
  // ------------------------------------------------------------
  hpcrun_fmt_hdr_t hdr;
  ret = hpcrun_fmt_hdr_fread(&hdr, fs, hpcfmt_alloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'fmt-hdr'");
  }

  // TODO: extract target field from nvpairs in hdr:
  //   char *target = hpcfmt_nvpair_search(&hdr.nvps, "target");

  // FIXME: hpcrun_fmt_hdr_free()

  // ------------------------------------------------------------
  // Read each epoch and merge them to form one Profile
  // ------------------------------------------------------------
  
  Profile* prof = NULL;

  while ( !feof(fs) ) {

    Profile* myprof = NULL;

    try {
      string locStr = fnm; // ":epoch " + 1;
      ret = hpcrun_fmt_epoch_fread(myprof, fs, locStr, outfs);
      if (ret == HPCFMT_EOF) {
	break;
      }
    }
    catch (const Diagnostics::Exception& x) {
      delete myprof;
      DIAG_Throw("error reading 'epoch': " << x.what());
    }

    if (! prof) {
      prof = myprof;
    }
    else {
      prof->merge(*myprof, /*isSameThread*/true);
    }
  }

  if (! prof) {
    prof = new Profile(0);
  }

  // ------------------------------------------------------------
  // Cleanup
  // ------------------------------------------------------------
  hpcio_close(fs);

  return prof;
}


int
Profile::hpcrun_fmt_epoch_fread(Profile* &prof, FILE* infs, 
				std::string locStr, FILE* outfs)
{
  using namespace Prof;

  int ret;

  // ------------------------------------------------------------
  // Read epoch data
  // ------------------------------------------------------------

  // ----------------------------------------
  // epoch-hdr
  // ----------------------------------------
  hpcrun_fmt_epoch_hdr_t ehdr;
  ret = hpcrun_fmt_epoch_hdr_fread(&ehdr, infs, hpcfmt_alloc);
  if (ret == HPCFMT_EOF) {
    return HPCFMT_EOF;
  }
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'epoch-hdr'");
  }

  // FIXME: hpcrun_fmt_epoch_hdr_free()
  
  // ----------------------------------------
  // metric-tbl
  // ----------------------------------------
  metric_tbl_t metric_tbl;
  ret = hpcrun_fmt_metric_tbl_fread(&metric_tbl, infs, hpcfmt_alloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }

  uint num_metrics = metric_tbl.len;
  
  // ----------------------------------------
  // loadmap
  // ----------------------------------------
  loadmap_t loadmap_tbl;
  ret = hpcrun_fmt_loadmap_fread(&loadmap_tbl, infs, hpcfmt_alloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'loadmap'");
  }
  
  // ------------------------------------------------------------
  // Create Profile
  // ------------------------------------------------------------

  prof = new Profile(num_metrics);
  prof->name("[Profile Name]");
  
  // ----------------------------------------
  // metric-tbl
  // ----------------------------------------

  DIAG_Msg(3, locStr << ": metrics found: " << num_metrics);

  metric_desc_t* m_lst = metric_tbl.lst;
  for (uint i = 0; i < num_metrics; i++) {
    SampledMetricDesc* metric = prof->metric(i);
    metric->name(m_lst[i].name);
    metric->flags(m_lst[i].flags);
    metric->period(m_lst[i].period);
  }

  hpcrun_fmt_metric_tbl_free(&metric_tbl, hpcfmt_free);

  // ----------------------------------------
  // loadmap
  // ----------------------------------------
  uint num_lm = loadmap_tbl.len;

  LoadMap loadmap(num_lm);

  // FIXME: do we need to iterate backward now?
  for (int i = num_lm - 1; i >= 0; --i) { 
    string nm = loadmap_tbl.lst[i].name;
    RealPathMgr::singleton().realpath(nm);
    VMA loadAddr = loadmap_tbl.lst[i].mapaddr;
    size_t sz = 0; //loadmap_tbl->epoch_modlist[loadmap_id].loadmodule[i].size;

    LoadMap::LM* lm = new LoadMap::LM(nm, loadAddr, sz);
    loadmap.lm_insert(lm);
  }

  DIAG_MsgIf(DBG, loadmap.toString());

  try {
    loadmap.compute_relocAmt();
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(locStr << "': Cannot fully process samples from unavailable load modules:\n" << x.what());
  }

  hpcrun_fmt_loadmap_free(&loadmap_tbl, hpcfmt_free);


  // ------------------------------------------------------------
  // CCT (cct)
  // ------------------------------------------------------------
  hpcrun_fmt_cct_fread(prof->cct(), num_metrics, infs, outfs);

  cct_fixRoot(prof->cct(), prof->name().c_str());  
  cct_fixLeaves(prof->cct()->root());

  prof->cct_canonicalize(loadmap); // initializes isUsed()

  std::vector<ALoadMap::MergeChange> mergeChg = 
    prof->loadMapMgr()->merge(loadmap);
  prof->cct_canonicalizePostMerge(mergeChg);

  return HPCFMT_OK;
}


void
Profile::hpcrun_fmt_cct_fread(CCT::Tree* cct, int num_metrics, 
			      FILE* infs, FILE* outfs)
{
  typedef std::map<int, CCT::ANode*> CCTIdToCCTNodeMap;

  DIAG_Assert(infs, "Bad file descriptor!");
  
  CCTIdToCCTNodeMap cctNodeMap;

  int ret = HPCFMT_ERR;

  if (outfs) {
    fprintf(outfs, "{cct:\n"); 
  }

  // ------------------------------------------------------------
  // Read num cct nodes
  // ------------------------------------------------------------
  uint64_t num_nodes = 0;
  hpcfmt_byte8_fread(&num_nodes, infs);

  // ------------------------------------------------------------
  // Read each CCT node
  // ------------------------------------------------------------

  hpcfile_cstree_node_t ndata;
  ndata.data.num_metrics = num_metrics;
  ndata.data.metrics = 
    (hpcrun_metric_data_t*)alloca(num_metrics * sizeof(hpcfmt_uint_t));

  for (uint i = 0; i < num_nodes; ++i) {

    // ----------------------------------------------------------
    // Read the node
    // ----------------------------------------------------------
    ret = hpcfile_cstree_node__fread(&ndata, infs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("Error reading CCT node " << ndata.id);
    }
    if (outfs) {
      hpcfile_cstree_node__fprint(&ndata, outfs, "  ");
    }

    // Find parent of node
    CCT::ANode* node_parent = NULL;
    if (ndata.id_parent != HPCRUN_FMT_CCTNode_NULL) {
      CCTIdToCCTNodeMap::iterator it = cctNodeMap.find(ndata.id_parent);
      if (it != cctNodeMap.end()) {
	node_parent = it->second;
      }
      else {
	DIAG_Throw("Cannot find parent for node " << ndata.id);	
      }
    }

    if ( !(ndata.id_parent < ndata.id) ) {
      DIAG_Throw("Invalid parent " << ndata.id_parent << " for node " << ndata.id);
    }

    // ----------------------------------------------------------
    // Create node and link to parent
    // ----------------------------------------------------------

    uint32_t id = (ndata.id & RETAIN_ID_FOR_TRACE_FLAG) ? ndata.id : 0;
    CCT::ANode* node = cct_makeNode(cct, id, &ndata.data);
    DIAG_DevMsgIf(0, "hpcrun_fmt_cct_fread: " << hex << node << " -> " << node_parent <<dec);

    if (node_parent) {
      node->Link(node_parent);
    }
    else {
      DIAG_Assert(cct->empty(), "Must only have one root node!");
      cct->root(node);
    }

    cctNodeMap.insert(std::make_pair(ndata.id, node));
  }

  if (outfs) {
    fprintf(outfs, "}\n"); 
  }
}


void
Profile::cct_canonicalize(const LoadMap& loadmap)
{
  CCT::ANode* root = cct()->root();
  
  for (CCT::ANodeIterator it(root); it.CurNode(); ++it) {
    CCT::ANode* n = it.CurNode();

    CCT::ADynNode* n_dyn = dynamic_cast<CCT::ADynNode*>(n);
    if (n_dyn) { // n_dyn->lm_id() == LoadMap::LM_id_NULL
      VMA ip = n_dyn->CCT::ADynNode::ip();
      LoadMap::LM* lm = loadmap.lm_find(ip);
      VMA ip_ur = ip - lm->relocAmt();
      DIAG_MsgIf(0, "cct_canonicalize: " << hex << ip << dec << " -> " << lm->id());

      n_dyn->lm_id(lm->id());
      n_dyn->ip(ip_ur, n_dyn->opIndex());
      lm->isUsed(true); // FIXME:
    }
  }
}


void 
Profile::cct_canonicalizePostMerge(std::vector<LoadMap::MergeChange>& mergeChg)
{
  CCT::ANode* root = cct()->root();
  
  for (CCT::ANodeIterator it(root); it.CurNode(); ++it) {
    CCT::ANode* n = it.CurNode();
    
    CCT::ADynNode* n_dyn = dynamic_cast<CCT::ADynNode*>(n);
    if (n_dyn) {

      LoadMap::LM_id_t y_lm_id = n_dyn->lm_id();
      for (uint i = 0; i < mergeChg.size(); ++i) {
	const LoadMap::MergeChange& chg = mergeChg[i];
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

static Prof::CCT::ANode*
cct_makeNode(Prof::CCT::Tree* cct, uint32_t id, 
	     hpcfile_cstree_nodedata_t* data)
{
  using namespace Prof;
  
  VMA ip = (VMA)data->ip; // tallent:FIXME: Use ISA::ConvertVMAToOpVMA
  ushort opIdx = 0;

  lush_lip_t* lip = new lush_lip_t;
  memcpy(lip, &data->lip, sizeof(lush_lip_t));

  std::vector<hpcrun_metric_data_t> metricVec;
  metricVec.clear();
  for (uint i = 0; i < data->num_metrics; i++) {
    metricVec.push_back(data->metrics[i]);
  }

  // tallent:FIXME: If this is an interior node that has non-zero
  // metric counts, then create an interior node without metrics
  // (associated with the node-id) and a leaf node with metrics
  // (that does not need to be recorded in the cctNodeMap).  Ensure
  // the tree merge algorithm will merge interior nodes with
  // interior nodes and leaf nodes with leaves.
  
  DIAG_DevMsgIf(0, "hpcrun_fmt_cct_fread: " << hex << data->ip << dec);
  CCT::Call* n = new CCT::Call(NULL, data->as_info, ip, opIdx, lip,
			       id, &cct->metadata()->metricDesc(), 
			       metricVec);
  return n;
}


// 1. Create a (PGM) root for the CCT
// 2. Remove the two outermost frames: 
//      "synthetic-root -> monitor_main"
static void
cct_fixRoot(Prof::CCT::Tree* cct, const char* progName)
{
  using namespace Prof;

  CCT::ANode* root = cct->root();

  // idempotent
  if (root && root->type() == CCT::ANode::TyRoot) {
    return;
  }

  CCT::ANode* newRoot = new CCT::Root(progName);

  // 1. find the splice point
  CCT::ANode* spliceRoot = root;
  if (root && root->ChildCount() == 1) {
    spliceRoot = root->firstChild();
  }
  
  // 2. splice: move all children of 'spliceRoot' to 'newRoot'
  if (spliceRoot) {
    for (CCT::ANodeChildIterator it(spliceRoot); it.Current(); /* */) {
      CCT::ANode* n = it.CurNode();
      it++; // advance iterator -- it is pointing at 'n'
      n->Unlink();
      n->Link(newRoot);
    }
    
    delete root; // N.B.: also deletes 'spliceRoot'
  }
  
  cct->root(newRoot);
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


static void* 
hpcfmt_alloc(size_t sz)
{
  return (new char[sz]);
}


static void  
hpcfmt_free(void* mem)
{
  delete[] (char*)mem;
}
