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

#ifndef HTMLDriver_h 
#define HTMLDriver_h 

//************************ System Include Files ******************************

//************************* User Include Files *******************************

#include <include/general.h>

#include "Args.hpp"
#include "PerfMetric.hpp"
#include "PerfIndex.hpp"

#include <lib/support/String.hpp>
#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

class ScopesInfo; 
class ScopeInfo; 
class FileScope; 
class HTMLTable; 
class HTMLScopes; 
class Driver; 

//****************************************************************************

extern const char* SOURCE;  
extern const char* SCOPES_SELF;
extern const char* SCOPES_KIDS;
extern const char* ScopesDir;

#define NO_FLATTEN_DEPTH (-1)

class HTMLDriver : public Unique { // at most one instance 
public: 
  HTMLDriver(const ScopesInfo &scps, 
	         const char* fileHome,  // home of static files 
	         const char* htmlDir,
	         const Args &pgmArgs);  // output dir for html and js files 
  
  ~HTMLDriver(); 

  bool Write(const Driver& driver) const; // terminate with -1
  
  static String UniqueName(const ScopeInfo *s, 
			   int pIndex, int flattenDepth); // s non-NULL 

  static String UniqueNameForSelf(const ScopeInfo *s);

  static const DataDisplayInfo NameDisplayInfo; 
 
  String ToString() const; 

private:
  void WriteFiles(const char* name) const; 
  void WriteHeader(const char* name, const String& tit) const; 
  void WriteIndexFile(PgmScope *pgmScope, 
		      HTMLTable &table, int perfIndex, 
		      HTMLScopes& scopes, 
		      const String & title, 
		      const char* headerFile) const; 

  const ScopesInfo &scopes; 
  const char* fileHome; 
  const char* htmlDir; 

  const Args &args;               // program argument information
};

extern void HTMLDriverTest(int argc, const char**argv); 
// a tester that bypasses XMP parsing and simply builds PerfMetrics and 
// a small scope tree which it writes out.

#endif 
