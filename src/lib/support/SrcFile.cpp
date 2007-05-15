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

//************************* System Include Files ****************************

#ifdef NO_STD_CHEADERS
# include <stdio.h>
#else
# include <cstdio> // for 'fopen'
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "SrcFile.hpp" 

//*************************** Forward Declarations **************************

using std::ostream;
using std::endl;

#define MAXLINESIZE 2048

//***************************************************************************

SrcFile::SrcFile(const char* fN) 
  : fName((fN) ? fN : "")
{
   FILE *srcFile = fopen(fN, "r");
   known = (srcFile != NULL); 
   if (known) {
     char lineBuf[MAXLINESIZE];
     unsigned int ln = 1; 
     while (fgets(lineBuf, MAXLINESIZE, srcFile) != NULL) {
       int llen = strlen(lineBuf); 
       if (lineBuf[llen-1] == '\n') {
	 lineBuf[llen-1] = '\0'; 
       } 
       line[ln++] = lineBuf; 
     }
   } 
}

bool 
SrcFile::GetLine(unsigned int i, 
		 char* lineBuf, unsigned int bufSize) const 
{
  bool ok = false; 
  if (known && (i < line.GetNumElements())) {
    std::string& linei = ((SrcFile*) this)->line[i];
    unsigned int len = linei.length();
    if (bufSize > len) { 
      strncpy(lineBuf, linei.c_str(), len);
      lineBuf[len] = '\0';
      ok = true; 
    }
  } 
  return ok; 
} 

void 
SrcFile::Dump(ostream &out) const 
{
  out << "-----------------------------------" << endl; 
  out << "SrcFile:: fName=" << fName << " known=" 
      << ((known) ? "true" : "false") << " START" << endl; 
  if (known) { 
    for (unsigned int i = 1; i < line.GetNumElements(); i++) {
      out << ((SrcFile*) this)->line[i] << endl; 
    }
  }
  out << "SrcFile:: fName=" << fName << " END" << endl; 
  out << "-----------------------------------" << endl << endl; 
} 

	      
