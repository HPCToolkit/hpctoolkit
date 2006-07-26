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

#ifdef NO_STD_CHEADERS
# include <limits.h>
#else
# include <climits>
using namespace std; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include <include/general.h>

#include "Args.hpp"
#include "HTMLScopes.hpp"
#include "HTMLDriver.hpp"
#include "HTMLTable.hpp"
#include "HTMLTable.hpp"
#include <lib/support/IntVector.hpp>
#include <lib/support/Assertion.h>
#include <lib/support/Trace.hpp>

//************************ Forward Declarations ******************************

using std::endl;

//****************************************************************************

#ifdef IMAGE_SPACER
#define NOARROW "<img src='noarrow.gif' alt='no arrow' align='top' width='9' border='no'>"
#else
#ifdef NETSCAPE_SPACER
#define NOARROW "<spacer type='horizontal' size='9'>"
#else
#define NOARROW "  "
#endif
#endif

#define UPARROW "<img src='uparrow.gif' alt='up arrow' align='top' border='no' hspace='2'>"
#define DWNARROW "<img src='downarrow.gif' alt='down arrow' align='top' border='no' hspace='2'>"

/*
#define UPARROW "<img class=arrow src=\"uparrow.gif\" border=no>"
#define DWNARROW "<img class=arrow src=\"downarrow.gif\" border=no>"
#define ENDLINE "<br clear=all>"
*/

static bool HasInterestingChildren(const ScopeInfo *scope);

const DataDisplayInfo 
HTMLScopes::HeaderDisplayInfo = DataDisplayInfo("ScopesHeader", "blue", 
						12, false); 

typedef enum { OUTER, CURRENT, INNER } ScopeLevel; 

HTMLScopes::HTMLScopes(const PgmScopeTree& scpes, 
		       IntVector *perfIndicesForColumns,
			   const Args &pgmArgs) 
  : scopes(scpes), args(pgmArgs)
{
  perfIndex = perfIndicesForColumns; 
  BriefAssertion(perfIndex != NULL); 
  BriefAssertion((*perfIndex).size() > 0) ; 
  BriefAssertion((*perfIndex)[(*perfIndex).size()-1] == -1); 
  IFTRACE << "HTMLScopes " << ToString() << endl; 
}

HTMLScopes::~HTMLScopes()
{
  IFTRACE << "~HTMLScopes " << ToString() << endl; 
}

const char* 
HTMLScopes::Name()
{ 
  return ScopesDir; 
}

String 
HTMLScopes::SelfFileName(const ScopeInfo* si, int pIndex, int flattening)
{
  String result =
    String(HTMLScopes::Name()) + "/" + HTMLDriver::UniqueName(si, pIndex, flattening) + ".self"; 

  return result;
}

String 
HTMLScopes::KidsFileName(const ScopeInfo* si, int pIndex, int flattening)
{
  String result =
    String(HTMLScopes::Name()) + "/" + 
    HTMLDriver::UniqueName(si, pIndex, flattening) +".kids"; 
  return result;
}

String 
HTMLScopes::AnchorName(const ScopeInfo* si, int pIndex, int flattening)
{
  // Do not use pIndex for now. We'll need it only if we decide to 
  // generate the data for all performance metrics into the same file
  String result = HTMLDriver::UniqueName(si, NO_PERF_INDEX,
                 NO_FLATTEN_DEPTH);
  if (flattening != NO_FLATTEN_DEPTH)
     result = "l" + String((long)flattening) + "_" + result;
  return result;
}

String 
HTMLScopes::HeadFileName() const 
{
  return "scopes.header" ; 
}


String 
HTMLScopes::SelfHeadFileName()
{
  return "scopes.self.hdr" ; 
}

String 
HTMLScopes::KidsHeadFileName()
{
  return "scopes.kids.hdr" ; 
}

String 
HTMLScopes::SelfFrameName()
{
  return SCOPES_SELF; 
}

String 
HTMLScopes::KidsFrameName()
{
  return SCOPES_KIDS; 
}



String 
HTMLScopes::ToString() const
{
  String s = "HTMLScopes: "; 
  s += String("name=") + name + " " ;
  s += String("scopes=") + String((unsigned long) &scopes, true /*hex*/) 
    + " "; 
  s += String("perfIndex: "); 
  for (unsigned int i = 0; i < (*perfIndex).size(); i++) {
    s += String((long) (*perfIndex)[i]) + " "; 
  }
  return s; 
}

void 
HTMLScopes::Write(const char* htmlDir, 
		  const char* bodyBgColor, const char* headBgColor) const
{
  IFTRACE << "HTMLScopes::Write" << endl;

  int i = 0; 
  while  ((*perfIndex)[i] != -1) {
    bool ok = true; 
    if (IndexToPerfDataInfo((*perfIndex)[i]).SortBy()) 
       ok = WriteScopesForMetric(htmlDir, (*perfIndex)[i], headBgColor, 
			      bodyBgColor);
    if (!ok) { 
      i = 0; // start over 
      // WriteScopesForMetric increased DisplayInfo Width, so that 
      // now numbers should fit 
    } else {
      i++; 
    } 
  } 
}

int
HTMLScopes::WriteScopesForMetric(const char* htmlDir, int sortByPerfIndex,
				 const char* headBgColor,
				 const char* bodyBgColor) const
{

  ScopeInfo *root = scopes.Root();
  int numViewsOfRoot;

  // check whether there is flattening to do at the root level
  if (args.depthToFlatten > 0) {
    numViewsOfRoot = root->ScopeHeight();
  } else {
    // just produce the normal unflattened view of the tree
    numViewsOfRoot = 1;
  }

  if (args.OldStyleHTML)
  {
     for( int flat=0 ; flat<numViewsOfRoot ; flat++ )
        WriteScopeOldStyle(htmlDir, bodyBgColor, *scopes.Root(), 
                  sortByPerfIndex, flat); 
  }
  else
     WriteScope(htmlDir, bodyBgColor, *scopes.Root(), sortByPerfIndex, 
 	       numViewsOfRoot); 
  return true; // for now
}

void 
HTMLScopes::WriteScope(const char* htmlDir, const char* bgColor, 
		       const ScopeInfo &scope, int pIndex, 
		       int maxFlattening) const
{
  // This writes three files: 
  //   SelfFileName()      with perf info for scope and its parent 
  //   KidsFileName()      with 2 divs one for highlighter the other for perf 
  //                       data of scopes's children
  //                       performance data of scopes's children in the
  //                       second div
  //

  int depth = scope.ScopeDepth();
  int height = scope.ScopeHeight();

  //---------------------------------------------------------
  // the normal amount of flattening that can be applied to 
  // a node is one less than its scope depth. 
  // 
  // if (depth - 1 < 0), we adjust it to 0 because we MUST 
  // execute one trip of the loop for the root PGM node.
  //---------------------------------------------------------
  int levels = MAX(depth - 1, 0);
  if (args.depthToFlatten == 0) {
     // bypass all flattening if command line argument so indicates
     levels = 0;
  } 
  int outerlevel = levels;
  String baseName = "HPCView Scopes File: ";
  String selfFile = SelfFileName(&scope, pIndex, NO_FLATTEN_DEPTH); 
  // write a file containing scopes's and scope's parent's perf data
  String scopeFileName = baseName + selfFile;
  HTMLFile hf(htmlDir, selfFile, scopeFileName);
  hf.SetBgColor(bgColor);  

  //-------------------------------------------------------
  // the maximum number of times a node may be flattened
  // is its height minus one (it must have at least a non leaf
  // child to be able to flatten it. This maxflatten represents
  // how many times we can click the flatten button with the current
  // scope unchanged (== height).
  // On the other hand, levels represents how many flattenings we can
  // do before we reach the current scope (== depth)
  //-------------------------------------------------------
  int maxflatten = height - 1;
  if (scope.ScopeDepth() >= args.depthToFlatten) {
     // no scope deeper than the deepest scope to be flattened
     // will be allowed to be flattened
     maxflatten = 0;
  }
      
  hf.SetOnLoad(String("handle_scope_self_onload(") + "\"" + selfFile
       + "\",\"" + HTMLDriver::UniqueName(&scope, NO_PERF_INDEX,
				      NO_FLATTEN_DEPTH)
       + "\"," + String((long)maxflatten) + "," 
       + String((long)depth) + ");");
  hf.StartBodyOrFrameset();
  hf.WriteHighlightDiv();

  for (; levels >= 0; levels--) {
    ScopeInfo *outer = scope.Parent();
    int i = levels;
    while(i-- > 0) { outer = outer->Parent(); }

    hf << "<div class=\"" << LINES_DIV_NAME << "\" id=\"" 
       << LINES_DIV_NAME << "_" << String((long)levels) << "\">" << endl;
    hf << "<pre>" << endl;
    WriteLine(hf, OUTER, outer, pIndex, levels, false);
    WriteHR(hf);
    WriteLine(hf, CURRENT, &scope, pIndex, levels, false);
    hf << "</pre>" << endl;
    hf << "</div>" << endl; 

    if (scope.ScopeDepth() >= args.depthToFlatten && 
       (outerlevel - levels) >= args.depthToFlatten)  
    {
       // restricted flattening: bypass flattening at all
       // intermediate levels
       levels = MIN(1,levels);
    } 
  }

  String kidsFile = KidsFileName(&scope, pIndex, NO_FLATTEN_DEPTH);

  CompareByPerfInfo_MetricIndex = pIndex;

  // write a file containing layer for scopes's children's perf data 
  // and highlighter 
  HTMLFile hfk(htmlDir, kidsFile, "HPCView Scopes File"); 
  hfk.SetOnLoad(String("handle_scope_frame_onload(") + "\"" + kidsFile 
       + "\");"); 
  // FIXME: eraxxon: this is an invalid attribute
  // hfk.SetOnResize(String("resize_table(window.name);")); 
  hfk.SetBgColor(bgColor);
  hfk.StartBodyOrFrameset(); 

  hfk.WriteHighlightDiv();

  for( int flatIt=0 ; flatIt<maxFlattening ; flatIt++ )
  {
    SortedCodeInfoChildIterator kids(&scope, flatIt, CompareByPerfInfo); 
    hfk << "<div class=\"" << LINES_DIV_NAME << "\" id=\"" 
       << LINES_DIV_NAME << "_" << String((long)flatIt) << "\">" << endl;
    hfk << "<pre>" << endl;

    int linesLeft = args.maxLinesPerPerfPane;
    for (; kids.Current() && linesLeft-- > 0 ; kids++) {
       WriteLine(hfk, INNER, kids.Current(), pIndex, flatIt, true); 
    } 
    hfk << "</pre>" << endl;
    hfk << "</div>" << endl;
  }
  
  SortedCodeInfoChildIterator kids(&scope, 0, CompareByPerfInfo); 
  // check whether there is flattening to do at the current level
  int maxFlattenings;
  if (args.depthToFlatten > scope.ScopeDepth()) {
    maxFlattenings = INT_MAX;
  } else {
    // just produce the normal unflattened view of the tree
    maxFlattenings = 1;
  }
  // recurse: write a file for all non leaf children 
  for (; kids.Current(); kids++) {
     ScopeInfo *child = kids.Current();
     if (!child->IsLeaf() && HasInterestingChildren(child)) {
        int childFlattenDepth = MIN(maxFlattenings, child->ScopeHeight());
        WriteScope(htmlDir, bgColor, *child, pIndex, childFlattenDepth); 
     }
  } 
}


void 
HTMLScopes::WriteScopeOldStyle(const char* htmlDir, const char* bgColor, 
		       const ScopeInfo &scope, int pIndex, 
		       int flattening) const
{
  // This writes three files: 
  //   SelfFileName()      with perf info for scope and its parent 
  //   KidsFileName()      with 2 divs one for highlighter the other for perf 
  //                       data of scopes's children
  //                       performance data of scopes's children in the
  //                       second div
  //

  if (flattening == 0) { 
    int depth = scope.ScopeDepth();
    int height = scope.ScopeHeight();

    //---------------------------------------------------------
    // the normal amount of flattening that can be applied to 
    // a node is one less than its scope depth. 
    // 
    // if (depth - 1 < 0), we adjust it to 0 because we MUST 
    // execute one trip of the loop for the root PGM node.
    //---------------------------------------------------------
    int levels = MAX(depth - 1, 0);
    if (args.depthToFlatten == 0) {
      // bypass all flattening if command line argument so indicates
      levels = 0;
    } 
    int outerlevel = levels;
    String baseName = "HPCView Scopes File: ";
    for (; levels >= 0; levels--) {
      String selfFile = SelfFileName(&scope, pIndex, levels); 
      // write a file containing scopes's and scope's parent's perf data
      String scopeFileName = baseName + selfFile;
      HTMLFile hf(htmlDir, selfFile, scopeFileName);
      hf.SetBgColor(bgColor);  

      //-------------------------------------------------------
      // the maximum number of times a node may be flattened
      // is its height minus one (it must have at least a non leaf
      // child to be able to flatten it. This maxflatten represents
      // how many times we can click the flatten button with the current
      // scope unchanged (== height).
      // On the other hand, levels represents how many flattenings we can
      // do before we reach the current scope (== depth)
      //-------------------------------------------------------
      int maxflatten = height - 1;
      if (scope.ScopeDepth() >= args.depthToFlatten) {
         // no scope deeper than the deepest scope to be flattened
         // will be allowed to be flattened
         maxflatten = 0;
      }
      
      hf.SetOnLoad(String("handle_scope_self_onload(") + "\"" + selfFile
		   + "\",\"" + HTMLDriver::UniqueName(&scope, NO_PERF_INDEX,
						      NO_FLATTEN_DEPTH)
		   + "\"," + String((long)maxflatten) + "," 
		   + String((long)depth) + ");");
      hf.StartBodyOrFrameset();
      hf.WriteHighlightDiv();

      ScopeInfo *outer = scope.Parent();
      int i = levels;
      while(i-- > 0) { outer = outer->Parent(); }

      hf << "<div id=\"" << LINES_DIV_NAME << "\">" << endl;
      hf << "<pre>" << endl;
      WriteLine(hf, OUTER, outer, pIndex, NO_FLATTEN_DEPTH, false);
      WriteHR(hf);
      WriteLine(hf, CURRENT, &scope, pIndex, NO_FLATTEN_DEPTH, false);
      hf << "</pre>" << endl;
      hf << "</div>" << endl; 

      if (scope.ScopeDepth() >= args.depthToFlatten && 
         (outerlevel - levels) >= args.depthToFlatten)  {
        // restricted flattening: bypass flattening at all
        // intermediate levels
        levels = MIN(1,levels);
      } 
    }
  }

  String kidsFile = KidsFileName(&scope, pIndex, flattening);  

  CompareByPerfInfo_MetricIndex = pIndex;
  SortedCodeInfoChildIterator kids(&scope, flattening, CompareByPerfInfo); 
  { 
    // write a file containing layer for scopes's children's perf data 
    // and highlighter 
    HTMLFile hf(htmlDir, kidsFile, "HPCView Scopes File"); 
    hf.SetOnLoad(String("handle_scope_frame_onload(") + "\"" + kidsFile 
		 + "\");"); 
    // FIXME: eraxxon: this is an invalid attribute
    // hf.SetOnResize(String("resize_table(window.name);")); 
    hf.SetBgColor(bgColor);
    hf.StartBodyOrFrameset(); 

    hf.WriteHighlightDiv();

    hf << "<div id=\"" << LINES_DIV_NAME << "\">" << endl; 
    hf << "<pre>" << endl;

    int linesLeft = args.maxLinesPerPerfPane;
    for (; kids.Current() && linesLeft-- > 0 ; kids++) {
      WriteLine(hf, INNER, kids.Current(), pIndex, NO_FLATTEN_DEPTH, true); 
    } 
    hf << "</pre>" << endl;
    hf << "</div>" << endl;
  }
  
  if (flattening == 0) {
    // check whether there is flattening to do at the current level
    int maxFlattenings;
    if (args.depthToFlatten > scope.ScopeDepth()) {
      maxFlattenings = INT_MAX;
    } else {
      // just produce the normal unflattened view of the tree
      maxFlattenings = 1;
    }
    // recurse: write a file for all non leaf children 
    kids.Reset();
    for (; kids.Current(); kids++) {
      ScopeInfo *child = kids.Current();
      if (!child->IsLeaf() && HasInterestingChildren(child)) {
        for(int childFlattenDepth = 0; 
            childFlattenDepth < MIN(maxFlattenings, child->ScopeHeight());
            childFlattenDepth++ ) {
          WriteScopeOldStyle(htmlDir, bgColor, *child, pIndex, 
                     childFlattenDepth); 
        }
      } 
    }
  }
}


void 
HTMLScopes::WriteLine(HTMLFile &hf, unsigned int level, 
		      const ScopeInfo *si, int pIndex, int flattening,
		      bool anchor) const
{
  
  if (si != NULL) {  

    // filter out lines that don't meet threshold
    if (anchor && 
	!HTMLTable::MetricMeetsThreshold
	(perfIndex, pIndex, args.scopeThresholdPercent, si, *scopes.Root())) 
      return;


    if ((si->IsLeaf()) && !HTMLTable::UsesIt(*si)) {
      return; 
    } 

    if (anchor) 
      hf.Anchor(AnchorName(si, pIndex, flattening)); 

    int width = HTMLDriver::NameDisplayInfo.Width(); 
    String code = HTMLTable::CodeName(*si); 

    // Write beginning navigation icon (up/down/no arrow)
    if (!si->IsLeaf() && HasInterestingChildren(si)) {
      const char* lnk;
      int direction = 0;
      switch (level) {
      case OUTER:   lnk = DWNARROW; direction = -1; break; 
      case CURRENT: lnk = NOARROW;  break; 
      case INNER:   lnk = UPARROW;  direction = 1; break;
      }; 
      if (level == CURRENT) { 
	hf << lnk; 
      } else {
	hf.ScopesSetCurrent(HTMLDriver::UniqueName(si, pIndex, 
						   NO_FLATTEN_DEPTH), 
			    lnk, direction, code, 1); 
      }
    } else {
      hf << NOARROW; 
    }

    // Write procedure/loop/src ref
    if (!anchor) 
      hf.Anchor(AnchorName(si, pIndex, flattening)); 

    if (si->Type() == ScopeInfo::PROC && level == INNER) {
      hf.NavigateFrameHrefForProc((ProcScope*)si, width, flattening); 
    } else if (si->Type() == ScopeInfo::LOOP) {
      hf.NavigateFrameHrefForLoop((LoopScope*)si, width, flattening); 
    } else if (HTMLTable::UsesIt(*si)) {
      hf.NavigateFrameHref
	(HTMLDriver::UniqueName(si, NO_PERF_INDEX, 
				NO_FLATTEN_DEPTH), code, width); 
    } else { 
      code.LeftFormat(width); 
      hf << code; 
    } 
    hf << " | "; 
  } else {
    // si is NULL write empty line 
    String empty = ""; 
    empty.LeftFormat(HTMLDriver::NameDisplayInfo.Width()); 
    hf << NOARROW << empty << " | ";
  }

  // Write performance data (ignoring return value!)
  (void) HTMLTable::WriteMetricValues(hf, perfIndex, si, *scopes.Root());

  hf << endl; 
}

void 
HTMLScopes::WriteHR(HTMLFile &hf) const
{
  // width of first column + initial image space ('  ') and trailing ' | '
  int width = 2 + HTMLDriver::NameDisplayInfo.Width() + 3;

  // add width needed for all metrics
  for (int i=0; (*perfIndex)[i] != -1; i++) {
    int index = (*perfIndex)[i]; 
    const DataDisplayInfo& dspInfo = IndexToPerfDataInfo(index).DisplayInfo(); 
    int colWidth = dspInfo.Width() + 3; // add space for trailing ' | '
    width += colWidth;
  }
  width--; // subtract the trailing ' '
  
  // String hr(width, '-'); // is this faster than multiple "os << '-'"?
  for (int i=0; i < width; i++) { hf << "-"; }
  
  hf << "    " << endl; // write 2-3 spaces at the end of each line
                        // for a nicer scroll effect. Add one space for
                        // the trailing ' ' that was removed.
}

static bool HasInterestingChildren(const ScopeInfo *scope)
{
  if (scope->IsLeaf()) return false;

  switch (scope->Type()) {
  case ScopeInfo::PGM: return true;
  case ScopeInfo::GROUP: return true;
  case ScopeInfo::LM: return true;
  case ScopeInfo::FILE: return true;
  default:
    {
      if (scope->CodeInfoParent()) {
	// scope must be something inside a FileScope
	FileScope *srcFile = scope->File();
	return (srcFile->HasSourceFile());
      }
    }
  }
  return false;
}
