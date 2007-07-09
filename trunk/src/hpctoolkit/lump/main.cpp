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
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
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

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/SrcFile.hpp>

//*************************** Forward Declarations ***************************

// A list of addresses (VMAs)
typedef std::list<VMA> VMAList;
typedef std::list<VMA>::iterator VMAListIt;
typedef std::list<VMA>::const_iterator VMAListItC;

// A map of a line number to an VMAList
typedef std::map<SrcFile::ln, VMAList*> LineToVMAListMap;
typedef std::map<SrcFile::ln, VMAList*>::iterator LineToVMAListMapIt;
typedef std::map<SrcFile::ln, VMAList*>::value_type LineToVMAListMapItVal;

void ClearLineToVMAListMap(LineToVMAListMap* map);

// Dump Helpers
void DumpSymbolicInfoOld(std::ostream& os, binutils::LM* lm);

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
  binutils::LM* lm = NULL;
  try {
    lm = new binutils::LM();
    lm->Open(args.inputFile.c_str());
    lm->Read();
  } 
  catch (...) {
    DIAG_EMsg("Exception encountered while reading " << args.inputFile);
    throw;
  }

  // ------------------------------------------------------------
  // Dump load module
  // ------------------------------------------------------------
  try {
    if (args.dumpOld) {
      DumpSymbolicInfoOld(std::cout, lm);
    }
    
    // Assume parser sanity checked arguments
    binutils::LM::DumpTy ty = binutils::LM::DUMP_Mid;
    if (args.dumpShort) {
      ty = binutils::LM::DUMP_Short;
    } 
    if (args.dumpLong) {
      ty = binutils::LM::DUMP_Long;
    }
    if (args.dumpDecode) {
      ty = (binutils::LM::DumpTy)(ty | binutils::LM::DUMP_Flg_Insn_decode);
    }
    
    lm->dump(std::cout, ty);

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
DumpHeaderInfo(std::ostream& os, binutils::LM* lm, const char* pre = "")
{
  os << "Begin LoadModule Stmt Dump\n";
  os << pre << "Name: `" << lm->GetName() << "'\n";
  os << pre << "Type: `";
  switch (lm->GetType()) {
    case binutils::LM::Executable:
      os << "Executable (fully linked except for possible DSOs)'\n";
      break;
    case binutils::LM::SharedLibrary:
      os << "Dynamically Shared Library'\n";
      break;
    default:
      DIAG_Die("Unknown LM type!"); 
  }
  os << pre << "ISA: `" << typeid(*binutils::LM::isa).name() << "'\n"; // std::type_info
}

//****************************************************************************

void DumpSymbolicInfoForFunc(std::ostream& os, const char* pre, 
			     const char* func, LineToVMAListMap* map, 
			     const char* file);

void 
DumpSymbolicInfoOld(std::ostream& os, binutils::LM* lm)
{
  string pre = "  ";
  string pre1 = pre + "  ";

  DumpHeaderInfo(os, lm, pre.c_str());

  // ------------------------------------------------------------------------
  // Iterate through the VMA values of the text section, collect
  //   symbolic information on a source line basis, and output the results
  // ------------------------------------------------------------------------  

  os << pre << "Dump:\n";
  for (binutils::LMSegIterator it(*lm); it.IsValid(); ++it) {
    binutils::Seg* sec = it.Current();
    if (sec->GetType() != binutils::Seg::Text) { continue; }
    
    // We have a 'TextSeg'.  Iterate over procedures.
    binutils::TextSeg* tsec = dynamic_cast<binutils::TextSeg*>(sec);
    for (binutils::TextSegProcIterator it(*tsec); it.IsValid(); ++it) {
      binutils::Proc* p = it.Current();
      string pName = GetBestFuncName(p->name());

      
      // We have a 'Procedure'.  Iterate over VMA values     
      string theFunc = pName, theFile;
      LineToVMAListMap map;

      for (binutils::ProcInsnIterator it(*p); it.IsValid(); ++it) {
	binutils::Insn* inst = it.Current();
	VMA vma = inst->GetVMA();
	VMA opVMA = binutils::LM::isa->ConvertVMAToOpVMA(vma, inst->GetOpIndex());
	
	// 1. Attempt to find symbolic information
	string func, file;
	SrcFile::ln line;
	p->GetSourceFileInfo(vma, inst->GetOpIndex(), func, file, line);
	func = GetBestFuncName(func);
	
	// Bad line number: cannot fix; advance iteration
	if ( !SrcFile::isValid(line) ) {
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
    SrcFile::ln line = (*it).first;
    VMAList* list = (*it).second;
    
    os << p1 << "  " << line << ": {" << hex;

    VMAListIt it1;
    for (it1 = list->begin(); it1 != list->end(); ++it1) {
      VMA vma = *it1;
      os << " 0x" << vma;
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
