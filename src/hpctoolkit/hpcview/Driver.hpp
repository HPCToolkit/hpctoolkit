// -*-Mode: C++;-*-
// $Id$
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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

#ifndef Driver_h
#define Driver_h 

//************************ System Include Files ******************************

#include <iostream>
#include <ostream>
#include <vector> // STL
#include <utility> // STL

//************************* User Include Files *******************************

#include <include/general.h>

#include "PGMDocHandler.hpp"

#include <lib/support/Unique.hpp>
#include <lib/support/String.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// PathTuple: a {path, viewname} pair.
//   PathTuple.first = path; PathTuple.second = viewname
// PathTupleVec: the vector of all 'PathTuple'
typedef std::pair<String, String> PathTuple;
typedef std::vector<PathTuple> PathTupleVec;

class PerfMetric; 
class ScopesInfo;
class PgmScope;

class Driver : public Unique { // at most one instance 
public: 
  Driver(int deleteUnderscores, bool _cpySrcFiles); 
  ~Driver(); 
  
  void SetTitle(const char* tit)        { title = tit; }
  const String& Title() const           { return title; }

  void AddStructureFile(const char* pf) { structureFiles.push_back(new String(pf)); }
  const String& GetStructureFile(int i) const { return *structureFiles[i]; }
  int NumberOfStructureFiles() { return structureFiles.size(); }

  void AddGroupFile(const char* pf) { groupFiles.push_back(new String(pf)); }
  const String& GetGroupFile(int i) const { return *groupFiles[i]; }
  int NumberOfGroupFiles() { return groupFiles.size(); }

  void AddPath(const char* _path, const char* _viewname);
  const String& Path() const { return path; }
  const PathTupleVec& PathVec() const { return pathVec; }

  void AddReplacePath(const char* inPath, const char* outPath); 
  String ReplacePath(const char* path);
  
  bool MustDeleteUnderscore( void );
  bool CopySrcFiles() { return cpySrcFiles; }
  
  unsigned int NumberOfMetrics() const       { return dataSrc.size(); }
  const PerfMetric& PerfDataSrc(int i) const { return *dataSrc[i]; }
  void Add(PerfMetric* metric); 
  
  void MakePerfData(ScopesInfo& scopesInfo);

  String ToString() const; 
  void Dump() const { std::cerr << ToString() << std::endl; }

  // see 'ScopeInfo' class for dump flags
  void XML_Dump(PgmScope* pgm, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void XML_Dump(PgmScope* pgm, int dumpFlags, std::ostream &os = std::cout, 
		const char *pre = "") const;
  void CSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;
  void TSV_Dump(PgmScope* pgm, std::ostream &os = std::cout) const;

private: 
  
  void ProcessPGMFile(NodeRetriever* nretriever, 
		      PGMDocHandler::Doc_t docty, 
		      std::vector<String*>* files);

  String title;
  int deleteTrailingUnderscores;
  bool cpySrcFiles;

  String path;             // a string-list of paths (includes '.') 
  PathTupleVec pathVec;    // a list of {path, viewname} 

  std::vector<String> replaceInPath; 
  std::vector<String> replaceOutPath; 

  std::vector<PerfMetric*> dataSrc;
  std::vector<String*> structureFiles;
  std::vector<String*> groupFiles;
};

#endif 
