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


#ifndef HTMLScopes_h 
#define HTMLScopes_h 

//************************ System Include Files ******************************

//************************* User Include Files *******************************

#include "ScopesInfo.hpp"
#include "HTMLFile.hpp"
#include <lib/support/String.hpp>

//************************ Forward Declarations ******************************

class IntVector; 
class Args;

//****************************************************************************

class HTMLScopes { 
public: 
  HTMLScopes(const ScopesInfo &scopes, 
	     IntVector *perfIndicesForColumns, // terminate with -1 
	     const Args &pgmArgs); 

  ~HTMLScopes(); 

  void Write(const char* htmlDir, const char* bgColor, 
	     const char* headerColor) const; 
    
  static const char* Name(); 
  
  // html file that contains col header with metric names
  String HeadFileName() const;      

  // html file that contains row header for current scope and its parent
  static String SelfHeadFileName();  

  // html file that contains row header for current scope's children 
  static String KidsHeadFileName();  

  // frame that contains info for current scope and its parent
  static String SelfFrameName();  

  // frame file that contains info for current scope's children 
  static String KidsFrameName();  
  
  // html file that contains perf data for current scope and parent
  static String SelfFileName(const ScopeInfo* si, int pIndex, 
			     int flattening = 0); 

  // html file that contains perf data for current scope's children 
  static String KidsFileName(const ScopeInfo* si, int pIndex, 
			     int flattening); 

  // generate an uniq anchor name for the given scope info with the
  // specified performance metric and flattening level
  static String AnchorName(const ScopeInfo* si, int pIndex, 
			     int flattening); 

  String ToString() const; 

private:
  int WriteScopesForMetric(const char* htmlDir, int sortByPerfIndex,
			   const char* headBgColor, 
			   const char* bodyBgColor) const;

  void WriteHeader(HTMLFile &hf) const; 
  void WriteScope(const char* htmlDir, const char* bgColor, 
		  const ScopeInfo &scope, int pIndex, int flattening) const;
  void WriteScopeOldStyle(const char* htmlDir, const char* bgColor, 
		  const ScopeInfo &scope, int pIndex, int flattening) const;
  void WriteLine(HTMLFile &hf, unsigned int level, 
		 const ScopeInfo *si, int pIndex, int flattening, 
		 bool anchor) const;
  void WriteHR(HTMLFile &hf) const; // write horizontal rule
  
  // the fields 
  String name; 
  const ScopesInfo &scopes;       // contains all refs/lines/loops/procs to
                                  // be considered
  
  IntVector *perfIndex;           // the perfIndices for perfData we should 
                                  // display in table columns

  const Args &args;               // program argument information 
  
  static const DataDisplayInfo HeaderDisplayInfo; 
  static const char* outerHeader; 
  static const char* currentHeader; 
  static const char* innerHeader; 


}; 
#endif 
