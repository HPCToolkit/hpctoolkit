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
# include <stdio.h>
#else
# include <cstdio> // for 'sprintf'
using namespace std; // For compatibility with non-std C headers
#endif


//************************* User Include Files *******************************

#include "HTMLTable.h" 
#include "HTMLDriver.h" 
#include "HTMLScopes.h" 
#include "HTMLFile.h"
#include "StaticFiles.h"
#include <lib/support/IntVector.h>
#include <lib/support/String.h>
#include <lib/support/Assertion.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::endl;

//****************************************************************************

#ifdef IMAGE_SPACER
#define NOARROW "<img src='scopes/noarrow.gif' alt='no arrow' align='top' width='9' border='no'>"
#else
#ifdef NETSCAPE_SPACER
#define NOARROW "<spacer type='horizontal' size='9'>"
#else
#define NOARROW "  "
#endif
#endif

int HTMLTable::percentWidth =  3; 

const char* HTMLTable::highlightColor = "red"; 
 
HTMLTable::HTMLTable(const char* nm, 
		     const ScopesInfo &scpes, 
		     const ScopeInfoFilter &entryFilter, 
		     bool lOnly, 
		     IntVector* perfIndices,
		     const Args &pgmArgs)  // -1 is used as end marker
  : name(nm), filter(entryFilter), leavesOnly(lOnly), scopes(scpes),
    args(pgmArgs)
{
  perfIndex = perfIndices; 
  BriefAssertion(perfIndex != NULL); 
  BriefAssertion((*perfIndex).size() > 0) ; 
  BriefAssertion((*perfIndex)[(*perfIndex).size()-1] == -1); 
  // trace = 0; 
  IFTRACE << "HtmlTable::" << ToString() << endl; 
}
  
HTMLTable::~HTMLTable() 
{ 
  IFTRACE << "~HTMLTable " << ToString() << endl; 
} 

String
HTMLTable::ToString() const 
{
  String s = "HTMLTable: "; 
  s += String("name=") + name + " " ;
  s += String("filter.Name()=") + filter.Name() + " " ;
  s += String("leavesOnly=") + ((leavesOnly) ? "true " : "false "); 
  s += String("scopes=") + String((unsigned long) &scopes, true /*hex*/) 
    + " "; 
  s += String("perfIndex: "); 
  for (unsigned int i = 0; i < (*perfIndex).size(); i++) {
    s += String((long) (*perfIndex)[i]) + " "; 
  }
  return s; 
}
 
String
HTMLTable::Name() const 
{
  String nm = name + "." + filter.Name(); 
  return nm; 
}

void
HTMLTable::Write(const char* dir, 
		 const char* headBgColor, 
		 const char* bodyBgColor) const 
{
  IFTRACE << "HTMLTable::Write" << endl;
  int i = 0; 
  while  ((*perfIndex)[i] != -1) {
     bool ok = true;
     if (IndexToPerfDataInfo((*perfIndex)[i]).SortBy())
        ok = WriteTableHead(dir, (*perfIndex)[i], headBgColor)  
#if 0
             && WriteTableBody(dir, (*perfIndex)[i], bodyBgColor); 
#else
;
#endif
     if (!ok) { 
       i = 0; // start over 
       // WriteRow increased DisplayInfo Width, so that now numbers should fit 
     } else {
       i++; 
     } 
  } 
}

bool 
HTMLTable::WriteTableHead(const char* dir, 
		          int sortByPerfIndex, 
			  const char* bgColor) const {
  HTMLFile hf(dir, HeadFileName(sortByPerfIndex), NULL); 
  hf.SetBgColor(bgColor); 
  hf.TableStart(name, IndexToPerfDataInfo(sortByPerfIndex).Name(), NULL); 
  hf << "<div id=\"" << LINES_DIV_NAME << "\">" << endl;
  hf << "<pre>" << endl;
    
  // first row contains links that sort table by column values
  String empty; 
  empty.LeftFormat(HTMLDriver::NameDisplayInfo.Width());
  //  hf << NOARROW << empty << " <a name=\"dummy\"></a>| "; 
  hf << NOARROW << empty << " | "; 
  for (int i = 0; (*perfIndex)[i] != -1; i++) {
    int index = (*perfIndex)[i]; 
    const DataDisplayInfo& dspInfo = IndexToPerfDataInfo(index).DisplayInfo(); 
    int noPercentAdjustment = 0;
    if (!IndexToPerfDataInfo(index).Percent()) {
       noPercentAdjustment = percentWidth + 1;
    } 
    if (index == sortByPerfIndex) { 
      String label = "sorted"; 
      label.LeftFormat(dspInfo.Width() - noPercentAdjustment); 
      hf.FontColorStart(highlightColor);
      hf << label; 
      hf.FontColorStop(highlightColor); 
    } else {
      if (IndexToPerfDataInfo(index).SortBy()) {
         hf.SetTableAndScopes(Name(), TableFileName(index), 
			   HTMLScopes::SelfFrameName(), 
			   // HTMLScopes::SelfFileName(scopes.Root(),index),
			   HTMLScopes::KidsFrameName(), 
			   // HTMLScopes::KidsFileName(scopes.Root(),index),
			   IndexToPerfDataInfo(index).Name(),
			   IndexToPerfDataInfo(index).DisplayInfo().Name(),
			   dspInfo.Width() - noPercentAdjustment); 
      } else {
         String label = ""; 
         label.LeftFormat(dspInfo.Width() - noPercentAdjustment); 
         hf << label; 
      }
    } 
    hf << " | "; 
  }
  hf << "   " << endl; // write 2-3 spaces at the end of each line
                       // for a nicer scroll effect
  
  // second row contains names
  WriteMetricHeader(hf, perfIndex); 
  
  // third row contains totals (i.e. sum of all numbers in column) 
  if (!WriteRow(hf, *scopes.Root(), false)) {
    return false; 
  }; 
  
  hf.TableStop(); 
  hf << "</div>" << endl;
  return true; 
}
  
static bool 
UseForTable(const ScopeInfo &sinfo, long unused)
{
  if (sinfo.CodeInfoParent()) {
    // sinfo is something inside a FileScope
    FileScope *srcFile = sinfo.File(); 
    BriefAssertion(srcFile); 
    if (sinfo.Type() == ScopeInfo::PROC) {
      return true;
    } else if (sinfo.IsLeaf()) {
      return (srcFile->HasSourceFile()); 
    } 
  }
  return false; 
} 

static ScopeInfoFilter UseForPerfTable(UseForTable, "PerfTableEntryFilter", 0);

bool 
HTMLTable::UsesIt(const ScopeInfo &sinfo) 
{ 
  return UseForTable(sinfo,0); 
} 

bool 
HTMLTable::WriteTableBody(const char* dir, 
		          int sortByPerfIndex, 
                          const char* bgColor) const 
{
  IFTRACE << "HTMLTable::WriteTableBody " 
	  << "dir=" << dir << " " 
	  << "sortByPerfIndex=" << sortByPerfIndex << endl;
  
  String tableFile = TableFileName(sortByPerfIndex); 
  String tableBodyFile = tableFile + ".tbl"; 
  { 
    HTMLFile hf(dir, tableFile, NULL);
    hf.SetOnLoad(String("handle_on_load();")); 
    hf.SetOnResize(String("resize_table(window.name);")); 
    hf.SetBgColor(bgColor); 
    hf.StartBodyOrFrameset(); 
    // hf << "<LAYER NAME='under' SRC=" << StaticFileName[HIGHLIGHTLINE] 
    //    << "></LAYER>" << endl; 
    // hf << "<LAYER NAME='over' SRC=" << tableBodyFile << ".html></LAYER>" << endl;
  }

  { 
    HTMLFile hf(dir, tableBodyFile, NULL);
    hf.TableStart(tableFile,HeadFileName(sortByPerfIndex)); 
    hf.StartBodyOrFrameset();
    hf << "<pre>"; 
    
    SortedCodeInfoIterator it(scopes.Root(), 
			      sortByPerfIndex, &(UseForPerfTable));
    int linesLeft = args.maxLinesPerPerfPane;
    for (; it.Current() && linesLeft-- > 0; it++) {
      if (!WriteRow(hf, *it.Current(), true, sortByPerfIndex)) {
	return false; 
      }
      IFTRACE << "ROW done" << endl; 
    } 
    IFTRACE << "HTMLTable::WriteTableBody  no more ROWS" << endl; 
    hf.TableStop(); 
  } 
  return true; 
}
  
bool 
HTMLTable::WriteRow(HTMLFile &hf, const ScopeInfo &scope, bool labelIsLink,
		    int perfIndexForThresholding) const
{
  IFTRACE << "HTMLTable::WriteRow " << scope.ToString() << endl; 

  // only write row if not thresholding, or if threshold met
  if (perfIndexForThresholding == NO_PERF_INDEX || 
      MetricMeetsThreshold(perfIndex, perfIndexForThresholding,
			    100.0,
//			    args.tableThresholdPercent,
			    &scope, *scopes.Root())) {

    String scopeId = HTMLDriver::UniqueName(&scope, NO_PERF_INDEX, NO_FLATTEN_DEPTH); 
    hf.Anchor(scopeId); 
  
    // write row's label 
    hf.FontColorStart(HTMLDriver::NameDisplayInfo.Color()); 
    String nm = CodeName(scope); 
    hf << NOARROW;
    if (labelIsLink) {
      hf.NavigateFrameHref(scopeId, nm, HTMLDriver::NameDisplayInfo.Width()); 
    }  else {
      nm.LeftFormat(HTMLDriver::NameDisplayInfo.Width()); 
      hf << nm; 
    } 
    hf.FontColorStop(HTMLDriver::NameDisplayInfo.Color()); 
    hf << " | "; 
    if (!WriteMetricValues(hf, perfIndex, &scope, *scopes.Root())) {
      return false; 
    } 
    hf << endl; 
  }
  return true; 
}
  
String 
HTMLTable::CodeName(const ScopeInfo& scope)
{
  String nm;
  ScopeInfo::ScopeType t = scope.Type();
  if (t == ScopeInfo::PGM) {
    nm = "Program"; 
  } else if (t == ScopeInfo::LM || t == ScopeInfo::GROUP) {
    nm = String(ScopeInfo::ScopeTypeToName(t)) + ":" + scope.Name();
  } else {
    const CodeInfo* code = dynamic_cast<const CodeInfo*>(&scope);
    BriefAssertion(code);
    nm = code->CodeName(); 
  }
  return nm; 
}

void
HTMLTable::WriteMetricHeader(HTMLFile& hf, const IntVector *perfIndex)
{ 
  hf.FontColorStart(HTMLDriver::NameDisplayInfo.Color()); 
  String label = HTMLDriver::NameDisplayInfo.Name();
  label.LeftFormat(HTMLDriver::NameDisplayInfo.Width());
  hf << NOARROW << label;
  hf.FontColorStop(HTMLDriver::NameDisplayInfo.Color()); 
  hf << " | " ; 
  
  String percentStr; 
  for (int i =0; (*perfIndex)[i] != -1; i++) {
    int index = (*perfIndex)[i]; 
    const DataDisplayInfo& dspInfo = IndexToPerfDataInfo(index).DisplayInfo(); 
    int valWidth =  dspInfo.Width() - percentWidth - 1; 
    int myPercentWidth = percentWidth;
    label = dspInfo.Name(); 
    label.LeftFormat(valWidth); 
    if (IndexToPerfDataInfo(index).Percent()) { 
      percentStr = "%"; 
    } else {
      percentStr = ""; 
      myPercentWidth = 0;
    }
    if (myPercentWidth > 0) percentStr.RightFormat(myPercentWidth + 1); 
    const char* color =  dspInfo.Color();
    hf.FontColorStart(color);
    hf << label << percentStr; 
    hf.FontColorStop(color); 
    hf << " | "; 
  }
  hf << "   " << endl; // write 2-3 spaces at the end of each line
                       // for a nicer scroll effect
}

bool 
HTMLTable::WriteMetricValues(HTMLFile& hf, const IntVector *perfIndex, 
			     const ScopeInfo *scope, 
			     const PgmScope &root)
{ 
  for (int i =0; (*perfIndex)[i] != -1; i++) {
    int index = (*perfIndex)[i]; 
    BriefAssertion(root.HasPerfData(index)); 
    DataDisplayInfo& dspInfo = IndexToPerfDataInfo(index).DisplayInfo(); 
    int valWidth =  dspInfo.Width() - percentWidth - 1; 
    int myPercentWidth = percentWidth;
    hf.FontColorStart(dspInfo.Color()); 
    String valStr, percentStr; 
    if (scope != NULL) {
    if (scope->HasPerfData(index)) { 
      double val = scope->PerfData(index); 
      if (dspInfo.FormatAsInt()) {
	valStr = String((long) (val + 0.5));  // round x.5 up to (x+1)
      } else {
	char valChars[100]; 
	sprintf(valChars, "%.2e", val); 
	valStr = String(valChars);
      } 
      if (valStr.Length() > (unsigned int)valWidth) {
        dspInfo.SetWidth(valStr.Length() + percentWidth + 1);
	hf.FontColorStop(dspInfo.Color()); // eraxxon: must stop font
	return false; 
      }  
      int valPercent = (int) ((val / root.PerfData(index)) * 100 + 0.5);
      if (IndexToPerfDataInfo(index).Percent()) { 
        percentStr = String((long) valPercent); 
      } else {
        myPercentWidth = 0;
      }
    } else {
        if (!IndexToPerfDataInfo(index).Percent()) myPercentWidth = 0;
    }
    } else {
        if (!IndexToPerfDataInfo(index).Percent()) myPercentWidth = 0;
    }
    valStr.RightFormat(valWidth); 
    if (myPercentWidth > 0) percentStr.RightFormat(myPercentWidth + 1); 

    hf << valStr << percentStr << " | "; 
    hf.FontColorStop(dspInfo.Color()); 
  }
  return true; 
}


bool 
HTMLTable::MetricMeetsThreshold(const IntVector *perfIndex, 
				const int whichIndex, 
				const float threshold,
				const ScopeInfo *scope, 
				const PgmScope &root)
{ 
  int index = whichIndex;
  BriefAssertion(root.HasPerfData(index)); 

  if (threshold < 0.0 ) return true;
  else if (scope->HasPerfData(index)) { 
    double val = scope->PerfData(index); 
    //    int valPercent = (int) ((val / root.PerfData(index)) * 100 + 0.5); 
    float valPercent = (int) ((val / root.PerfData(index)) * 100.0); 
    if (valPercent >= threshold) return true;
  }
  return false; 
}

String
HTMLTable::TableFileName(int pIndex) const 
{
  return name + "." + filter.Name() + "." 
              + IndexToPerfDataInfo(pIndex).Name() ; 
}

String
HTMLTable::HeadFileName(int pIndex) const {
  return name + "." + filter.Name() + "." 
              + IndexToPerfDataInfo(pIndex).Name() + ".head" ; 
} 
