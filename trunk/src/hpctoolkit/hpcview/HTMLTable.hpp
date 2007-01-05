// $Id$
// -*-C++-*-
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

#ifndef HTMLTable_h 
#define HTMLTable_h 

//************************ System Include Files ******************************

//************************* User Include Files *******************************

#include "PerfIndex.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/String.hpp>

//************************ Forward Declarations ******************************

class Args;
class IntVector; 
class HTMLFile; 

//****************************************************************************

class HTMLTable { 
public: 
  HTMLTable(const char* name, 
	    const PgmScopeTree &scopes, 
            const ScopeInfoFilter &entryFilter, 
            bool leavesOnly, 
	    IntVector *perfIndicesForColumns,
	    const Args& pgmArgs); // terminate with -1 
  ~HTMLTable(); 

  void Write(const char* dir, 
	     const char* headBgColor, 
	     const char* bodyBgColor) const; 
    
  String Name() const; 
  
  String TableFileName(int perfIndex) const; 
  String HeadFileName(int perfIndex) const; 

  static void WriteMetricHeader(HTMLFile& hf, const IntVector *perfIndex);
  static bool WriteMetricValues(HTMLFile& hf, const IntVector *perfIndex, 
				const ScopeInfo *scope, 
				const PgmScope &root);

  static bool MetricMeetsThreshold(const IntVector *perfIndex, 
				   const int whichIndex, 
				   const float threshold,
				   const ScopeInfo *scope, 
				   const PgmScope &root);


  static String CodeName(const ScopeInfo &si); 
  static bool UsesIt(const ScopeInfo &sinfo); 
  
  String ToString() const; 
private:
  static const char* highlightColor;  // used to highlight sorted col's header
  
  static DataDisplayInfo NameDisplayInfo; 
  static int percentWidth; 
  String TableHeadFileName(const char* dir) const;
  
  bool WriteTableHead(const char* dir, 
			 int sortByPerfIndex, 
			 const char* bgColor) const;
  
  bool WriteTableBody(const char* dir, 
			 int sortByPerfIndex, 
			 const char* bgColor) const;
  
  bool WriteRow(HTMLFile &os, const ScopeInfo &scope, bool giveId,
		   int perfIndexToThreshold = NO_PERF_INDEX) const; 
  void ScopeToRowLabel(const CodeInfo &code, String &nm, String &fill) const;

  // the fields 
  String name; 
  const ScopeInfoFilter filter;   // only include when filter.fct(entry)==true
  const bool leavesOnly;          // if true only include leaves, 
                                  // otherwise include internal nodes as well
  
  const PgmScopeTree &scopes;     // contains all refs/lines/loops/procs to
                                  // be considered
  
  IntVector *perfIndex;           // the perfIndeces for perfData we should 
                                  // display in table columns

  const Args &args;               // program argument information
}; 

#endif 
