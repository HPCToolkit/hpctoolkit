// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   tree (ANode) and performance information parsing logic.
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

#include "Struct-TreeInterface.hpp"
#include "Struct-Tree.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

//************************ Local Declarations ******************************

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

namespace Prof {
namespace Struct {

TreeInterface::TreeInterface(Pgm* _root, const string& p) 
  : root(_root), currentLM(NULL), currentFile(NULL), currentProc(NULL), path(p)
{
  DIAG_Assert(root != NULL, "");
  DIAG_Assert(!p.empty(), "");
}

TreeInterface::~TreeInterface()
{
}

Group*
TreeInterface::MoveToGroup(ANode* parent, const char* name)
{
  DIAG_Assert(parent && name, "");
  
  Group* grp = root->findGroup(name);
  if (grp == NULL) {
    grp = new Group(name, parent);
    DIAG_DevMsg(3, "TreeInterface::MoveToGroup new Group: " << name);
  } 
  return grp;
}

LM*
TreeInterface::MoveToLM(const char* name) 
{
  DIAG_Assert(name, "");

  LM* lm = root->findLM(name);
  if (lm == NULL) {
    lm = new LM(name, root);
    DIAG_DevMsg(3, "TreeInterface::MoveToLM new LM: " << name);
  } 
  currentLM = lm; 

  // Invalidate any previous file, proc
  currentFile = NULL; 
  currentProc = NULL;

  return lm;
}

File* 
TreeInterface::MoveToFile(const char* name) 
{
  DIAG_Assert(name, "");
  DIAG_Assert(currentLM, "");

#if 0
  // If the file is not found, a load module will be created, if
  // needed, with the same name as the root.
  if (!currentLM) {
    currentLM = MoveToLM(root->name());
  } 
#endif

  // -------------------------------------------------------
  // Obtain a 'canonical' file name
  // -------------------------------------------------------
  string knownByName = name;
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

  // -------------------------------------------------------
  // Search for the file
  // -------------------------------------------------------
  File* f = currentLM->FindFile(filePath);
  const char* msg = "File Scope Found "; // an optimistic variable
  if (f == NULL) {
    f = new File(filePath, srcIsReadable, currentLM);

    msg = "File Created for ";
    DIAG_DevMsg(3, "TreeInterface::MoveToFile makes new File:" << endl
		<< "  name=" << name << "  fname=" << filePath);
  } 
  currentFile = f;

  // Invalidate any previous proc
  currentProc = NULL;

  DIAG_Msg(2, msg << filePath); // debug path replacement
  return f;
}

Proc* 
TreeInterface::MoveToProc(const char* name) 
{
  DIAG_Assert(name, "");
  DIAG_Assert(currentLM, "");
  DIAG_Assert(currentFile, "");

  Proc* p = currentFile->FindProc(name);
  if (p == NULL) {
    p = new Proc(name, currentFile, false, "");
  }
  currentProc = p;

  return p;
}

} // namespace Struct
} // namespace Prof
