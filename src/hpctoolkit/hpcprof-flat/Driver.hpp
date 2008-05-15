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

#ifndef Driver_hpp
#define Driver_hpp 

//************************ System Include Files ******************************

#include <iostream>
#include <ostream>
#include <list>    // STL
#include <vector>  // STL
#include <utility> // STL
#include <string>

//************************* User Include Files *******************************

#include <include/general.h>

#include "Args.hpp"
#include <lib/analysis/DerivedPerfMetrics.hpp>

#include <lib/prof-juicy-x/PGMDocHandler.hpp>
#include <lib/prof-juicy-x/DocHandlerArgs.hpp>

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

typedef std::map<string, int> StringToIntMap;

//****************************************************************************

class Driver : public Unique { // at most one instance 
public: 
  Driver(Args& args);
  ~Driver(); 
  
  // -------------------------------------------------------
  // Results of ConfigParser
  // -------------------------------------------------------

  unsigned int numMetrics() const          { return m_metrics.size(); }
  const PerfMetric& getMetric(int i) const { return *m_metrics[i]; }
  void addMetric(PerfMetric* metric);

  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  void makePerfMetricDescs(std::vector<std::string>& profileFiles);
  void createProgramStructure(PgmScopeTree& scopes);
  void correlateMetricsWithStructure(PgmScopeTree& scopes);

  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  std::string ReplacePath(const char* path);
  std::string ReplacePath(const std::string& path)
    { return ReplacePath(path.c_str()); }

  std::string SearchPathStr() const;

  std::string makeUniqueName(StringToIntMap& tbl, const std::string& nm);

  // see 'ScopeInfo' class for dump flags
  void XML_Dump(PgmScope* pgm, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void XML_Dump(PgmScope* pgm, int dumpFlags, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void CSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;
  void TSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;

  void write_config(std::ostream &os = std::cout) const;


  std::string ToString() const; 
  void Dump() const { std::cerr << ToString() << std::endl; }

private:
  typedef std::list<FilePerfMetric*> MetricList_t;
  
  void ProcessPGMFile(NodeRetriever* nretriever, 
		      PGMDocHandler::Doc_t docty, 
		      const std::vector<std::string>& files);

  void computeSampledMetrics(PgmScopeTree& scopes);
  void computeDerivedMetrics(PgmScopeTree& scopes);
  void computeFlatProfileMetrics(PgmScopeTree& scopes,
				 const string& profFilenm,
				 const MetricList_t& metricList);

private:
  Args& m_args;
  std::vector<PerfMetric*> m_metrics;
};

//****************************************************************************

class DriverDocHandlerArgs : public DocHandlerArgs {
public:
  DriverDocHandlerArgs(Driver* driver) 
    : m_driver(driver) { }
  
  ~DriverDocHandlerArgs() { }
  
  virtual string ReplacePath(const char* oldpath) const { 
    return m_driver->ReplacePath(oldpath);
  };
  
private:
  Driver* m_driver;
};

#endif 
