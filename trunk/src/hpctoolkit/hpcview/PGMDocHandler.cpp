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
//    PGMDocHandler.C
//
// Purpose:
//    XML adaptor for the program structure file (PGM)
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

#include "PGMDocHandler.h"
#include "XMLAdapter.h"
#include "ScopesInfo.h"
#include <lib/support/Assertion.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::cerr;
using std::endl;

//****************************************************************************

// ----------------------------------------------------------------------
// -- PGMDocHandler(NodeRetriever* const retriever) --
//   Constructor.
//
//   -- arguments --
//     retriever:        a const pointer to a NodeRetriever object
// ----------------------------------------------------------------------

PGMDocHandler::PGMDocHandler(NodeRetriever* const retriever,
			     Driver *_driver) 
  : SAXHandlerBase(true) 
{
  // trace = 1;
  nodeRetriever = retriever;
  driver = _driver;
  pgmVersion = -1;
  srcFileName = "";
  funcScope = NULL; 
  currentScope = NULL;
}

// ----------------------------------------------------------------------
// -- ~PGMDocHandler() --
//   Destructor.
//
//   -- arguments --
//     None
// ----------------------------------------------------------------------

PGMDocHandler::~PGMDocHandler() 
{
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

// element names
s_c_XMLCh_c elemPgm    = XMLStr("PGM");
s_c_XMLCh_c elemGroup  = XMLStr("G");
s_c_XMLCh_c elemLM     = XMLStr("LM");
s_c_XMLCh_c elemFile   = XMLStr("F");
s_c_XMLCh_c elemProc   = XMLStr("P");
s_c_XMLCh_c elemLoop   = XMLStr("L");
s_c_XMLCh_c elemStmt   = XMLStr("S");

// attribute names
s_c_XMLCh_c attrVer    = XMLStr("version");
s_c_XMLCh_c attrName   = XMLStr("n");
s_c_XMLCh_c attrLnName = XMLStr("ln");
s_c_XMLCh_c attrBegin  = XMLStr("b");
s_c_XMLCh_c attrEnd    = XMLStr("e");
s_c_XMLCh_c attrId     = XMLStr("id");

void PGMDocHandler::startElement(const XMLCh* const name,
				 AttributeList& attributes) 
{  
  // -----------------------------------------------------------------
  // PGM
  // -----------------------------------------------------------------
  if (isXMLStrSame(name, elemPgm)) {
    String pgmName = getAttr(attributes, attrName);
    String verStr = getAttr(attributes, attrVer);
    
    pgmName = driver->ReplacePath(pgmName);
    double ver = atof(verStr);

    pgmVersion = ver;
    if (pgmVersion < 3.0) {
      String error = "This file format version is outdated; please regenerate the file."; 
      throw PGMException(error); 
    }
    
    IFTRACE << "PGM: name=" << pgmName << " ver=" << ver << endl;

    PgmScope* root = nodeRetriever->GetRoot();
    root->SetName(pgmName);
  }
  
  // G(roup)
  if (isXMLStrSame(name, elemGroup)) {
    // For now, GROUPs are meaningless in a PGM.
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
    String srcFile = getAttr(attributes, attrName);
    srcFile = driver->ReplacePath(srcFile);
    IFTRACE << "F(ile): name=" << srcFile << endl;
    
    // if the source file name is the same as the previous one, error.
    // otherwise find another one; it should not be the same as the
    // previous one
    BriefAssertion( srcFile != srcFileName );
    
    srcFileName = srcFile;
    FileScope * fileScope = nodeRetriever->MoveToFile(srcFileName); 
    scopeStack.Push(fileScope);
  }

  // P(roc)
  if (isXMLStrSame(name, elemProc)) {
    // currentScope should be NULL, scopeStack should contain file
    BriefAssertion( currentScope == NULL );
    BriefAssertion( scopeStack.Depth() == 1 );
    
    String name  = getAttr(attributes, attrName);   // must exist
    String lname = getAttr(attributes, attrLnName); // optional
    if (driver->MustDeleteUnderscore()) {
      if ((name.Length() > 0) && (name[name.Length()-1] == '_')) {
	name[name.Length()-1] = '\0';
      }
      if ((lname.Length() > 0) && (lname[lname.Length()-1] == '_')) {
	lname[lname.Length()-1] = '\0';
      }
    }
    IFTRACE << "P(roc): name="  << name << " lname=" << lname << endl;
    
    int lnB = UNDEF_LINE, lnE = UNDEF_LINE;
    String lineB = getAttr(attributes, attrBegin);
    String lineE = getAttr(attributes, attrEnd);
    if (lineB.Length() > 0) { lnB = atoi(lineB); }
    if (lineE.Length() > 0) { lnE = atoi(lineE); }
    IFTRACE << " b="  << lnB << " e=" << lnE << endl;
    
    // Create the procedure.  If one with the same name already
    // exists, print an warning.
    FileScope *f = (FileScope *)scopeStack.Top();
    funcScope = f->FindProc(name);
    if (!funcScope) {
      funcScope = new ProcScope(name, f, lnB, lnE);
    } else {
      cerr << "WARNING: Procedure " << name << " was found multiple times"
	   << " in the file " << f->Name() << "; information for this"
	   << " procedure will be aggregated. If you do not want this,"
	   << " edit the STRUCTURE file and adjust the names by hand.\n";
    }

    currentScope = funcScope;
    scopeStack.Push(funcScope);
  }

  // L(oop)
  if (isXMLStrSame(name, elemLoop)) {
    // stack depth should be at least 1
    BriefAssertion( scopeStack.Depth() >= 2 );
    int numAttr = attributes.getLength();
    
    // both 'begin' and 'end' are implied (and can be in any order)
    BriefAssertion(numAttr >= 0 && numAttr <= 2);

    int lnB = UNDEF_LINE, lnE = UNDEF_LINE;
    String lineB = getAttr(attributes, attrBegin);
    String lineE = getAttr(attributes, attrEnd);
    if (lineB.Length() > 0) { lnB = atoi(lineB); }
    if (lineE.Length() > 0) { lnE = atoi(lineE); }

    IFTRACE << "L(oop): numberB=" << lineB << " numberE=" << lineE << endl;
    
    // by now the file and function names should have been found    
    BriefAssertion(currentScope != NULL); 
    CodeInfo* loopNode = new LoopScope(currentScope, lnB, lnE); 
    BriefAssertion( loopNode != NULL );
    currentScope = loopNode;
    scopeStack.Push( loopNode );
  }
  
  // S(tmt)
  if (isXMLStrSame(name, elemStmt)) {
    int numAttr = attributes.getLength();
    
    // 'begin' is required but 'end' is implied (and can be in any order)
    BriefAssertion(numAttr == 1 || numAttr == 2);
    
    int lnB = UNDEF_LINE, lnE = UNDEF_LINE;
    String lineB = getAttr(attributes, attrBegin);
    String lineE = getAttr(attributes, attrEnd);
    if (lineB.Length() > 0) { lnB = atoi(lineB); }
    if (lineE.Length() > 0) { lnE = atoi(lineE); }
    BriefAssertion( lnB != UNDEF_LINE );

    // IF lineE is undefined, set it to lineB
    if (lnE == UNDEF_LINE) { lnE = lnB; }

    // Check that lnB and lnE are valid line numbers: FIXME

    IFTRACE << "S(tmt): numberB=" << lineB << " numberE=" << lineE << endl;
    
    // for now assume that lnB and lnE are equals, exit otherwise
    BriefAssertion( lnB == lnE );

    // by now the file and function names should have been found
    BriefAssertion(funcScope != NULL); 
    LineScope* lineNode = funcScope->CreateLineScope(currentScope, lnB);
#if 0
    // FIXME: we should be using stmt ranges not line scopes
    CodeInfo* lineNode = new StmtRangeScope(currentScope, lnB, lnE);
#endif
    BriefAssertion( lineNode != NULL );
  }

}


void PGMDocHandler::endElement(const XMLCh* const name)
{
  // LM (load module)
  if (isXMLStrSame(name, elemLM)) {
    lmName = "";
  }

  // F(ile)
  if (isXMLStrSame(name, elemFile)) {
    BriefAssertion( scopeStack.Depth() == 1 );
    scopeStack.Pop();
    currentScope = NULL;
    srcFileName = "";
  }

  // P(roc)
  if (isXMLStrSame(name, elemProc)) {
    // currentScope should be Proc, scopeStack should one level
    BriefAssertion( currentScope == funcScope );
    BriefAssertion( scopeStack.Depth() == 2 );
    
    scopeStack.Pop();
    currentScope = NULL;
    funcName = "";
  }
  
  // L(oop)
  if (isXMLStrSame(name, elemLoop)) {
    // stack depth should be at least 3
    BriefAssertion( scopeStack.Depth() >= 3 );
    BriefAssertion(currentScope != NULL); 
    
    scopeStack.Pop();
    currentScope = static_cast<CodeInfo*>(scopeStack.Top());
  }
}

