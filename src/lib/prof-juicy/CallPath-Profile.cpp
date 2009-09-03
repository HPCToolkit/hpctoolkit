// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
//   $HeadURL$
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
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

#define DBG 0

//***************************************************************************


//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {


Profile::Profile(const std::string name, uint numMetrics)
{
  m_name = name;
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
      nm = ((typeid(*strct) == typeid(Struct::Alien)) ? 
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
  return (typeid(x) == typeid(Struct::File) || typeid(x) == typeid(Struct::Alien));
}


static bool 
writeXML_ProcFilter(const Struct::ANode& x, long type)
{
  return (typeid(x) == typeid(Struct::Proc) || typeid(x) == typeid(Struct::Alien));
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

static std::pair<Prof::CCT::ANode*, Prof::CCT::ANode*>
cct_makeNode(Prof::CCT::Tree* cct, uint32_t id,
	     hpcfile_cstree_nodedata_t* data);

//***************************************************************************

namespace Prof {

namespace CallPath {

Profile* 
Profile::make(const char* fnm, FILE* outfs) 
{
  int ret;

  FILE* fs = hpcio_open_r(fnm);
  if (!fs) {
    DIAG_Throw("error opening file");
  }

  Profile* prof = NULL;
  ret = hpcrun_fmt_fread(prof, fs, fnm, outfs);
  
  hpcio_close(fs);

  return prof;
}


int
Profile::hpcrun_fmt_fread(Profile* &prof, FILE* infs, 
			  std::string ctxtStr, FILE* outfs)
{
  int ret;

  // ------------------------------------------------------------
  // Read header
  // ------------------------------------------------------------
  hpcrun_fmt_hdr_t hdr;
  ret = hpcrun_fmt_hdr_fread(&hdr, infs, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'fmt-hdr'");
  }
  if (outfs) {
    hpcrun_fmt_hdr_fprint(&hdr, outfs);
  }


  string progNm;

  // search for known name-value pairs
  const char* val = NULL;
  val = hpcfmt_nvpair_search(&hdr.nvps, "program-name");
  if (val) {
    progNm = val;
  }

  hpcrun_fmt_hdr_free(&hdr, free);

  // ------------------------------------------------------------
  // Read each epoch and merge them to form one Profile
  // ------------------------------------------------------------
  
  prof = NULL;

  uint num_epochs = 0;
  while ( !feof(infs) ) {

    Profile* myprof = NULL;

    try {
      ctxtStr += ": epoch " + StrUtil::toStr(num_epochs + 1);
      ret = hpcrun_fmt_epoch_fread(myprof, infs, progNm, ctxtStr, outfs);
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

    num_epochs++;
  }

  if (! prof) {
    prof = new Profile(progNm, 0);
    prof->cct_canonicalize();
  }

  if (outfs) {
    fprintf(outfs, "\n{num-epochs: %d}\n", num_epochs);
  }

  return HPCFMT_OK;
}


int
Profile::hpcrun_fmt_epoch_fread(Profile* &prof, FILE* infs, 
				std::string progName, std::string ctxtStr,
				FILE* outfs)
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
  ret = hpcrun_fmt_epoch_hdr_fread(&ehdr, infs, malloc);
  if (ret == HPCFMT_EOF) {
    return HPCFMT_EOF;
  }
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'epoch-hdr'");
  }
  if (outfs) {
    hpcrun_fmt_epoch_hdr_fprint(&ehdr, outfs);
  }

  // ----------------------------------------
  // metric-tbl
  // ----------------------------------------
  metric_tbl_t metric_tbl;
  ret = hpcrun_fmt_metric_tbl_fread(&metric_tbl, infs, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }
  if (outfs) {
    hpcrun_fmt_metric_tbl_fprint(&metric_tbl, outfs);
  }

  uint num_metrics = metric_tbl.len;
  
  // ----------------------------------------
  // loadmap
  // ----------------------------------------
  loadmap_t loadmap_tbl;
  ret = hpcrun_fmt_loadmap_fread(&loadmap_tbl, infs, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'loadmap'");
  }
  if (outfs) {
    hpcrun_fmt_loadmap_fprint(&loadmap_tbl, outfs);
  }

  // ------------------------------------------------------------
  // Create Profile
  // ------------------------------------------------------------

  prof = new Profile(progName, num_metrics);
  
  // ----------------------------------------
  // metric-tbl
  // ----------------------------------------

  metric_desc_t* m_lst = metric_tbl.lst;
  for (uint i = 0; i < num_metrics; i++) {
    SampledMetricDesc* metric = prof->metric(i);
    metric->name(m_lst[i].name);
    metric->flags(m_lst[i].flags);
    metric->period(m_lst[i].period);
  }

  hpcrun_fmt_metric_tbl_free(&metric_tbl, free);

  // ----------------------------------------
  // loadmap
  // ----------------------------------------
  uint num_lm = loadmap_tbl.len;

  LoadMap loadmap(num_lm);

  for (uint i = 0; i < num_lm; ++i) { 
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
    DIAG_EMsg(ctxtStr << ": Cannot fully process samples from unavailable load modules:\n" << x.what());
  }

  hpcrun_fmt_loadmap_free(&loadmap_tbl, free);


  // ------------------------------------------------------------
  // CCT (cct)
  // ------------------------------------------------------------
  hpcrun_fmt_cct_fread(prof->cct(), ehdr.flags, num_metrics, infs, outfs);

  prof->cct_canonicalize();
  prof->cct_canonicalize(loadmap); // initializes isUsed()

  std::vector<ALoadMap::MergeChange> mergeChg = 
    prof->loadMapMgr()->merge(loadmap);
  prof->cct_canonicalizePostMerge(mergeChg);


  hpcrun_fmt_epoch_hdr_free(&ehdr, free);
  
  return HPCFMT_OK;
}


void
Profile::hpcrun_fmt_cct_fread(CCT::Tree* cct, epoch_flags_t flags,
			      int num_metrics, FILE* infs, FILE* outfs)
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
    ret = hpcfile_cstree_node__fread(&ndata, flags, infs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("Error reading CCT node " << ndata.id);
    }
    if (outfs) {
      hpcfile_cstree_node__fprint(&ndata, outfs, flags, "  ");
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

    std::pair<CCT::ANode*, CCT::ANode*> nodes = 
      cct_makeNode(cct, ndata.id, &ndata.data);
    CCT::ANode* node = nodes.first;
    CCT::ANode* node_sib = nodes.second;

    DIAG_DevMsgIf(0, "hpcrun_fmt_cct_fread: " << hex << node << " -> " << node_parent <<dec);

    if (node_parent) {
      node->Link(node_parent);
      if (node_sib) {
	node_sib->Link(node_parent);
      }
    }
    else {
      DIAG_Assert(cct->empty() && !node_sib, "Must only have one root node!");
      cct->root(node);
    }

    cctNodeMap.insert(std::make_pair(ndata.id, node));
  }

  if (outfs) {
    fprintf(outfs, "}\n"); 
  }
}


// 1. Create a (PGM) root for the CCT
// 2. Remove the two outermost frames: 
//      "synthetic-root -> monitor_main"
void
Profile::cct_canonicalize()
{
  using namespace Prof;

  CCT::ANode* root = m_cct->root();

  // idempotent
  if (root && typeid(*root) == typeid(CCT::Root)) {
    return;
  }

  CCT::ANode* newRoot = new CCT::Root(m_name);

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
  
  m_cct->root(newRoot);
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

static std::pair<Prof::CCT::ANode*, Prof::CCT::ANode*>
cct_makeNode(Prof::CCT::Tree* cct, uint32_t id_bits, 
	     hpcfile_cstree_nodedata_t* data)
{
  using namespace Prof;

  // ----------------------------------------------------------
  // Gather node parameters
  // ----------------------------------------------------------
  bool isLeaf = false;
  uint cpId = 0;

  int id_tmp = (int)id_bits;
  if (id_tmp < 0) {
    isLeaf = true;
    id_tmp = -id_tmp;
  }
  if (hpcrun_fmt_do_retain_id(id_bits)) {
    cpId = id_tmp;
  }

  VMA ip = (VMA)data->ip; // tallent:FIXME: Use ISA::ConvertVMAToOpVMA
  ushort opIdx = 0;

  lush_lip_t* lip = NULL;
  if (!lush_lip_eq(&data->lip, &lush_lip_NULL)) {
    lip = new lush_lip_t;
    memcpy(lip, &data->lip, sizeof(lush_lip_t));
  }

  bool hasMetrics = false;
  std::vector<hpcrun_metric_data_t> metricVec(data->num_metrics);
  for (uint i = 0; i < data->num_metrics; i++) {
    hpcrun_metric_data_t m = data->metrics[i];
    metricVec[i] = m;
    if (!hpcrun_metric_data_iszero(m)) {
      hasMetrics = true;
    }
  }

  DIAG_DevMsgIf(0, "cct_makeNode: " << hex << data->ip << dec);


  // ----------------------------------------------------------
  // Create nodes.  
  //
  // Note that it is possible for an interior node to have
  // a non-zero metric count.  If this is the case, the node should be
  // split into two sibling nodes: 1) an interior node with metrics
  // == 0 (that has cpId == 0 *and* that is the primary return node);
  // and 2) a leaf node with the metrics and the cpId.
  // ----------------------------------------------------------
  Prof::CCT::ANode* n = NULL;
  Prof::CCT::ANode* n_leaf = NULL;

  if (hasMetrics || isLeaf) {
    n = new CCT::Stmt(NULL, cpId, data->as_info, ip, opIdx, lip,
		      &cct->metadata()->metricDesc(), metricVec);
  }

  if (!isLeaf) {
    if (hasMetrics) {
      n_leaf = n;

      std::vector<hpcrun_metric_data_t> metricVec0(data->num_metrics);
      n = new CCT::Call(NULL, 0, data->as_info, ip, opIdx, lip,
			&cct->metadata()->metricDesc(), metricVec0);
    }
    else {
      n = new CCT::Call(NULL, cpId, data->as_info, ip, opIdx, lip,
			&cct->metadata()->metricDesc(), metricVec);
    }
  }

  return std::make_pair(n, n_leaf);
}

