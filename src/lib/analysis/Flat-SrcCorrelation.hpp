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

#ifndef Analysis_Flat_SrcCorrelation_hpp
#define Analysis_Flat_SrcCorrelation_hpp

//************************ System Include Files ******************************

#include <iostream>
#include <list>    // STL
#include <vector>  // STL
#include <string>

//************************* User Include Files *******************************

#include <include/uint.h>

#include "Args.hpp"
#include "TextUtil.hpp"

#include <lib/profxml/PGMDocHandler.hpp>
#include <lib/profxml/DocHandlerArgs.hpp>

#include <lib/prof/Flat-ProfileData.hpp>
#include <lib/prof/Metric-Mgr.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Analysis {

namespace Flat {


class Driver : public Unique { // not copyable
public:
  Driver(const Analysis::Args& args,
	 Prof::Metric::Mgr& mMgr, Prof::Struct::Tree& structure);
  ~Driver();

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  int
  run();
  
  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  // Test the specified path against each of the paths in the
  // database.  Replace with the pair of the first matching path.
  std::string
  replacePath(const char* path);

  std::string
  replacePath(const std::string& path)
  { return replacePath(path.c_str()); }

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  void
  write_experiment(std::ostream &os) const;

  void
  write_csv(std::ostream &os) const;

  void
  write_txt(std::ostream &os) const;

  void
  write_config(std::ostream &os = std::cout) const;

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  std::string
  toString() const;
  
  void
  dump() const;

public:
  typedef std::map<string, bool> StringToBoolMap;

  typedef std::pair<Prof::Flat::ProfileData*,
		    Prof::Metric::ADescVec*> ProfToMetricsTuple;
  typedef std::vector<ProfToMetricsTuple> ProfToMetricsTupleVec;

private:
  void
  populateStructure(Prof::Struct::Tree& structure);

  void
  correlateMetricsWithStructure(Prof::Metric::Mgr& mMgr,
				Prof::Struct::Tree& structure);


  // -------------------------------------------------------

  void
  computeRawMetrics(Prof::Metric::Mgr& mMgr, Prof::Struct::Tree& structure);

  void
  computeRawBatchJob_LM(const string& lmname, const string& lmname_orig,
			Prof::Struct::Tree& structure,
			ProfToMetricsTupleVec& profToMetricsVec,
			bool useStruct);

  void
  correlateRaw(Prof::Metric::ADesc* metric,
	       const Prof::Flat::EventData& profevent,
	       VMA lm_load_addr,
	       Prof::Struct::Tree& structure,
	       Prof::Struct::LM* lmStrct,
	       /*const*/ BinUtil::LM* lm,
	       bool useStruct);
  
  bool
  getNextRawBatch(ProfToMetricsTupleVec& batchJob,
		  Prof::Metric::Mgr::StringToADescVecMap::const_iterator& it,
		  const Prof::Metric::Mgr::StringToADescVecMap::const_iterator& it_end);

  void
  clearRawBatch(ProfToMetricsTupleVec& batchJob);

  bool
  hasStructure(const string& lmname, Prof::Struct::Tree& structure,
	       StringToBoolMap& hasStructureTbl);

  // -------------------------------------------------------

  void
  computeDerivedMetrics(Prof::Metric::Mgr& mMgr, Prof::Struct::Tree& structure);

  // [mBegId, mEndId)
  void
  computeDerivedBatch(Prof::Struct::Tree& structure,
		      const Prof::Metric::AExpr** mExprVec,
		      uint mBegId, uint mEndId);

  // -------------------------------------------------------

  Prof::Flat::ProfileData*
  readProf(const string& fnm);

  void
  readProf(Prof::Flat::ProfileData* prof);

  BinUtil::LM*
  openLM(const string& fnm);

  // -------------------------------------------------------

  void
  write_txt_secSummary(std::ostream& os,
		       Analysis::TextUtil::ColumnFormatter& colFmt,
		       const std::string& title,
		       const Prof::Struct::ANodeFilter* filter) const;
  
  void
  write_txt_annotateFile(std::ostream& os,
			 Analysis::TextUtil::ColumnFormatter& colFmt,
			 const Prof::Struct::File* fileStrct) const;

  void
  write_txt_hdr(std::ostream& os, const std::string& hdr) const;


private:
  const Analysis::Args& m_args;

  Prof::Metric::Mgr& m_mMgr;
  Prof::Struct::Tree& m_structure;

  static uint profileBatchSz;
};


//****************************************************************************

class DriverDocHandlerArgs
  : public DocHandlerArgs {
public:
  DriverDocHandlerArgs(Driver* driver)
    : DocHandlerArgs(NULL), m_driver(driver)
  { }

  virtual
  ~DriverDocHandlerArgs()
  { }

  // Would be better to use realpath() and RealPathMgr
  string
  replacePath(const char* oldpath) const
  { return m_driver->replacePath(oldpath); }

  string
  replacePath(const std::string& oldpath) const
  { return m_driver->replacePath(oldpath); }
  
private:
  Driver* m_driver;
};

//****************************************************************************

} // namespace Flat

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Flat_SrcCorrelation_hpp
