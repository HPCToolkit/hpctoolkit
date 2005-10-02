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
//    main.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <typeinfo>
#include <new>

#include <map>
#include <list>

//*************************** User Include Files ****************************

#include "Args.h"

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/Section.h>
#include <lib/binutils/Procedure.h>
#include <lib/binutils/Instruction.h>
#include <lib/binutils/BinUtils.h>
#include <lib/support/String.h> 
#include <lib/support/Assertion.h> 

//*************************** Forward Declarations ***************************

using std::cerr;
using std::endl;
using std::hex;
using std::dec;

// A list of addresses (VMAs)
typedef std::list<Addr> AddrList;
typedef std::list<Addr>::iterator AddrListIt;
typedef std::list<Addr>::const_iterator AddrListItC;

// A map of a line number to an AddrList
typedef std::map<suint, AddrList*> LineToAddrListMap;
typedef std::map<suint, AddrList*>::iterator LineToAddrListMapIt;
typedef std::map<suint, AddrList*>::value_type LineToAddrListMapItVal;

void ClearLineToAddrListMap(LineToAddrListMap* map);

// Dump Helpers
void DumpSymbolicInfo(std::ostream& os, LoadModule* lm);

//****************************************************************************

int
main(int argc, char* argv[])
{
  Args args(argc, argv);

  // ------------------------------------------------------------
  // Read load module
  // ------------------------------------------------------------
  LoadModule* lm = NULL;
  try {
    lm = new LoadModule();
    if (!lm->Open(args.inputFile)) { exit(1); } // Error already printed 
    if (!lm->Read()) { exit(1); }               // Error already printed 
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading load module!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading load module!\n";
    exit(2);
  }

  // ------------------------------------------------------------
  // Dump load module
  // ------------------------------------------------------------
  try {

    if (args.symbolicDump) {
      DumpSymbolicInfo(std::cout, lm);
    } else {
      lm->Dump(std::cout);
    }
    
  } catch (...) {
    cerr << "Error: Exception encountered while dumping load module!\n";
    exit(2);
  }

  delete lm;
  return (0);
}

//****************************************************************************

void DumpSymbolicInfoForFunc(std::ostream& os, const char* pre, 
			     const char* func, LineToAddrListMap* map, 
			     const char* file);

void 
DumpSymbolicInfo(std::ostream& os, LoadModule* lm)
{
  String pre = "  ";
  String pre1 = pre + "  ";

  // ------------------------------------------------------------------------
  // Iterate through the PC values of the text section, collect
  //   symbolic information on a source line basis, and output the results
  // ------------------------------------------------------------------------  
  
  os << "Begin LoadModule Stmt Dump\n";
  os << pre << "Name: `" << lm->GetName() << "'\n";
  os << pre << "Type: `";
  switch (lm->GetType()) {
    case LoadModule::Executable:
      os << "Executable (fully linked except for possible DSOs)'\n";
      break;
    case LoadModule::SharedLibrary:
      os << "Dynamically Shared Library'\n";
      break;
    default:
      os << "-unknown-'\n";
      BriefAssertion(false); 
  }
  os << pre << "ISA: `" << typeid(*isa).name() << "'\n"; // std::type_info

  os << pre << "Dump:\n";
  for (LoadModuleSectionIterator it(*lm); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->GetType() != Section::Text) { continue; }
    
    // We have a 'TextSection'.  Iterate over procedures.
    TextSection* tsec = dynamic_cast<TextSection*>(sec);
    for (TextSectionProcedureIterator it(*tsec); it.IsValid(); ++it) {
      Procedure* p = it.Current();
      String pName = GetBestFuncName(p->GetName());

      
      // We have a 'Procedure'.  Iterate over PC values     
      String theFunc = pName, theFile;
      LineToAddrListMap map;

      for (ProcedureInstructionIterator it(*p); it.IsValid(); ++it) {
	Instruction* inst = it.Current();
	Addr pc = inst->GetPC();
	Addr opPC = isa->ConvertPCToOpPC(pc, inst->GetOpIndex());
	
	// 1. Attempt to find symbolic information
	String func, file;
	suint line;
	p->GetSourceFileInfo(pc, inst->GetOpIndex(), func, file, line);
	func = GetBestFuncName(func);
	
	// Bad line number: cannot fix; advance iteration
	if ( !IsValidLine(line) ) {
	  continue; // cannot continue without valid symbolic info
	}

	// Bad/Different func name: ignore for now and use 'theFunc' (FIXME)
	if (func.Empty()) { func = theFunc; }
	
	// Bad/Different file name: ignore and try 'theFile' (FIXME)
	if (file.Empty() && !theFile.Empty() // possible replacement...
	    && func == theFunc) { // ...the replacement is valid
	  file = theFile; 
	}

	// 2. We have decent symbolic info.  Squirrel this away.
	if (theFunc.Empty()) { theFunc = func; }
	if (theFile.Empty()) { theFile = file; }
	
	LineToAddrListMapIt it1 = map.find(line);
	AddrList* list;
	if (it1 != map.end()) { 
	  list = (*it1).second; // modify existing list
	  list->push_back(opPC);	  
	} else { 
	  list = new AddrList; // create a new list and insert pair
	  list->push_back(opPC);
	  map.insert(LineToAddrListMapItVal(line, list));
	}
      }
      
      DumpSymbolicInfoForFunc(os, pre1, theFunc, &map, theFile);
      ClearLineToAddrListMap(&map);
      
    }
  }
  os << "\n" << "End LoadModule Stmt Dump\n";
}

void 
DumpSymbolicInfoForFunc(std::ostream& os, const char* pre, 
			const char* func, LineToAddrListMap* map, 
			const char* file)
{
  String p = pre;
  String p1 = p + "  ";

  unsigned int mapSz = map->size();
  
  os << p << "* " << func << "\n"
     << p << "  [" << file << "]\n";

  LineToAddrListMapIt it;
  for (it = map->begin(); it != map->end(); ++it) {
    suint line = (*it).first;
    AddrList* list = (*it).second;
    
    os << p1 << "  " << line << ": {" << hex;

    AddrListIt it1;
    for (it1 = list->begin(); it1 != list->end(); ++it1) {
      Addr pc = *it1;
      os << " 0x" << pc;
    }
    os << " }" << dec << endl;
  }
}


//****************************************************************************

void ClearLineToAddrListMap(LineToAddrListMap* map)
{
  LineToAddrListMapIt it;
  for (it = map->begin(); it != map->end(); ++it) {
    delete (*it).second;
  }
  map->clear();
}
