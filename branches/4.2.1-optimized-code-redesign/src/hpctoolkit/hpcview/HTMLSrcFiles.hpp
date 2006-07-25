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

#ifndef HTMLSrcFiles_h 
#define HTMLSrcFiles_h 

//************************ System Include Files ******************************

//************************* User Include Files *******************************

#include "ScopeInfo.hpp"
#include <lib/support/String.hpp>

//************************ Forward Declarations ******************************

class Args;
class FileScope; 
class HTMLFile;

//****************************************************************************

class HTMLSrcFiles { 
public: 
  HTMLSrcFiles(const ScopesInfo &scopes, const Args &pgmArgs, 
	       const ScopeInfoFilter &entryFilter, 
	       bool leavesOnly);
  
  void WriteLabels(const char* dir, 
		   const char* fgColor, 
		   const char* bgColor) const; 
  void WriteSrcFiles(const char* dir) const;   

  static String LabelFileName(const char* name); 

private:
  void WriteSourceLabel(const char* dir, const FileScope *file, 
			const char *fgColor, const char* bgColor) const;
  void WriteSrcFile(const char* dir, FileScope &file) const;   
  void WriteSrc(HTMLFile &hf, FileScope &file) const;
  void GenerateSrc(HTMLFile &hf, FileScope &file) const;
  void GenSrc(HTMLFile &hf, CodeInfo &ci, unsigned int level, 
	      unsigned int &lastLine) const; 

  const ScopeInfoFilter filter;   // only include when filter.fct(entry)==true
  const bool leavesOnly;       // if true only include leaves, 
                                  // otherwise include internal nodes as well
  
  const ScopesInfo &scopes;       // contains all refs/lines/loops/procs to
                                  // be considered
  const Args &programArgs;       
}; 

#endif 
