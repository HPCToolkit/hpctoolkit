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

#ifndef prof_juicy_Prof_CallPath_Profile_hpp 
#define prof_juicy_Prof_CallPath_Profile_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>

#include <cstdio>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "MetricDesc.hpp"
#include "LoadMapMgr.hpp"
#include "CCT-Tree.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {


class Profile: public Unique {
public:
  Profile(uint numMetrics);
  virtual ~Profile();
  
  // -------------------------------------------------------
  // name
  // -------------------------------------------------------
  const std::string& name() const 
  { return m_name; }

  void name(const char* s) 
  { m_name = (s) ? s : ""; }
  
  // -------------------------------------------------------
  // Metrics
  // -------------------------------------------------------
  uint numMetrics() const
  { return m_metricdesc.size(); }
  
  SampledMetricDesc* metric(uint i) const 
  { return m_metricdesc[i]; }

  const SampledMetricDescVec& metricDesc() const 
  { return m_metricdesc; }
  
  uint addMetric(SampledMetricDesc* m) {
    m_metricdesc.push_back(m);
    uint m_id = numMetrics() - 1;
    return m_id;
  }

  // -------------------------------------------------------
  // LoadMapMgr
  // -------------------------------------------------------
  LoadMapMgr* loadMapMgr() const
  { return m_loadmapMgr; }
  
  // -------------------------------------------------------
  // CCT
  // -------------------------------------------------------
  CCT::Tree* cct() const 
  { return m_cct; }

  // -------------------------------------------------------
  // Static structure
  // -------------------------------------------------------
  Prof::Struct::Tree* structure() const
  { return m_structure; }

  void structure(Prof::Struct::Tree* x)
  { m_structure = x; }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // Given a Profile y, merge y into x = 'this'
  // ASSUMES: both x and y are in canonical form (cct_canonicalize())
  // WARNING: the merge may change/destroy y
  void merge(Profile& y, bool isSameThread);


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  static Profile* 
  make(const char* fnm, FILE* outfs);

  // hpcrun_fmt_epoch_fread(): 
  // hpcrun_fmt_cct_fread(): 
  //
  // Reads the appropriate hpcrun_fmt object from the file stream
  // 'infs', checking for errors, and constructs appropriate
  // Prof::Profile::CallPath objects.  If 'outfs' is non-null, a
  // textual form of the data is echoed to 'outfs' for human
  // inspection.

  static int
  hpcrun_fmt_epoch_fread(Profile* &prof, FILE* infs, 
			 std::string locStr, FILE* outfs);

  static void
  hpcrun_fmt_cct_fread(CCT::Tree* cct, epoch_flags_t flags, int num_metrics,
		       FILE* infs, FILE* outfs);


  // -------------------------------------------------------
  // Dump contents for inspection
  // -------------------------------------------------------

  std::ostream& 
  writeXML_hdr(std::ostream& os = std::cerr, 
	       int oFlags = 0, const char *pre = "") const;

  //std::ostream& writeXML_cct(...) const;

  virtual std::ostream& dump(std::ostream& os = std::cerr) const;
  virtual void ddump() const;

  static const int StructMetricIdFlg = 0;

private:
  // 1. annotate CCT::Tree nodes with associated ALoadMap::LM_id_t from
  //    the LoadMap.
  // 2. normalize CCT::Tree node IPs (unrelocate) to prepare for LoadMapMgr
  void 
  cct_canonicalize(const LoadMap& loadmap);

  // maintain the CCT invariants after merging two profiles
  void 
  cct_canonicalizePostMerge(std::vector<LoadMap::MergeChange>& mergeChg);
 
private:
  std::string m_name;
  SampledMetricDescVec m_metricdesc;
  LoadMapMgr* m_loadmapMgr;
  CCT::Tree* m_cct;
  Prof::Struct::Tree* m_structure;
};

} // namespace CallPath

} // namespace Prof


//***************************************************************************


//***************************************************************************

#endif /* prof_juicy_Prof_CallPath_Profile_hpp */

