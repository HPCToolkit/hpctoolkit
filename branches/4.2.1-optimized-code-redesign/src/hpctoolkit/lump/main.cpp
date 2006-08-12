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
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <fstream>
#include <typeinfo>
#include <new>

#include <map>
#include <list>

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/binutils/LoadModule.hpp>
#include <lib/binutils/Section.hpp>
#include <lib/binutils/Procedure.hpp>
#include <lib/binutils/Instruction.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

// A list of addresses (VMAs)
typedef std::list<VMA> VMAList;
typedef std::list<VMA>::iterator VMAListIt;
typedef std::list<VMA>::const_iterator VMAListItC;

// A map of a line number to an VMAList
typedef std::map<suint, VMAList*> LineToVMAListMap;
typedef std::map<suint, VMAList*>::iterator LineToVMAListMapIt;
typedef std::map<suint, VMAList*>::value_type LineToVMAListMapItVal;

void ClearLineToVMAListMap(LineToVMAListMap* map);

// Dump Helpers
void DumpSymbolicInfo(std::ostream& os, LoadModule* lm);
void DumpSymbolicInfoOld(std::ostream& os, LoadModule* lm);

//****************************************************************************

int realmain(int argc, char* const* argv);

int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  } 
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  } 
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }

  return ret;
}


int
realmain(int argc, char* const argv[])
{
  Args args(argc, argv);

  // ------------------------------------------------------------
  // Read load module
  // ------------------------------------------------------------
  LoadModule* lm = NULL;
  try {
    lm = new LoadModule();
    if (!lm->Open(args.inputFile.c_str())) { 
      exit(1); // Error already printed 
    }
    if (!lm->Read()) { 
      exit(1); // Error already printed 
    }
  } 
  catch (...) {
    DIAG_EMsg("Exception encountered while reading " << args.inputFile);
    throw;
  } 

  // ------------------------------------------------------------
  // Dump load module
  // ------------------------------------------------------------
  try {
    if (args.symbolicDump) {
      DumpSymbolicInfo(std::cout, lm);
    } 
    else if (args.symbolicDumpOld) {
      DumpSymbolicInfoOld(std::cout, lm);
    }
    else {
      lm->Dump(std::cout);
    }
    
  } 
  catch (...) {
    DIAG_EMsg("Exception encountered while dumping " << args.inputFile);
    throw;
  }

  delete lm;
  return (0);
}


//****************************************************************************

void 
DumpHeaderInfo(std::ostream& os, LoadModule* lm, const char* pre = "")
{
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
}


//****************************************************************************

void 
DumpSymbolicInfo(std::ostream& os, LoadModule* lm)
{
  string pre = "  ";
  string pre1 = pre + "  ";

  DumpHeaderInfo(os, lm, pre.c_str());

  // ------------------------------------------------------------------------
  // Iterate through the VMA values of the text section, and dump the 
  //   symbolic information associated with each VMA
  // ------------------------------------------------------------------------  

  os << pre << "Dump:\n";
  for (LoadModuleSectionIterator it(*lm); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->GetType() != Section::Text) { continue; }
    
    // We have a 'TextSection'.  Iterate over procedures.
    TextSection* tsec = dynamic_cast<TextSection*>(sec);
    for (TextSectionProcedureIterator it(*tsec); it.IsValid(); ++it) {
      Procedure* p = it.Current();
      string pName = GetBestFuncName(p->GetName());

      os << "* " << pName << hex
	 << " [" << "0x" << p->GetBegVMA() << ", 0x" << p->GetEndVMA()
	 << "]\n" << dec;

      // We have a 'Procedure'.  Iterate over VMA values     
      for (ProcedureInstructionIterator it(*p); it.IsValid(); ++it) {
	Instruction* inst = it.Current();
	VMA pc = inst->GetVMA();
	VMA opVMA = isa->ConvertVMAToOpVMA(pc, inst->GetOpIndex());
	
	// Find and dump symbolic info attched to VMA
	string func, file;
	suint line;
	p->GetSourceFileInfo(pc, inst->GetOpIndex(), func, file, line);
	func = GetBestFuncName(func);
	
	os << pre << "0x" << hex << opVMA << dec 
	   << " " << file << ":" << line << ":" << func << "\n";
      }
      os << std::endl;
    }
  }
  os << "\n" << "End LoadModule Stmt Dump\n";
}

//****************************************************************************

void DumpSymbolicInfoForFunc(std::ostream& os, const char* pre, 
			     const char* func, LineToVMAListMap* map, 
			     const char* file);

void 
DumpSymbolicInfoOld(std::ostream& os, LoadModule* lm)
{
  string pre = "  ";
  string pre1 = pre + "  ";

  DumpHeaderInfo(os, lm, pre.c_str());

  // ------------------------------------------------------------------------
  // Iterate through the VMA values of the text section, collect
  //   symbolic information on a source line basis, and output the results
  // ------------------------------------------------------------------------  

  os << pre << "Dump:\n";
  for (LoadModuleSectionIterator it(*lm); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->GetType() != Section::Text) { continue; }
    
    // We have a 'TextSection'.  Iterate over procedures.
    TextSection* tsec = dynamic_cast<TextSection*>(sec);
    for (TextSectionProcedureIterator it(*tsec); it.IsValid(); ++it) {
      Procedure* p = it.Current();
      string pName = GetBestFuncName(p->GetName());

      
      // We have a 'Procedure'.  Iterate over VMA values     
      string theFunc = pName, theFile;
      LineToVMAListMap map;

      for (ProcedureInstructionIterator it(*p); it.IsValid(); ++it) {
	Instruction* inst = it.Current();
	VMA pc = inst->GetVMA();
	VMA opVMA = isa->ConvertVMAToOpVMA(pc, inst->GetOpIndex());
	
	// 1. Attempt to find symbolic information
	string func, file;
	suint line;
	p->GetSourceFileInfo(pc, inst->GetOpIndex(), func, file, line);
	func = GetBestFuncName(func);
	
	// Bad line number: cannot fix; advance iteration
	if ( !IsValidLine(line) ) {
	  continue; // cannot continue without valid symbolic info
	}

	// Bad/Different func name: ignore for now and use 'theFunc' (FIXME)
	if (func.empty()) { func = theFunc; }
	
	// Bad/Different file name: ignore and try 'theFile' (FIXME)
	if (file.empty() && !theFile.empty() // possible replacement...
	    && func == theFunc) { // ...the replacement is valid
	  file = theFile; 
	}

	// 2. We have decent symbolic info.  Squirrel this away.
	if (theFunc.empty()) { theFunc = func; }
	if (theFile.empty()) { theFile = file; }
	
	LineToVMAListMapIt it1 = map.find(line);
	VMAList* list;
	if (it1 != map.end()) { 
	  list = (*it1).second; // modify existing list
	  list->push_back(opVMA);	  
	} else { 
	  list = new VMAList; // create a new list and insert pair
	  list->push_back(opVMA);
	  map.insert(LineToVMAListMapItVal(line, list));
	}
      }
      
      DumpSymbolicInfoForFunc(os, pre1.c_str(), 
			      theFunc.c_str(), &map, theFile.c_str());
      ClearLineToVMAListMap(&map);
      
    }
  }
  os << "\n" << "End LoadModule Stmt Dump\n";
}


void 
DumpSymbolicInfoForFunc(std::ostream& os, const char* pre, 
			const char* func, LineToVMAListMap* map, 
			const char* file)
{
  string p = pre;
  string p1 = p + "  ";

  unsigned int mapSz = map->size();
  
  os << p << "* " << func << "\n"
     << p << "  [" << file << "]\n";

  LineToVMAListMapIt it;
  for (it = map->begin(); it != map->end(); ++it) {
    suint line = (*it).first;
    VMAList* list = (*it).second;
    
    os << p1 << "  " << line << ": {" << hex;

    VMAListIt it1;
    for (it1 = list->begin(); it1 != list->end(); ++it1) {
      VMA pc = *it1;
      os << " 0x" << pc;
    }
    os << " }" << dec << endl;
  }
}


void 
ClearLineToVMAListMap(LineToVMAListMap* map)
{
  LineToVMAListMapIt it;
  for (it = map->begin(); it != map->end(); ++it) {
    delete (*it).second;
  }
  map->clear();
}
