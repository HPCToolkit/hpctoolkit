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
//   tree (ScopeInfo) and performance information parsing logic.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_juicy_PgmScopeTreeInterface_hpp
#define prof_juicy_PgmScopeTreeInterface_hpp

//************************ System Include Files ******************************

#include <string>

//************************* User Include Files *******************************

//************************ Forward Declarations ******************************

class ScopeInfo;
class CodeInfo;

class PgmScope;
class GroupScope;
class LoadModScope;
class FileScope;
class ProcScope;

//************************ Forward Declarations ******************************

//****************************************************************************

class NodeRetriever {
public: 
  // root must not be NULL
  // path = non empty list of directories 
  NodeRetriever(PgmScope *root, const std::string& path); 
  ~NodeRetriever();
  
  PgmScope* GetRoot() const { return root; }; 

  // get/make group scope with given parent and name.  We need a
  // parent scope for now because a Group can be a child of basically
  // anything and we do not keep an 'enclosingscope' pointer.
  GroupScope* MoveToGroup(ScopeInfo* parent, const char* name);
  GroupScope* MoveToGroup(ScopeInfo* parent, const std::string& name)
    { return MoveToGroup(parent, name.c_str()); }


  // get/make load module with name 'name' and remember it as current
  // load module.  Resets current file and proc.
  LoadModScope* MoveToLoadMod(const char* name);
  LoadModScope* MoveToLoadMod(const std::string& name)
    { return MoveToLoadMod(name.c_str()); }

  // get/make file with name 'name' and remember it as current file.
  // Requires the current load module to be set. Resets the current
  // proc.
  FileScope* MoveToFile(const char* name);
  FileScope* MoveToFile(const std::string& name)
    { return MoveToFile(name.c_str()); }

  // get/make procedure with name 'name' within current file and load
  // module (i.e. these must not be NULL).
  ProcScope* MoveToProc(const char* name);
  ProcScope* MoveToProc(const std::string& name)
    { return MoveToProc(name.c_str()); }

private:
  PgmScope *root;
  LoadModScope* currentLM;
  FileScope* currentFile;
  ProcScope* currentProc;
  std::string path; 
};

#endif /* prof_juicy_PgmScopeTreeInterface_hpp */
