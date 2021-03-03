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
// Copyright ((c)) 2002-2021, Rice University
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

#ifndef prof_Prof_CallPath_Profile_hpp
#define prof_Prof_CallPath_Profile_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <cstdio>

#include <vector>
#include <set>
#include <string>


//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Metric-Mgr.hpp"
#include "LoadMap.hpp"
#include "CCT-Tree.hpp"
#include "StringSet.hpp"

#include <lib/support/FileUtil.hpp> // dirname

#include "../binutils/SimpleSymbolsFactories.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {


class Profile
  : public Unique // non copyable
{
public:
  Profile(const std::string name);
  virtual ~Profile();
  
  // -------------------------------------------------------
  // meta data
  // -------------------------------------------------------
  const std::string&
  name() const
  { return m_name; }
  
  void
  name(const std::string& x)
  { m_name = x; }

  void
  name(const char* x)
  { m_name = (x) ? x : ""; }


  double
  fmtVersion() const
  { return m_fmtVersion; }


  const StringSet&
  traceFileNameSet() const
  { return m_traceFileNameSet; }


  StringSet&
  traceFileNameSet()
  { return m_traceFileNameSet; }

  // enable/disable redundancy of procedure names
  // @param flag: true  -- redundancy is eliminated
  // 		  false -- redundancy is allowed
  void disable_redundancy(bool flag) {
    m_remove_redundancy = flag;
  }

  void
  addDirectory(std::string filename) {
    std::string directory = FileUtil::dirname(filename);
    if (!directory.empty()) {
      m_directorySet.insert(directory);
    }
  }

  void
  copyDirectory(const StringSet &rhs) {
    m_directorySet += rhs;
  }

  StringSet&
  directorySet() {
    return m_directorySet;
  }

  // -------------------------------------------------------
  // MetricMgr
  // -------------------------------------------------------

  const Metric::Mgr*
  metricMgr() const
  { return m_mMgr; }

  Metric::Mgr*
  metricMgr()
  { return m_mMgr; }

  void
  metricMgr(Metric::Mgr* mMgr)
  { m_mMgr = mMgr; }


  // isMetricMgrVirtual: It is sometimes useful for the metric manager
  //   to contain descriptions of metrics for which there are no
  //   corresponding values.  We call this a virtual metric manager.
  //   The main use of this is when profiles are read with the
  //   RFlg_VirtualMetrics flag.
  //
  // NOTE: alternatively, we could have the metric manager read just
  // enough of each file to pull out the metric descriptors (just as
  // it does for flat information).
  bool
  isMetricMgrVirtual() const
  { return m_isMetricMgrVirtual; }

  void
  isMetricMgrVirtual(bool x)
  { m_isMetricMgrVirtual = x; }

  // -------------------------------------------------------
  // LoadMap
  // -------------------------------------------------------
  const LoadMap*
  loadmap() const
  { return m_loadmap; }

  LoadMap*
  loadmap()
  { return m_loadmap; }

  
  // -------------------------------------------------------
  // CCT
  // -------------------------------------------------------
  CCT::Tree*
  cct() const
  { return m_cct; }


  // -------------------------------------------------------
  // Static structure
  // -------------------------------------------------------
  Prof::Struct::Tree*
  structure() const
  { return m_structure; }

  void
  structure(Prof::Struct::Tree* x)
  { m_structure = x; }


  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  enum {
    // merge y's metrics into x's by name: the group of names in y
    // are merged with a matching group of names in x or are created
    Merge_MergeMetricByName = -2,

    // for each metric in y, create a corresponding metric in x
    Merge_CreateMetric = -1,

    // merge y's metrics into x's by id: first metric in y maps to
    // given metric id in x (assumes complete overlap)
    Merge_MergeMetricById = 0 // or >= 0
  };


  // merge: Given a Profile y, merge y into x = 'this'.  The 'mergeTy'
  //   parameter indicates how to merge y's metrics into x.  Returns
  //   the index of the first merged metric in x.
  // ASSUMES: both x and y are in canonical form (canonicalize())
  // WARNING: the merge may change/destroy y
  uint
  merge(Profile& y, int mergeTy, uint mrgFlag = 0);

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  enum {
    // only read metric table, even if CCT nodes have metrics
    RFlg_VirtualMetrics = (1 << 0),

    // do not add suffixes to metric descriptors
    RFlg_NoMetricSfx    = (1 << 1),

    // make inclusive/exclusive descriptors and (if non-virtual)
    // corresponding space within CCT nodes
    RFlg_MakeInclExcl   = (1 << 2),

    // *private*: CCT nodes have no metrics, even if metric table is
    // non-empty.  (E.g.: When reading a profile that has been written
    // with WFlg_virtualMetrics, there is a non-empty metric table but
    // no metric values attached to nodes.)
    RFlg_NoMetricValues = (1 << 3),

    // *private*: Data generated by hpcrun as opposed to hpcprof. This
    // affects the normalizations applied to obtain a canonical CCT.
    RFlg_HpcrunData = (1 << 4),

    // only write metric descriptors, even if CCT nodes have metrics
    WFlg_VirtualMetrics = (1 << 15)
  };

  // if non-zero, indicates that CCT nodes do not have metric values
  // even if the metric table is non-empty
  static const char* FmtEpoch_NV_virtualMetrics;


  // make: build an empty Profile or build one from profile file 'fnm'
  static Profile*
  make(uint rFlags);

  static Profile*
  make(const char* fnm, uint rFlags, FILE* outfs, bool sm_easyToGrep);

  
  // fmt_*_fread(): Reads the appropriate hpcrun_fmt object from the
  // file stream 'infs', checking for errors, and constructs
  // appropriate Prof::Profile::CallPath objects.  If 'outfs' is
  // non-null, a textual form of the data is echoed to 'outfs' for
  // human inspection.


  static int
  fmt_fread(Profile* &prof, FILE* infs, uint rFlags,
	    std::string ctxtStr, const char* filename, FILE* outfs, bool sm_easyToGrep);

  static int
  fmt_epoch_fread(Profile* &prof, FILE* infs, uint rFlags,
		  const hpcrun_fmt_hdr_t& hdr, const hpcrun_fmt_footer_t& footer,
		  std::string ctxtStr, const char* filename, FILE* outfs, bool sm_easyToGrep);

#if 0
  static int
  fmt_cct_fread(Profile& prof, FILE* infs, uint rFlags,
		const metric_tbl_t& metricTbl,
		std::string ctxtStr, FILE* outfs);
#else
//YUMENG: No need to parse metricTbl for sparse format
static int
  fmt_cct_fread(Profile& prof, FILE* infs, uint rFlags,
		std::string ctxtStr, FILE* outfs);
#endif


  // fmt_*_fwrite(): Write the appropriate object as hpcrun_fmt to the
  // file stream 'outfs', checking for errors.
  //
  // N.B.: hpcrun-fmt cannot represent static structure.  Therefore,
  // fmt_cct_fwrite() only writes out nodes of type CCT::ADynNode.

  static int
  fmt_fwrite(const Profile& prof, FILE* outfs, uint wFlags);

  static int
  fmt_epoch_fwrite(const Profile& prof, FILE* outfs, uint wFlags);

  static int
  fmt_cct_fwrite(const Profile& prof, FILE* fs, uint wFlags);

  // -------------------------------------------------------
  // Output
  // -------------------------------------------------------

  std::ostream&
  writeXML_hdr(std::ostream& os, uint metricBeg, uint metricEnd,
	       uint oFlags, const char* pfx = "") const;
  
  std::ostream&
  writeXML_hdr(std::ostream& os, uint oFlags = 0, const char* pfx = "") const
  { return writeXML_hdr(os, 0, m_mMgr->size(), oFlags, pfx); }


  // TODO: move Analysis::CallPath::write() here?
  //std::ostream& writeXML_cct(...) const;

  std::ostream&
  dump(std::ostream& os = std::cerr) const;

  void
  ddump() const;

  static const int StructMetricIdFlg = 0;

private:
  void
  canonicalize(uint rFlags = 0);

  uint
  mergeMetrics(Profile& y, int mergeTy, uint& x_newMetricBegIdx);

  // apply MergeEffects after merging two profiles
  void
  merge_fixCCT(const std::vector<LoadMap::MergeEffect>* mrgEffects);

  void
  merge_fixTrace(const CCT::MergeEffectList* mrgEffects);


private:
  std::string m_name;
  double m_fmtVersion;
  epoch_flags_t m_flags;
  uint64_t m_measurementGranularity;

  std::string m_profileFileName; // non-empty, if relevant
  StringSet m_directorySet; // set of directories containing profiles

  std::string m_traceFileName;   // non-empty, if relevant
  StringSet m_traceFileNameSet;
  uint64_t m_traceMinTime, m_traceMaxTime;

  //typedef std::map<std::string, std::string> StrToStrMap;
  //StrToStrMap m_nvPairMap;

  Metric::Mgr* m_mMgr;
  bool m_isMetricMgrVirtual;

  LoadMap* m_loadmap;

  CCT::Tree* m_cct;

  Prof::Struct::Tree* m_structure;
 
  bool m_remove_redundancy;
};

} // namespace CallPath

} // namespace Prof


//***************************************************************************


//***************************************************************************

#endif /* prof_Prof_CallPath_Profile_hpp */

