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
#include <lib/support/FileUtil.hpp>
#include <lib/support/Logic.hpp>
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
  m_fmtVersion = 0.0;
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
Profile::merge(Profile& y, int mergeTy, uint mrgFlag)
{
  Profile& x = (*this);

  DIAG_Assert(!y.m_structure, "Profile::merge: source profile should not have structure yet!");

  // -------------------------------------------------------
  // merge name, flags, etc
  // -------------------------------------------------------

  // Note: these values can be 'null' if the hpcrun-fmt data had no epochs
  if (x.m_fmtVersion == 0.0) {
    x.m_fmtVersion = y.m_fmtVersion;
  }
  else if (y.m_fmtVersion == 0.0) {
    y.m_fmtVersion = x.m_fmtVersion;
  }

  if (x.m_flags.bits == 0) {
    x.m_flags.bits = y.m_flags.bits;
  }
  else if (y.m_flags.bits == 0) {
    y.m_flags.bits = x.m_flags.bits;
  }

  if (x.m_measurementGranularity == 0) {
    x.m_measurementGranularity = y.m_measurementGranularity;
  }
  else if (y.m_measurementGranularity == 0) {
    y.m_measurementGranularity = x.m_measurementGranularity;
  }

  if (x.m_raToCallsiteOfst == 0) {
    x.m_raToCallsiteOfst = y.m_raToCallsiteOfst;
  }
  else if (y.m_raToCallsiteOfst == 0) {
    y.m_raToCallsiteOfst = m_raToCallsiteOfst;
  }

  DIAG_WMsgIf(x.m_fmtVersion != y.m_fmtVersion,
	      "CallPath::Profile::merge(): ignoring incompatible versions: "
	      << x.m_fmtVersion << " vs. " << y.m_fmtVersion);
  DIAG_WMsgIf(x.m_flags.bits != y.m_flags.bits,
	      "CallPath::Profile::merge(): ignoring incompatible flags: "
	      << x.m_flags.bits << " vs. " << y.m_flags.bits);
  DIAG_WMsgIf(x.m_measurementGranularity != y.m_measurementGranularity,
	      "CallPath::Profile::merge(): ignoring incompatible measurement-granularity: " << x.m_measurementGranularity << " vs. " << y.m_measurementGranularity);
  DIAG_WMsgIf(x.m_raToCallsiteOfst != y.m_raToCallsiteOfst,
	      "CallPath::Profile::merge(): ignoring incompatible RA-to-callsite-offset" << x.m_raToCallsiteOfst << " vs. " << y.m_raToCallsiteOfst);

  x.m_profileFileName = "";
  x.m_traceFileName = "";
  x.m_traceFileNameSet.insert(y.m_traceFileNameSet.begin(),
			      y.m_traceFileNameSet.end());

  // -------------------------------------------------------
  // merge metrics
  // -------------------------------------------------------
  uint x_newMetricBegIdx = 0;
  uint firstMergedMetric = mergeMetrics(y, mergeTy, x_newMetricBegIdx);
  
  // -------------------------------------------------------
  // merge LoadMaps
  //
  // Post-INVARIANT: y's cct refers to x's LoadMapMgr
  // -------------------------------------------------------
  std::vector<LoadMap::MergeEffect>* mrgEffects1 =
    x.m_loadmapMgr->merge(*y.loadMapMgr());
  y.merge_fixCCT(mrgEffects1);
  delete mrgEffects1;

  // -------------------------------------------------------
  // merge CCTs
  // -------------------------------------------------------

  if (mrgFlag & CCT::MrgFlg_NormalizeTraceFileY) {
    mrgFlag |= CCT::MrgFlg_PropagateEffects;
  }

  CCT::MergeEffectList* mrgEffects2 =
    x.cct()->merge(y.cct(), x_newMetricBegIdx, mrgFlag);

  DIAG_Assert(Logic::implies(mrgEffects2 && !mrgEffects2->empty(),
			     mrgFlag & CCT::MrgFlg_NormalizeTraceFileY),
	      "CallPath::Profile::merge: there should only be CCT::MergeEffects when MrgFlg_NormalizeTraceFileY is passed");

  y.merge_fixTrace(mrgEffects2);
  delete mrgEffects2;

  return firstMergedMetric;
}


uint
Profile::mergeMetrics(Profile& y, int mergeTy, uint& x_newMetricBegIdx)
{
  Profile& x = (*this);

  DIAG_Assert(x.m_isMetricMgrVirtual == y.m_isMetricMgrVirtual,
	      "CallPath::Profile::merge(): incompatible metrics");

  DIAG_MsgIf(0, "Profile::mergeMetrics: init\n"
	     << "x: " << x.metricMgr()->toString("  ")
	     << "y: " << y.metricMgr()->toString("  "));

  uint yBeg_mapsTo_xIdx = 0;

  x_newMetricBegIdx = 0; // first metric in y maps to (metricsMapTo)

  // -------------------------------------------------------
  // Translate Merge_mergeMetricByName to a primitive merge type
  // -------------------------------------------------------
  if (mergeTy == Merge_MergeMetricByName) {
    uint mapsTo = x.metricMgr()->findGroup(*y.metricMgr());
    mergeTy = (mapsTo == Metric::Mgr::npos) ? Merge_CreateMetric : (int)mapsTo;
  }

  // -------------------------------------------------------
  // process primitive merge types
  // -------------------------------------------------------
  uint y_newMetricIdx = Metric::Mgr::npos;

  if (mergeTy == Merge_CreateMetric) {
    yBeg_mapsTo_xIdx = x.metricMgr()->size();

    y_newMetricIdx = 0;
  }
  else if (mergeTy >= Merge_MergeMetricById) {
    yBeg_mapsTo_xIdx = (uint)mergeTy; // [
    
    uint yEnd_mapsTo_xIdx = yBeg_mapsTo_xIdx + y.metricMgr()->size(); // )
    if (x.metricMgr()->size() < yEnd_mapsTo_xIdx) {
      y_newMetricIdx = yEnd_mapsTo_xIdx - x.metricMgr()->size();
    }
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }

  for (uint i = y_newMetricIdx; i < y.metricMgr()->size(); ++i) {
    const Metric::ADesc* m = y.metricMgr()->metric(i);
    x.metricMgr()->insert(m->clone());
  }

  if (!isMetricMgrVirtual()) {
    x_newMetricBegIdx = yBeg_mapsTo_xIdx;
  }

  DIAG_MsgIf(0, "Profile::mergeMetrics: fini:\n" << m_mMgr->toString("  "));

  return yBeg_mapsTo_xIdx;
}


void
Profile::merge_fixCCT(const std::vector<LoadMap::MergeEffect>* mrgEffects)
{
  // early exit for trivial case
  if (!mrgEffects || mrgEffects->empty()) {
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
      
      for (uint i = 0; i < mrgEffects->size(); ++i) {
	const LoadMap::MergeEffect& chg = (*mrgEffects)[i];
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
Profile::merge_fixTrace(const CCT::MergeEffectList* mrgEffects)
{
  typedef std::map<uint, uint> UIntToUIntMap;

  // early exit for trivial case
  if (m_traceFileName.empty()) {
    return;
  }
  else if (!mrgEffects || mrgEffects->empty()) {
    return; // rely on Analysis::Util::copyTraceFiles() to copy orig file
  }

  // N.B.: We could build a map of old->new cpIds within
  // Profile::merge(), but the list of effects is more general and
  // extensible.  There are no asymptotic problems with building the
  // following map for local use.
  UIntToUIntMap cpIdMap;
  for (CCT::MergeEffectList::const_iterator it = mrgEffects->begin();
       it != mrgEffects->end(); ++it) {
    const CCT::MergeEffect& effct = *it;
    cpIdMap.insert(std::make_pair(effct.old_cpId, effct.new_cpId));
  }

  // ------------------------------------------------------------
  // Rewrite trace file
  // ------------------------------------------------------------
  int ret;

  DIAG_MsgIf(0, "Profile::merge_fixTrace: " << m_traceFileName);

  FILE* infs = hpcio_fopen_r(m_traceFileName.c_str());
  if (!infs) {
    DIAG_Throw("error opening trace file '" << m_traceFileName << "'");
  }
  ret = hpctrace_fmt_hdr_fread(infs);

  string traceFileNameTmp = m_traceFileName + "." + HPCPROF_TmpFnmSfx;
  FILE* outfs = hpcio_fopen_w(traceFileNameTmp.c_str(), 1/*overwrite*/);
  if (!outfs) {
    DIAG_Throw("error opening trace file '" << traceFileNameTmp << "'");
  }
  ret = hpctrace_fmt_hdr_fwrite(outfs);

  while ( !feof(infs) ) {
    // 1. Read trace record (exit on EOF)
    uint64_t timestamp;
    ret = hpcfmt_int8_fread(&timestamp, infs);
    if (ret == HPCFMT_EOF) {
      break;
    }
    else if (ret == HPCFMT_ERR) {
      DIAG_Throw("error reading trace file '" << m_traceFileName << "'");
    }
    
    uint cctId_old;
    ret = hpcfmt_int4_fread(&cctId_old, infs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading trace file '" << m_traceFileName << "'");
    }

    // 2. Translate cct id
    uint cctId_new = cctId_old;
    UIntToUIntMap::iterator it = cpIdMap.find(cctId_old);
    if (it != cpIdMap.end()) {
      cctId_new = it->second;
      DIAG_MsgIf(0, "  " << cctId_old << " -> " << cctId_new);
    }

    // 3. Write new trace record
    hpcfmt_int8_fwrite(timestamp, outfs);
    hpcfmt_int4_fwrite(cctId_new, outfs);
  }

  hpcio_fclose(infs);
  hpcio_fclose(outfs);
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
		      uint oFlags, const char* pfx) const
{
  typedef std::map<uint, string> UIntToStringMap;
  UIntToStringMap metricIdToFormula;

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  os << "  <MetricTable>\n";
  for (uint i = metricBeg; i < metricEnd; i++) {
    const Metric::ADesc* m = m_mMgr->metric(i);
    const Metric::SampledDesc* mSmpl =
      dynamic_cast<const Metric::SampledDesc*>(m);
    const Metric::DerivedIncrDesc* mDrvd1 =
      dynamic_cast<const Metric::DerivedIncrDesc*>(m);

    // Metric
    os << "    <Metric i" << MakeAttrNum(i)
       << " n" << MakeAttrStr(m->name())
       << " v=\"" << m->toValueTyStringXML() << "\""
       << " t=\"" << Prof::Metric::ADesc::ADescTyToXMLString(m->type()) << "\"";
    if (m->partner()) {
      os << " partner" << MakeAttrNum(m->partner()->id());
    }
    os << " show=\"" << ((m->isVisible()) ? "1" : "0")  << "\""
       << " show-percent=\"" << ((m->doDispPercent()) ? "1" : "0") << "\""
       << ">\n";

    // MetricFormula
    if (mDrvd1) {

      // 0. retrieve combine formula (each DerivedIncrDesc corresponds
      // to an 'accumulator')
      string combineFrm;
      if (mDrvd1->expr()) {
	combineFrm = mDrvd1->expr()->combineString1();

	if (mDrvd1->expr()->hasAccum2()) {
	  uint mId = mDrvd1->expr()->accum2Id();
	  string frm = mDrvd1->expr()->combineString2();
	  metricIdToFormula.insert(std::make_pair(mId, frm));
	}
      }
      else {
	// must represent accumulator 2
	uint mId = mDrvd1->id();
	UIntToStringMap::iterator it = metricIdToFormula.find(mId);
	DIAG_Assert((it != metricIdToFormula.end()), DIAG_UnexpectedInput);
	combineFrm = it->second;
      }

      // 1. MetricFormula: combine
      os << "      <MetricFormula t=\"combine\""
	 << " frm=\"" << combineFrm << "\"/>\n";

      // 2. MetricFormula: finalize
      if (mDrvd1->expr()) {
	os << "      <MetricFormula t=\"finalize\""
	   << " frm=\"" << mDrvd1->expr()->finalizeString() << "\"/>\n";
      }
    }
    
    // Info
    os << "      <Info>"
       << "<NV n=\"units\" v=\"events\"/>"; // or "samples" m->isUnitsEvents()
    if (mSmpl) {
      os << "<NV n=\"period\" v" << MakeAttrNum(mSmpl->period()) << "/>";
    }
    os << "</Info>\n";
    os << "    </Metric>\n";
  }
  os << "  </MetricTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  os << "  <MetricDBTable>\n";
  for (uint i = 0; i < m_mMgr->size(); i++) {
    const Metric::ADesc* m = m_mMgr->metric(i);
    if (m->hasDBInfo()) {
      os << "    <MetricDB i" << MakeAttrNum(i) 
	 << " n" << MakeAttrStr(m->name())
	 << " db-glob=\"" << m->dbFileGlob() << "\""
	 << " db-id=\"" << m->dbId() << "\""
	 << " db-num-metrics=\"" << m->dbNumMetrics() << "\"/>\n";
    }
  }
  os << "  </MetricDBTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  os << "  <LoadModuleTable>\n";
  writeXML_help(os, "LoadModule", m_structure, 
		&Struct::ANodeTyFilter[Struct::ANode::TyLM], 1);
  os << "  </LoadModuleTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  os << "  <FileTable>\n";
  Struct::ANodeFilter filt1(writeXML_FileFilter, "FileTable", 0);
  writeXML_help(os, "File", m_structure, &filt1, 2);
  os << "  </FileTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
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

  if (rFlags & RFlg_VirtualMetrics) {
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

  rFlags |= RFlg_HpcrunData; // TODO: for now assume an hpcrun file (verify!)

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
  if ( !(hdr.version >= HPCRUN_FMT_Version_19A) ) {
    // TODO: deprecate old file version: ratchet this to 2.0x
    DIAG_Throw("unsupported file version '" << hdr.versionStr << "'");
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
      ret = fmt_epoch_fread(myprof, infs, rFlags, &hdr,
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
      prof->merge(*myprof, Profile::Merge_MergeMetricById);
    }

    num_epochs++;
  }

  if (!prof) {
    prof = make(rFlags); // make an empty profile
  }

  prof->canonicalize(rFlags);
  prof->metricMgr()->computePartners();

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
			 const hpcrun_fmt_hdr_t* hdr,
			 std::string ctxtStr, const char* filename,
			 FILE* outfs)
{
  using namespace Prof;

  string profFileName = (filename) ? filename : "";

  int ret;

  // ------------------------------------------------------------
  // Read epoch data
  // ------------------------------------------------------------

  // ----------------------------------------
  // epoch-hdr
  // ----------------------------------------
  hpcrun_fmt_epochHdr_t ehdr;
  ret = hpcrun_fmt_epochHdr_fread(&ehdr, infs, malloc);
  if (ret == HPCFMT_EOF) {
    return HPCFMT_EOF;
  }
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'epoch-hdr'");
  }
  if (outfs) {
    hpcrun_fmt_epochHdr_fprint(&ehdr, outfs);
  }

  // ----------------------------------------
  // metric-tbl
  // ----------------------------------------
  metric_tbl_t metricTbl;
  ret = hpcrun_fmt_metricTbl_fread(&metricTbl, infs, hdr->version, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }
  if (outfs) {
    hpcrun_fmt_metricTbl_fprint(&metricTbl, outfs);
  }

  uint numMetricsSrc = metricTbl.len;
  
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
  val = hpcfmt_nvpairList_search(&hdr->nvps, HPCRUN_FMT_NV_prog);
  if (val && strlen(val) > 0) {
    progNm = val;
  }

  string mpiRankStr, tidStr;
  long   mpiRank = -1, tid = -1;
  //const char* jobid = hpcfmt_nvpairList_search(&hdr->nvps, HPCRUN_FMT_NV_jobId);
  val = hpcfmt_nvpairList_search(&hdr->nvps, HPCRUN_FMT_NV_mpiRank);
  if (val) {
    mpiRankStr = val;
    if (val[0] != '\0') { mpiRank = StrUtil::toLong(mpiRankStr); }
  }

  val = hpcfmt_nvpairList_search(&hdr->nvps, HPCRUN_FMT_NV_tid);
  if (val) {
    tidStr = val;
    if (val[0] != '\0') { tid = StrUtil::toLong(tidStr); }
  }

  // FIXME: temporary for dual-interpretations
  bool isNewFormat = true; 
  val = hpcfmt_nvpairList_search(&hdr->nvps, "nasty-message");
  if (val) { isNewFormat = false; }

  //val = hpcfmt_nvpairList_search(ehdr.&nvps, "to-find");

  bool isVirtualMetrics = false;
  val = hpcfmt_nvpairList_search(&(ehdr.nvps), FmtEpoch_NV_virtualMetrics);
  if (val && strcmp(val, "0") != 0) {
    isVirtualMetrics = true;
    rFlags |= RFlg_NoMetricValues;
  }


  // ----------------------------------------
  // 
  // ----------------------------------------
  
  prof = new Profile(progNm);

  prof->m_fmtVersion = hdr->version;
  prof->m_flags = ehdr.flags;
  prof->m_measurementGranularity = ehdr.measurementGranularity;
  prof->m_raToCallsiteOfst = ehdr.raToCallsiteOfst;

  CCT::ANode::s_raToCallsiteOfst = prof->m_raToCallsiteOfst;

  prof->m_profileFileName = profFileName;

  // TODO: extract trace file name from profile
  string t_fnm = profFileName;
  static const string ext_prof = string(".") + HPCRUN_ProfileFnmSfx;
  static const string ext_trace = string(".") + HPCRUN_TraceFnmSfx;
  
  size_t ext_pos = t_fnm.find(ext_prof);

  if (ext_pos != string::npos) {
    t_fnm.replace(t_fnm.begin() + ext_pos, t_fnm.end(), ext_trace);
    if (FileUtil::isReadable(t_fnm)) {
      prof->m_traceFileName = t_fnm;
      prof->m_traceFileNameSet.insert(t_fnm);
    }
  }


  // ----------------------------------------
  // make metric table
  // ----------------------------------------

  string m_sfx;
  if (!mpiRankStr.empty() && !tidStr.empty()) {
    m_sfx = "[" + mpiRankStr + "," + tidStr + "]";
  }
  else if (!mpiRankStr.empty()) {
    m_sfx = "[" + mpiRankStr + "]";
  }
  else if (!tidStr.empty()) {
    m_sfx = "[" + tidStr + "]";
  }

  if (rFlags & RFlg_NoMetricSfx) {
    m_sfx = "";
  }

  metric_desc_t* m_lst = metricTbl.lst;
  for (uint i = 0; i < numMetricsSrc; i++) {
    const metric_desc_t& mdesc = m_lst[i];
    
    string nm = mdesc.name;
    string desc = mdesc.description;
    string profRelId = StrUtil::toStr(i);

    DIAG_Assert(mdesc.flags.fields.ty == MetricFlags_Ty_Raw
		|| mdesc.flags.fields.ty == MetricFlags_Ty_Final,
		DIAG_UnexpectedInput);

    DIAG_Assert(Logic::implies(mdesc.flags.fields.ty == MetricFlags_Ty_Final,
			       !(rFlags & RFlg_MakeInclExcl)),
		DIAG_UnexpectedInput);

    // 1. Make 'regular'/'inclusive' metric descriptor
    Metric::SampledDesc* m =
      new Metric::SampledDesc(nm, desc, mdesc.period, true/*isUnitsEvents*/,
			      profFileName, profRelId, "HPCRUN");
    if ((rFlags & RFlg_MakeInclExcl)) {
      m->type(Metric::ADesc::TyIncl);
    }
    else {
      m->type(Metric::ADesc::fromHPCRunMetricValTy(mdesc.flags.fields.valTy));
    }
    if (!m_sfx.empty()) {
      m->nameSfx(m_sfx);
    }
    m->flags(mdesc.flags);
    
    prof->metricMgr()->insert(m);

    // 2. Make associated 'exclusive' descriptor, if applicable
    if ((rFlags & RFlg_MakeInclExcl)) {
      Metric::SampledDesc* mSmpl = 
	new Metric::SampledDesc(nm, desc, mdesc.period,
				true/*isUnitsEvents*/,
				profFileName, profRelId, "HPCRUN");
      mSmpl->type(Metric::ADesc::TyExcl);
      if (!m_sfx.empty()) {
	mSmpl->nameSfx(m_sfx);
      }
      mSmpl->flags(mdesc.flags);
      
      prof->metricMgr()->insert(mSmpl);
    }
  }

  if (isVirtualMetrics || (rFlags & RFlg_VirtualMetrics) ) {
    prof->isMetricMgrVirtual(true);
  }

  hpcrun_fmt_metricTbl_free(&metricTbl, free);

  // ----------------------------------------
  // make metric DB info
  // ----------------------------------------

  Prof::Metric::Mgr* mMgr = prof->metricMgr();
  for (uint mId = 0; mId < mMgr->size(); ++mId) {
    Prof::Metric::ADesc* m = mMgr->metric(mId);
    m->dbId(mId);
    m->dbNumMetrics(mMgr->size());
  }

  // ----------------------------------------
  // make loadmap
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

  std::vector<ALoadMap::MergeEffect>* mrgEffect =
    prof->loadMapMgr()->merge(loadmap);
  DIAG_Assert(mrgEffect->empty(), "Profile::fmt_epoch_fread: " << DIAG_UnexpectedInput);

  hpcrun_fmt_loadmap_free(&loadmap_tbl, free);
  delete mrgEffect;


  // ------------------------------------------------------------
  // cct
  // ------------------------------------------------------------

  LoadMap* loadmap_p = (isNewFormat) ? NULL : &loadmap; // FIXME:temporary
  fmt_cct_fread(*prof, infs, rFlags, loadmap_p, numMetricsSrc, ctxtStr, outfs);


  hpcrun_fmt_epochHdr_free(&ehdr, free);
  
  return HPCFMT_OK;
}


int
Profile::fmt_cct_fread(Profile& prof, FILE* infs, uint rFlags,
		       LoadMap* loadmap, uint numMetricsSrc,
		       std::string ctxtStr, FILE* outfs)
{
  typedef std::map<int, CCT::ANode*> CCTIdToCCTNodeMap;

  DIAG_Assert(infs, "Bad file descriptor!");
  
  CCTIdToCCTNodeMap cctNodeMap;

  int ret = HPCFMT_ERR;

  // ------------------------------------------------------------
  // Read number of cct nodes
  // ------------------------------------------------------------
  uint64_t numNodes = 0;
  hpcfmt_int8_fread(&numNodes, infs);

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
  if (rFlags & RFlg_NoMetricValues) {
    numMetricsSrc = 0;
  }

  hpcrun_fmt_cct_node_t nodeFmt;
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
	node_sib->link(node_parent);
      }
    }
    else {
      DIAG_AssertWarn(cct->empty(), ctxtStr << ": must only have one root node!");
      DIAG_AssertWarn(!node_sib, ctxtStr << ": root node cannot be split into interior and leaf!");
      cct->root(node);
    }

    cctNodeMap.insert(std::make_pair(nodeFmt.id, node));
  }

  if (outfs) {
    fprintf(outfs, "]\n");
  }

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
  if (prof.isMetricMgrVirtual() || (wFlags & WFlg_VirtualMetrics) ) {
    virtualMetrics = "1";
  }
 
  hpcrun_fmt_epochHdr_fwrite(fs, prof.m_flags,
			     prof.m_measurementGranularity,
			     prof.m_raToCallsiteOfst,
			     "TODO:epoch-name", "TODO:epoch-value",
			     FmtEpoch_NV_virtualMetrics, virtualMetrics,
			     NULL);

  // ------------------------------------------------------------
  // metric-tbl
  // ------------------------------------------------------------

  uint numMetrics = prof.metricMgr()->size();

  hpcfmt_int4_fwrite(numMetrics, fs);
  for (uint i = 0; i < numMetrics; i++) {
    const Metric::ADesc* m = prof.metricMgr()->metric(i);

    string nmFmt = m->nameToFmt();
    const string& desc = m->description();
    
    metric_desc_t mdesc = metricDesc_NULL;
    mdesc.flags = hpcrun_metricFlags_NULL;

    mdesc.name = const_cast<char*>(nmFmt.c_str());
    mdesc.description = const_cast<char*>(desc.c_str());
    mdesc.flags.fields.ty = MetricFlags_Ty_Final;
    mdesc.flags.fields.valTy = Metric::ADesc::toHPCRunMetricValTy(m->type());
    mdesc.flags.fields.valFmt = MetricFlags_ValFmt_Real;
    mdesc.period = 1;
    mdesc.formula = NULL;
    mdesc.format = NULL;

    hpcrun_fmt_metricDesc_fwrite(&mdesc, fs);
  }


  // ------------------------------------------------------------
  // loadmap
  // ------------------------------------------------------------

  const LoadMapMgr& loadMapMgr = *(prof.loadMapMgr());

  hpcfmt_int4_fwrite(loadMapMgr.size(), fs);
  for (ALoadMap::LM_id_t i = 1; i <= loadMapMgr.size(); i++) {
    const ALoadMap::LM* lm = loadMapMgr.lm(i);

    loadmap_entry_t lm_entry;
    lm_entry.id = lm->id();
    lm_entry.name = const_cast<char*>(lm->name().c_str());
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
  // Ensure CCT node ids follow conventions
  // ------------------------------------------------------------

  uint64_t numNodes = 0;
  uint nodeId_next = 2; // cf. s_nextUniqueId
  for (CCT::ANodeIterator it(prof.cct()->root()); it.Current(); ++it) {
    CCT::ANode* n = it.current();
    Prof::CCT::ADynNode* n_dyn = dynamic_cast<Prof::CCT::ADynNode*>(n);

    if (n_dyn && hpcrun_fmt_doRetainId(n_dyn->cpId())) {
      n->id(n_dyn->cpId());
    }
    else {
      n->id(nodeId_next);
      nodeId_next += 2;
    }
    numNodes++;
  }
  
  // ------------------------------------------------------------
  // Write number of cct nodes
  // ------------------------------------------------------------

  hpcfmt_int8_fwrite(numNodes, fs);

  // ------------------------------------------------------------
  // Write each CCT node
  // ------------------------------------------------------------

  uint numMetrics = prof.metricMgr()->size();
  if (prof.isMetricMgrVirtual() || (wFlags & WFlg_VirtualMetrics) ) {
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
  if (root_dyn && root_dyn->isSyntheticRoot()) {
    splicePoint = root;

    // CCTs generated by hpcrun have "monitor-main" as the first
    // non-NULL root [cf. CallPath::Profile::fmt_fwrite()]
    if ((rFlags & RFlg_HpcrunData) && splicePoint->childCount() == 1) {
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
  int nodeId = (int)nodeFmt.id;
  if (nodeId < 0) {
    isLeaf = true;
    nodeId = -nodeId;
  }
  // INVARIANT: nodeId > HPCRUN_FMT_CCTNodeId_NULL

  uint cpId = HPCRUN_FMT_CCTNodeId_NULL;
  if (hpcrun_fmt_doRetainId(nodeFmt.id)) {
    cpId = nodeId;
  }

  // ----------------------------------------
  // lmId and ip
  // ----------------------------------------
  ALoadMap::LM_id_t lmId = nodeFmt.lm_id;

  VMA ip = (VMA)nodeFmt.lm_offset; // FIXME:tallent: Use ISA::ConvertVMAToOpVMA
  ushort opIdx = 0;

  if (loadmap && (nodeFmt.id_parent != HPCRUN_FMT_CCTNodeId_NULL)) {
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
      VMA lip_ip = lush_lip_getLMOffset(lip);

      LoadMap::LM* lm = loadmap->lm_find(lip_ip);
      
      lush_lip_setLMId(lip, lm->id());
      lush_lip_setLMOffset(lip, lip_ip - lm->relocAmt()); // unrelocated ip
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
    || (rFlags & Prof::CallPath::Profile::RFlg_VirtualMetrics);

  bool hasMetrics = false;

  // [numMetricsSrc = nodeFmt.num_metrics] <= numMetricsDst
  uint numMetricsDst = prof.metricMgr()->size();
  if (rFlags & Prof::CallPath::Profile::RFlg_NoMetricValues) {
    numMetricsDst = 0;
  }

  Metric::IData metricData(numMetricsDst);
  for (uint i_dst = 0, i_src = 0; i_dst < numMetricsDst; i_dst++) {
    Metric::ADesc* adesc = prof.metricMgr()->metric(i_dst);
    Metric::SampledDesc* mdesc = dynamic_cast<Metric::SampledDesc*>(adesc);
    DIAG_Assert(mdesc, "inconsistency: no corresponding SampledDesc!");

    hpcrun_metricVal_t m = nodeFmt.metrics[i_src];

    double mval = 0;
    switch (mdesc->flags().fields.valFmt) {
      case MetricFlags_ValFmt_Int:
	mval = (double)m.i; break;
      case MetricFlags_ValFmt_Real:
	mval = m.r; break;
      default:
	DIAG_Die(DIAG_UnexpectedInput);
    }

    metricData.metric(i_dst) = mval * (double)mdesc->period();

    if (!hpcrun_metricVal_isZero(m)) {
      hasMetrics = true;
    }

    if (rFlags & Prof::CallPath::Profile::RFlg_MakeInclExcl) {
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
  // == 0 (that has cpId == NULL *and* that is the primary return node);
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

      const uint cpId0 = HPCRUN_FMT_CCTNodeId_NULL;

      uint mSz = (doZeroMetrics) ? 0 : numMetricsDst;
      Metric::IData metricData0(mSz);
      
      lush_lip_t* lipCopy = CCT::ADynNode::clone_lip(lip);

      n = new CCT::Call(NULL, cpId0, nodeFmt.as_info, lmId, ip, opIdx, lipCopy,
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

  n_fmt.id_parent = (n.parent()) ? n.parent()->id() : HPCRUN_FMT_CCTNodeId_NULL;

  const Prof::CCT::ADynNode* n_dyn_p = 
    dynamic_cast<const Prof::CCT::ADynNode*>(&n);
  if (typeid(n) == typeid(Prof::CCT::Root)) {
    n_fmt.as_info = lush_assoc_info_NULL;
    n_fmt.lm_id   = Prof::ALoadMap::LM_id_NULL;
    n_fmt.lm_offset = 0;
    lush_lip_init(&(n_fmt.lip));
    memset(n_fmt.metrics, 0, n_fmt.num_metrics * sizeof(hpcrun_metricVal_t));
  }
  else if (n_dyn_p) {
    const Prof::CCT::ADynNode& n_dyn = *n_dyn_p;

    if (flags.fields.isLogicalUnwind) {
      n_fmt.as_info = n_dyn.assocInfo();
    }
    
    n_fmt.lm_id = n_dyn.lmId();
    
    n_fmt.lm_offset = n_dyn.Prof::CCT::ADynNode::lmOffset();

    if (flags.fields.isLogicalUnwind) {
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

