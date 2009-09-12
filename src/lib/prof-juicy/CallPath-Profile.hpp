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

#include "LoadMapMgr.hpp"
#include "Metric-ADesc.hpp"
#include "CCT-Tree.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {


class Profile: public Unique {
public:
  Profile(const std::string name, uint numMetrics);
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


  // -------------------------------------------------------
  // Metrics
  // -------------------------------------------------------
  uint
  numMetrics() const
  { return m_metricdesc.size(); }
  
  Metric::SampledDesc* 
  metric(uint i) const 
  { return m_metricdesc[i]; }

  const Metric::SampledDescVec&
  metricDesc() const 
  { return m_metricdesc; }
  
  uint
  addMetric(Metric::SampledDesc* m)
  {
    m_metricdesc.push_back(m);
    uint m_id = numMetrics() - 1;
    return m_id;
  }

  // -------------------------------------------------------
  // LoadMapMgr
  // -------------------------------------------------------
  LoadMapMgr*
  loadMapMgr() const
  { return m_loadmapMgr; }
  
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

  // Given a Profile y, merge y into x = 'this'
  // ASSUMES: both x and y are in canonical form (canonicalize())
  // WARNING: the merge may change/destroy y
  void
  merge(Profile& y, bool isSameThread);


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  static Profile* 
  make(const char* fnm, FILE* outfs);

  
  // fmt_*_fread(): Reads the appropriate hpcrun_fmt object from the
  // file stream 'infs', checking for errors, and constructs
  // appropriate Prof::Profile::CallPath objects.  If 'outfs' is
  // non-null, a textual form of the data is echoed to 'outfs' for
  // human inspection.

  static int
  fmt_fread(Profile* &prof, FILE* infs, std::string ctxtStr, FILE* outfs);

  static int
  fmt_epoch_fread(Profile* &prof, FILE* infs, 
		  HPCFMT_List(hpcfmt_nvpair_t)* hdrNVPairs,
		  std::string ctxtStr, FILE* outfs);

  static int
  fmt_cct_fread(Profile& prof, FILE* infs, LoadMap* loadmap, FILE* outfs);


  // fmt_*_fwrite(): Write the appropriate object as hpcrun_fmt to the
  // file stream 'outfs', checking for errors.
  //
  // N.B.: hpcrun-fmt cannot represent static structure.  Therefore,
  // fmt_cct_fwrite() only writes out nodes of type CCT::ADynNode.

  static int
  fmt_fwrite(const Profile& prof, FILE* outfs);

  static int
  fmt_epoch_fwrite(const Profile& prof, FILE* outfs);

  static int
  fmt_cct_fwrite(const Profile& prof, FILE* fs);


  // -------------------------------------------------------
  // Output
  // -------------------------------------------------------

  std::ostream& 
  writeXML_hdr(std::ostream& os, int oFlags = 0, const char* pfx = "") const;

  // TODO: move Analysis::CallPath::write() here?
  //std::ostream& writeXML_cct(...) const;

  std::ostream&
  dump(std::ostream& os = std::cerr) const;

  void
  ddump() const;

  static const int StructMetricIdFlg = 0;

private:
  void 
  canonicalize();

  // apply CCT MergeChange after merging two profiles
  void 
  merge_fixCCT(std::vector<LoadMap::MergeChange>& mergeChg);
 
private:
  std::string m_name;
  epoch_flags_t m_flags;
  uint64_t m_measurementGranularity;
  uint32_t m_raToCallsiteOfst;

  //typedef std::map<std::string, std::string> StrToStrMap;
  //StrToStrMap m_nvPairMap;

  Metric::SampledDescVec m_metricdesc; // FIXME: Metric::Mgr?

  LoadMapMgr* m_loadmapMgr;

  CCT::Tree* m_cct;

  Prof::Struct::Tree* m_structure;
};

} // namespace CallPath

} // namespace Prof


//***************************************************************************


//***************************************************************************

#endif /* prof_juicy_Prof_CallPath_Profile_hpp */

