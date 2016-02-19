// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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

#define __STDC_LIMIT_MACROS /* stdint; locate here for CentOS 5/gcc 4.1.2) */

#include <iostream>
using std::hex;
using std::dec;

#include <typeinfo>

#include <string>
using std::string;

#include <map>
#include <algorithm>

#include <cstdio>
#include <cstring> // strcmp
#include <cmath> // abs

#include <stdint.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <alloca.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "CallPath-Profile.hpp"
#include "NameMappings.hpp"
#include "Struct-Tree.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcrun-metric.h>

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

// -------------------------------------------------------------------------
// special variables to store mapping between the original and unique ID
// this table is needed to avoid duplicate filenames that arise with alien 
// and loop nodes.
// this variable will be used by getFileIdFromMap in CCT-Tree.cpp
// ---------------------------------------------------
std::map<uint, uint> m_mapFileIDs;      // map between file IDs
std::map<uint, uint> m_mapProcIDs;      // map between proc IDs

namespace CallPath {


Profile::Profile(const std::string name)
{
  m_name = name;
  m_fmtVersion = 0.0;
  m_flags.bits = 0;
  m_measurementGranularity = 0;
  m_raToCallsiteOfst = 0;

  m_traceMinTime = UINT64_MAX;
  m_traceMaxTime = 0;

  m_mMgr = new Metric::Mgr;
  m_isMetricMgrVirtual = false;

  m_loadmap = new LoadMap;

  m_cct = new CCT::Tree(this);

  m_structure = NULL;

  canonicalize();
}


Profile::~Profile()
{
  delete m_mMgr;
  delete m_loadmap;
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
  x.m_traceMinTime = std::min(x.m_traceMinTime, y.m_traceMinTime);
  x.m_traceMaxTime = std::max(x.m_traceMaxTime, y.m_traceMaxTime);


  // -------------------------------------------------------
  // merge metrics
  // -------------------------------------------------------
  uint x_newMetricBegIdx = 0;
  uint firstMergedMetric = mergeMetrics(y, mergeTy, x_newMetricBegIdx);
  
  // -------------------------------------------------------
  // merge LoadMaps
  //
  // Post-INVARIANT: y's cct refers to x's LoadMap
  // -------------------------------------------------------
  std::vector<LoadMap::MergeEffect>* mrgEffects1 =
    x.m_loadmap->merge(*y.loadmap());
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
  // the index, within y, of the first new metric to add to x
  uint y_newMetricIdx = Metric::Mgr::npos;

  if (mergeTy == Merge_CreateMetric) {
    yBeg_mapsTo_xIdx = x.metricMgr()->size();

    y_newMetricIdx = 0;
  }
  else if (mergeTy >= Merge_MergeMetricById) {
    yBeg_mapsTo_xIdx = (uint)mergeTy; // [
    
    uint yEnd_mapsTo_xIdx = yBeg_mapsTo_xIdx + y.metricMgr()->size(); // )
    if (! (x.metricMgr()->size() >= yEnd_mapsTo_xIdx) ) {
      uint overlapSz = x.metricMgr()->size() - yBeg_mapsTo_xIdx;
      y_newMetricIdx = overlapSz;
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

      LoadMap::LMId_t lmId1, lmId2;
      lmId1 = n_dyn->lmId_real();
      lmId2 = (lip) ? lush_lip_getLMId(lip) : LoadMap::LMId_NULL;
      
      for (uint i = 0; i < mrgEffects->size(); ++i) {
	const LoadMap::MergeEffect& chg = (*mrgEffects)[i];
	if (chg.old_id == lmId1) {
	  n_dyn->lmId_real(chg.new_id);
	  if (lmId2 == LoadMap::LMId_NULL) {
	    break; // quick exit in the common case
	  }
	}
	if (chg.old_id == lmId2) {
	  lush_lip_setLMId(lip, (uint16_t) chg.new_id);
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

  string traceFileNameTmp = m_traceFileName + "." + HPCPROF_TmpFnmSfx;

  char* infsBuf = new char[HPCIO_RWBufferSz];
  char* outfsBuf = new char[HPCIO_RWBufferSz];

  const string& inFnm = m_traceFileName;
  FILE* infs = hpcio_fopen_r(inFnm.c_str());
  if (!infs) {
    DIAG_Throw("error opening trace file '" << m_traceFileName << "'");
  }
  ret = setvbuf(infs, infsBuf, _IOFBF, HPCIO_RWBufferSz);
  DIAG_AssertWarn(ret == 0, inFnm << ": Profile::merge_fixTrace: setvbuf!");
  hpctrace_fmt_hdr_t hdr;
  ret = hpctrace_fmt_hdr_fread(&hdr, infs);
  DIAG_AssertWarn(ret == HPCFMT_OK, inFnm << ": Profile::merge_fixTrace: hpctrace_fmt_hdr_fread!");

  const string& outFnm = traceFileNameTmp;
  FILE* outfs = hpcio_fopen_w(outFnm.c_str(), 1/*overwrite*/);
  if (!outfs) {
    DIAG_Throw("error opening trace file '" << outFnm << "'");
  }
  ret = setvbuf(outfs, outfsBuf, _IOFBF, HPCIO_RWBufferSz);
  DIAG_AssertWarn(ret == 0, outFnm << ": Profile::merge_fixTrace: setvbuf!");
  ret = hpctrace_fmt_hdr_fwrite(hdr.flags, outfs);
  DIAG_AssertWarn(ret == HPCFMT_OK, outFnm << ": Profile::merge_fixTrace: hpctrace_fmt_hdr_fwrite!");

  while ( !feof(infs) ) {
    // 1. Read trace record (exit on EOF)
    hpctrace_fmt_datum_t datum;
    ret = hpctrace_fmt_datum_fread(&datum, hdr.flags, infs);
    if (ret == HPCFMT_EOF) {
      break;
    }
    else if (ret == HPCFMT_ERR) {
      DIAG_Throw("error reading trace file '" << m_traceFileName << "'");
    }
    
    // 2. Translate cct id
    uint cctId_old = datum.cpId;
    uint cctId_new = datum.cpId;
    UIntToUIntMap::iterator it = cpIdMap.find(cctId_old);
    if (it != cpIdMap.end()) {
      cctId_new = it->second;
      DIAG_MsgIf(0, "  " << cctId_old << " -> " << cctId_new);
    }
    datum.cpId = cctId_new;

    // 3. Write new trace record
    hpctrace_fmt_datum_fwrite(&datum, hdr.flags, outfs);
  }

  hpcio_fclose(infs);
  hpcio_fclose(outfs);

  delete[] infsBuf;
  delete[] outfsBuf;
}



// ---------------------------------------------------
// String comparison used for hash map
// ---------------------------------------------------
class StringCompare {
public:
  bool operator()(const std::string n1,  const std::string n2) const {
    return n1.compare(n2)<0;
  } 
};

// ---------------------------------------------------
// special variables to store mapping between filename and the ID
// this hack is needed to avoid duplicate filenames
// which occurs with alien nodes
// ---------------------------------------------------
static std::map<std::string, uint, StringCompare> m_mapFiles; // map the filenames and the ID
static std::map<std::string, uint, StringCompare> m_mapProcs; // map the procedure names and the ID

// attempt to retrieve the filename of a node
// if the node is an alien or a loop or a file, then we are guaranteed to
// retrieve the current filename associated to the node.
//
// However, if the node is not of those types, we'll try to get 
// the ancestor file of the node. This is not the proper way to get the
// filename, but it's the closest we can get (AFAIK).
static const char *
getFileName(Struct::ANode* strct)
{
  const char *nm = NULL;
  if (strct)
  {
    const std::type_info &tid = typeid(*strct);

    if (tid == typeid(Struct::Alien)) {
	nm = static_cast<Struct::Alien*>(strct)->fileName().c_str();
    } else if (tid == typeid(Struct::Loop)) {
	nm = static_cast<Struct::Loop*>(strct)->fileName().c_str();
    } else if (tid == typeid(Struct::File)){
	nm = static_cast<Struct::File*>(strct)->name().c_str();
    } else {
      	Prof::Struct::File *file = strct->ancestorFile();
      	if (file) {
	  nm = file->name().c_str();
      	}
    }
  }
  return nm;
}

// writing XML dictionary in the header part of experiment.xml
static void
writeXML_help(std::ostream& os, const char* entry_nm,
	      Struct::Tree* structure, const Struct::ANodeFilter* filter,
	      int type, bool remove_redundancy)
{
  Struct::ANode* root = structure ? structure->root() : NULL;
  if (!root) {
    return;
  }

  for (Struct::ANodeIterator it(root, filter); it.Current(); ++it) {
    Struct::ANode* strct = it.current();
    
    uint id = strct->id();
    const char* nm = NULL;

    bool fake_procedure = false;
    
    if (type == 1) { // LoadModule
      nm = strct->name().c_str();
    }
    else if (type == 2) { // File
      nm = getFileName(strct);	
      // ---------------------------------------
      // avoid redundancy in XML filename dictionary
      // (exception for unknown-file)
      // ---------------------------------------
      if (m_mapFiles.find(nm) == m_mapFiles.end()) {
	//  the filename is not in the list. Add it.
	m_mapFiles[nm] = id;

      } else if ( nm != Prof::Struct::Tree::UnknownFileNm 
		  && nm[0] != '\0' )
      { // WARNING: We do not allow redundancy unless for some specific files
	// For "unknown-file" and empty file (alien case), we allow duplicates
	// Otherwise we remove duplicate filename, and use the existing one.
	uint id_orig = m_mapFiles[nm];

	// remember that this ID needs redirection to the existing ID
	Prof::m_mapFileIDs[id] = id_orig;
	continue;
      }
    }
    else if (type == 3) { // Proc
      const char *proc_name = strct->name().c_str();
      nm = normalize_name(proc_name, fake_procedure);

      if (remove_redundancy && 
	  proc_name != Prof::Struct::Tree::UnknownProcNm)
      {  
        // -------------------------------------------------------
        // avoid redundancy in XML procedure dictionary
        // a procedure can have the same name if they are from different
        // file or different load module
        // -------------------------------------------------------
        const char *filename = getFileName(strct);	
        // we need to allow the same function name from a different file
        std::string completProcName(filename);
        completProcName.append(":");
        const char *lnm;
  
        // a procedure name within the same file has to be unique.
        // However, for codes compiled with GCC, binutils (or parseAPI) 
        // it's better to compare internally with the mangled names
        Struct::Proc *proc = dynamic_cast<Struct::Proc *>(strct);
        if (proc)
        {
  	  if (proc->linkName().empty()) {
  	    // the proc has no mangled name
  	    lnm = proc_name;
  	  } else
  	  { // get the mangled name
           	  lnm = proc->linkName().c_str();
  	  }
        } else
        {
  	  lnm = strct->name().c_str();
        }
        completProcName.append(lnm);
        if (m_mapProcs.find(completProcName) == m_mapProcs.end()) 
        {
  	  // the proc is not in dictionary. Add it into the map.
  	  m_mapProcs[completProcName] = id;
        } else 
        {
  	  // the same procedure name already exists, we need to reuse
  	  // the previous ID instead of the original one.
  	  uint id_orig = m_mapProcs[completProcName];
  
   	  // remember that this ID needs redirection to the existing ID
  	  Prof::m_mapProcIDs[id] = id_orig;
  	  continue;
        }
      }
    } else {
      DIAG_Die(DIAG_UnexpectedInput);
    }
    
    os << "    <" << entry_nm << " i" << MakeAttrNum(id)
       << " n" << MakeAttrStr(nm);
    
    if (fake_procedure) {
       os << " f" << MakeAttrNum(1); 
    } 
   
    os << "/>\n";
  }
}


static bool
writeXML_FileFilter(const Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (typeid(x) == typeid(Struct::File) || typeid(x) == typeid(Struct::Alien) ||
	  typeid(x) == typeid(Struct::Loop)); 
}


static bool
writeXML_ProcFilter(const Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (typeid(x) == typeid(Struct::Proc) || typeid(x) == typeid(Struct::Alien));
}


std::ostream&
Profile::writeXML_hdr(std::ostream& os, uint metricBeg, uint metricEnd,
		      uint oFlags, const char* GCC_ATTR_UNUSED pfx) const
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

    bool isDrvd = false;
    Metric::IDBExpr* mDrvdExpr = NULL;
    if (typeid(*m) == typeid(Metric::DerivedDesc)) {
      isDrvd = true;
      mDrvdExpr = static_cast<const Metric::DerivedDesc*>(m)->expr();
    }
    else if (typeid(*m) == typeid(Metric::DerivedIncrDesc)) {
      isDrvd = true;
      mDrvdExpr = static_cast<const Metric::DerivedIncrDesc*>(m)->expr();
    }

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
    if (isDrvd) {

      // 0. retrieve combine formula (each DerivedIncrDesc corresponds
      // to an 'accumulator')
      string combineFrm;
      if (mDrvdExpr) {
	combineFrm = mDrvdExpr->combineString1();

	if (mDrvdExpr->hasAccum2()) {
	  uint mId = mDrvdExpr->accum2Id();
	  string frm = mDrvdExpr->combineString2();
	  metricIdToFormula.insert(std::make_pair(mId, frm));
	}
      }
      else {
	// must represent accumulator 2
	uint mId = m->id();
	UIntToStringMap::iterator it = metricIdToFormula.find(mId);
	DIAG_Assert((it != metricIdToFormula.end()), DIAG_UnexpectedInput);
	combineFrm = it->second;
      }

      // 1. MetricFormula: combine
      os << "      <MetricFormula t=\"combine\""
	 << " frm=\"" << combineFrm << "\"/>\n";

      // 2. MetricFormula: finalize
      if (mDrvdExpr) {
	os << "      <MetricFormula t=\"finalize\""
	   << " frm=\"" << mDrvdExpr->finalizeString() << "\"/>\n";
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
       	 << " t=\"" << Prof::Metric::ADesc::ADescTyToXMLString(m->type()) << "\"";
      if (m->partner()) {
         os << " partner" << MakeAttrNum(m->partner()->id());
      }
      os << " db-glob=\"" << m->dbFileGlob() << "\""
	 << " db-id=\"" << m->dbId() << "\""
	 << " db-num-metrics=\"" << m->dbNumMetrics() << "\""
	 << " db-header-sz=\"" << HPCMETRICDB_FMT_HeaderLen << "\""
	 << "/>\n";
    }
  }
  os << "  </MetricDBTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  if (!traceFileNameSet().empty()) {
    os << "  <TraceDBTable>\n";
    os << "    <TraceDB i" << MakeAttrNum(0)
       << " db-glob=\"" << "*." << HPCRUN_TraceFnmSfx << "\""
       << " db-min-time=\"" << m_traceMinTime << "\""
       << " db-max-time=\"" << m_traceMaxTime << "\""
       << " db-header-sz=\"" << HPCTRACE_FMT_HeaderLen << "\""
       << "/>\n";
    os << "  </TraceDBTable>\n";
  }

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  os << "  <LoadModuleTable>\n";
  writeXML_help(os, "LoadModule", m_structure,
		&Struct::ANodeTyFilter[Struct::ANode::TyLM], 1,
		m_remove_redundancy);
  os << "  </LoadModuleTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  os << "  <FileTable>\n";
  Struct::ANodeFilter filt1(writeXML_FileFilter, "FileTable", 0);
  writeXML_help(os, "File", m_structure, &filt1, 2, m_remove_redundancy);
  os << "  </FileTable>\n";

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  if ( !(oFlags & CCT::Tree::OFlg_Debug) ) {
    os << "  <ProcedureTable>\n";
    Struct::ANodeFilter filt2(writeXML_ProcFilter, "ProcTable", 0);
    writeXML_help(os, "Procedure", m_structure, &filt2, 3, m_remove_redundancy);
    os << "  </ProcedureTable>\n";
  }

  return os;
}


std::ostream&
Profile::dump(std::ostream& os) const
{
  os << m_name << std::endl;

  m_mMgr->dump(os);

  m_loadmap->dump(os);

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

static std::pair<Prof::CCT::ADynNode*, Prof::CCT::ADynNode*>
cct_makeNode(Prof::CallPath::Profile& prof,
	     const hpcrun_fmt_cct_node_t& nodeFmt, uint rFlags,
	     const std::string& ctxtStr);

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

  char* fsBuf = new char[HPCIO_RWBufferSz];
  ret = setvbuf(fs, fsBuf, _IOFBF, HPCIO_RWBufferSz);
  DIAG_AssertWarn(ret == 0, "Profile::make: setvbuf!");

  rFlags |= RFlg_HpcrunData; // TODO: for now assume an hpcrun file (verify!)

  Profile* prof = NULL;
  ret = fmt_fread(prof, fs, rFlags, fnm, fnm, outfs);
  
  hpcio_fclose(fs);

  delete[] fsBuf;

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
  if ( !(hdr.version >= HPCRUN_FMT_Version_20) ) {
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
    ctxtStr += ": " + myCtxtStr;

    try {
      ret = fmt_epoch_fread(myprof, infs, rFlags, hdr,
			    ctxtStr, filename, outfs);
      if (ret == HPCFMT_EOF) {
	break;
      }
    }
    catch (const Diagnostics::Exception& x) {
      delete myprof;
      DIAG_Throw("error reading " << ctxtStr << ": " << x.what());
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
    fprintf(outfs, "\n[You look fine today! (num-epochs: %u)]\n", num_epochs);
  }

  hpcrun_fmt_hdr_free(&hdr, free);

  return HPCFMT_OK;
}


int
Profile::fmt_epoch_fread(Profile* &prof, FILE* infs, uint rFlags,
			 const hpcrun_fmt_hdr_t& hdr,
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
  ret = hpcrun_fmt_metricTbl_fread(&metricTbl, infs, hdr.version, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }
  if (outfs) {
    hpcrun_fmt_metricTbl_fprint(&metricTbl, outfs);
  }

  const uint numMetricsSrc = metricTbl.len;
  
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

  // -------------------------
  // program name
  // -------------------------
  string progNm;
  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_prog);
  if (val && strlen(val) > 0) {
    progNm = val;
  }

  // -------------------------
  // environment
  // -------------------------

#if 0
  string envPath;
  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_envPath);
  if (val && strlen(val) > 0) {
    envPath = val;
  }
#endif

  // -------------------------
  // parallelism context (mpi rank, thread id)
  // -------------------------
  string mpiRankStr, tidStr;
  long   mpiRank = -1, tid = -1;

  // val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_jobId);
  
  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_mpiRank);
  if (val) {
    mpiRankStr = val;
    if (val[0] != '\0') { mpiRank = StrUtil::toLong(mpiRankStr); }
  }

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_tid);
  if (val) {
    tidStr = val;
    if (val[0] != '\0') { tid = StrUtil::toLong(tidStr); }
  }

  // -------------------------
  // trace information
  // -------------------------

  bool     haveTrace = false;
  string   traceFileName;

  string   traceMinTimeStr, traceMaxTimeStr;
  uint64_t traceMinTime = UINT64_MAX, traceMaxTime = 0;

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_traceMinTime);
  if (val) {
    traceMinTimeStr = val;
    if (val[0] != '\0') { traceMinTime = StrUtil::toUInt64(traceMinTimeStr); }
  }

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_traceMaxTime);
  if (val) {
    traceMaxTimeStr = val;
    if (val[0] != '\0') { traceMaxTime = StrUtil::toUInt64(traceMaxTimeStr); }
  }

  haveTrace = (traceMinTime != 0 && traceMaxTime != 0);

  // Note: 'profFileName' can be empty when reading from a memory stream
  if (haveTrace && !profFileName.empty()) {
    // TODO: extract trace file name from profile
    static const string ext_prof = string(".") + HPCRUN_ProfileFnmSfx;
    static const string ext_trace = string(".") + HPCRUN_TraceFnmSfx;

    traceFileName = profFileName;
    size_t ext_pos = traceFileName.find(ext_prof);
    if (ext_pos != string::npos) {
      traceFileName.replace(traceFileName.begin() + ext_pos,
			    traceFileName.end(), ext_trace);
      // DIAG_Assert(FileUtil::isReadable(traceFileName));
    }
  }

  // -------------------------
  // 
  // -------------------------

  // N.B.: We currently assume FmtEpoch_NV_virtualMetrics is set iff
  // we read from a memory buffer.  Possibly we need an explicit tag for this.

  bool isVirtualMetrics = false;
  val = hpcfmt_nvpairList_search(&(ehdr.nvps), FmtEpoch_NV_virtualMetrics);
  if (val && strcmp(val, "0") != 0) {
    isVirtualMetrics = true;
    rFlags |= RFlg_NoMetricValues;
  }


  // ----------------------------------------
  // make CallPath::Profile
  // ----------------------------------------
  
  prof = new Profile(progNm);

  prof->m_fmtVersion = hdr.version;
  prof->m_flags = ehdr.flags;
  prof->m_measurementGranularity = ehdr.measurementGranularity;
  prof->m_raToCallsiteOfst = ehdr.raToCallsiteOfst;

  CCT::ANode::s_raToCallsiteOfst = prof->m_raToCallsiteOfst;

  prof->m_profileFileName = profFileName;

  if (haveTrace) {
    prof->m_traceFileName = traceFileName;
    if (!traceFileName.empty()) {
      prof->m_traceFileNameSet.insert(traceFileName);
    }
    prof->m_traceMinTime = traceMinTime;
    prof->m_traceMaxTime = traceMaxTime;
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
    //if (!tidStr.empty()) { m_sfx = "[" + tidStr + "]"; } // TODO:threads
  }

  metric_desc_t* m_lst = metricTbl.lst;
  for (uint i = 0; i < numMetricsSrc; i++) {
    const metric_desc_t& mdesc = m_lst[i];

    // ----------------------------------------
    // 
    // ----------------------------------------
    string nm = mdesc.name;
    string desc = mdesc.description;
    string profRelId = StrUtil::toStr(i);

    bool doMakeInclExcl = (rFlags & RFlg_MakeInclExcl);
    

    // Certain metrics do not have both incl/excl values
    if (nm == HPCRUN_METRIC_RetCnt) {
      doMakeInclExcl = false;
    }
    
    DIAG_Assert(mdesc.flags.fields.ty == MetricFlags_Ty_Raw
		|| mdesc.flags.fields.ty == MetricFlags_Ty_Final,
		"Prof::CallPath::Profile::fmt_epoch_fread: unexpected metric type '"
		<< mdesc.flags.fields.ty << "'");

    DIAG_Assert(Logic::implies(mdesc.flags.fields.ty == MetricFlags_Ty_Final,
			       !(rFlags & RFlg_MakeInclExcl)),
		DIAG_UnexpectedInput);
    
    // ----------------------------------------
    // 1. Make 'regular'/'inclusive' metric descriptor
    // ----------------------------------------
    Metric::SampledDesc* m =
      new Metric::SampledDesc(nm, desc, mdesc.period, true/*isUnitsEvents*/,
			      profFileName, profRelId, "HPCRUN");

    if (doMakeInclExcl) {
      m->type(Metric::ADesc::TyIncl);
    }
    else {
      if (nm == HPCRUN_METRIC_RetCnt) {
	m->type(Metric::ADesc::TyExcl);
      }
      else {
	m->type(Metric::ADesc::fromHPCRunMetricValTy(mdesc.flags.fields.valTy));
      }
    }
    if (!m_sfx.empty()) {
      m->nameSfx(m_sfx);
    }
    m->flags(mdesc.flags);
    
    prof->metricMgr()->insert(m);

    // ----------------------------------------
    // 2. Make associated 'exclusive' descriptor, if applicable
    // ----------------------------------------
    if (doMakeInclExcl) {
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


  // ----------------------------------------
  // make metric DB info
  // ----------------------------------------

  // metric DB information:
  //   1. create when reading an actual data file
  //   2. preserve when reading a profile from a memory buffer
  if ( !isVirtualMetrics ) {
    // create metric db information
    Prof::Metric::Mgr* mMgr = prof->metricMgr();
    for (uint mId = 0; mId < mMgr->size(); ++mId) {
      Prof::Metric::ADesc* m = mMgr->metric(mId);
      m->dbId(mId);
      m->dbNumMetrics(mMgr->size());
    }
  }

  // ----------------------------------------
  // make loadmap
  // ----------------------------------------

  uint num_lm = loadmap_tbl.len;

  LoadMap loadmap(num_lm);

  for (uint i = 0; i < num_lm; ++i) {
    string nm = loadmap_tbl.lst[i].name;
    RealPathMgr::singleton().realpath(nm);

    LoadMap::LM* lm = new LoadMap::LM(nm);
    loadmap.lm_insert(lm);
    
    DIAG_Assert(lm->id() == i + 1, "Profile::fmt_epoch_fread: Currently expect load module id's to be in dense ascending order.");
  }

  DIAG_MsgIf(DBG, loadmap.toString());

  std::vector<LoadMap::MergeEffect>* mrgEffect =
    prof->loadmap()->merge(loadmap);
  DIAG_Assert(mrgEffect->empty(), "Profile::fmt_epoch_fread: " << DIAG_UnexpectedInput);

  hpcrun_fmt_loadmap_free(&loadmap_tbl, free);
  delete mrgEffect;


  // ------------------------------------------------------------
  // cct
  // ------------------------------------------------------------
  fmt_cct_fread(*prof, infs, rFlags, metricTbl, ctxtStr, outfs);


  hpcrun_fmt_epochHdr_free(&ehdr, free);
  hpcrun_fmt_metricTbl_free(&metricTbl, free);
  
  return HPCFMT_OK;
}


int
Profile::fmt_cct_fread(Profile& prof, FILE* infs, uint rFlags,
		       const metric_tbl_t& metricTbl,
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
    fprintf(outfs, "[cct: (num-nodes: %" PRIu64 ")\n", numNodes);
  }

  CCT::Tree* cct = prof.cct();
  
  if (numNodes > 0) {
    delete cct->root();
    cct->root(NULL);
  }

  // N.B.: numMetricsSrc <= [numMetricsDst = prof.metricMgr()->size()]
  uint numMetricsSrc = metricTbl.len;

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
      hpcrun_fmt_cct_node_fprint(&nodeFmt, outfs, prof.m_flags,
				 &metricTbl, "  ");
    }

    int nodeId   = (int)nodeFmt.id;
    int parentId = (int)nodeFmt.id_parent;

    // Find parent of node
    CCT::ANode* node_parent = NULL;
    if (parentId != HPCRUN_FMT_CCTNodeId_NULL) {
      CCTIdToCCTNodeMap::iterator it = cctNodeMap.find(parentId);
      if (it != cctNodeMap.end()) {
	node_parent = it->second;
      }
      else {
	DIAG_Throw("Cannot find parent for CCT node " << nodeId);
      }
    }

    // Disable the preorder id requirement for CCT nodes.  It is hard
    // for Profile::fmt_cct_fwrite to guarantee preorder ids when it
    // is also forced to preserve some trace ids.  This also makes it
    // easier for hpcrun to handle thread thread creation contexts.
#if 0
    if (! (abs(parentId) < abs(nodeId))) {
      DIAG_Throw("CCT node " << nodeId << " has invalid parent (" << parentId << ")");
    }
#endif

    // ----------------------------------------------------------
    // Create node and link to parent
    // ----------------------------------------------------------

    std::pair<CCT::ADynNode*, CCT::ADynNode*> n2 =
      cct_makeNode(prof, nodeFmt, rFlags, ctxtStr);
    CCT::ADynNode* node = n2.first;
    CCT::ADynNode* node_sib = n2.second;

    DIAG_DevMsgIf(0, "fmt_cct_fread: " << hex << node << " -> " << node_parent << dec);

    if (node_parent) {
      // If 'node' is not the secondary root, perform sanity check
      if (!node->isSecondarySynthRoot()) {
	if (node->lmId_real() == LoadMap::LMId_NULL) {
	  DIAG_WMsg(2, ctxtStr << ": CCT (non-root) node " << nodeId << " has invalid normalized IP: " << node->nameDyn());
	}
      }

      node->link(node_parent);
      if (node_sib) {
	node_sib->link(node_parent);
      }
    }
    else {
      DIAG_AssertWarn(cct->empty(), ctxtStr << ": CCT must only have one root!");
      DIAG_AssertWarn(!node_sib, ctxtStr << ": CCT root cannot be split into interior and leaf!");
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

  string traceMinTimeStr = StrUtil::toStr(prof.m_traceMinTime);
  string traceMaxTimeStr = StrUtil::toStr(prof.m_traceMaxTime);

  hpcrun_fmt_hdr_fwrite(fs,
			"TODO:hdr-name","TODO:hdr-value",
			HPCRUN_FMT_NV_traceMinTime, traceMinTimeStr.c_str(),
			HPCRUN_FMT_NV_traceMaxTime, traceMaxTimeStr.c_str(),
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

  const LoadMap& loadmap = *(prof.loadmap());

  hpcfmt_int4_fwrite(loadmap.size(), fs);
  for (LoadMap::LMId_t i = 1; i <= loadmap.size(); i++) {
    const LoadMap::LM* lm = loadmap.lm(i);

    loadmap_entry_t lm_entry;
    lm_entry.id = (uint16_t) lm->id();
    lm_entry.name = const_cast<char*>(lm->name().c_str());
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

  // N.B.: This may not generate preorder ids because it is necessary
  // to retain certain trace ids.
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
// 2. Normalize a CCT of the form [cf. CallPath::Profile::fmt_fwrite()]:
//
//      PrimaryRoot --> monitor_main    --> main --> ...
//                 |
//                 \--> [SecondaryRoot] --> ...
//
//    into the following:
//
//      CCT::Root --> main        --> ...
//               |
//               \--> [CCT::Root] --> ...
void
Profile::canonicalize(uint rFlags)
{
  using namespace Prof;

  CCT::ANode* root = m_cct->root();
  CCT::ADynNode* root_dyn = dynamic_cast<CCT::ADynNode*>(root);

  // ------------------------------------------------------------
  // idempotent
  // ------------------------------------------------------------
  if (root && typeid(*root) == typeid(CCT::Root)) {
    return;
  }

  // ------------------------------------------------------------
  // 1. Find secondary root (if it exists) and unlink it from CCT
  // ------------------------------------------------------------

  // INVARIANT: 'secondaryRoot' is one of the children of 'root'
  CCT::ADynNode* secondaryRoot = NULL;

  if (root_dyn && root_dyn->isPrimarySynthRoot()) {
    for (CCT::ANodeChildIterator it(root); it.Current(); ++it) {
      CCT::ANode* n = it.current();
      CCT::ADynNode* n_dyn = dynamic_cast<CCT::ADynNode*>(n);
      if (n_dyn && n_dyn->isSecondarySynthRoot()) {
	secondaryRoot = n_dyn;
	secondaryRoot->unlink();
	break;
      }
    }
  }

  // ------------------------------------------------------------
  // 2. Create new secondary root
  // ------------------------------------------------------------

  // CCT::ANode* secondaryRootNew = new CCT::Root(m_name);
  // copy all children of secondaryRoot to secondaryRootNew

  // ------------------------------------------------------------
  // 3. Find potential splice point
  // ------------------------------------------------------------

  // INVARIANT: 'splicePoint' is the innermost (closest to leaves)
  // node to be deleted.
  CCT::ANode* splicePoint = NULL;

  if (root_dyn && root_dyn->isPrimarySynthRoot()) {
    splicePoint = root;
#if 0
    if (rFlags & RFlg_HpcrunData) {
      // hpcrun generates CCTs in the form diagrammed above
      if (splicePoint->childCount() == 1) {
	splicePoint = splicePoint->firstChild();
      } 
    }
#endif
  }

  // ------------------------------------------------------------
  // 4. Create new primary root
  // ------------------------------------------------------------

  CCT::ANode* rootNew = new CCT::Root(m_name);
  
  if (splicePoint) {
    for (CCT::ANodeChildIterator it(splicePoint); it.Current(); /* */) {
      CCT::ANode* n = it.current();
      it++; // advance iterator -- it is pointing at 'n'
      n->unlink();
      n->link(rootNew);
    }

    delete root; // N.B.: also deletes 'splicePoint'
  }
  else if (root) {
    root->link(rootNew);
  }
  
  m_cct->root(rootNew);

  // ------------------------------------------------------------
  // 5. Relink secondary root
  // ------------------------------------------------------------

  if (secondaryRoot) {
    secondaryRoot->link(rootNew);
  }
}


} // namespace CallPath

} // namespace Prof


//***************************************************************************

static std::pair<Prof::CCT::ADynNode*, Prof::CCT::ADynNode*>
cct_makeNode(Prof::CallPath::Profile& prof,
	     const hpcrun_fmt_cct_node_t& nodeFmt, uint rFlags,
	     const std::string& ctxtStr)
{
  using namespace Prof;

  const LoadMap& loadmap = *(prof.loadmap());

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
  // normalized ip (lmId and lmIP)
  // ----------------------------------------
  LoadMap::LMId_t lmId = nodeFmt.lm_id;

  VMA lmIP = (VMA)nodeFmt.lm_ip; // FIXME:tallent: Use ISA::convertVMAToOpVMA
  ushort opIdx = 0;

  if (! (lmId <= loadmap.size() /*1-based*/) ) {
    DIAG_WMsg(1, ctxtStr << ": CCT node " << nodeId
	      << " has invalid load module: " << lmId);
    lmId = LoadMap::LMId_NULL;
  }
  loadmap.lm(lmId)->isUsed(true); // ok if LoadMap::LMId_NULL

  DIAG_MsgIf(0, "cct_makeNode(: "<< hex << lmIP << dec << ", " << lmId << ")");

  // ----------------------------------------
  // normalized lip
  // ----------------------------------------
  lush_lip_t* lip = NULL;
  if (!lush_lip_eq(&nodeFmt.lip, &lush_lip_NULL)) {
    lip = CCT::ADynNode::clone_lip(&nodeFmt.lip);
  }

  if (lip) {
    LoadMap::LMId_t lip_lmId = lush_lip_getLMId(lip);

    if (! (lip_lmId <= loadmap.size() /*1-based*/) ) {
      DIAG_WMsg(1, ctxtStr << ": CCT node " << nodeId
		<< " has invalid (logical) load module: " << lip_lmId);
      lip_lmId = LoadMap::LMId_NULL;
    }
    loadmap.lm(lip_lmId)->isUsed(true); // ok if LoadMap::LMId_NULL
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
  Prof::CCT::ADynNode* n = NULL;
  Prof::CCT::ADynNode* n_leaf = NULL;

  if (hasMetrics || isLeaf) {
    n = new CCT::Stmt(NULL, cpId, nodeFmt.as_info, lmId, lmIP, opIdx, lip,
		      metricData);
  }

  if (!isLeaf) {
    if (hasMetrics) {
      n_leaf = n;

      const uint cpId0 = HPCRUN_FMT_CCTNodeId_NULL;

      uint mSz = (doZeroMetrics) ? 0 : numMetricsDst;
      Metric::IData metricData0(mSz);
      
      lush_lip_t* lipCopy = CCT::ADynNode::clone_lip(lip);

      n = new CCT::Call(NULL, cpId0, nodeFmt.as_info, lmId, lmIP, opIdx,
			lipCopy, metricData0);
    }
    else {
      n = new CCT::Call(NULL, cpId, nodeFmt.as_info, lmId, lmIP, opIdx,
			lip, metricData);
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
    n_fmt.lm_id   = Prof::LoadMap::LMId_NULL;
    n_fmt.lm_ip   = 0;
    lush_lip_init(&(n_fmt.lip));
    memset(n_fmt.metrics, 0, n_fmt.num_metrics * sizeof(hpcrun_metricVal_t));
  }
  else if (n_dyn_p) {
    const Prof::CCT::ADynNode& n_dyn = *n_dyn_p;

    if (flags.fields.isLogicalUnwind) {
      n_fmt.as_info = n_dyn.assocInfo();
    }
    
    n_fmt.lm_id = (uint16_t) n_dyn.lmId();
    n_fmt.lm_ip = n_dyn.Prof::CCT::ADynNode::lmIP();

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

