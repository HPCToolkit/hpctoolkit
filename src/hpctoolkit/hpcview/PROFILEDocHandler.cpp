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

//***************************************************************************
//
// File:
//    PROFILEDocHandler.C
//
// Purpose:
//    XML adaptor for the profile data file (PROFILE)
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <iostream> 

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::atof; // For compatibility with non-std C headers
using std::atoi;
#endif

//************************* User Include Files *******************************

#include "PROFILEDocHandler.h"
#include "XMLAdapter.h"
#include "ScopesInfo.h"
#include <lib/support/Assertion.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::endl;

//****************************************************************************

// ----------------------------------------------------------------------
// -- PROFILEDocHandler(NodeRetriever* const retriever) --
//   Constructor.
//
//   -- arguments --
//     retriever:        a const pointer to a NodeRetriever object
// ----------------------------------------------------------------------

PROFILEDocHandler::PROFILEDocHandler(NodeRetriever* const retriever, 
				     Driver* _driver) 
  : SAXHandlerBase(true) 
{
  nodeRetriever = retriever;
  driver = _driver;
  metricPerfDataTblIndx = -1;
  profVersion = -1;
  funcScope = NULL;
  line = -1;
}

// ----------------------------------------------------------------------
// -- ~PROFILEDocHandler() --
//   Destructor.
//
//   -- arguments --
//     None
// ----------------------------------------------------------------------

PROFILEDocHandler::~PROFILEDocHandler() {
}

void
PROFILEDocHandler::Initialize(int metricIndx, const char* profileFileName) 
{
  BriefAssertion(IsPerfDataIndex(metricIndx)); 
  IFTRACE << "PROFILEDocHandler::InitMetricHandler: " << metricIndx << endl; 
  metricPerfDataTblIndx = metricIndx;
  profileFile = profileFileName; 
  metricName  = IndexToPerfDataInfo(metricIndx).NativeName();
}

// ----------------------------------------------------------------------
// -- startElement(const XMLCh* const name,
//                 AttributeList& attributes) --
//   Process the element start tag and extract out attributes.
//
//   -- arguments --
//     name:         element name
//     attributes:   attributes
// ----------------------------------------------------------------------

#define s_c_XMLCh_c static const XMLCh* const

// Note: indentation shows basic element heirarchy
s_c_XMLCh_c elemProfile            = XMLStr("PROFILE");
s_c_XMLCh_c   elemProfileHdr       = XMLStr("PROFILEHDR");
s_c_XMLCh_c   elemProfileParams    = XMLStr("PROFILEPARAMS");
s_c_XMLCh_c     elemTarget         = XMLStr("TARGET");
s_c_XMLCh_c     elemMetrics        = XMLStr("METRICS");
s_c_XMLCh_c       elemMetricDef    = XMLStr("METRIC");
s_c_XMLCh_c   elemProfileScopeTree = XMLStr("PROFILESCOPETREE");
s_c_XMLCh_c     elemPgm            = XMLStr("PGM");
s_c_XMLCh_c     elemGroup          = XMLStr("G");
s_c_XMLCh_c     elemLM             = XMLStr("LM");
s_c_XMLCh_c     elemFile           = XMLStr("F");
s_c_XMLCh_c     elemProc           = XMLStr("P");
s_c_XMLCh_c     elemLoop           = XMLStr("L");
s_c_XMLCh_c     elemStmt           = XMLStr("S");
s_c_XMLCh_c     elemMetric         = XMLStr("M");

// attribute names for PROFILE elements
s_c_XMLCh_c attrVer         = XMLStr("version");
s_c_XMLCh_c attrShortName   = XMLStr("shortName");
s_c_XMLCh_c attrNativeName  = XMLStr("nativeName");
s_c_XMLCh_c attrPeriod      = XMLStr("period");
s_c_XMLCh_c attrUnits       = XMLStr("units");
s_c_XMLCh_c attrDisplayName = XMLStr("displayName");
s_c_XMLCh_c attrDisplay     = XMLStr("display");

// attribute names for PGM elements
s_c_XMLCh_c attrName   = XMLStr("n");
s_c_XMLCh_c attrLnName = XMLStr("ln");
s_c_XMLCh_c attrBegin  = XMLStr("b");
s_c_XMLCh_c attrEnd    = XMLStr("e");
s_c_XMLCh_c attrId     = XMLStr("id");
s_c_XMLCh_c attrVal    = XMLStr("v");

void PROFILEDocHandler::startElement(const XMLCh* const name,
				     AttributeList& attributes) 
{
  // -----------------------------------------------------------------
  // PROFILE
  // -----------------------------------------------------------------  
  if (isXMLStrSame(name, elemProfile)) {
    String verStr = getAttr(attributes, attrVer);
    double ver = atof(verStr);
    
    // We can handle versions 3.0+
    profVersion = ver;
    if (profVersion < 3.0) {
      String error = "This file format version is outdated; please regenerate the file."; 
      throw PROFILEException(error); 
    }
    
    IFTRACE << "PROFILE: version=" << ver << endl;
  }

  // -----------------------------------------------------------------
  // PROFILEHDR
  // -----------------------------------------------------------------
  if (isXMLStrSame(name, elemProfileHdr)) {
    IFTRACE << "PROFILEHDR" << endl;
  }

  // -----------------------------------------------------------------
  // PROFILEPARAMS
  // -----------------------------------------------------------------
  if (isXMLStrSame(name, elemProfileParams)) {
    IFTRACE << "PROFILEPARAMS" << endl;
  }

  // TARGET: executable name
  if (isXMLStrSame(name, elemTarget)) {
    String target = getAttr(attributes, 0 /*name*/); 
    IFTRACE << "TARGET: name=" << target << endl;
  }
  
  // METRICS
  if (isXMLStrSame(name, elemMetrics)) {
    IFTRACE << "METRICS" << endl;
  }

  // METRIC: list of profiling metrics
  if (isXMLStrSame(name, elemMetricDef)) {
    String shortName   = getAttr(attributes, attrShortName);
    String nativeName  = getAttr(attributes, attrNativeName);
    String period      = getAttr(attributes, attrPeriod);
    String units       = getAttr(attributes, attrUnits);       // optional
    String displayName = getAttr(attributes, attrDisplayName); // optional
    String display     = getAttr(attributes, attrDisplay);     // optional

    // if this is the metric we are particularly interested in, save it
    if (strcmp(metricName, shortName) == 0) {
      metricNameShort = shortName;
    }
    
    int prd = atoi(period); 
    IndexToPerfDataInfo(metricPerfDataTblIndx).SetEventsPerCount(prd); 
    
    IFTRACE << "METRIC: shortName=" << shortName
	    << " nativeName=" << nativeName
	    << " period=" << period
	    << " units=" << units
	    << " displayName=" << displayName
	    << " display=" << display << endl;
  }
  
  // -----------------------------------------------------------------
  // PROFILESCOPETREE
  // -----------------------------------------------------------------

  // PROFILESCOPETREE
  if (isXMLStrSame(name, elemProfileScopeTree)) {
    IFTRACE << "PROFILESCOPETREE" << endl;
  }

  // PGM [See PGMDocHandler.C]
  if (isXMLStrSame(name, elemPgm)) {
    String pgmName = getAttr(attributes, attrName);
    pgmName = driver->ReplacePath(pgmName);
    IFTRACE << "PGM: name= " << pgmName << endl;

    PgmScope* root = nodeRetriever->GetRoot(); 
    if (root->Name().Length() == 0) { root->SetName(pgmName); }
  }

  // G(roup)
  if (isXMLStrSame(name, elemGroup)) {
    // For now, GROUPs are meaningless in a PROFILE.
    IFTRACE << "G(roup)" << endl;
  }    

  // LM (load module)
  if (isXMLStrSame(name, elemLM)) {
    String lm = getAttr(attributes, attrName); // must exist
    lm = driver->ReplacePath(lm);

    BriefAssertion(lmName.Length() == 0);
    lmName = lm;
    nodeRetriever->MoveToLoadMod(lmName);
    
    IFTRACE << "LM (load module): name= " << lmName << endl;
  }

  // F(ile)
  if (isXMLStrSame(name, elemFile)) {
    String srcFile = getAttr(attributes, attrName); // must exist
    srcFile = driver->ReplacePath(srcFile);

    BriefAssertion(srcFileName.Length() == 0);
    srcFileName = srcFile;
    nodeRetriever->MoveToFile(srcFileName); 

    IFTRACE << "F(ile): name= " << srcFile << endl;       
  }
  
  // P(roc)
  if (isXMLStrSame(name, elemProc)) {
    String name = getAttr(attributes, attrName); // must exist
    if (driver->MustDeleteUnderscore()) {
      if ((name.Length() > 0) && (name[name.Length()-1] == '_')) {
	name[name.Length()-1] = '\0';
      }
    }
    IFTRACE << "P(roc): name="  << name << endl;

    // For now, 'attrLnName,' 'attrBegin,' and 'attrEnd' are
    // meaningless in a PROFILE.  See PGMDocHandler.C
    
    BriefAssertion(funcName.Length() == 0);
    funcName = name;
    funcScope = nodeRetriever->MoveToProc(funcName);
    
    IFTRACE << "P(roc): name= " << name << endl;       
  }

  // L(oop)
  if (isXMLStrSame(name, elemLoop)) {
    // For now, LOOPs are meaningless in a PROFILE.
    IFTRACE << "L(oop)" << endl;
  }    
  
  // S(tmt)
  if (isXMLStrSame(name, elemStmt)) {
    int numAttr = attributes.getLength();
    BriefAssertion(numAttr >= 1 && numAttr <= 3);
    
    // For now, we are not interested in 'attrEnd' or 'attrId'
    int lnB = UNDEF_LINE, lnE = UNDEF_LINE, lnId = UNDEF_LINE;
    String lineB  = getAttr(attributes, attrBegin);
    String lineE  = getAttr(attributes, attrEnd);
    String lineId = getAttr(attributes, attrId);
    if (lineB.Length() > 0) { lnB = atoi(lineB); }
    if (lineE.Length() > 0) { lnE = atoi(lineE); }
    if (lineId.Length() > 0) { lnId = atoi(lineE); }

    // IF lineE is undefined, set it to lineB
    if (lnE == UNDEF_LINE) { lnE = lnB; }
    
    // Check that lnB and lnE are valid line numbers: FIXME

    IFTRACE << "S(tmt): b= " << lnB << " e= " << lnE << " i= " << lnId << endl;

    BriefAssertion(line == -1);
    line = lnB; 
  }

  // M(etric)
  if (isXMLStrSame(name, elemMetric)) {
    String name  = getAttr(attributes, attrName);
    String value = getAttr(attributes, attrVal);

    // Consider only the one metric we are interested in
    if (strcmp(name, metricNameShort) == 0) {
      // FIXME: Metrics can be attached to any element of the tree,
      // not just stmts.  (use StmtRangeScope not LineScope)
      if (line != -1) { // FIXME change to IsValid
	double val = atof(value);
	CodeInfo* lineNode = funcScope->GetLineScope(line); 
	lineNode->SetPerfData(metricPerfDataTblIndx, val); 
      }
    }
    
    IFTRACE << " M(etric): name= " << name
	    << " value= " << value << endl;
  }
}

void PROFILEDocHandler::endElement(const XMLCh* const name)
{
  // -----------------------------------------------------------------
  // PROFILEHDR
  // -----------------------------------------------------------------

  // -----------------------------------------------------------------
  // PROFILEPARAMS
  // -----------------------------------------------------------------

  // METRICS
  if (isXMLStrSame(name, elemMetrics)) {
    // If 'metricNameShort' has not been found then 'metricName' is invalid
    if ( metricNameShort.Length() == 0 ){
      String error = "Metric with shortName= " + metricName 
	+ " not found in file: " + profileFile	; 
      throw PROFILEException(error); 
    }
  }

  // -----------------------------------------------------------------
  // PROFILESCOPETREE
  // -----------------------------------------------------------------

  // LM (load module)
  if (isXMLStrSame(name, elemLM)) {
    lmName = "";
  }

  // F(ile)
  if (isXMLStrSame(name, elemFile)) {
    srcFileName = "";
  }

  // P(roc)
  if (isXMLStrSame(name, elemProc)) {
    funcName = "";
    funcScope = NULL;
  }

  // S(tmt)
  if (isXMLStrSame(name, elemStmt)) {
    line = -1;
  }
}
