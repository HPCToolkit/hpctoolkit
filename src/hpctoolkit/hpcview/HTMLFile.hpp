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

/*************************************************************
 * Author: Liwei Peng
 * Date: August 1999
 *************************************************************/

#ifndef _HTMLFile_h_
#define _HTMLFile_h_

//************************ System Include Files ******************************

#include <fstream> 

//************************* User Include Files *******************************

#include <include/general.h>

#include "StaticFiles.hpp"
#include "ScopeInfo.hpp"

#include <lib/support/String.hpp>

//************************ Forward Declarations ******************************

// Escapes or unescapes the string
String HTMLEscapeStr(const char* str);
String HTMLUnEscapeStr(const char* str);

//****************************************************************************

class HTMLFile : public std::ofstream {

public:
  HTMLFile(const char* dir, 
	   const char *filename, 
	   const char* title,
	   bool frameset = false, 
           int font_sz = 3);
  ~HTMLFile();
  
  void SetFgColor(const char *);
  void SetBgColor(const char *);
  void SetOnLoad(const char *);
  void SetOnResize(const char *);

  void StopHead();  // head is automatically started upon construction
  void StartBodyOrFrameset();

  void PrintNoBodyOrFrameset(); // file has no <body> or <frameset> to print

  std::ostream &BodyOrFrameset();  
  
  void JSFileInclude(StaticFileId  i, const char* dir = NULL);
  void CSSFileInclude(StaticFileId  i, const char* dir = NULL);

  void FontColorStart(const char* color); 
  void FontColorStop(const char* color);
   
  void LayerIdStart(const char* id); 
  void LayerIdStop();
  void Anchor(const char* id); 
  void WriteHighlightDiv();
  
  void JavascriptHref(const char *display, const char* jsCode); 

  void LoadHref(const char *displayName, 
		const char* frameName, const char *fileName, 
		unsigned int width);

  void SetTableAndScopes(const char* tableFrame, const char *tableFile, 
			 const char* selfFrame, 
			 // const char *selfFile, 
			 const char* kidsFrame, 
			 //const char *kidsFile,
			 const char *metric_name,
			 const char *metric_display_name,
			 unsigned int width);

  void NavigateFrameHref(const char *anchor, const char* text, 
			 unsigned int width); 

  void NavigateFrameHrefForLoop(LoopScope* ls, unsigned int width, 
                    int flattening); 
  void NavigateFrameHrefForProc(ProcScope* ls, unsigned int width,
                    int flattening); 

  void NavigateSourceHref(const char *navid, const char *parentid, const int scope_depth, const char *text,
			  unsigned int width); 
  
  void ScopesSetCurrent(const char* anchor, const char* text, const int depth, 
	        const char *mouseOverText,  unsigned int width);
  
  void GotoSrcHref(const char *displayStr, const char* srcFile);
  
  void TableStart(const char* name, const char *sortMetricName,
		  const char* headerFile = NULL); 
  void TableStop(); 
  
  const char * FileName() {
     return fName;
  }

private:
  // disable
  HTMLFile(const HTMLFile &);
  
private:
  std::ofstream *ofs; 

  String fName; 

  String fgcolor;
  String bgcolor;
  String onload;
  String onresize;
  
  int font_size;

  void StartHead(const char* title); 
  void StopBodyOrFrameset();
  void SetUp();

  // head
  bool hasHead;      // eraxxon: every doc should have a header to ensure
                        //   doctype and character encoding are set.
  bool headStopped; 

  // body/frameset
  bool usesBodyOrFrameset;    // eraxxon: this is only for convenience
  bool isFrameset;            // eraxxon: if false, then assume BODY. (BODY
                                 //   (and FRAMESET are mutually exclusive)
  bool bodyOrFramesetStarted; // BODY or FRAMESET has been started
};


#endif
