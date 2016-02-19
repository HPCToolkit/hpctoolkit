// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
//   $HeadURL$
//
// Purpose:
//   XML adaptor for the program structure file (PGM)
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <stdlib.h>

#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <typeinfo>

//************************ Xerces Include Files ******************************

#include <xercesc/sax/ErrorHandler.hpp>

#include <xercesc/util/XMLString.hpp>
using XERCES_CPP_NAMESPACE::XMLString;

//************************* User Include Files *******************************

#include "PGMDocHandler.hpp"
#include "XercesSAX2.hpp"
#include "XercesUtil.hpp"
#include "XercesErrorHandler.hpp"

#include <lib/prof/Struct-Tree.hpp>
using namespace Prof;

#include <lib/support/diagnostics.h>
#include <lib/support/SrcFile.hpp>
using SrcFile::ln_NULL;
#include <lib/support/StrUtil.hpp>
#include <lib/support/Trace.hpp>

//************************ Forward Declarations ******************************

#define DBG 0

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
//      by default because Struct::TreeInterface maintains LM/F/P cursors and
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


PGMDocHandler::PGMDocHandler(Doc_t ty,
			     Struct::Tree* structure,
			     DocHandlerArgs& args)
  : m_docty(ty),
    m_args(args),
    m_structure(structure),

    // element names
    elemStructure(XMLString::transcode("HPCToolkitStructure")),
    elemLM(XMLString::transcode("LM")),
    elemFile(XMLString::transcode("F")),
    elemProc(XMLString::transcode("P")),
    elemAlien(XMLString::transcode("A")),
    elemLoop(XMLString::transcode("L")),
    elemStmt(XMLString::transcode("S")),
    elemGroup(XMLString::transcode("G")),

    // attribute names
    attrVer(XMLString::transcode("version")),
    attrId(XMLString::transcode("i")),
    attrName(XMLString::transcode("n")),
    attrFile(XMLString::transcode("f")),
    attrLnName(XMLString::transcode("ln")),
    attrLine(XMLString::transcode("l")),
    attrVMA(XMLString::transcode("v"))
{
  m_version = -1;

  m_curLM   = NULL;
  m_curFile = NULL;
  m_curProc = NULL;

  groupNestingLvl = 0;
}


PGMDocHandler::~PGMDocHandler()
{
  // element names
  XMLString::release((XMLCh**)&elemStructure);
  XMLString::release((XMLCh**)&elemLM);
  XMLString::release((XMLCh**)&elemFile);
  XMLString::release((XMLCh**)&elemProc);
  XMLString::release((XMLCh**)&elemAlien);
  XMLString::release((XMLCh**)&elemLoop);
  XMLString::release((XMLCh**)&elemStmt);
  XMLString::release((XMLCh**)&elemGroup);

  // attribute names
  XMLString::release((XMLCh**)&attrVer);
  XMLString::release((XMLCh**)&attrId);
  XMLString::release((XMLCh**)&attrName);
  XMLString::release((XMLCh**)&attrFile);
  XMLString::release((XMLCh**)&attrLnName);
  XMLString::release((XMLCh**)&attrLine);
  XMLString::release((XMLCh**)&attrVMA);

  DIAG_Assert(scopeStack.Depth() == 0, "Invalid state reading HPCStructure.");
}


void
PGMDocHandler::startElement(const XMLCh* const GCC_ATTR_UNUSED uri,
			    const XMLCh* const name,
			    const XMLCh* const GCC_ATTR_UNUSED qname,
			    const XERCES_CPP_NAMESPACE::Attributes& attributes)
{
  Struct::ANode* curStrct = NULL;

  // Structure
  if (XMLString::equals(name, elemStructure)) {
    string verStr = getAttr(attributes, attrVer);
    double ver = StrUtil::toDbl(verStr);

    m_version = ver;
    if (m_version < 4.5) {
      PGM_Throw("Found file format version " << m_version << ": This format is outdated; please regenerate the file.");
    }

    m_curRoot = m_structure->root();
    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << m_curRoot->toStringMe());

    curStrct = m_curRoot;
  }

  // Load Module
  else if (XMLString::equals(name, elemLM)) {
    string nm = getAttr(attributes, attrName); // must exist
    DIAG_Assert(m_curRoot && !m_curLM, "Parse error!");

    nm = m_args.realpath(nm);
    m_curLM = Prof::Struct::LM::demand(m_curRoot, nm);
    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << m_curLM->toStringMe());

    m_curFile = NULL;
    m_curProc = NULL;

    curStrct = m_curLM;
  }

  // File
  else if (XMLString::equals(name, elemFile)) {
    string nm = getAttr(attributes, attrName);
    DIAG_Assert(m_curLM && !m_curFile, "Parse error!");

    nm = m_args.realpath(nm);
    m_curFile = Struct::File::demand(m_curLM, nm);
    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << m_curFile->toStringMe());

    m_curProc = NULL;

    curStrct = m_curFile;
  }

  // Proc
  else if (XMLString::equals(name, elemProc)) {
    string nm  = getAttr(attributes, attrName);   // must exist
    string lnm = getAttr(attributes, attrLnName); // optional
    string id  = getAttr(attributes, attrId); 	  // ID: must exist

    SrcFile::ln begLn, endLn;
    getLineAttr(begLn, endLn, attributes);

    string vma = getAttr(attributes, attrVMA);
    string node_id = getAttr(attributes, attrId);

    DIAG_Assert(m_curLM && m_curFile && !m_curProc, "Parse error: Support for nested procedures is disabled (cf. buildLMSkeleton())!");

    // -----------------------------------------------------
    // Find/Create the procedure.
    // -----------------------------------------------------

    //m_curProc = Struct::Proc::demand(m_curFile, procnm, line);

    m_curProc = m_curFile->findProc(nm);
    if (m_curProc) {
      // STRUCTURE files usually have qualifying VMA information.
      // Assume that VMA information fully qualifies procedures.
      if (m_docty == Doc_STRUCT
	  && !m_curProc->vmaSet().empty() && !vma.empty()) {
	m_curProc = NULL;
      }
    }

    if (!m_curProc) {
      m_curProc = new Struct::Proc(nm, m_curFile, lnm, false, begLn, endLn);
      if (!vma.empty()) {
	m_curProc->vmaSet().fromString(vma.c_str());
      }
      m_curProc->m_origId = atoi(node_id.c_str());
    }
    else {
      if (m_docty == Doc_STRUCT) {
	// If a proc with the same name already exists, print a warning.
	DIAG_Msg(0, "Warning: Found procedure '" << nm << "' multiple times within file '" << m_curFile->name() << "'; information for this procedure will be aggregated. If you do not want this, edit the STRUCTURE file and adjust the names by hand.");
      }
    }

    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << m_curProc->toStringMe());

    curStrct = m_curProc;
    PGMDocHandler::idToProcMap[id] = (Prof::Struct::Proc*) m_curProc;
  }

  // Alien
  else if (XMLString::equals(name, elemAlien)) {
    int numAttr = attributes.getLength();
    DIAG_Assert(0 <= numAttr && numAttr <= 6, DIAG_UnexpectedInput);

    string nm  = getAttr(attributes, attrName);
    string ln  = getAttr(attributes, attrLnName);
    string fnm = getAttr(attributes, attrFile);
    fnm = m_args.realpath(fnm);

    SrcFile::ln begLn, endLn;
    getLineAttr(begLn, endLn, attributes);

    Struct::ACodeNode* parent = dynamic_cast<Struct::ACodeNode*>(getCurrentScope());
    Struct::Alien* alien = new Struct::Alien(parent, fnm, nm, nm, begLn, endLn);
    alien->proc( idToProcMap[ln] );

    string node_id = getAttr(attributes, attrId);
    alien->m_origId = atoi(node_id.c_str());

    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << alien->toStringMe());

    curStrct = alien;
  }

  // Loop
  else if (XMLString::equals(name, elemLoop)) {
    DIAG_Assert(scopeStack.Depth() >= 3, ""); // at least has Proc, File, LM

    // both 'begin' and 'end' are implied (and can be in any order)
    int numAttr = attributes.getLength();
    DIAG_Assert(0 <= numAttr && numAttr <= 5, DIAG_UnexpectedInput);

    SrcFile::ln begLn, endLn;
    getLineAttr(begLn, endLn, attributes);

    string fnm = getAttr(attributes, attrFile);
    fnm = m_args.realpath(fnm);

    // by now the file and function names should have been found
    Struct::ACodeNode* parent = dynamic_cast<Struct::ACodeNode*>(getCurrentScope());
    Struct::ACodeNode* loopNode = new Struct::Loop(parent, fnm, begLn, endLn);

    string node_id = getAttr(attributes, attrId);
    loopNode->m_origId = atoi(node_id.c_str());

    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << loopNode->toStringMe());

    curStrct = loopNode;
  }

  // Stmt
  else if (XMLString::equals(name, elemStmt)) {
    int numAttr = attributes.getLength();

    // 'begin' is required but 'end' is implied (and can be in any order)
    DIAG_Assert(1 <= numAttr && numAttr <= 4, DIAG_UnexpectedInput);

    SrcFile::ln begLn, endLn;
    getLineAttr(begLn, endLn, attributes);

    // for now insist that line range include one line (since we don't nest S)
    DIAG_Assert(begLn == endLn, "S line range [" << begLn << ", " << endLn << "]");

    string vma = getAttr(attributes, attrVMA);

    // by now the file and function names should have been found
    Struct::ACodeNode* parent = dynamic_cast<Struct::ACodeNode*>(getCurrentScope());
    DIAG_Assert(m_curProc != NULL, "");

    Struct::Stmt* stmtNode = new Struct::Stmt(parent, begLn, endLn);
    if (!vma.empty()) {
      stmtNode->vmaSet().fromString(vma.c_str());
    }
    string node_id = getAttr(attributes, attrId);
    stmtNode->m_origId = atoi(node_id.c_str());

    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << stmtNode->toStringMe());

    curStrct = stmtNode;
  }

  // Group
  else if (XMLString::equals(name, elemGroup)) {
    string grpnm = getAttr(attributes, attrName); // must exist
    DIAG_Assert(!grpnm.empty(), "");

    Struct::ANode* parent = getCurrentScope(); // enclosing scope
    Struct::Group* grpStrct
      = Prof::Struct::Group::demand(m_curRoot, grpnm, parent);
    DIAG_DevMsgIf(DBG, "PGMDocHandler: " << grpStrct->toStringMe());

    groupNestingLvl++;
    curStrct = grpStrct;
  }


  // -----------------------------------------------------------------
  //
  // -----------------------------------------------------------------

  // The current top of the stack must be a non-leaf.  Since we are
  // using SAX parsing, we don't know this until now.
  StackEntry_t* entry = getStackEntry(0);
  if (entry) {
    entry->SetLeaf(false);
  }

  pushCurrentScope(curStrct);
}


void
PGMDocHandler::endElement(const XMLCh* const GCC_ATTR_UNUSED uri,
			  const XMLCh* const name,
			  const XMLCh* const GCC_ATTR_UNUSED qname)
{

  // Structure
  if (XMLString::equals(name, elemStructure)) {
    m_curRoot = NULL;
  }

  // Load Module
  else if (XMLString::equals(name, elemLM)) {
    DIAG_Assert(scopeStack.Depth() >= 1, "");
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
    m_curLM = NULL;
  }

  // File
  else if (XMLString::equals(name, elemFile)) {
    DIAG_Assert(scopeStack.Depth() >= 2, ""); // at least has LM
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
    m_curFile = NULL;
  }

  // Proc
  else if (XMLString::equals(name, elemProc)) {
    DIAG_Assert(scopeStack.Depth() >= 3, ""); // at least has File, LM
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
    m_curProc = NULL;
  }

  // Alien
  else if (XMLString::equals(name, elemAlien)) {
    // stack depth should be at least 4
    DIAG_Assert(scopeStack.Depth() >= 4, "");
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
  }

  // Loop
  else if (XMLString::equals(name, elemLoop)) {
    // stack depth should be at least 4
    DIAG_Assert(scopeStack.Depth() >= 4, "");
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
  }

  // Stmt
  else if (XMLString::equals(name, elemStmt)) {
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
  }

  // Group
  else if (XMLString::equals(name, elemGroup)) {
    DIAG_Assert(scopeStack.Depth() >= 1, "");
    DIAG_Assert(groupNestingLvl >= 1, "");
    if (m_docty == Doc_GROUP) { processGroupDocEndTag(); }
    groupNestingLvl--;
  }

  popCurrentScope();
}


void
PGMDocHandler::getLineAttr(SrcFile::ln& begLn, SrcFile::ln& endLn,
			   const XERCES_CPP_NAMESPACE::Attributes& attributes)
{
  begLn = ln_NULL;
  endLn = ln_NULL;

  string begStr, endStr;

  // 1. Obtain string representation of begin and end line
  string lineStr = getAttr(attributes, attrLine);
  if (!lineStr.empty()) {
    size_t dashpos = lineStr.find_first_of('-');
    if (dashpos == std::string::npos) {
      begStr = lineStr;
    }
    else {
      begStr = lineStr.substr(0, dashpos);
      endStr = lineStr.substr(dashpos+1);
    }
  }

  // 2. Parse begin and end line strings
  if (!begStr.empty()) {
    begLn = (SrcFile::ln)StrUtil::toLong(begStr);
  }
  if (!endStr.empty()) {
    endLn = (SrcFile::ln)StrUtil::toLong(endStr);
  }

  // 3. Sanity check
  if (endLn == ln_NULL) {
    endLn = begLn;
  }
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
//  SAX2 ErrorHandler interface
// ---------------------------------------------------------------------------

void
PGMDocHandler::error(const SAXParseException& e)
{
  XercesErrorHandler::report(cerr, "HPCStructure non-fatal error", ToString(m_docty), e);
}


void
PGMDocHandler::fatalError(const SAXParseException& e)
{
  XercesErrorHandler::report(cerr, "HPCStructure fatal error", ToString(m_docty), e);
  exit(1);
}


void
PGMDocHandler::warning(const SAXParseException& e)
{
  XercesErrorHandler::report(cerr, "HPCStructure warning", ToString(m_docty), e);
}


// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

Struct::File*
PGMDocHandler::findCurrentFile()
{
  for (unsigned int i = 0; i < scopeStack.Depth(); ++i) {
    Struct::ANode* s = getScope(i);
    if (typeid(*s) == typeid(Struct::File)) {
      return dynamic_cast<Struct::File*>(s);
    }
  }
  return NULL;
}


uint
PGMDocHandler::findEnclosingGroupScopeDepth()
{
  for (uint i = 1; i < scopeStack.Depth(); ++i) {
    Struct::ANode* x = getScope(i);
    if (typeid(*x) == typeid(Struct::Group)) {
      return i + 1; // depth is index + 1
    }
  }
  return 0;
}


// For processing the GROUP file
void
PGMDocHandler::processGroupDocEndTag()
{
  StackEntry_t* top = getStackEntry(0);

  // If the top of the stack is a non-group leaf node or a nested
  // group node... (Note 'groupNestingLvl' has not been decremented yet)
  if ( (groupNestingLvl >= 1 && top->IsNonGroupLeaf()) ||
       (groupNestingLvl >= 2 && top->IsGroupScope()) ) {

    // 1. Find enclosing GROUP node g (excluding the top of the stack)
    unsigned int enclGrpDepth = findEnclosingGroupScopeDepth();
    DIAG_Assert(enclGrpDepth >= 2, ""); // must be found && not at top
    unsigned int enclGrpIdx = enclGrpDepth - 1;

    // 2. For each node between g (excluding g) to the node before
    // top-of-stack, create a shadow node
    Struct::ANode* parentNode = getScope(enclGrpIdx);
    for (unsigned int i = enclGrpIdx - 1; i >= 1; --i) {
      StackEntry_t* entry = getStackEntry(i);
      Struct::ANode* curNode = entry->getScope();
      Struct::ANode* shadowNode = entry->GetShadow();
      if (!shadowNode) {
	shadowNode = curNode->clone();
	shadowNode->link(parentNode);
	entry->SetShadow(shadowNode);
      }
      parentNode = shadowNode;
    }

    // 3. Make the top node a child of the most recent shadow node
    Struct::ANode* topNode = getScope(0);
    topNode->unlink();
    topNode->link(parentNode);
  }
}

