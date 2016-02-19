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

static int
realmain(int argc, char* const* argv);

// A list of addresses (VMAs)
typedef std::list<VMA> VMAList;
typedef std::list<VMA>::iterator VMAListIt;
typedef std::list<VMA>::const_iterator VMAListItC;

// A map of a line number to an VMAList
typedef std::map<SrcFile::ln, VMAList*> LineToVMAListMap;
typedef std::map<SrcFile::ln, VMAList*>::iterator LineToVMAListMapIt;
typedef std::map<SrcFile::ln, VMAList*>::value_type LineToVMAListMapItVal;

static void
clearLineToVMAListMap(LineToVMAListMap* map);

// Dump Helpers
static void
dumpSymbolicInfoOld(std::ostream& os, BinUtil::LM* lm);


//****************************************************************************

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


static int
realmain(int argc, char* const argv[])
{
  Args args(argc, argv);

  // ------------------------------------------------------------
  // Read load module
  // ------------------------------------------------------------
  BinUtil::LM* lm = NULL;
  try {
    lm = new BinUtil::LM();
    lm->open(args.inputFile.c_str());
    lm->read(BinUtil::LM::ReadFlg_ALL);
  }
  catch (...) {
    DIAG_EMsg("Exception encountered while reading '" << args.inputFile << "'");
    throw;
  }

  // ------------------------------------------------------------
  // Dump load module
  // ------------------------------------------------------------
  try {
    if (args.dumpOld) {
      dumpSymbolicInfoOld(std::cout, lm);
    }
    
    // Assume parser sanity checked arguments
    BinUtil::LM::DumpTy ty = BinUtil::LM::DUMP_Mid;
    if (args.dumpShort) {
      ty = BinUtil::LM::DUMP_Short;
    }
    if (args.dumpLong) {
      ty = BinUtil::LM::DUMP_Long;
    }
    if (args.dumpDecode) {
      ty = (BinUtil::LM::DumpTy)(ty | BinUtil::LM::DUMP_Flg_Insn_decode);
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

static void
dumpHeaderInfo(std::ostream& os, BinUtil::LM* lm, const char* pre = "")
{
  os << "Begin LoadModule Stmt Dump\n";
  os << pre << "Name: `" << lm->name() << "'\n";
  os << pre << "Type: `";
  switch (lm->type()) {
    case BinUtil::LM::TypeExe:
      os << "Executable (fully linked except for possible DSOs)'\n";
      break;
    case BinUtil::LM::TypeDSO:
      os << "Dynamically Shared Library'\n";
      break;
    default:
      DIAG_Die("Unknown LM type!");
  }
  os << pre << "ISA: `" << typeid(*BinUtil::LM::isa).name() << "'\n"; // std::type_info
}

//****************************************************************************

static void
dumpSymbolicInfoForFunc(std::ostream& os, const char* pre,
			const char* func, LineToVMAListMap* map,
			const char* file);

static void
dumpSymbolicInfoOld(std::ostream& os, BinUtil::LM* lm)
{
  string pre = "  ";
  string pre1 = pre + "  ";

  dumpHeaderInfo(os, lm, pre.c_str());

  // ------------------------------------------------------------------------
  // Iterate through the VMA values of the text section, collect
  //   symbolic information on a source line basis, and output the results
  // ------------------------------------------------------------------------

  os << pre << "Dump:\n";
  for (BinUtil::LM::ProcMap::iterator it = lm->procs().begin();
       it != lm->procs().end(); ++it) {
    BinUtil::Proc* p = it->second;
    string pName = BinUtil::canonicalizeProcName(p->name());

      
    // We have a 'Procedure'.  Iterate over VMA values
    string theFunc = pName, theFile;
    LineToVMAListMap map;

    for (BinUtil::ProcInsnIterator it1(*p); it1.isValid(); ++it1) {
      BinUtil::Insn* inst = it1.current();
      VMA vma = inst->vma();
      VMA opVMA = BinUtil::LM::isa->convertVMAToOpVMA(vma, inst->opIndex());
	
      // 1. Attempt to find symbolic information
      string func, file;
      SrcFile::ln line;
      p->findSrcCodeInfo(vma, inst->opIndex(), func, file, line);
      func = BinUtil::canonicalizeProcName(func);
	
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
	
      LineToVMAListMapIt it2 = map.find(line);
      VMAList* list;
      if (it2 != map.end()) {
	list = (*it2).second; // modify existing list
	list->push_back(opVMA);	
      }
      else {
	list = new VMAList; // create a new list and insert pair
	list->push_back(opVMA);
	map.insert(LineToVMAListMapItVal(line, list));
      }
    }
      
    dumpSymbolicInfoForFunc(os, pre1.c_str(),
			    theFunc.c_str(), &map, theFile.c_str());
    clearLineToVMAListMap(&map);
  }
  os << "\n" << "End LoadModule Stmt Dump\n";
}


static void
dumpSymbolicInfoForFunc(std::ostream& os, const char* pre,
			const char* func, LineToVMAListMap* map,
			const char* file)
{
  string p = pre;
  string p1 = p + "  ";

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


static void
clearLineToVMAListMap(LineToVMAListMap* map)
{
  LineToVMAListMapIt it;
  for (it = map->begin(); it != map->end(); ++it) {
    delete (*it).second;
  }
  map->clear();
}
