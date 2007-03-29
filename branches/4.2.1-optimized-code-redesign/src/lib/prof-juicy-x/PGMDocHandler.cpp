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
//   XML adaptor for the program structure file (PGM)
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

//************************ Xerces Include Files ******************************

#include <xercesc/sax/ErrorHandler.hpp>

#include <xercesc/util/XMLString.hpp>         
using XERCES_CPP_NAMESPACE::XMLString;

//************************* User Include Files *******************************

#include "PGMDocHandler.hpp"
#include "XercesSAX2.hpp"
#include "XercesUtil.hpp"
#include "XercesErrorHandler.hpp"

#include <lib/prof-juicy/PgmScopeTreeInterface.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// General parsing notes:
//   - We assume a more restricted input than the PGM specification
//   allows.  For example, a Proc must always have a File parent.
//
//
// Group Implementation notes.
// 
// The goal is to obtain a structure tree, including groups, that can be
// traversed by the PROFILE handler, which finds the appropriate location
// in the scope tree by examining load module, file, procedure and line
// maps.  Thus, given a LM, F, P and S/LINE, we must be able to locate
// and annotate the statements in partitioning defined by the groups.
// 
// While processing the group file, we maintain/build the standard scope
// tree in addition to creating leaf Group nodes (even if the it will not
// eventually be a leaf) in appropriate places.  When a Group scope is
// closed, we fill in Group subtrees by creating shadow copies of the
// corresponding non-leaf children of the Group.  The leaf children are
// transported to become children of the shadow nodes.
// 
// This scheme allows the shadow and transported nodes to be found using
// the standard maps when processing the PROFILE file.  The grouped
// shadow copies will recive aggregate metric data and the original
// versions of these nodes will be eliminated when the tree is pruned.
// 
// Algorithm:
// 
//   - Maintain a stack of [scope, shadow-scope] pairs
//   - Maintain a counter of group nesting
// 
// 1. Move down the tree and process it as we process PGM nodes, creating
//    a stack of nodes. 
// 
//    - if tree is empty it will be populated as if it were a structure
//      file (mostly).  IOW, a LM, F or P that is a child of a group will
//      be added to the tree as if it were _not_ (shadow nodes will be
//      created when the group scope is closed).  We get this behavior
//      by default because NodeRetriever maintains LM/F/P cursors and
//      igores the group.
// 
//      The exception is that unlike LM, F and P, L and S will be added
//      as children of G; neither have names that the node retriever can
//      use. However, they will be correctly annotated with profile info
//      because of the line map in a P.
// 
//    - When a G node is seen, add it to the current enclosing scope
//      and increment group nesting counter
// 
// 2. When a non-group-leaf or leaf node is closed within a group context:
//    a. Find enclosing GROUP node g (excluding the top of the stack)
//    b. For each node between g (excluding g) to the node before
//       top-of-stack, create a shadow node
//    c. Make the top node a child of the most recent shadow node
// 
// Example:
//
// structure file:
//   PGM
//     LM
//       F1
//         P1
//         P2
//       F2
// 
// group file:
//   PGM 
//     G1 
//      LM 
//        G2
//          F1 
//            P1
// 
// A. Structure file has been processed.  After processing Group file
//    down to P1 (completion of step 1)
// 
// scope tree:              stack:
//   PGM                    PGM
//     G1   <- new          G1
//     LM                   LM
//       G2 <- new          G2  <=
//       F1                 F1 
//         P1               P1  <- top
//         P2 
//       F2
// 
// B. After processing from G2 to P1 on stack (completion of step 2)
//
// scope tree:
//   PGM
//     G1
//     LM
//       G2
//         F1'  <- shadow
//           P1 <- moved
//       F1     <- retains map pointer to P1
//         P2
//       F2
// 

//****************************************************************************

// ----------------------------------------------------------------------
// -- PGMDocHandler(NodeRetriever* const retriever) --
//   Constructor.
//
//   -- arguments --
//     retriever:        a const pointer to a NodeRetriever object
// ----------------------------------------------------------------------

PGMDocHandler::PGMDocHandler(Doc_t ty,
			     NodeRetriever* const retriever,
			     DocHandlerArgs& args) 
  : m_docty(ty),
    m_nodeRetriever(retriever),
    m_args(args),
  
    // element names
    elemPgm(XMLString::transcode("PGM")), 
    elemLM(XMLString::transcode("LM")),
    elemFile(XMLString::transcode("F")),
    elemProc(XMLString::transcode("P")),
    elemAlien(XMLString::transcode("A")),
    elemLoop(XMLString::transcode("L")),
    elemStmt(XMLString::transcode("S")),
    elemGroup(XMLString::transcode("G")),

    // attribute names
    attrVer(XMLString::transcode("version")),
    attrId(XMLString::transcode("id")),
    attrName(XMLString::transcode("n")),
    attrAlienFile(XMLString::transcode("f")),
    attrLnName(XMLString::transcode("ln")),
    attrBegin(XMLString::transcode("b")),
    attrEnd(XMLString::transcode("e")),
    attrVMA(XMLString::transcode("vma"))
{
  // trace = 1;
  pgmVersion = -1;
    
  currentLmName= "";
  currentFileName = "";
  currentFuncName = "";
  currentFuncScope = NULL; 
  groupNestingLvl = 0;
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
  DIAG_Assert(scopeStack.Depth() == 0, "Invalid state reading PGM.");
}


// ----------------------------------------------------------------------
// -- startElement(const XMLCh* const uri, 
//                 const XMLCh* const name, 
//                 const XMLCh* const qname, 
//                 const Attributes& attributes)
//   Process the element start tag and extract out attributes.
//
//   -- arguments --
//     name:         element name
//     attributes:   attributes
// ----------------------------------------------------------------------

#define s_c_XMLCh_c static const XMLCh* const
void PGMDocHandler:: startElement(const XMLCh* const uri, 
				  const XMLCh* const name, 
				  const XMLCh* const qname, 
				  const XERCES_CPP_NAMESPACE::Attributes& attributes)
{ 
  ScopeInfo* currentScope = NULL;
  
  // -----------------------------------------------------------------
  // PGM
  // -----------------------------------------------------------------
  if (XMLString::equals(name, elemPgm)) {
    string verStr = getAttr(attributes, attrVer);
    double ver = StrUtil::toDbl(verStr);

    pgmVersion = ver;
    if (pgmVersion < 4.5) {
      string error = "Found file format version " + StrUtil::toStr(pgmVersion)
	+ ": This format is outdated; please regenerate the file."; 
      throw PGMException(error);
    }
    
    IFTRACE << "PGM: ver=" << ver << endl;

    PgmScope* root = m_nodeRetriever->GetRoot();
    currentScope = root;
  }
  
  // LM (load module)
  else if (XMLString::equals(name, elemLM)) {
    string lm = getAttr(attributes, attrName); // must exist
    lm = m_args.ReplacePath(lm);
    DIAG_Assert(currentLmName.empty(), "Parse or internal error!");
    currentLmName = lm;
    IFTRACE << "LM (load module): name= " << currentLmName << endl;
    
    LoadModScope* lmscope = m_nodeRetriever->MoveToLoadMod(currentLmName);
    DIAG_Assert(lmscope != NULL, "");
    currentScope = lmscope;
  }
  
  // F(ile)
  else if (XMLString::equals(name, elemFile)) {
    string srcFile = getAttr(attributes, attrName);
    srcFile = m_args.ReplacePath(srcFile);
    IFTRACE << "F(ile): name=" << srcFile << endl;
    
    // if the source file name is the same as the previous one, error.
    // otherwise find another one; it should not be the same as the
    // previous one
    DIAG_Assert(srcFile != currentFileName, "");
    
    currentFileName = srcFile;
    FileScope* fileScope = m_nodeRetriever->MoveToFile(currentFileName);
    DIAG_Assert(fileScope != NULL, "");
    currentScope = fileScope;
  }

  // P(roc)
  else if (XMLString::equals(name, elemProc)) {
    DIAG_Assert(scopeStack.Depth() >= 2, ""); // at least has File, LM
    
    string name  = getAttr(attributes, attrName);   // must exist
    string lname = getAttr(attributes, attrLnName); // optional
    if (m_args.MustDeleteUnderscore()) {
      if (!name.empty() && (name[name.length()-1] == '_')) {
	name[name.length()-1] = '\0';
      }
      if (!lname.empty() && (lname[lname.length()-1] == '_')) {
	lname[lname.length()-1] = '\0';
      }
    }
    IFTRACE << "P(roc): name="  << name << " lname=" << lname << endl;
    
    suint lnB = UNDEF_LINE, lnE = UNDEF_LINE;
    string lineB = getAttr(attributes, attrBegin);
    string lineE = getAttr(attributes, attrEnd);
    if (!lineB.empty()) { lnB = (suint)StrUtil::toLong(lineB); }
    if (!lineE.empty()) { lnE = (suint)StrUtil::toLong(lineE); }
    IFTRACE << " b="  << lnB << " e=" << lnE << endl;
    
    string vma = getAttr(attributes, attrVMA);
    
    // Find enclosing File scope
    FileScope* curFile = FindCurrentFileScope();
    if (!curFile) {
      string error = "No F(ile) scope for P(roc) scope '" + name + "'";
      throw PGMException(error);
    }

    // -----------------------------------------------------
    // Find/Create the procedure.
    // -----------------------------------------------------
    currentFuncScope = curFile->FindProc(name);
    if (currentFuncScope) {
      // STRUCTURE files usually have qualifying VMA information.
      // Assume that VMA information fully qualifies procedures.
      if (m_docty == Doc_STRUCT && !currentFuncScope->vmaSet().empty() 
	  && !vma.empty()) {
	currentFuncScope = NULL;
      }
    }

    if (!currentFuncScope) {
      currentFuncScope = new ProcScope(name, curFile, lname, lnB, lnE);
      if (!vma.empty()) {
	currentFuncScope->vmaSet().fromString(vma.c_str());
      }
    }
    else {
      if (m_docty == Doc_STRUCT) {
	// If a proc with the same name already exists, print a warning.
	DIAG_Msg(0, "Warning: Found procedure '" << name << "' multiple times within file '" << curFile->name() << "'; information for this procedure will be aggregated. If you do not want this, edit the STRUCTURE file and adjust the names by hand.");
      }
    }
    
    currentScope = currentFuncScope;
  }
  
  // A(lien)
  else if (XMLString::equals(name, elemAlien)) {
    int numAttr = attributes.getLength();
    DIAG_Assert(0 <= numAttr && numAttr <= 5, "");
    
    string nm = getAttr(attributes, attrName); 
    string fnm = getAttr(attributes, attrAlienFile);

    suint begLn = UNDEF_LINE, endLn = UNDEF_LINE;
    string lineB = getAttr(attributes, attrBegin);
    string lineE = getAttr(attributes, attrEnd);
    if (!lineB.empty()) { begLn = (suint)StrUtil::toLong(lineB); }
    if (!lineE.empty()) { endLn = (suint)StrUtil::toLong(lineE); }

    IFTRACE << "A(lien): name= " << nm << endl;

    CodeInfo* enclScope = 
      dynamic_cast<CodeInfo*>(GetCurrentScope()); // enclosing scope

    CodeInfo* alien = new AlienScope(enclScope, fnm, nm, begLn, endLn);
    currentScope = alien;
  }

  // L(oop)
  else if (XMLString::equals(name, elemLoop)) {
    DIAG_Assert(scopeStack.Depth() >= 3, ""); // at least has Proc, File, LM

    // both 'begin' and 'end' are implied (and can be in any order)
    int numAttr = attributes.getLength();
    DIAG_Assert(0 <= numAttr && numAttr <= 3, "");
    
    suint lnB = UNDEF_LINE, lnE = UNDEF_LINE;
    string lineB = getAttr(attributes, attrBegin);
    string lineE = getAttr(attributes, attrEnd);
    if (!lineB.empty()) { lnB = (suint)StrUtil::toLong(lineB); }
    if (!lineE.empty()) { lnE = (suint)StrUtil::toLong(lineE); }

    IFTRACE << "L(oop): numberB=" << lineB << " numberE=" << lineE << endl;
    
    // by now the file and function names should have been found
    CodeInfo* enclScope = 
      dynamic_cast<CodeInfo*>(GetCurrentScope()); // enclosing scope

    CodeInfo* loopNode = new LoopScope(enclScope, lnB, lnE);
    currentScope = loopNode;
  }
  
  // S(tmt)
  else if (XMLString::equals(name, elemStmt)) {
    int numAttr = attributes.getLength();
    
    // 'begin' is required but 'end' is implied (and can be in any order)
    DIAG_Assert(1 <= numAttr && numAttr <= 3, DIAG_UnexpectedInput);
    
    suint lnB = UNDEF_LINE, lnE = UNDEF_LINE;
    string lineB = getAttr(attributes, attrBegin);
    string lineE = getAttr(attributes, attrEnd);
    if (!lineB.empty()) { lnB = (suint)StrUtil::toLong(lineB); }
    if (!lineE.empty()) { lnE = (suint)StrUtil::toLong(lineE); }
    //DIAG_Assert(lnB != UNDEF_LINE, "S beg line is " << UNDEF_LINE);

    // Check that lnB and lnE are valid line numbers:
    //   if lineE is undefined, set it to lineB
    if (lnE == UNDEF_LINE) { lnE = lnB; }
    IFTRACE << "S(tmt): numberB=" << lineB << " numberE=" << lineE << endl;

    // for now insist that lnB and lnE are equal
    DIAG_Assert(lnB == lnE, "S beg/end lines not equal: b=" << lnB 
		<< " e=" << lnE);
    
    string vma = getAttr(attributes, attrVMA);

    // by now the file and function names should have been found
    CodeInfo* enclScope = 
      dynamic_cast<CodeInfo*>(GetCurrentScope()); // enclosing scope
    DIAG_Assert(currentFuncScope != NULL, "");

    StmtRangeScope* stmtNode = new StmtRangeScope(enclScope, lnB, lnE);

    if (!vma.empty()) {
      stmtNode->vmaSet().fromString(vma.c_str());
    }
    currentScope = stmtNode;
  }
  
  // G(roup)
  else if (XMLString::equals(name, elemGroup)) {
    string grpnm = getAttr(attributes, attrName); // must exist
    DIAG_Assert(!grpnm.empty(), "");
    IFTRACE << "G(roup): name= " << grpnm << endl;

    ScopeInfo* enclScope = GetCurrentScope(); // enclosing scope
    GroupScope* grpscope = m_nodeRetriever->MoveToGroup(enclScope, grpnm);
    DIAG_Assert(grpscope != NULL, "");
    groupNestingLvl++;
    currentScope = grpscope;
  }

  
  // -----------------------------------------------------------------
  // 
  // -----------------------------------------------------------------

  // The current top of the stack must be a non-leaf.  Since we are
  // using SAX parsing, we don't know this until now.
  StackEntry_t* entry = GetStackEntry(0);
  if (entry) {
    entry->SetLeaf(false);
  }

  PushCurrentScope(currentScope);
}


void PGMDocHandler::endElement(const XMLCh* const uri, 
			       const XMLCh* const name, 
			       const XMLCh* const qname)
{

  // PGM
  if (XMLString::equals(name, elemPgm)) {
  }

  // LM (load module)
  else if (XMLString::equals(name, elemLM)) {
    DIAG_Assert(scopeStack.Depth() >= 1, "");
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
    currentLmName = "";
  }

  // F(ile)
  else if (XMLString::equals(name, elemFile)) {
    DIAG_Assert(scopeStack.Depth() >= 2, ""); // at least has LM
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
    currentFileName = "";
  }

  // P(roc)
  else if (XMLString::equals(name, elemProc)) {
    DIAG_Assert(scopeStack.Depth() >= 3, ""); // at least has File, LM
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
    currentFuncName = "";
    currentFuncScope = NULL;
  }

  // A(lien)
  else if (XMLString::equals(name, elemAlien)) {
    // stack depth should be at least 4
    DIAG_Assert(scopeStack.Depth() >= 4, "");
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
  }
  
  // L(oop)
  else if (XMLString::equals(name, elemLoop)) {
    // stack depth should be at least 4
    DIAG_Assert(scopeStack.Depth() >= 4, "");
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
  }
  
  // S(tmt)
  else if (XMLString::equals(name, elemStmt)) {
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
  }

  // G(roup)
  else if (XMLString::equals(name, elemGroup)) {
    DIAG_Assert(scopeStack.Depth() >= 1, "");
    DIAG_Assert(groupNestingLvl >= 1, "");
    if (m_docty == Doc_GROUP) { ProcessGroupDocEndTag(); }
    groupNestingLvl--;
  }

  PopCurrentScope();
}


// ---------------------------------------------------------------------------
//  
// ---------------------------------------------------------------------------

const char* 
PGMDocHandler::ToString(Doc_t m_docty)
{
  switch (m_docty) {
    case Doc_NULL:   return "Document:NULL";
    case Doc_STRUCT: return "Document:STRUCTURE";
    case Doc_GROUP:  return "Document:GROUP";
    default: DIAG_Die("Invalid Doc_t!");
  }
}


// ---------------------------------------------------------------------------
//  implementation of SAX2 ErrorHandler interface
// ---------------------------------------------------------------------------

void 
PGMDocHandler::error(const SAXParseException& e)
{
  XercesErrorHandler::report(cerr, "PGM non-fatal error", 
			     ToString(m_docty), e);
}

void 
PGMDocHandler::fatalError(const SAXParseException& e)
{
  XercesErrorHandler::report(cerr, "PGM fatal error", 
			     ToString(m_docty), e);
  exit(1);
}

void 
PGMDocHandler::warning(const SAXParseException& e)
{
  XercesErrorHandler::report(cerr, "PGM warning", 
			     ToString(m_docty), e);
}


// ---------------------------------------------------------------------------
//  
// ---------------------------------------------------------------------------

FileScope* 
PGMDocHandler::FindCurrentFileScope() 
{
  for (unsigned int i = 0; i < scopeStack.Depth(); ++i) {
    ScopeInfo* s = GetScope(i);
    if (s->Type() == ScopeInfo::FILE) {
      return dynamic_cast<FileScope*>(s);
    }
  }
  return NULL;
}

unsigned int
PGMDocHandler::FindEnclosingGroupScopeDepth() 
{
  for (unsigned int i = 1; i < scopeStack.Depth(); ++i) {
    ScopeInfo* s = GetScope(i);
    if (s->Type() == ScopeInfo::GROUP) {
      return i + 1; // depth is index + 1
    }
  }
  return 0;
}

// For processing the GROUP file
void 
PGMDocHandler::ProcessGroupDocEndTag()
{
  StackEntry_t* top = GetStackEntry(0);
  
  // If the top of the stack is a non-group leaf node or a nested
  // group node... (Note 'groupNestingLvl' has not been decremented yet)
  if ( (groupNestingLvl >= 1 && top->IsNonGroupLeaf()) ||
       (groupNestingLvl >= 2 && top->IsGroupScope()) ) {
    
    // 1. Find enclosing GROUP node g (excluding the top of the stack)
    unsigned int enclGrpDepth = FindEnclosingGroupScopeDepth();
    DIAG_Assert(enclGrpDepth >= 2, ""); // must be found && not at top
    unsigned int enclGrpIdx = enclGrpDepth - 1;
    
    // 2. For each node between g (excluding g) to the node before
    // top-of-stack, create a shadow node
    ScopeInfo* parentNode = GetScope(enclGrpIdx);
    for (unsigned int i = enclGrpIdx - 1; i >= 1; --i) {
      StackEntry_t* entry = GetStackEntry(i);
      ScopeInfo* curNode = entry->GetScope();
      ScopeInfo* shadowNode = entry->GetShadow();
      if (!shadowNode) {
	shadowNode = curNode->Clone();
	shadowNode->Link(parentNode);
	entry->SetShadow(shadowNode);
      }
      parentNode = shadowNode;
    }
    
    // 3. Make the top node a child of the most recent shadow node
    ScopeInfo* topNode = GetScope(0);
    topNode->Unlink();
    topNode->Link(parentNode);
  }
}

