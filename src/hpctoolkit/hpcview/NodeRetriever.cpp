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
//    NodeRetriever.C
//
// Purpose:
//    Class NodeRetriever provides an interface between the scope info
//    tree (ScopeInfo) and performance information parsing logic.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <iostream>

//************************* User Include Files *******************************

#include "Args.h"
#include "NodeRetriever.h"
#include "ScopeInfo.h"
#include <lib/support/String.h>
#include <lib/support/Assertion.h>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::endl;
using std::cerr;


//************************ Local Declarations ******************************

const char *unknownFileName = "<unknown>";

//****************************************************************************

static String 
RemoveTrailingBlanks(const char* name)
{
  int i;
  String noBlanks = name; 
  for (i = strlen(name) - 1; i >= 0; i--) { 
    if (noBlanks[(unsigned int)i] != ' ') 
      break;
  }
  i++; 
  if ((unsigned int)i < strlen(name) ) { 
    noBlanks[(unsigned int)i] = '\0'; 
  }
  return noBlanks; 
}

static String 
GetFormattedSourceFileName(const char* nm)
{ 
  String name = RemoveTrailingBlanks(nm); 
  return name; 
}

//****************************************************************************

NodeRetriever::NodeRetriever(PgmScope* _root, const String& p) 
  : root(_root), currentLM(NULL), currentFile(NULL), currentProc(NULL), path(p)
{
  BriefAssertion(root != NULL);
  BriefAssertion(p.Length() > 0);
}

NodeRetriever::~NodeRetriever()
{
}

GroupScope*
NodeRetriever::MoveToGroup(ScopeInfo* parent, const char* name)
{
  BriefAssertion(parent && name);
  
  GroupScope* grp = root->FindGroup(name);
  if (grp == NULL) {
    grp = new GroupScope(name, parent);
    IFTRACE << "NodeRetriever::MoveToGroup new GroupScope: " << name
	    << endl;
  } 
  return grp;
}

LoadModScope*
NodeRetriever::MoveToLoadMod(const char* name) 
{
  BriefAssertion(name);

  LoadModScope* lm = root->FindLoadMod(name);
  if (lm == NULL) {
    lm = new LoadModScope(name, root);
    IFTRACE << "NodeRetriever::MoveToLoadMod new LoadModScope: " << name
	    << endl;
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
  String knownByName;
  
  BriefAssertion(name);

  if (strcmp(name, unknownFileName) == 0) {
    if (!currentLM) {
      currentLM = MoveToLoadMod(root->Name());
    } 
    knownByName = "Unknown file in " + currentLM->BaseName();
    unknownFileIndex++;
    
  } else {
    knownByName = name;
  }
 
  // Obtain a 'canonical' file name
  String fileName = GetFormattedSourceFileName(knownByName);  
  
  bool srcIsReadable = true; 

  String filePath = String(pathfind_r(path, fileName, "r")); 
  if (filePath.Length() == 0) {
    srcIsReadable = false; 
    filePath = fileName; 
  } else {
    filePath = RealPath(filePath); 
  }

  // Search for the file
  FileScope *f = root->FindFile(filePath);
  const char* msg = "File Scope Found "; // an optimistic variable
  if (f == NULL) {

    if (!currentLM) {
      currentLM = MoveToLoadMod(root->Name());
    } 
    f = new FileScope(filePath, srcIsReadable, "", currentLM); 

    msg = "File Scope Created for ";
    IFTRACE << "NodeRetriever::MoveToFile makes new FileScope:" << endl
	    << "  name=" << name << "  fname=" << filePath  << endl;
  } else {
    currentLM = f->LoadMod();
    BriefAssertion(currentLM);
  }
  currentFile = f;

  // Invalidate any previous proc
  currentProc = NULL; 

  if (fileTrace) {
    cerr << msg << filePath << endl;
  }
  return f;
}

ProcScope* 
NodeRetriever::MoveToProc(const char* name) 
{
  BriefAssertion(name);
  BriefAssertion(currentLM);
  BriefAssertion(currentFile);

  ProcScope *p = currentFile->FindProc(name);
  if (p == NULL) {
    p = new ProcScope(name, currentFile, UNDEF_LINE, UNDEF_LINE);
  }
  currentProc = p;

  return p;
}
