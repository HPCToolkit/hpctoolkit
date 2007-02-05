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

//***************************************************************************
//
// File:
//   $Source$
//
// Purpose:
//   XML adaptor for the profile data file (PROFILE)
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <iostream> 
using std::cerr;
using std::endl;

#include <string>
using std::string;

//************************* Xerces Include Files *****************************

#include <xercesc/util/XMLString.hpp>         
using XERCES_CPP_NAMESPACE::XMLString;

//************************* User Include Files *******************************

#include "PROFILEDocHandler.hpp"
#include "XercesSAX2.hpp"
#include "XercesErrorHandler.hpp"

#include <lib/prof-juicy/PgmScopeTreeInterface.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// ----------------------------------------------------------------------
// -- PROFILEDocHandler(NodeRetriever* const retriever) --
//   Constructor.
//
//   -- arguments --
//     retriever:        a const pointer to a NodeRetriever object
// ----------------------------------------------------------------------

PROFILEDocHandler::PROFILEDocHandler(NodeRetriever* const retriever, 
				     DocHandlerArgs& args)  
  : m_nodeRetriever(retriever),
    m_args(args),

    // element names
    elemProfile(XMLString::transcode("PROFILE")),
    elemProfileHdr(XMLString::transcode("PROFILEHDR")),
    elemProfileParams(XMLString::transcode("PROFILEPARAMS")),
    elemTarget(XMLString::transcode("TARGET")),
    elemMetrics(XMLString::transcode("METRICS")),
    elemMetricDef(XMLString::transcode("METRIC")),
    elemProfileScopeTree(XMLString::transcode("PROFILESCOPETREE")),
    elemPgm(XMLString::transcode("PGM")),
    elemGroup(XMLString::transcode("G")),
    elemLM(XMLString::transcode("LM")),
    elemFile(XMLString::transcode("F")),
    elemProc(XMLString::transcode("P")),
    elemLoop(XMLString::transcode("L")),
    elemStmt(XMLString::transcode("S")),
    elemMetric(XMLString::transcode("M")),
    
    // attribute names for PROFILE elements
    attrVer(XMLString::transcode("version")),
    attrShortName(XMLString::transcode("shortName")),
    attrNativeName(XMLString::transcode("nativeName")),
    attrPeriod(XMLString::transcode("period")),
    attrUnits(XMLString::transcode("units")),
    attrDisplayName(XMLString::transcode("displayName")),
    attrDisplay(XMLString::transcode("display")),
    
    // attribute names for PGM elements
    attrName(XMLString::transcode("n")),
    attrLnName(XMLString::transcode("ln")),
    attrBegin(XMLString::transcode("b")),
    attrEnd(XMLString::transcode("e")),
    attrId(XMLString::transcode("id")),
    attrVal(XMLString::transcode("v"))
{
  metricPerfDataTblIndx = -1;
  profVersion = -1;
  procScope = NULL;
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
  DIAG_Assert(IsPerfDataIndex(metricIndx), ""); 
  IFTRACE << "PROFILEDocHandler::InitMetricHandler: " << metricIndx << endl; 
  metricPerfDataTblIndx = metricIndx;
  profileFile = profileFileName; 
  metricName  = IndexToPerfDataInfo(metricIndx).NativeName();
}

// ----------------------------------------------------------------------
// -- startElement(const XMLCh* const uri, const XMLCh* const name, 
//                 const XMLCh* const qname, const Attributes& attributes)
//   Process the element start tag and extract out attributes.
//
//   -- arguments --
//     name:         element name
//     attributes:   attributes
// ----------------------------------------------------------------------


void PROFILEDocHandler::startElement(const XMLCh* const uri, const XMLCh* const name, 
				     const XMLCh* const qname, const Attributes& attributes)
{
  // -----------------------------------------------------------------
  // PROFILE
  // -----------------------------------------------------------------  
  if (XMLString::equals(name, elemProfile)) {
    string verStr = getAttr(attributes, attrVer);
    double ver = StrUtil::toDbl(verStr);
    
    // We can handle versions 3.0+
    profVersion = ver;
    if (profVersion < 3.0) {
      string error = "This file format version is outdated; please regenerate the file."; 
      throw PROFILEException(error); 
    }
    
    IFTRACE << "PROFILE: version=" << ver << endl;
  }

  // -----------------------------------------------------------------
  // PROFILEHDR
  // -----------------------------------------------------------------
  if (XMLString::equals(name, elemProfileHdr)) {
    IFTRACE << "PROFILEHDR" << endl;
  }

  // -----------------------------------------------------------------
  // PROFILEPARAMS
  // -----------------------------------------------------------------
  if (XMLString::equals(name, elemProfileParams)) {
    IFTRACE << "PROFILEPARAMS" << endl;
  }

  // TARGET: executable name
  if (XMLString::equals(name, elemTarget)) {
    string target = getAttr(attributes, 0 /*name*/); 
    IFTRACE << "TARGET: name=" << target << endl;
  }
  
  // METRICS
  if (XMLString::equals(name, elemMetrics)) {
    IFTRACE << "METRICS" << endl;
  }

  // METRIC: list of profiling metrics
  if (XMLString::equals(name, elemMetricDef)) {
    string shortName   = getAttr(attributes, attrShortName);
    string nativeName  = getAttr(attributes, attrNativeName);
    string period      = getAttr(attributes, attrPeriod);
    string units       = getAttr(attributes, attrUnits);       // optional
    string displayName = getAttr(attributes, attrDisplayName); // optional
    string display     = getAttr(attributes, attrDisplay);     // optional

    // if this is the metric we are particularly interested in, save it
    if (metricName == shortName) {
      metricNameShort = shortName;
    }
    
    int prd = (int)StrUtil::toLong(period); 
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
  if (XMLString::equals(name, elemProfileScopeTree)) {
    IFTRACE << "PROFILESCOPETREE" << endl;
  }

  // PGM [See PGMDocHandler.C]
  if (XMLString::equals(name, elemPgm)) {
    string pgmName = getAttr(attributes, attrName);
    pgmName = m_args.ReplacePath(pgmName);
    IFTRACE << "PGM: name= " << pgmName << endl;

    PgmScope* root = m_nodeRetriever->GetRoot(); 
    if (root->name().empty()) { root->SetName(pgmName); }
  }

  // G(roup)
  if (XMLString::equals(name, elemGroup)) {
    // For now, GROUPs are meaningless in a PROFILE.
    IFTRACE << "G(roup)" << endl;
  }    

  // LM (load module)
  if (XMLString::equals(name, elemLM)) {
    string lm = getAttr(attributes, attrName); // must exist
    lm = m_args.ReplacePath(lm);

    DIAG_Assert(lmName.empty(), "");
    lmName = lm;
    lmScope = m_nodeRetriever->MoveToLoadMod(lmName);
    
    IFTRACE << "LM (load module): name= " << lmName << endl;
  }

  // F(ile)
  if (XMLString::equals(name, elemFile)) {
    string srcFile = getAttr(attributes, attrName); // must exist
    srcFile = m_args.ReplacePath(srcFile);

    DIAG_Assert(srcFileName.empty(), "");
    srcFileName = srcFile;
    fileScope = m_nodeRetriever->MoveToFile(srcFileName); 

    IFTRACE << "F(ile): name= " << srcFile << endl;       
  }
  
  // P(roc)
  if (XMLString::equals(name, elemProc)) {
    string name = getAttr(attributes, attrName); // must exist
    if (m_args.MustDeleteUnderscore()) {
      if (!name.empty() && (name[name.length()-1] == '_')) {
	name[name.length()-1] = '\0';
      }
    }
    IFTRACE << "P(roc): name="  << name << endl;

    // For now, 'attrLnName,' 'attrBegin,' and 'attrEnd' are
    // meaningless in a PROFILE.  See PGMDocHandler.C
    
    DIAG_Assert(funcName.empty(), "");
    funcName = name;
    procScope = m_nodeRetriever->MoveToProc(funcName);
    // FIXME: should take linkname into account
    
    IFTRACE << "P(roc): name= " << name << endl;       
  }

  // L(oop)
  if (XMLString::equals(name, elemLoop)) {
    // For now, LOOPs are meaningless in a PROFILE.
    IFTRACE << "L(oop)" << endl;
  }    
  
  // S(tmt)
  if (XMLString::equals(name, elemStmt)) {
    int numAttr = attributes.getLength();
    DIAG_Assert(numAttr >= 1 && numAttr <= 3, "");
    
    // For now, we are not interested in 'attrEnd' or 'attrId'
    int lnB = UNDEF_LINE, lnE = UNDEF_LINE, lnId = UNDEF_LINE;
    string lineB  = getAttr(attributes, attrBegin);
    string lineE  = getAttr(attributes, attrEnd);
    string lineId = getAttr(attributes, attrId);
    if (!lineB.empty()) { lnB = (int)StrUtil::toLong(lineB); }
    if (!lineE.empty()) { lnE = (int)StrUtil::toLong(lineE); }
    if (!lineId.empty()) { lnId = (int)StrUtil::toLong(lineE); }

    // IF lineE is undefined, set it to lineB
    if (lnE == UNDEF_LINE) { lnE = lnB; }
    
    // Check that lnB and lnE are valid line numbers: FIXME

    IFTRACE << "S(tmt): b= " << lnB << " e= " << lnE << " i= " << lnId << endl;

    DIAG_Assert(line == -1, "");
    line = lnB; 
  }

  // M(etric)
  if (XMLString::equals(name, elemMetric)) {
    string name  = getAttr(attributes, attrName);
    string value = getAttr(attributes, attrVal);

    // Consider only the one metric we are interested in
    if (name == metricNameShort) {
      // Metrics can be attached to any element of the tree (not just stmts).
      double val = StrUtil::toDbl(value);
      if (line != -1) { // FIXME change to IsValid
	StmtRangeScope* stmt = procScope->FindStmtRange(line);
	if (!stmt) {
	  stmt = new StmtRangeScope(procScope, line, line, 0, 0);
	}
	stmt->SetPerfData(metricPerfDataTblIndx, val);
      } 
      else {
	if (procScope) {
	  procScope->SetPerfData(metricPerfDataTblIndx, val); 
	} 
	else if (fileScope) {
	  fileScope->SetPerfData(metricPerfDataTblIndx, val); 
	} 
	else {
	  lmScope->SetPerfData(metricPerfDataTblIndx, val); 
	}
      }
    }
    
    IFTRACE << " M(etric): name= " << name
	    << " value= " << value << endl;
  }
}

void PROFILEDocHandler::endElement(const XMLCh* const uri, 
				   const XMLCh* const name, 
				   const XMLCh* const qname)
{
  // -----------------------------------------------------------------
  // PROFILEHDR
  // -----------------------------------------------------------------

  // -----------------------------------------------------------------
  // PROFILEPARAMS
  // -----------------------------------------------------------------

  // METRICS
  if (XMLString::equals(name, elemMetrics)) {
    // If 'metricNameShort' has not been found then 'metricName' is invalid
    if ( metricNameShort.empty() ){
      string error = "METRIC with shortName=" + metricName 
	+ " not found in PROFILE file '" + profileFile + "'."; 
      throw PROFILEException(error); 
    }
  }

  // -----------------------------------------------------------------
  // PROFILESCOPETREE
  // -----------------------------------------------------------------

  // LM (load module)
  if (XMLString::equals(name, elemLM)) {
    lmName = "";
    lmScope = NULL;
  }

  // F(ile)
  if (XMLString::equals(name, elemFile)) {
    srcFileName = "";
    fileScope = NULL;
  }

  // P(roc)
  if (XMLString::equals(name, elemProc)) {
    funcName = "";
    procScope = NULL;
  }

  // S(tmt)
  if (XMLString::equals(name, elemStmt)) {
    line = -1;
  }
}


const char *PROFILE = "PROFILE";

// ---------------------------------------------------------------------------
//  implementation of SAX2 ErrorHandler interface
// ---------------------------------------------------------------------------
void PROFILEDocHandler::error(const SAXParseException& e)
{
  HPCViewXMLErrHandler::report(cerr, "hpcview non-fatal error", PROFILE, e);
}

void PROFILEDocHandler::fatalError(const SAXParseException& e)
{
  HPCViewXMLErrHandler::report(cerr, "hpcview fatal error", PROFILE, e);
  exit(1);
}

void PROFILEDocHandler::warning(const SAXParseException& e)
{
  HPCViewXMLErrHandler::report(cerr, "hpcview warning", PROFILE, e);
}

