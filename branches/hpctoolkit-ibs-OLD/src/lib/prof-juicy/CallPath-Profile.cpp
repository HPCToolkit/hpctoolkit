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
// Copyright ((c)) 2002-2010, Rice University 
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

#include <cstring> // strcmp

#include <alloca.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

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


Profile::Profile(const std::string name)
{
  m_name = name;
  m_flags.bits = 0;
  m_measurementGranularity = 0;
  m_raToCallsiteOfst = 0;

  m_mMgr = new Metric::Mgr;
  m_isMetricMgrVirtual = false;

  m_loadmapMgr = new LoadMapMgr;

  m_cct = new CCT::Tree(this);

  m_structure = NULL;

  canonicalize();
}


Profile::~Profile()
{
  delete m_mMgr;
  delete m_loadmapMgr;
  delete m_cct;
  delete m_structure;
}


uint 
Profile::merge(Profile& y, int mergeTy, int dbgFlg)
{
  DIAG_Assert(!y.m_structure, "Profile::merge: source profile should not have structure yet!");

  // -------------------------------------------------------
  // merge name, flags, etc
  // -------------------------------------------------------

  // Note: these values can be 'null' if the hpcrun-fmt data had no epochs
  if (m_flags.bits == 0) {
    m_flags.bits = y.m_flags.bits;
  }
  else if (y.m_flags.bits == 0) {
    y.m_flags.bits = m_flags.bits;
  }

  if (m_measurementGranularity == 0) {
    m_measurementGranularity = y.m_measurementGranularity;
  }
  else if (y.m_measurementGranularity == 0) {
    y.m_measurementGranularity = m_measurementGranularity;
  }

  if (m_raToCallsiteOfst == 0) {
    m_raToCallsiteOfst = y.m_raToCallsiteOfst;
  }
  else if (y.m_raToCallsiteOfst == 0) {
    y.m_raToCallsiteOfst = m_raToCallsiteOfst;
  }

  DIAG_WMsgIf(m_flags.bits != y.m_flags.bits,
	      "CallPath::Profile::merge(): ignoring incompatible flags");
  DIAG_WMsgIf(m_measurementGranularity != y.m_measurementGranularity,
	      "CallPath::Profile::merge(): ignoring incompatible measurement-granularity: " << m_measurementGranularity << " vs. " << y.m_measurementGranularity);
  DIAG_WMsgIf(m_raToCallsiteOfst != y.m_raToCallsiteOfst,
	      "CallPath::Profile::merge(): ignoring incompatible RA-to-callsite-offset" << m_raToCallsiteOfst << " vs. " << y.m_raToCallsiteOfst);

  // -------------------------------------------------------
  // merge metrics 
  // -------------------------------------------------------
  uint x_newMetricBegIdx = 0, y_newMetrics = 0;
  uint firstMergedMetric =
    mergeMetrics(y, mergeTy, x_newMetricBegIdx, y_newMetrics);
  
  // -------------------------------------------------------
  // merge LoadMaps
  //
  // Post-INVARIANT: y's cct refers to x's LoadMapMgr
  // -------------------------------------------------------
  std::vector<LoadMap::MergeChange> mergeChg = 
    m_loadmapMgr->merge(*y.loadMapMgr());
  y.merge_fixCCT(mergeChg);

  // -------------------------------------------------------
  // merge CCTs
  // -------------------------------------------------------
  m_cct->merge(y.cct(), x_newMetricBegIdx, y_newMetrics, dbgFlg);

  return firstMergedMetric;
}


uint
Profile::mergeMetrics(Profile& y, int mergeTy, 
		      uint& x_newMetricBegIdx, uint& y_newMetrics)
{
  uint begMergeIdx = 0;

  x_newMetricBegIdx = 0; // first metric in y maps to (metricsMapTo)
  y_newMetrics      = 0; // number of new metrics y introduces

  DIAG_Assert(m_isMetricMgrVirtual == y.m_isMetricMgrVirtual,
	      "CallPath::Profile::merge(): incompatible metrics");

  // -------------------------------------------------------
  // Translate Merge_mergeMetricByName to a primitive merge type
  // -------------------------------------------------------
  if (mergeTy == Merge_mergeMetricByName) {
    uint mapsTo = m_mMgr->findGroup(*y.metricMgr());
    mergeTy = (mapsTo == Metric::Mgr::npos) ? Merge_createMetric : (int)mapsTo;
  }

  // -------------------------------------------------------
  // process primitive merge types
  // -------------------------------------------------------
  if (mergeTy == Merge_createMetric) {
    begMergeIdx = m_mMgr->size();

    for (uint i = 0; i < y.metricMgr()->size(); ++i) {
      const Metric::ADesc* m = y.metricMgr()->metric(i);
      m_mMgr->insert(m->clone());
    }

    if (!isMetricMgrVirtual()) {
      x_newMetricBegIdx = begMergeIdx;
      y_newMetrics      = y.metricMgr()->size();
    }
  }
  else if (mergeTy >= Merge_mergeMetricById) {
    begMergeIdx = (uint)mergeTy;

    if (!isMetricMgrVirtual()) {
      x_newMetricBegIdx = begMergeIdx;
      y_newMetrics      = 0; // no metric descriptors to insert
    }
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }

  return begMergeIdx;
}


void 
Profile::merge_fixCCT(std::vector<LoadMap::MergeChange>& mergeChg)
{
  // early exit for trivial case
  if (mergeChg.empty()) {
    return;
  }

  CCT::ANode* root = cct()->root();
  
  for (CCT::ANodeIterator it(root); it.Current(); ++it) {
    CCT::ANode* n = it.current();
    
    CCT::ADynNode* n_dyn = dynamic_cast<CCT::ADynNode*>(n);
    if (n_dyn) {
      lush_lip_t* lip = n_dyn->lip();

      LoadMap::LM_id_t lmId1, lmId2;
      lmId1 = n_dyn->lmId_real();
      lmId2 = (lip) ? lush_lip_getLMId(lip) : LoadMap::LM_id_NULL;
      
      for (uint i = 0; i < mergeChg.size(); ++i) {
	const LoadMap::MergeChange& chg = mergeChg[i];
	if (chg.old_id == lmId1) {
	  n_dyn->lmId_real(chg.new_id);
	  if (lmId2 == LoadMap::LM_id_NULL) {
	    break; // quick exit in the common case
	  }
	}
	if (chg.old_id == lmId2) {
	  lush_lip_setLMId(lip, chg.new_id);
	}
      }
    }
  }
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
    Struct::ANode* strct = it.current();
    
    uint id = strct->id();
    const char* nm = NULL;
    
    if (type == 1) { // LoadModule
      nm = strct->name().c_str();
    }
    else if (type == 2) { // File
      nm = ((typeid(*strct) == typeid(Struct::Alien)) ?
	    static_cast<Struct::Alien*>(strct)->fileName().c_str() :
	    static_cast<Struct::File*>(strct)->name().c_str());
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
Profile::writeXML_hdr(std::ostream& os, uint metricBeg, uint metricEnd,
		      int oFlags, const char* pfx) const
{
  typedef std::map<uint, string> UIntToStringMap;
  UIntToStringMap metricIdToFormula;

  os << "  <MetricTable>\n";
  for (uint i = metricBeg; i < metricEnd; i++) {
    const Metric::ADesc* m = m_mMgr->metric(i);
    const Metric::SampledDesc* m1 = dynamic_cast<const Metric::SampledDesc*>(m);
    const Metric::DerivedIncrDesc* m2 =
      dynamic_cast<const Metric::DerivedIncrDesc*>(m);

    // Metric
    os << "    <Metric i" << MakeAttrNum(i) 
       << " n" << MakeAttrStr(m->name());
    if (m1->fmt_flag() == 1) //add by Xu Liu
       os << " v=\"" << "final" << "\"" << " k=\"" << "long" << "\"";
    else
       os << " v=\"" << m->toValueTyStringXML() << "\"";
    os << " t=\"" << Prof::Metric::ADesc::ADescTyToXMLString(m->type()) << "\""
       << " show=\"" << ((m->isVisible()) ? "1" : "0") << "\"";
    if (m1->fmt_flag() == 1) //add by Xu Liu
       os << " fmt=\""<< "%x" << "\"";
    os << ">\n";

    // MetricFormula
    if (m2) {

      // 0. retrieve combine formula (each DerivedIncrDesc corresponds
      // to an 'accumulator')
      string combineFrm;
      if (m2->expr()) {
	combineFrm = m2->expr()->combineString1();

	if (m2->expr()->hasAccum2()) {
	  uint mId = m2->expr()->accum2Id();
	  string frm = m2->expr()->combineString2();
	  metricIdToFormula.insert(std::make_pair(mId, frm));
	}
      }
      else {
	// must represent accumulator 2
	uint mId = m2->id();
	UIntToStringMap::iterator it = metricIdToFormula.find(mId);
	DIAG_Assert((it != metricIdToFormula.end()), DIAG_UnexpectedInput);
	combineFrm = it->second;
      }

      // 1. MetricFormula: combine
      os << "      <MetricFormula t=\"combine\""
	 << " frm=\"" << combineFrm << "\"/>\n";

      // 2. MetricFormula: finalize
      if (m2->expr()) {
	os << "      <MetricFormula t=\"finalize\""
	   << " frm=\"" << m2->expr()->finalizeString() << "\"/>\n";
      }
    }
    
    // Info
    os << "      <Info>"
       << "<NV n=\"units\" v=\"events\"/>"; // or "samples" m->isUnitsEvents()
    if (m1) {
      os << "<NV n=\"period\" v" << MakeAttrNum(m1->period()) << "/>"
	 << "<NV n=\"flags\" v" << MakeAttrNum(m1->flags(), 16) << "/>";
    }
    os << "</Info>\n";
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

  m_mMgr->dump(os);

  m_loadmapMgr->dump(os);

  if (m_cct) {
    m_cct->dump(os, CCT::Tree::OFlg_DebugAll);
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
cct_makeNode(Prof::CallPath::Profile& prof,
	     const hpcrun_fmt_cct_node_t& nodeFmt,
	     uint rFlags, Prof::LoadMap* loadmap);

static void
fmt_cct_makeNode(hpcrun_fmt_cct_node_t& n_fmt, const Prof::CCT::ANode& n,
		 epoch_flags_t flags);


//***************************************************************************

namespace Prof {

namespace CallPath {

const char* Profile::FmtEpoch_NV_virtualMetrics = "is-virtual-metrics";


Profile* 
Profile::make(uint rFlags)
{
  Profile* prof = new Profile("[program-name]");

  if (rFlags & RFlg_virtualMetrics) {
    prof->isMetricMgrVirtual(true);
  }
  prof->canonicalize();

  return prof;
}


Profile* 
Profile::make(const char* fnm, uint rFlags, FILE* outfs)
{
  int ret;

  FILE* fs = hpcio_fopen_r(fnm);
  if (!fs) {
    DIAG_Throw("error opening file");
  }

  rFlags |= RFlg_hpcrunData; // TODO: for now assume an hpcrun file (verify!)

  Profile* prof = NULL;
  ret = fmt_fread(prof, fs, rFlags, fnm, fnm, outfs);
  
  hpcio_fclose(fs);

  return prof;
}


int
Profile::fmt_fread(Profile* &prof, FILE* infs, uint rFlags,
		   std::string ctxtStr, const char* filename, FILE* outfs)
{
  int ret;

  // ------------------------------------------------------------
  // hdr
  // ------------------------------------------------------------
  hpcrun_fmt_hdr_t hdr;
  ret = hpcrun_fmt_hdr_fread(&hdr, infs, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'fmt-hdr'");
  }
  if (outfs) {
    hpcrun_fmt_hdr_fprint(&hdr, outfs);
  }


  // ------------------------------------------------------------
  // epoch: Read each epoch and merge them to form one Profile
  // ------------------------------------------------------------
  
  prof = NULL;

  uint num_epochs = 0;
  while ( !feof(infs) ) {

    Profile* myprof = NULL;
    
    string myCtxtStr = "epoch " + StrUtil::toStr(num_epochs + 1);
    try {
      ctxtStr += ": " + myCtxtStr;
      ret = fmt_epoch_fread(myprof, infs, rFlags, &hdr.nvps, 
			    ctxtStr, filename, outfs);
      if (ret == HPCFMT_EOF) {
	break;
      }
    }
    catch (const Diagnostics::Exception& x) {
      delete myprof;
      DIAG_Throw("error reading " << myCtxtStr << ": " << x.what());
    }

    if (! prof) {
      prof = myprof;
    }
    else {
      prof->merge(*myprof, Profile::Merge_mergeMetricById);
    }

    num_epochs++;
  }

  if (!prof) {
    prof = make(rFlags); // make an empty profile
  }

  prof->canonicalize(rFlags);

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  if (outfs) {
    fprintf(outfs, "\n[You look fine today! (num-epochs: %d)]\n", num_epochs);
  }

  hpcrun_fmt_hdr_free(&hdr, free);

  return HPCFMT_OK;
}


int
Profile::fmt_epoch_fread(Profile* &prof, FILE* infs, uint rFlags,
			 HPCFMT_List(hpcfmt_nvpair_t)* hdrNVPairs,
			 std::string ctxtStr, const char* filename,
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
  ret = hpcrun_fmt_metricTbl_fread(&metric_tbl, infs, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }
  if (outfs) {
    hpcrun_fmt_metricTbl_fprint(&metric_tbl, outfs);
  }

  uint numMetricsSrc = metric_tbl.len;
  
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

  // ----------------------------------------
  // obtain meta information
  // ----------------------------------------

  const char* val;

  string progNm;
  val = hpcfmt_nvpair_search(hdrNVPairs, HPCRUN_FMT_NV_prog);
  if (val && strlen(val) > 0) {
    progNm = val;
  }

  string mpiRank, tid;
  //const char* jobid = hpcfmt_nvpair_search(hdrNVPairs, HPCRUN_FMT_NV_jobId);
  val = hpcfmt_nvpair_search(hdrNVPairs, HPCRUN_FMT_NV_mpiRank);
  if (val) { mpiRank = val; }
  val = hpcfmt_nvpair_search(hdrNVPairs, HPCRUN_FMT_NV_tid);
  if (val) { tid = val; }

  // FIXME: temporary for dual-interpretations
  bool isNewFormat = true; 
  val = hpcfmt_nvpair_search(hdrNVPairs, "nasty-message");
  if (val) { isNewFormat = false; }

  //val = hpcfmt_nvpair_search(ehdr.&nvps, "to-find");

  bool isVirtualMetrics = false;
  val = hpcfmt_nvpair_search(&(ehdr.nvps), FmtEpoch_NV_virtualMetrics);
  if (val && strcmp(val, "0") != 0) {
    isVirtualMetrics = true;
    rFlags |= RFlg_noMetricValues;
  }


  // ----------------------------------------
  // 
  // ----------------------------------------
  
  prof = new Profile(progNm);

  prof->m_flags = ehdr.flags;
  prof->m_measurementGranularity = ehdr.measurementGranularity;
  prof->m_raToCallsiteOfst = ehdr.raToCallsiteOfst;
  
  CCT::Tree::raToCallsiteOfst = prof->m_raToCallsiteOfst;

  
  // ----------------------------------------
  // add metrics
  // ----------------------------------------

  string m_sfx;
  if (!mpiRank.empty() && !tid.empty()) {
    m_sfx = "[" + mpiRank + "," + tid + "]";
  }
  else if (!mpiRank.empty()) {
    m_sfx = "[" + mpiRank + "]";
  }
  else if (!tid.empty()) {
    m_sfx = "[" + tid + "]";
  }

  if (rFlags & RFlg_noMetricSfx) {
    m_sfx = "";
  }

  metric_desc_t* m_lst = metric_tbl.lst;
  for (uint i = 0; i < numMetricsSrc; i++) {
    string nm = m_lst[i].name;
    string desc = m_lst[i].description;
    string profFile = (filename) ? filename : "";
    string profRelId = StrUtil::toStr(i);

    bool isInclExclPossible = true; // TODO: filter return counts
    
    // 1. Make 'regular'/'inclusive' metric descriptor
    Metric::SampledDesc* m = 
      new Metric::SampledDesc(nm, desc, m_lst[i].period, true/*isUnitsEvents*/,
			      profFile, profRelId, "HPCRUN");
    if ((rFlags & RFlg_makeInclExcl) && isInclExclPossible) {
      m->type(Metric::ADesc::TyIncl);
    }
    if (!m_sfx.empty()) {
      m->nameSfx(m_sfx);
    }
    m->flags(m_lst[i].flags);

    m->fmt_flag(m_lst[i].fmt_flag);
    
    prof->metricMgr()->insert(m);

    // 2. Make associated 'exclusive' descriptor, if applicable
    if ((rFlags & RFlg_makeInclExcl) && isInclExclPossible) {
      Metric::SampledDesc* m1 = 
	new Metric::SampledDesc(nm, desc, m_lst[i].period,
				true/*isUnitsEvents*/,
				profFile, profRelId, "HPCRUN");
      m1->type(Metric::ADesc::TyExcl);
      if (!m_sfx.empty()) {
	m1->nameSfx(m_sfx);
      }
      m1->flags(m_lst[i].flags);

      m1->fmt_flag(m_lst[i].fmt_flag);
      
      prof->metricMgr()->insert(m1);
    }
  }

  if (isVirtualMetrics || (rFlags & RFlg_virtualMetrics) ) {
    prof->isMetricMgrVirtual(true);
  }

  hpcrun_fmt_metricTbl_free(&metric_tbl, free);

  // ----------------------------------------
  // add loadmap
  // ----------------------------------------
  uint num_lm = loadmap_tbl.len;

  LoadMap loadmap(num_lm);

  for (uint i = 0; i < num_lm; ++i) { 
    string nm = loadmap_tbl.lst[i].name;
    RealPathMgr::singleton().realpath(nm);
    VMA loadAddr = loadmap_tbl.lst[i].mapaddr;
    size_t sz = 0;

    LoadMap::LM* lm = new LoadMap::LM(nm, loadAddr, sz);
    loadmap.lm_insert(lm);
    
    DIAG_Assert(lm->id() == i + 1, "FIXME: Profile::fmt_epoch_fread: Expect lm id's to be in order to support dual-interpretations.");
  }

  DIAG_MsgIf(DBG, loadmap.toString());

  try {
    loadmap.compute_relocAmt();
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(ctxtStr << ": Cannot fully process samples from unavailable load modules:\n" << x.what());
  }

  std::vector<ALoadMap::MergeChange> mergeChg = 
    prof->loadMapMgr()->merge(loadmap);
  DIAG_Assert(mergeChg.empty(), "Profile::fmt_epoch_fread: " << DIAG_UnexpectedInput);


  hpcrun_fmt_loadmap_free(&loadmap_tbl, free);


  // ------------------------------------------------------------
  // cct
  // ------------------------------------------------------------

  LoadMap* loadmap_p = (isNewFormat) ? NULL : &loadmap; // FIXME:temporary
  fmt_cct_fread(*prof, infs, rFlags, loadmap_p, numMetricsSrc, ctxtStr, outfs);

  hpcrun_fmt_epoch_hdr_free(&ehdr, free);
  
  return HPCFMT_OK;
}


int
Profile::fmt_cct_fread(Profile& prof, FILE* infs, uint rFlags,
		       LoadMap* loadmap, uint numMetricsSrc,
		       std::string ctxtStr, FILE* outfs)
{
  /*For multi-thread, we need to make sure malloc node in main thread (thread 0) 
 *  * can be reached by all other threads. So we use static structure to maintain
 *  maps in main thread*/
  static uint mainThread=0;

  typedef std::map<int, CCT::ANode*> CCTIdToCCTNodeMap;

  typedef std::multimap<CCT::ANode*, int> CCTIdToMallocIdMap; //(Xu)

  DIAG_Assert(infs, "Bad file descriptor!");
  
  CCTIdToCCTNodeMap cctNodeMap;

  CCTIdToMallocIdMap mallocNodeMap;//(Xu)
  static CCTIdToMallocIdMap mainMallocNodeMap;
  static CCTIdToCCTNodeMap mainCctNodeMap;

  int ret = HPCFMT_ERR;

  // ------------------------------------------------------------
  // Read number of cct nodes
  // ------------------------------------------------------------
  uint64_t numNodes = 0;
  hpcfmt_byte8_fread(&numNodes, infs);

  // ------------------------------------------------------------
  // Read each CCT node
  // ------------------------------------------------------------

  if (outfs) {
    fprintf(outfs, "[cct: (num-nodes: %"PRIu64")\n", numNodes);
  }

  CCT::Tree* cct = prof.cct();
  
  if (numNodes > 0) {
    delete cct->root();
    cct->root(NULL);
  }

  // N.B.: numMetricsSrc <= [numMetricsDst = prof.metricMgr()->size()]
  if (rFlags & RFlg_noMetricValues) {
    numMetricsSrc = 0;
  }

  hpcrun_fmt_cct_node_t nodeFmt;
  nodeFmt.num_malloc_id = 0; // init number of malloc nodes
  nodeFmt.num_metrics = numMetricsSrc;
  nodeFmt.metrics = (numMetricsSrc > 0) ? 
    (hpcrun_metricVal_t*)alloca(numMetricsSrc * sizeof(hpcrun_metricVal_t))
    : NULL;
  for (uint i = 0; i < numNodes; ++i) {

    // ----------------------------------------------------------
    // Read the node
    // ----------------------------------------------------------
    ret = hpcrun_fmt_cct_node_fread(&nodeFmt, prof.m_flags, infs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("Error reading CCT node " << nodeFmt.id);
    }
    if (outfs) {
      hpcrun_fmt_cct_node_fprint(&nodeFmt, outfs, prof.m_flags, "  ");
    }

    // Find parent of node
    CCT::ANode* node_parent = NULL;
    if (nodeFmt.id_parent != HPCRUN_FMT_CCTNodeId_NULL) {
      CCTIdToCCTNodeMap::iterator it = cctNodeMap.find(nodeFmt.id_parent);
      if (it != cctNodeMap.end()) {
	node_parent = it->second;
      }
      else {
	DIAG_Throw("Cannot find parent for node " << nodeFmt.id);	
      }
    }

    if ( !(nodeFmt.id_parent < nodeFmt.id) ) {
      DIAG_Throw("Invalid parent " << nodeFmt.id_parent << " for node " << nodeFmt.id);
    }


    // ----------------------------------------------------------
    // Create node and link to parent
    // ----------------------------------------------------------

    std::pair<CCT::ANode*, CCT::ANode*> n2 = 
      cct_makeNode(prof, nodeFmt, rFlags, loadmap);
    CCT::ANode* node = n2.first;
    CCT::ANode* node_sib = n2.second;

    DIAG_DevMsgIf(0, "fmt_cct_fread: " << hex << node << " -> " << node_parent << dec);

    if (node_parent) {
      node->link(node_parent);
      if (node_sib) {
        node_sib->setAssSet(nodeFmt.ass_set);
        if(nodeFmt.ass_set>=0)
          printf("ass_set = %d\n", nodeFmt.ass_set);
	node_sib->link(node_parent);
        if(nodeFmt.num_malloc_id>0) {
          node_sib->setMallocNodeNum(nodeFmt.num_malloc_id);
          node_sib->mallocLinks(nodeFmt.num_malloc_id);
          for (uint i=0; i<nodeFmt.num_malloc_id; i++) {
            mallocNodeMap.insert(std::make_pair(node_sib, nodeFmt.malloc_id_list[i]));
            if(mainThread == 0)//main thread
              mainMallocNodeMap.insert(std::make_pair(node_sib, nodeFmt.malloc_id_list[i]));
            printf("test %d => %d\n", nodeFmt.id, nodeFmt.malloc_id_list[i]);
          }
        }
      }
      else {
        node->setAssSet(nodeFmt.ass_set);
        if(nodeFmt.ass_set >= 0)
          printf("ass_set = %d\n", nodeFmt.ass_set);
        if(nodeFmt.num_malloc_id>0) {
          node->setMallocNodeNum(nodeFmt.num_malloc_id);
          node->mallocLinks(nodeFmt.num_malloc_id);
          for (uint i=0; i<nodeFmt.num_malloc_id; i++) {
            mallocNodeMap.insert(std::make_pair(node, nodeFmt.malloc_id_list[i]));
            if(mainThread == 0)//main thread
              mainMallocNodeMap.insert(std::make_pair(node_sib, nodeFmt.malloc_id_list[i]));
            printf("test %d => %d\n", nodeFmt.id, nodeFmt.malloc_id_list[i]);
          }
        }
      }
    }
    else {
      DIAG_AssertWarn(cct->empty(), ctxtStr << ": must only have one root node!");
      DIAG_AssertWarn(!node_sib, ctxtStr << ": root node cannot be split into interior and leaf!");
      cct->root(node);
    }
    cctNodeMap.insert(std::make_pair(nodeFmt.id, node));
    if(mainThread == 0)
      mainCctNodeMap.insert(std::make_pair(nodeFmt.id, node));
  }

    // ----------------------------------------------------------
    // Create links to malloc nodes
    // ----------------------------------------------------------

  // Traverse all nodes with malloc info on tree Xu Liu
  CCTIdToMallocIdMap::iterator itMalloc;
  for (itMalloc = mallocNodeMap.begin(); itMalloc != mallocNodeMap.end(); itMalloc++) {
    if ((*itMalloc).first->getMallocNodeNum() > 0) {
      printf("malloc node num is %d\n", (*itMalloc).first->getMallocNodeNum());
      CCT::ANode* node_malloc = NULL;
      if ((*itMalloc).second != HPCRUN_FMT_CCTNodeId_NULL) {
        printf("node %d => %d\n", (*itMalloc).first->id(), -(*itMalloc).second);
        if((*itMalloc).second < 0)//in bss section not heap
        {
          (*itMalloc).first->linkMalloc((*itMalloc).second);
          continue;
        }
        CCTIdToCCTNodeMap::iterator it1 = cctNodeMap.find((*itMalloc).second);
        if (it1 != cctNodeMap.end()) {
          node_malloc = it1->second;
        }
        else { //find in main thread
          it1 = mainCctNodeMap.find(-(*itMalloc).second);
          if (it1 != mainCctNodeMap.end()) {
            node_malloc = it1->second;
          }
          else {
            itMalloc->first->setMallocNodeNum(0);
//            DIAG_Throw("Cannot find malloc node for node " << nodeFmt.id);
          }
        }
      }
      
      (*itMalloc).first->linkMalloc(node_malloc);
    }
  }

  if (outfs) {
    fprintf(outfs, "]\n");
  }

  mainThread++; //indicate out of main thread
  return HPCFMT_OK;
}


//***************************************************************************

int
Profile::fmt_fwrite(const Profile& prof, FILE* fs, uint wFlags)
{
  // ------------------------------------------------------------
  // header
  // ------------------------------------------------------------
  hpcrun_fmt_hdr_fwrite(fs, 
			"TODO:hdr-name","TODO:hdr-value",
			NULL);

  // ------------------------------------------------------------
  // epoch
  // ------------------------------------------------------------
  fmt_epoch_fwrite(prof, fs, wFlags);

  return HPCFMT_OK;
}


int
Profile::fmt_epoch_fwrite(const Profile& prof, FILE* fs, uint wFlags)
{
  // ------------------------------------------------------------
  // epoch-hdr
  // ------------------------------------------------------------
  const char* virtualMetrics = "0";
  if (prof.isMetricMgrVirtual() || (wFlags & WFlg_virtualMetrics) ) {
    virtualMetrics = "1";
  }
 
  hpcrun_fmt_epoch_hdr_fwrite(fs, prof.m_flags,
			      prof.m_measurementGranularity,
			      prof.m_raToCallsiteOfst,
			      "TODO:epoch-name", "TODO:epoch-value",
			      FmtEpoch_NV_virtualMetrics, virtualMetrics,
			      NULL);

  // ------------------------------------------------------------
  // metric-tbl
  // ------------------------------------------------------------

  uint numMetrics = prof.metricMgr()->size();

  hpcfmt_byte4_fwrite(numMetrics, fs);
  for (uint i = 0; i < numMetrics; i++) {
    const Metric::ADesc* m = prof.metricMgr()->metric(i);

    string nmFmt = m->nameToFmt();
    const string& desc = m->description();
    
    metric_desc_t mdesc;
    mdesc.name = const_cast<char*>(nmFmt.c_str());
    mdesc.description = const_cast<char*>(desc.c_str());
    mdesc.flags = HPCRUN_MetricFlag_Real;
    mdesc.period = 1;

    hpcrun_fmt_metricDesc_fwrite(&mdesc, fs);
  }


  // ------------------------------------------------------------
  // loadmap
  // ------------------------------------------------------------

  const LoadMapMgr& loadMapMgr = *(prof.loadMapMgr());

  hpcfmt_byte4_fwrite(loadMapMgr.size(), fs);
  for (ALoadMap::LM_id_t i = 1; i <= loadMapMgr.size(); i++) {
    const ALoadMap::LM* lm = loadMapMgr.lm(i);

    loadmap_entry_t lm_entry;
    lm_entry.id = lm->id();
    lm_entry.name = const_cast<char*>(lm->name().c_str());
    lm_entry.vaddr = 0;
    lm_entry.mapaddr = lm->id(); // avoid problems reading as a LoadMap!
    lm_entry.flags = 0; // TODO:flags
    
    hpcrun_fmt_loadmapEntry_fwrite(&lm_entry, fs);
  }

  // ------------------------------------------------------------
  // cct
  // ------------------------------------------------------------
  fmt_cct_fwrite(prof, fs, wFlags);

  return HPCFMT_OK;
}


int
Profile::fmt_cct_fwrite(const Profile& prof, FILE* fs, uint wFlags)
{
  int ret;
  
  // ------------------------------------------------------------
  // Write number of cct nodes
  // ------------------------------------------------------------

  uint64_t numNodes = 0;
  uint nodeId = 2; // cf. s_nextUniqueId
  for (CCT::ANodeIterator it(prof.cct()->root()); it.Current(); ++it) {
    CCT::ANode* n = it.current();
    n->id(nodeId);
    nodeId += 2;
    numNodes++;
  }
  hpcfmt_byte8_fwrite(numNodes, fs);

  // ------------------------------------------------------------
  // Write each CCT node
  // ------------------------------------------------------------

  uint numMetrics = prof.metricMgr()->size();
  if (prof.isMetricMgrVirtual() || (wFlags & WFlg_virtualMetrics) ) {
    numMetrics = 0;
  }

  hpcrun_fmt_cct_node_t nodeFmt;
  nodeFmt.num_metrics = numMetrics;
  nodeFmt.metrics = 
    (hpcrun_metricVal_t*) alloca(numMetrics * sizeof(hpcrun_metricVal_t));

  for (CCT::ANodeIterator it(prof.cct()->root()); it.Current(); ++it) {
    CCT::ANode* n = it.current();
    fmt_cct_makeNode(nodeFmt, *n, prof.m_flags);

    ret = hpcrun_fmt_cct_node_fwrite(&nodeFmt, prof.m_flags, fs);
    if (ret != HPCFMT_OK) {
      return HPCFMT_ERR;
    }
  }

  return HPCFMT_OK;
}


//***************************************************************************

// 1. Create a CCT::Root node for the CCT
// 2. Remove the two outermost nodes corresponding to:
//      "synthetic-root -> monitor_main"
//    (if they exist).
void
Profile::canonicalize(uint rFlags)
{
  using namespace Prof;

  CCT::ANode* root = m_cct->root();

  // idempotent
  if (root && typeid(*root) == typeid(CCT::Root)) {
    return;
  }

  // 1. Create root
  CCT::ANode* newRoot = new CCT::Root(m_name);

  // 2. Find potential splice point
  CCT::ANode* splicePoint = NULL;

  CCT::ADynNode* root_dyn = dynamic_cast<CCT::ADynNode*>(root);
  if (root_dyn && root_dyn->ip() == 0 
      /*TODO: root_dyn->lmId() == LoadMap::LM_id_NULL &&*/) {
    splicePoint = root;

    // CCTs generated by hpcrun have "monitor-main" as the first
    // non-NULL root [cf. CallPath::Profile::fmt_fwrite()]
    if ((rFlags & RFlg_hpcrunData) && splicePoint->childCount() == 1) {
      splicePoint = splicePoint->firstChild();
    }
  }
  
  // 3. link or splice current tree to newRoot
  if (splicePoint) {
    for (CCT::ANodeChildIterator it(splicePoint); it.Current(); /* */) {
      CCT::ANode* n = it.current();
      it++; // advance iterator -- it is pointing at 'n'
      n->unlink();
      n->link(newRoot);
    }

    delete root; // N.B.: also deletes 'splicePoint'
  }
  else if (root) {
    root->link(newRoot);
  }
  
  m_cct->root(newRoot);
}


} // namespace CallPath

} // namespace Prof


//***************************************************************************

static std::pair<Prof::CCT::ANode*, Prof::CCT::ANode*>
cct_makeNode(Prof::CallPath::Profile& prof,
	     const hpcrun_fmt_cct_node_t& nodeFmt,
	     uint rFlags,
	     Prof::LoadMap* loadmap /*FIXME:temp*/)
{
  using namespace Prof;

  // ----------------------------------------------------------
  // Gather node parameters
  // ----------------------------------------------------------
  bool isLeaf = false;

  // ----------------------------------------
  // cpId
  // ----------------------------------------
  uint cpId = 0;
  int id_tmp = (int)nodeFmt.id;
  if (id_tmp < 0) {
    isLeaf = true;
    id_tmp = -id_tmp;
  }
  if (hpcrun_fmt_doRetainId(nodeFmt.id)) {
    cpId = id_tmp;
  }

  // ----------------------------------------
  // lmId and ip
  // ----------------------------------------
  ALoadMap::LM_id_t lmId = nodeFmt.lm_id;

  VMA ip = (VMA)nodeFmt.ip; // FIXME:tallent: Use ISA::ConvertVMAToOpVMA
  ushort opIdx = 0;

  if (loadmap) {
    VMA ip_orig = ip;
    LoadMap::LM* lm = loadmap->lm_find(ip_orig);

    ip = ip_orig - lm->relocAmt(); // unrelocated ip
    lmId = lm->id();
  }

  if (lmId != ALoadMap::LM_id_NULL) {
    prof.loadMapMgr()->lm(lmId)->isUsed(true);
  }

  DIAG_MsgIf(0, "cct_makeNode(: " << hex << ip << dec << ", " << lmId << ")");

  // ----------------------------------------  
  // lip
  // ----------------------------------------
  lush_lip_t* lip = NULL;
  if (!lush_lip_eq(&nodeFmt.lip, &lush_lip_NULL)) {
    lip = CCT::ADynNode::clone_lip(&nodeFmt.lip);
  }

  if (lip) {
    if (loadmap) {
      VMA lip_ip = lush_lip_getIP(lip);

      LoadMap::LM* lm = loadmap->lm_find(lip_ip);
      
      lush_lip_setLMId(lip, lm->id());
      lush_lip_setIP(lip, lip_ip - lm->relocAmt()); // unrelocated ip
    }

    ALoadMap::LM_id_t lip_lmId = lush_lip_getLMId(lip);
    if (lip_lmId != ALoadMap::LM_id_NULL) {
      prof.loadMapMgr()->lm(lip_lmId)->isUsed(true);
    }
  }

  // ----------------------------------------  
  // metrics
  // ----------------------------------------  

  bool doZeroMetrics = prof.isMetricMgrVirtual() 
    || (rFlags & Prof::CallPath::Profile::RFlg_virtualMetrics);

  bool hasMetrics = false;

  // [numMetricsSrc = nodeFmt.num_metrics] <= numMetricsDst
  uint numMetricsDst = prof.metricMgr()->size();
  if (rFlags & Prof::CallPath::Profile::RFlg_noMetricValues) {
    numMetricsDst = 0;
  }

  Metric::IData metricData(numMetricsDst);
  for (uint i_dst = 0, i_src = 0; i_dst < numMetricsDst; i_dst++) {
    Metric::ADesc* adesc = prof.metricMgr()->metric(i_dst);
    Metric::SampledDesc* mdesc = dynamic_cast<Metric::SampledDesc*>(adesc);
    DIAG_Assert(mdesc, "inconsistency: no corresponding SampledDesc!");

    hpcrun_metricVal_t m = nodeFmt.metrics[i_src];

    double mval = 0;
    if (hpcrun_metricFlags_isFlag(mdesc->flags(), HPCRUN_MetricFlag_Real)) {
      mval = m.r;
    }
    else {
      mval = (double)m.i;
    }

    metricData.metric(i_dst) = mval * (double)mdesc->period();
    metricData.fmt(i_dst) = mdesc->fmt_flag();// add by Xu Liu

    if (!hpcrun_metricVal_isZero(m)) {
      hasMetrics = true;
    }

    if (rFlags & Prof::CallPath::Profile::RFlg_makeInclExcl) {
      if (adesc->type() == Prof::Metric::ADesc::TyNULL ||
	  adesc->type() == Prof::Metric::ADesc::TyExcl) {
	i_src++;
      }
      // Prof::Metric::ADesc::TyIncl: reuse i_src
    }
    else {
      i_src++;
    }
  }

  if (doZeroMetrics) {
    metricData.clearMetrics();
  }

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
    n = new CCT::Stmt(NULL, cpId, nodeFmt.as_info, lmId, ip, opIdx, lip,
		      metricData);
  }

  if (!isLeaf) {
    if (hasMetrics) {
      n_leaf = n;

      uint mSz = (doZeroMetrics) ? 0 : numMetricsDst;
      Metric::IData metricData0(mSz);
      
      lush_lip_t* lipCopy = CCT::ADynNode::clone_lip(lip);

      n = new CCT::Call(NULL, 0, nodeFmt.as_info, lmId, ip, opIdx, lipCopy,
			metricData0);
    }
    else {
      n = new CCT::Call(NULL, cpId, nodeFmt.as_info, lmId, ip, opIdx, lip,
			metricData);
    }
  }

  return std::make_pair(n, n_leaf);
}


static void
fmt_cct_makeNode(hpcrun_fmt_cct_node_t& n_fmt, const Prof::CCT::ANode& n,
		 epoch_flags_t flags)
{
  n_fmt.id = (n.isLeaf()) ? -(n.id()) : n.id();

  n_fmt.id_parent = (n.parent()) ? n.parent()->id() : 0;

  const Prof::CCT::ADynNode* n_dyn_p = 
    dynamic_cast<const Prof::CCT::ADynNode*>(&n);
  if (typeid(n) == typeid(Prof::CCT::Root)) {
    n_fmt.as_info = lush_assoc_info_NULL;
    n_fmt.lm_id   = Prof::ALoadMap::LM_id_NULL;
    n_fmt.ip      = 0;
    lush_lip_init(&(n_fmt.lip));
    memset(n_fmt.metrics, 0, n_fmt.num_metrics * sizeof(hpcrun_metricVal_t));
  }
  else if (n_dyn_p) {
    const Prof::CCT::ADynNode& n_dyn = *n_dyn_p;

    if (flags.flags.isLogicalUnwind) {
      n_fmt.as_info = n_dyn.assocInfo();
    }
    
    n_fmt.lm_id = n_dyn.lmId();
    
    n_fmt.ip = n_dyn.Prof::CCT::ADynNode::ip();

    if (flags.flags.isLogicalUnwind) {
      lush_lip_init(&(n_fmt.lip));
      if (n_dyn.lip()) {
	memcpy(&n_fmt.lip, n_dyn.lip(), sizeof(lush_lip_t));
      }
    }

    // Note: use n_fmt.num_metrics rather than n_dyn.numMetrics() to
    // support skipping the writing of metrics.
    for (uint i = 0; i < n_fmt.num_metrics; ++i) {
      hpcrun_metricVal_t m; // C99: (hpcrun_metricVal_t){.r = n_dyn.metric(i)};
      m.r = n_dyn.metric(i);
      n_fmt.metrics[i] = m;
    }
  }
  else {
    DIAG_Die("fmt_cct_makeNode: unknown CCT node type");
  }
}

