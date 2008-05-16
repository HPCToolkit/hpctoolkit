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
#include "MetricDescMgr.hpp"

#include <lib/prof-juicy-x/PGMDocHandler.hpp>
#include <lib/prof-juicy-x/DocHandlerArgs.hpp>

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Analysis {

namespace Flat {


class Driver : public Unique { // at most one instance 
public: 
  Driver(const Analysis::Args& args, const Analysis::MetricDescMgr& mMgr);
  ~Driver(); 
  
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  void createProgramStructure(PgmScopeTree& scopes);
  void correlateMetricsWithStructure(PgmScopeTree& scopes);

  // see 'ScopeInfo' class for dump flags
  void XML_Dump(PgmScope* pgm, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void XML_Dump(PgmScope* pgm, int dumpFlags, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void CSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;
  void TSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;

  void write_config(std::ostream &os = std::cout) const;

  std::string toString() const; 
  void dump() const { std::cerr << toString() << std::endl; }


  std::string ReplacePath(const char* path);
  std::string ReplacePath(const std::string& path)
    { return ReplacePath(path.c_str()); }

private:
  void computeSampledMetrics(PgmScopeTree& scopes);
  void computeDerivedMetrics(PgmScopeTree& scopes);
  void computeFlatProfileMetrics(PgmScopeTree& scopes,
				 const string& profFilenm,
				 const MetricDescMgr::MetricList_t& metricList);

  std::string SearchPathStr() const;

private:
  const Analysis::Args& m_args;
  const Analysis::MetricDescMgr& m_metricMgr;
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

//****************************************************************************

} // namespace Flat

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Flat_SrcCorrelation_hpp
