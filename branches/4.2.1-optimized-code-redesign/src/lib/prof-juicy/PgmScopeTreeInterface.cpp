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
//   Class NodeRetriever provides an interface between the scope info
//   tree (ScopeInfo) and performance information parsing logic.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <iostream>
using std::endl;
using std::cerr;

#include <string>
using std::string;

//************************* User Include Files *******************************

#include "PgmScopeTreeInterface.hpp"
#include "PgmScopeTree.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

//************************ Local Declarations ******************************

const char *unknownFileName = "<unknown>";

//****************************************************************************

static string 
RemoveTrailingBlanks(const string& name)
{
  int i;
  string noBlanks = name; 
  for (i = name.length() - 1; i >= 0; i--) { 
    if (noBlanks[(unsigned int)i] != ' ') 
      break;
  }
  i++; 
  if ((unsigned int)i < name.length() ) { 
    noBlanks[(unsigned int)i] = '\0'; 
  }
  return noBlanks; 
}

static string 
GetFormattedSourceFileName(const string& nm)
{ 
  string name = RemoveTrailingBlanks(nm); 
  return name; 
}

//****************************************************************************

NodeRetriever::NodeRetriever(PgmScope* _root, const string& p) 
  : root(_root), currentLM(NULL), currentFile(NULL), currentProc(NULL), path(p)
{
  DIAG_Assert(root != NULL, "");
  DIAG_Assert(!p.empty(), "");
}

NodeRetriever::~NodeRetriever()
{
}

GroupScope*
NodeRetriever::MoveToGroup(ScopeInfo* parent, const char* name)
{
  DIAG_Assert(parent && name, "");
  
  GroupScope* grp = root->FindGroup(name);
  if (grp == NULL) {
    grp = new GroupScope(name, parent);
    DIAG_DevMsg(3, "NodeRetriever::MoveToGroup new GroupScope: " << name);
  } 
  return grp;
}

LoadModScope*
NodeRetriever::MoveToLoadMod(const char* name) 
{
  DIAG_Assert(name, "");

  LoadModScope* lm = root->FindLoadMod(name);
  if (lm == NULL) {
    lm = new LoadModScope(name, root);
    DIAG_DevMsg(3, "NodeRetriever::MoveToLoadMod new LoadModScope: " << name);
  } 
  currentLM = lm; 

  // Invalidate any previous file, proc
  currentFile = NULL; 
  currentProc = NULL;

  return lm;
}

FileScope *
NodeRetriever::MoveToFile(const char* name) 
{
  static long unknownFileIndex = 0;
  string knownByName;
  
  DIAG_Assert(name, "");

  if (strcmp(name, unknownFileName) == 0) {
    if (!currentLM) {
      currentLM = MoveToLoadMod(root->name());
    } 
    knownByName = "Unknown file in " + currentLM->BaseName();
    unknownFileIndex++;
    
  } 
  else {
    knownByName = name;
  }
 
  // Obtain a 'canonical' file name
  string fileName = GetFormattedSourceFileName(knownByName);
  
  bool srcIsReadable = true; 

  string filePath;
  const char* pf = pathfind_r(path.c_str(), fileName.c_str(), "r");
  if (!pf) {
    srcIsReadable = false;
    filePath = fileName; 
  } 
  else {
    filePath = RealPath(pf);
  }

  // Search for the file
  FileScope *f = root->FindFile(filePath);
  const char* msg = "File Scope Found "; // an optimistic variable
  if (f == NULL) {

    if (!currentLM) {
      currentLM = MoveToLoadMod(root->name());
    } 
    f = new FileScope(filePath, srcIsReadable, currentLM); 

    msg = "File Scope Created for ";
    DIAG_DevMsg(3, "NodeRetriever::MoveToFile makes new FileScope:" << endl
		<< "  name=" << name << "  fname=" << filePath);
  } else {
    currentLM = f->LoadMod();
    DIAG_Assert(currentLM, "");
  }
  currentFile = f;

  // Invalidate any previous proc
  currentProc = NULL;

  DIAG_Msg(2, msg << filePath); // debug path replacement
  return f;
}

ProcScope* 
NodeRetriever::MoveToProc(const char* name) 
{
  DIAG_Assert(name, "");
  DIAG_Assert(currentLM, "");
  DIAG_Assert(currentFile, "");

  ProcScope *p = currentFile->FindProc(name);
  if (p == NULL) {
    p = new ProcScope(name, currentFile, "");
  }
  currentProc = p;

  return p;
}
