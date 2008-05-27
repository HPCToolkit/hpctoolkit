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

#ifndef Analysis_Flat_SrcCorrelation_hpp
#define Analysis_Flat_SrcCorrelation_hpp 

//************************ System Include Files ******************************

#include <iostream>
#include <list>    // STL
#include <vector>  // STL
#include <string>

//************************* User Include Files *******************************

#include <include/general.h>

#include "Args.hpp"
#include "TextUtil.hpp"

#include <lib/prof-juicy-x/PGMDocHandler.hpp>
#include <lib/prof-juicy-x/DocHandlerArgs.hpp>

#include <lib/prof-juicy/FlatProfileReader.hpp>
#include <lib/prof-juicy/MetricDescMgr.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Analysis {

namespace Flat {


class Driver : public Unique { // not copyable
public: 
  Driver(const Analysis::Args& args, 
	 Prof::MetricDescMgr& mMgr, PgmScopeTree& structure);
  ~Driver(); 

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  int run();
  
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  // Test the specified path against each of the paths in the
  // database.  Replace with the pair of the first matching path.
  std::string replacePath(const char* path);
  std::string replacePath(const std::string& path)
    { return replacePath(path.c_str()); }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  void write_experiment(std::ostream &os) const;
  void write_csv(std::ostream &os) const;
  void write_txt(std::ostream &os) const;

  void write_config(std::ostream &os = std::cout) const;

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  std::string toString() const;
  void dump() const;

public:
  typedef std::map<string, bool> StringToBoolMap;

  typedef std::pair<Prof::Flat::Profile*, 
		    Prof::MetricDescMgr::PerfMetricVec*> ProfToMetricsTuple;
  typedef std::vector<ProfToMetricsTuple> ProfToMetricsTupleVec;

public:
  void 
  populatePgmStructure(PgmScopeTree& structure);

  void 
  correlateMetricsWithStructure(Prof::MetricDescMgr& mMgr,
				PgmScopeTree& structure);


  // -------------------------------------------------------

  void 
  computeRawMetrics(Prof::MetricDescMgr& mMgr, PgmScopeTree& structure);

  void
  computeRawBatchJob_LM(const string& lmname, const string& lmname_orig,
			NodeRetriever& structIF,
			ProfToMetricsTupleVec& profToMetricsVec,
			bool useStruct);

  void
  correlate(PerfMetric* metric,
	    const Prof::Flat::EventData& profevent,
	    VMA lm_load_addr,
	    NodeRetriever& structIF,
	    LoadModScope* lmStrct,
	    /*const*/ binutils::LM* lm,
	    bool useStruct);
  
  bool
  getNextBatch(ProfToMetricsTupleVec& batchJob,
	       Prof::MetricDescMgr::StringPerfMetricVecMap::const_iterator& it,
	       const Prof::MetricDescMgr::StringPerfMetricVecMap::const_iterator& it_end);

  void
  clearBatch(ProfToMetricsTupleVec& batchJob);

  bool
  hasStructure(const string& lmname, NodeRetriever& structIF,
	       StringToBoolMap& hasStructureTbl);

  // -------------------------------------------------------

  void 
  computeDerivedMetrics(Prof::MetricDescMgr& mMgr, PgmScopeTree& structure);

  // -------------------------------------------------------

  Prof::Flat::Profile*
  readProf(const string& fnm);

  void
  readProf(Prof::Flat::Profile* prof);

  binutils::LM*
  openLM(const string& fnm);

  std::string 
  searchPathStr() const;

  // -------------------------------------------------------

  void write_secSummary(std::ostream& os, 
			Analysis::TextUtil::ColumnFormatter& colFmt,
			const std::string& title,
			const ScopeInfoFilter* filter) const;

private:
  const Analysis::Args& m_args;

  Prof::MetricDescMgr& m_mMgr;
  PgmScopeTree& m_structure;

  static uint profileBatchSz;
};

//****************************************************************************

class DriverDocHandlerArgs : public DocHandlerArgs {
public:
  DriverDocHandlerArgs(Driver* driver) 
    : m_driver(driver) { }
  
  ~DriverDocHandlerArgs() { }
  
  virtual string replacePath(const char* oldpath) const { 
    return m_driver->replacePath(oldpath);
  };
  
private:
  Driver* m_driver;
};

//****************************************************************************

} // namespace Flat

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Flat_SrcCorrelation_hpp
