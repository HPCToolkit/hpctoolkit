// -*-C++-*-
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

//************************ System Include Files ******************************

#include <iostream> 
using std::endl;

#include <string>
using std::string;

//************************* User Include Files *******************************

#include "PerfMetric.hpp"
#include "PgmScopeTree.hpp"

#include <lib/support/Assertion.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// ************************************************************************* //
// PerfMetric
// ************************************************************************* //
PerfMetric::PerfMetric(const char *nm, const char *nativeNm, 
		       const char* displayNm, 
		       bool disp, bool perc, bool propComputed, bool sort)
 : name(nm), nativeName(nativeNm), eventsPerCount(0), dispInfo(NULL), 
   display(disp), percent(perc), pcomputed(propComputed), sortBy(sort) 
{
  Ctor(nm, displayNm);
}


PerfMetric::PerfMetric(const std::string& nm, const std::string& nativeNm, 
		       const std::string& displayNm, 
		       bool disp, bool perc, bool propComputed, bool sort)
 : name(nm), nativeName(nativeNm), eventsPerCount(0), dispInfo(NULL), 
   display(disp), percent(perc), pcomputed(propComputed), sortBy(sort) 
{
  Ctor(nm.c_str(), displayNm.c_str());
}


void
PerfMetric::Ctor(const char *nm, const char* displayNm)
{
  // trace = 1; 
  BriefAssertion(!IsPerfDataIndex(NameToPerfDataIndex(nm)));
  int i = PerfMetricTable.GetNumElements();
  PerfMetricTable[i] = this;
  perfInfoIndex = i;

  BriefAssertion( (displayNm != NULL) && (strlen(displayNm) > 0) ); 
  dispInfo = new DataDisplayInfo(displayNm, NULL, 9, false); 

  IFTRACE << "PerfMetric: " << ToString() << endl; 
}


PerfMetric::~PerfMetric() 
{
  IFTRACE << "~PerfMetric: " << ToString() << endl; 
  delete dispInfo; 
}


unsigned int
PerfMetric::EventsPerCount()  const 
{
  BriefAssertion(eventsPerCount > 0); 
  return eventsPerCount; 
}


void
PerfMetric::SetEventsPerCount(int events) 
{
  if (events <= 0)  events =1;
  eventsPerCount = events; 
  // BriefAssertion(eventsPerCount > 0); 
}


void
PerfMetric::Make(NodeRetriever& ret) 
{
  // nothing to do
}


VectorTmpl<PerfMetric*> PerfMetric::PerfMetricTable; 

bool 
IsPerfDataIndex(int i) 
{
  return ((i >= 0) &&
	  ((unsigned int)i < PerfMetric::PerfMetricTable.GetNumElements())); 
}

unsigned int      
NumberOfPerfDataInfos()
{
   return PerfMetric::PerfMetricTable.GetNumElements(); 
}


PerfMetric&    
IndexToPerfDataInfo(int i)
{
   BriefAssertion(IsPerfDataIndex(i)); 
   PerfMetric* pds = PerfMetric::PerfMetricTable[i];
   BriefAssertion(pds); 
   return *pds; 
}


int       
NameToPerfDataIndex(const char* name)
{
  for (unsigned int i =0; 
       i < PerfMetric::PerfMetricTable.GetNumElements(); i++) {
    PerfMetric* pds = PerfMetric::PerfMetricTable[i];
    BriefAssertion(pds); 
    if (pds->Name() == name) 
      return i;
  } 
  return UNDEF_METRIC_INDEX; 
} 


void 
ClearPerfDataSrcTable() 
{
  IFTRACE << "ClearPerfDataSrcTable" << endl; 
  for (unsigned int i =0; 
       i < PerfMetric::PerfMetricTable.GetNumElements(); i++) {
    delete PerfMetric::PerfMetricTable[i]; 
  }
  PerfMetric::PerfMetricTable.SetNumElements(0); 
  IFTRACE << "ClearPerfDataSrcTable done" << endl; 
}


// ************************************************************************* //
// DataDisplayInfo
// ************************************************************************* //

DataDisplayInfo::~DataDisplayInfo() 
{
   IFTRACE << "~DataDisplayInfo " << ToString() << endl; 
}

// ************************************************************************** //
// ToString methods 
// ************************************************************************** //
string 
DataDisplayInfo::ToString() const 
{
  return string("DisplayInfo ") + 
    "name=" + name + " " + 
    "color=" + color + " " + 
    "width=" + StrUtil::toStr(width) + " " + 
    "asInt=" + ((formatAsInt) ? "true" : "false"); 
}


string
PerfMetric::ToString() const 
{
  return string("PerfMetric: ") + 
    "name=" + name + " " + 
    "display=" + ((display) ? "true " : "false ") + 
    "perfInfoIndex=" + StrUtil::toStr(perfInfoIndex) + " " + 
    "eventsPerCount=" + StrUtil::toStr(eventsPerCount) + " " + 
    "dispInfo=" + dispInfo->ToString(); 
}

