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

//************************ System Include Files ******************************

#include <iostream> 

//************************* User Include Files *******************************

#include "StaticFiles.h"
#include <lib/support/Files.h>

//************************ Forward Declarations ******************************

using std::cout;
using std::cerr;
using std::endl;

//****************************************************************************

const char* StaticFileName[] = {
  "detect_bs.js",     // JavaScript
  "global.js",
  "utilsForHere.js",
  "utilsForAll.js",
  "utilsForAll_old.js",

  "styleForAll.css",  // HPCView StyleSheets
  "styleHere.css",

  "manual.html",      // Manual (also uses flatten.gif and unflatten.gif)
  "man.html",
  "manContents.html",
  "manTitle.html",
  "styleForMan.css",
  "uparrow.gif",
  "downarrow.gif",
  
  "scopes/styleHere.css",  // HPCView scope files
  "scopes/utilsForHere.js",
  "scopes/uparrow.gif",
  "scopes/downarrow.gif",
  "scopes/noarrow.gif",

  "flatten.gif",       // HPCView files
  "unflatten.gif",
  "scopes.self.hdr.html",
  "scopes.kids.hdr.html",
  "clr_highlights.jpg",
  NULL
}; 

extern bool IsJSFile(StaticFileId id) 
{
  // scopes/utilsForHere.js is a javascript file but should never be included 
  // via HTMLFile::JSFileInclude, which is the user of IsJSFile
  return (id < MANUAL);   
} 

int 
StaticFiles::CopyAllFiles(bool oldStyleHtml)  const
{
  int ret = 0; 
  if (oldStyleHtml)
    ret = ret || Copy(StaticFileName[(int)UTILSALLOLD], 
                StaticFileName[(int)UTILSALL]); 
  else
    ret = ret || Copy(StaticFileName[(int)UTILSALL], 
                StaticFileName[(int)UTILSALL]); 
     
  for (int i = 0; StaticFileName[i]; i++) {
    if ((StaticFileId)i != UTILSALLOLD && (StaticFileId)i != UTILSALL)
      ret = ret || Copy(StaticFileName[i], StaticFileName[i]); 
  }
  return ret; 
}

int 
StaticFiles::Copy(const char* fnameSrc, const char* fnameDst)  const
{
   String srcFname = fileHome + "/" + fnameSrc; 
   String dstFname = htmlDir + "/" + fnameDst; 
   const char* error = CopyFile(dstFname, (const char*) srcFname, NULL); 
   if (error) {
     cerr << "ERROR: " << error << endl; 
     return 1; 
   } 
   return 0; 
}

