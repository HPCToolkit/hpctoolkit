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

#include "Args.hpp" // for FilePerfMetric
#include "DerivedPerfMetrics.hpp" // for FilePerfMetric

#include <lib/prof-juicy-x/PGMDocHandler.hpp>
#include <lib/prof-juicy-x/DocHandlerArgs.hpp>

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

class Driver : public Unique { // at most one instance 
public: 
  Driver(Args& args);
  ~Driver(); 
  
  // -------------------------------------------------------
  // Backwards compatibility interace to Args
  // -------------------------------------------------------

  void SetTitle(const std::string& x) { m_args.title = x; }
  const std::string& Title() const    { return m_args.title; }

  void AddSearchPath(const std::string& _path, const std::string& _viewname)
    { m_args.searchPaths.push_back(PathTuple(_path, _viewname)); }
  const PathTupleVec& SearchPathVec() const { return m_args.searchPaths; }

  void AddStructureFile(const std::string& pf) { m_args.structureFiles.push_back(pf); }
  const std::string& GetStructureFile(int i) const { return m_args.structureFiles[i]; }
  int NumberOfStructureFiles() const { return m_args.structureFiles.size(); }

  void AddGroupFile(const std::string& pf) { m_args.groupFiles.push_back(pf); }
  const std::string& GetGroupFile(int i) const { return m_args.groupFiles[i]; }
  int NumberOfGroupFiles() const { return m_args.groupFiles.size(); }

  void AddReplacePath(const std::string& inPath, const std::string& outPath);

  bool MustDeleteUnderscore( void ) { return m_args.deleteUnderscores > 0; }
  bool CopySrcFiles() { return m_args.db_copySrcFiles; }


  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  unsigned int NumberOfMetrics() const       { return dataSrc.size(); }
  const PerfMetric& PerfDataSrc(int i) const { return *dataSrc[i]; }
  void Add(PerfMetric* metric); 

  std::string ReplacePath(const char* path);
  std::string ReplacePath(const std::string& path)
    { return ReplacePath(path.c_str()); }

  std::string SearchPathStr() const;


  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  void ScopeTreeInitialize(PgmScopeTree& scopes);
  void ScopeTreeComputeMetrics(PgmScopeTree& scopes);

  // see 'ScopeInfo' class for dump flags
  void XML_Dump(PgmScope* pgm, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void XML_Dump(PgmScope* pgm, int dumpFlags, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void CSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;
  void TSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;

  std::string ToString() const; 
  void Dump() const { std::cerr << ToString() << std::endl; }

private:
  typedef std::list<FilePerfMetric*> MetricList_t;
  
  void ProcessPGMFile(NodeRetriever* nretriever, 
		      PGMDocHandler::Doc_t docty, 
		      const std::vector<std::string>& files);

  void ScopeTreeComputeHPCRUNMetrics(PgmScopeTree& scopes);
  void ScopeTreeComputeOtherMetrics(PgmScopeTree& scopes);
  void ScopeTreeInsertHPCRUNData(PgmScopeTree& scopes,
				 const string& profFilenm,
				 const MetricList_t& metricList);
  
private:
  Args& m_args;
  std::vector<PerfMetric*> dataSrc;
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
  
  virtual bool MustDeleteUnderscore() const { 
    return m_driver->MustDeleteUnderscore();
  }

private:
  Driver* m_driver;
};

#endif 
