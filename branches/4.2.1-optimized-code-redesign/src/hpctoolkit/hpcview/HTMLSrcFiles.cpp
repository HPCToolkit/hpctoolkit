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

#include <fstream> 

#ifdef NO_STD_CHEADERS
# include <stdio.h>
#else
# include <cstdio> // for 'fopen'
using namespace std; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include "HTMLSrcFiles.hpp"
#include "HTMLDriver.hpp"
#include "HTMLFile.hpp"
#include "ScopeInfo.hpp"
#include <lib/support/String.hpp>
#include <lib/support/Assertion.h>
#include <lib/support/Trace.hpp>

//************************ Forward Declarations ******************************

using std::cout;
using std::cerr;
using std::endl;

#define CONT_LINE_INDENT 5

typedef VectorTmpl<CodeInfoLine*> CodeInfoList; 

//****************************************************************************

// eraxxon: FIXME: It would be nice to replace this with
// 'HTMLEscapeStr' when there is more time.
static const char *HTMLEscapeChar(const char &buf)
{
  static char text[2];
  if (buf == '<')     { return "&lt;"; }
  else if (buf == '>') { return "&gt;"; }
  else if (buf == '&') { return "&amp;"; }
  else if (buf == '"') { return "&quot;"; }

  // Default case
  text[0] = buf;
  text[1] = '\0';
  return text;
}

static void ReformatSrc(HTMLFile &hf, const char* lineBuf, 
			unsigned int curLine, 
			CodeInfoList* list); 
static String BuildContLineHead();

HTMLSrcFiles::HTMLSrcFiles(const PgmScopeTree& scpes, const Args& pgmArgs, 
			   const ScopeInfoFilter& entryFilter, 
			   bool lOnly) 
  : filter(entryFilter), leavesOnly(lOnly), scopes(scpes), programArgs(pgmArgs)
{

}

String 
HTMLSrcFiles::LabelFileName(const char* name)
{
  return  String(name) + ".srclabel"; 
}

void 
HTMLSrcFiles::WriteSourceLabel(const char* dir, 
			       const FileScope *file, 
			       const char* fgColor, 
			       const char* bgColor) const             
{
  String pgmName = "";  
  String name = "";  
  if (file) {
    pgmName = file->Name(); 
    name = HTMLDriver::UniqueName(file, NO_PERF_INDEX, NO_FLATTEN_DEPTH); 
  }
  
  HTMLFile hf(dir, LabelFileName(name), NULL); 
  hf.SetFgColor(fgColor); 
  hf.SetBgColor(bgColor);
  hf.StartBodyOrFrameset(); 
  if (file && (file->Name().Length() > 0)) { 
    hf << "<pre> SOURCE FILE: " << pgmName << "</pre>" << endl; 
  } else {
    hf << "<pre> FILE SYNOPSIS: " << pgmName << "</pre>" << endl; 
  }
}
 
void
HTMLSrcFiles::WriteLabels(const char* dir, 
			  const char* fgColor, const char* bgColor) const 
{
  WriteSourceLabel(dir, NULL, fgColor, bgColor); 

  // for each FILE write a label file 
  ScopeInfoIterator it(scopes.Root(), &ScopeTypeFilter[ScopeInfo::FILE]); 
  for (; it.Current(); it++) {
     const FileScope *fi = dynamic_cast<FileScope*>(it.Current()); 
     BriefAssertion(fi != NULL); 
     WriteSourceLabel(dir, fi, fgColor, bgColor); 
  } 
}

void
HTMLSrcFiles::WriteSrcFiles(const char* dir) const 
{
   // for each FILE write a label file 
  ScopeInfoIterator it(scopes.Root(), &ScopeTypeFilter[ScopeInfo::FILE]); 
  for (; it.Current(); it++) {
     WriteSrcFile(dir, *dynamic_cast<FileScope*>(it.Current())); 
  } 
} 

void
HTMLSrcFiles::WriteSrcFile(const char* dir, FileScope &file) const 
{
  String uName = HTMLDriver::UniqueName(&file, NO_PERF_INDEX, 
					NO_FLATTEN_DEPTH); 
  HTMLFile hf(dir, uName, NULL); 
  hf.SetBgColor("white"); 
  String onLoad = String("handle_src_onload(\"") + LabelFileName(uName) 
                  + "\")"; 
  hf.SetOnLoad(onLoad); 

  hf.StartBodyOrFrameset(); 
  hf.WriteHighlightDiv();

  hf << "<div id=\"" << LINES_DIV_NAME << "\">" << endl; 
  hf << "<pre>" << endl; 
  if (file.HasSourceFile()) {
    WriteSrc(hf, file); 
  } else {
    GenerateSrc(hf, file); 
  } 
  hf << "</pre>" << endl; 
  hf << "</div>" << endl; 
}

#define MAXLINESIZE 2048
static const int LINE_NUM_WIDTH = 6; 
static const int CODE_WIDTH = 120;
static const String CODE_SPACE((char)' ', (unsigned int)4); // 4 blanks 


static bool 
UseForSrc(const ScopeInfo &sinfo, long unused)
{
  if (sinfo.IsLeaf() || sinfo.Type() == ScopeInfo::LOOP
              || sinfo.Type() == ScopeInfo::PROC) return true;
  return false; 
} 

static ScopeInfoFilter UseForSrcCode(UseForSrc, "SrcCodeEntryFilter", 0);


void
HTMLSrcFiles::WriteSrc(HTMLFile &hf, FileScope &file) const
{
  FILE *srcFile = fopen(file.Name(), "r");
  BriefAssertion(srcFile); 
  IFTRACE << "WRITESRC: file name=" << file.Name() << endl;

  // go through leaves in LineOrder and generate source lines 
  LineSortedIteratorForLargeScopes it(&file, 
				  &(UseForSrcCode), 0); // leaves,loops,procs
  CodeInfoLine *cil = it.Current(); 
  unsigned int curLine = 1; 
  char lineBuf[MAXLINESIZE];
  while (fgets(lineBuf, MAXLINESIZE, srcFile) != NULL) {
    if ((cil != NULL) && (curLine == cil->GetLine())) {
      // format line with reference to ci 
      CodeInfoList linesRefs; 
      for (cil = it.Current(); 
	   (cil != NULL) && cil->GetLine() == curLine; 
	   it++, cil = it.Current()) {

         linesRefs[linesRefs.GetNumElements()] = cil; 
  
      } 
      BriefAssertion((cil == NULL) || cil->GetLine() > curLine); 
      if (linesRefs.GetNumElements() == 0)
         ReformatSrc(hf, lineBuf, curLine, NULL);
      else
      {
         linesRefs[linesRefs.GetNumElements()] = NULL; 
         IFTRACE << "WRITESRC: lineNo=" << curLine 
                 << ", numElem=" << linesRefs.GetNumElements() << endl;
         cerr.flush();

         ReformatSrc(hf, lineBuf, curLine, &linesRefs); 
      }
    } else { 
      // no CodeInfo is referring to this line 
      ReformatSrc(hf, lineBuf, curLine, NULL);
    } 
    curLine++; 
  } 
  fclose(srcFile);
}

void
HTMLSrcFiles::GenerateSrc(HTMLFile &hf, FileScope &file) const
{
  BriefAssertion (!file.HasSourceFile()); 

  if (programArgs.warningLevel > 0) {
    cerr << "File \"" << file.Name() <<
      "\" not found. Generating synopsis." << endl;
  }

  // go through leaves in LineOrder and generate source lines 
  unsigned int lastLine = 0; 
  LineSortedChildIterator it(&file, &ScopeTypeFilter[ScopeInfo::PROC]);
  for (; it.Current(); it++) {
    GenSrc(hf, *it.CurCode(), 0, lastLine); 
  }
}

void 
HTMLSrcFiles::GenSrc(HTMLFile &hf, CodeInfo &ci, unsigned int level, 
		     unsigned int &lastLine) const
{
  BriefAssertion(ci.Type() == ScopeInfo::PROC); 
  ReformatSrc(hf, "\n", UNDEF_LINE, NULL); 
  
  String line = String((char)' ', (unsigned int)(level * 3)); 
  line += ci.Name() + "(...) "; 
  if (ci.BegLine() == ci.EndLine()) {
    line += "{ ... }\n"; 
  } else {
    line += "{\n"; 
  }

  CodeInfoLine cil(&ci, IS_BEG_LINE); 
  CodeInfoList lst; 
  lst[0] = &cil; 
  lst[1] = NULL; 
  ReformatSrc(hf, line, ci.BegLine(), &lst); 

  if (ci.BegLine() != ci.EndLine()) {
  
    line = String((char)' ', (unsigned int)(level * 3)) + "..." + "\n"; 
    ReformatSrc(hf, line, UNDEF_LINE, NULL); 
  
    line = String((char)' ', (unsigned int)(level * 3)) + "} /* end " 
      + ci.Name() + " */\n"; 
    ReformatSrc(hf, line, ci.EndLine(), NULL); 
  }
  line = String((char)' ', (unsigned int)(level * 3)) + "\n"; 
  ReformatSrc(hf, line, UNDEF_LINE, NULL); 
}

static RefScope*  
NextRef(CodeInfo *ci, int *nextBegPos, int *nextEndPos, int lineLen)
{
  RefScope *curRef = dynamic_cast<RefScope*>(ci); 
  if (ci != NULL)
     IFTRACE << "NEXTREF: ciTYPE=" << ci->Type() << ", curRefTYPE=" << curRef->Type() << endl;
  cerr.flush();

  BriefAssertion(curRef == ci); 
  if (curRef == NULL) {
    *nextBegPos = lineLen;  // past the end of the line 
    *nextEndPos = lineLen; 
  }  else {
    *nextBegPos = curRef->BegPos() - 1; 
    *nextEndPos = curRef->EndPos() - 1; 
  } 
  return curRef; 
}

static void
WriteLineNumber(HTMLFile &hf, suint curLine, CodeInfoList *list)
{
  String lineNum; 
  if (curLine != UNDEF_LINE) {
    lineNum = String((long) curLine); 
  } else if (list != NULL) { // first line of a synopsis
    lineNum = String("?");
  }
   
  if (list == NULL) { 
    lineNum.RightFormat(LINE_NUM_WIDTH); 
    hf << lineNum; 
  } else {
    int nLen = lineNum.Length(); 
    int preLen = LINE_NUM_WIDTH - nLen; 
    if (preLen <=0) preLen = 1; 
    String pre((char)' ', (unsigned int)preLen); 
    hf << pre; 

    String uName;
    int maxDepth = -1;
    CodeInfo* winner = NULL;
    if (list != NULL) {
    // Place the anchors here, not at the beginning of line, because of a bug
    // in Netscape that discards multiple consecutive anchors at the beginning
    // of line
      for (unsigned int i=0 ; i<list->GetNumElements() ; i++)
      {
         if (list->Element(i) != NULL)
         {
            String anchor = "";
            CodeInfo* aci = list->Element(i)->GetCodeInfo();
            if (aci->Type() == ScopeInfo::LOOP 
		|| aci->Type() == ScopeInfo::PROC) 
            { 
               if (list->Element(i)->IsEndLine()) {
                  if (aci->GetLast()->EndLine() == aci->EndLine())
                    anchor = HTMLDriver::UniqueName( aci->GetLast(),
                             NO_PERF_INDEX, NO_FLATTEN_DEPTH) + "X" +
                             HTMLDriver::UniqueNameForSelf(aci);
                  else
                    anchor = HTMLDriver::UniqueName( aci,
                             NO_PERF_INDEX, NO_FLATTEN_DEPTH);
                  anchor += "e";
               } else {
                  if (aci->GetFirst()->BegLine() == aci->BegLine())
                    anchor = HTMLDriver::UniqueName( aci->GetFirst(),
                             NO_PERF_INDEX, NO_FLATTEN_DEPTH) + "X" +
                             HTMLDriver::UniqueNameForSelf(aci);
                  else
                    anchor = HTMLDriver::UniqueName( aci,
                             NO_PERF_INDEX, NO_FLATTEN_DEPTH);
                  anchor += "s";
               }
            }
            if (aci->Type() == ScopeInfo::LINE) 
            { 
               anchor = HTMLDriver::UniqueName( aci,
                             NO_PERF_INDEX, NO_FLATTEN_DEPTH);
            }
            hf.Anchor(anchor);
            if (aci->ScopeDepth() > maxDepth)  
                 // search for the inner most scope
            {
               maxDepth = aci->ScopeDepth();
               uName = anchor;
               winner = aci;
            }
         }
      }
    }
    BriefAssertion( winner != NULL );

    String pName = HTMLDriver::UniqueName(winner->Parent(), NO_PERF_INDEX, 
					  NO_FLATTEN_DEPTH); 
    hf.NavigateSourceHref(uName, pName, winner->Parent()->ScopeDepth(),
			  lineNum, lineNum.Length());
  } 
}

static void 
ReformatSrc(HTMLFile &hf, const char* lineBuf, unsigned int curLine, 
	    CodeInfoList *list)
{
  // list is either empty or has at least one element 
  // in the second case: 
  //         all list elements but the last are non null 
  //         all non null element's getLine == curLine
  //         list contains one or more non RefScope*, 
  //              which are at the beginning of the list
  //         RefScope pointers are sorted in ascending position order, i.e. 
  //              list[i]->EndPos() < list[i+1]->BegPos()
  //
  unsigned int i;

  BriefAssertion((list == NULL) || (list->GetNumElements() > 0)); 
  if (list != NULL) { 
    RefScope *lastRef = NULL; 
    for ( i = 0; i < list->GetNumElements(); i++) {
      if (list->Element(i) == NULL) {
	BriefAssertion(i == list->GetNumElements() - 1); 
      } else { 
	BriefAssertion(list->Element(i)->GetLine() == curLine);
	// Now we can have ProcScopes, LoopScopes, StmtRangeScopes
	if (list->Element(i)->Type() == ScopeInfo::REF) {
	  if (lastRef != NULL) {
	    BriefAssertion(lastRef->EndPos() <
			   dynamic_cast<RefScope*>(list->Element(i)->GetCodeInfo())->BegPos());
	  } 
	  lastRef = dynamic_cast<RefScope*>(list->Element(i)->GetCodeInfo());
	}
      }
    }
  }
  

  WriteLineNumber(hf, curLine, list); 
  hf << CODE_SPACE; 
  
  { 
    // we'll print the line character by character inserting layer ids as 
    // needed
    int lineLen = strlen(lineBuf);      // lineBuf length 
    if (lineBuf[lineLen-1] == '\n') {
      lineLen--; 
    } 
    
    // curRef points to the RefScope that needs to be printed next 
    // curRef == list->Element(curListEntry) or NULL 
    // lineBuf[nextBegPos ... nextStopPos-1] corresponds to  curRef
    // curRef == NULL => nextBegPos is past the end of lineBuf
    RefScope *curRef = NULL; 
    int nextBegPos = lineLen;                  
    int nextEndPos = lineLen;                    
    int curListEntry  = 0; 
    if ((list != NULL) && (list->Element(0)->Type() != ScopeInfo::REF)) {
      curListEntry = list->GetNumElements()-1; 
    }
    if (list != NULL) { 
       if (list->Element(curListEntry))
          curRef = NextRef(list->Element(curListEntry)->GetCodeInfo(),
		       &nextBegPos, &nextEndPos, lineLen); 
       else
          curRef = NextRef(NULL,
		       &nextBegPos, &nextEndPos, lineLen); 
    }
    
    int cPos = 0;    // current postition in lineBuf
    int linePos = 0; // current position in output line 
    while (cPos < lineLen) { 
      // first: look for next blank and if character sequence doesn't fit on 
      //        line begin a new line 
      // then:  write all characters up to next blank or up to next reference 
      //        (which ever comes first) 
      // then:  if the next thing is a reference write it out together with its 
      //        html ids. 
      
      int nextStop = cPos; 
      while (nextStop < lineLen) { 
	if (lineBuf[nextStop] == ' ') 
	  break; 
        nextStop++; 
      } 
      if (nextStop == cPos) {
        nextStop++; 
      } 
      int nChar = nextStop - cPos; 
      if ((linePos > CONT_LINE_INDENT) && ((linePos + nChar) > CODE_WIDTH)) { 
	// insert newline 
	hf << endl << BuildContLineHead();
	linePos = 0; 
      } 
      while ((cPos < nextStop) && (cPos < nextBegPos) && (cPos < lineLen)) {
	hf << HTMLEscapeChar(lineBuf[cPos]); 
	cPos++; 
	linePos++;
      } 
      if ((curRef != NULL) && (cPos == nextBegPos)) {
	if (linePos + (nextEndPos - nextBegPos) >= CODE_WIDTH) {
	   hf << endl << BuildContLineHead();
	   linePos = 0; 
	} 
	String uName = HTMLDriver::UniqueName(curRef, NO_PERF_INDEX, 
					      NO_FLATTEN_DEPTH); 
	hf.NavigateFrameHref(uName, "#", 1); 
	linePos++; 
//	hf.LayerIdBeg(uName);
	hf.Anchor(uName); 
	while (cPos <  nextEndPos) { 
	  hf << HTMLEscapeChar(lineBuf[cPos]); 
	  cPos++; 
	  linePos++; 
	}  
//	hf.LayerIdStop(); 
	
	curListEntry++; 
    if (list->Element(curListEntry))
       curRef = NextRef(list->Element(curListEntry)->GetCodeInfo(), 
			 &nextBegPos, &nextEndPos, lineLen); 
    else
       curRef = NextRef(NULL,
			 &nextBegPos, &nextEndPos, lineLen); 

      }
    }
  }
  hf << endl; 
} 

static String
BuildContLineHead()
{
  String head((char)' ', (unsigned int)(LINE_NUM_WIDTH + CONT_LINE_INDENT));
  head += CODE_SPACE; 
  return head;
}
