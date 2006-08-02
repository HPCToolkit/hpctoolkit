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
//    PCToSrcLineMap.H
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef PCToSrcLineMap_H 
#define PCToSrcLineMap_H

//************************* System Include Files ****************************

#include <iostream>
#include <list>
#include <vector>
#include <map>

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/ISA/ISATypes.hpp>
#include <lib/support/String.hpp>

//*************************** Forward Declarations ***************************

class PCToSrcLineXMap;

PCToSrcLineXMap* ReadPCToSrcLineMap(const char* fnm);

//****************************************************************************

class SrcLineX;
class ProcPCToSrcLineXMap;

// 'PCToSrcLineXMap' defines a PC to 'SrcLineX' map for a load module.
// This map consists of a series of Procedure PCToSrcLineXMap's. See
// below for comments about a 'SrcLineX'.
class PCToSrcLineXMap
{
public:
  PCToSrcLineXMap(); 
  ~PCToSrcLineXMap();

  // 'startVMA' <= 'pc' <= 'endVMA'. 
  SrcLineX*            Find(VMA pc) const;
  ProcPCToSrcLineXMap* FindProc(VMA pc) const;
  ProcPCToSrcLineXMap* FindProc(const char* s) const;

  // starting and ending address of the first and last instruction in the map
  VMA                 GetStartVMA() const { return startVMA; }
  VMA                 GetEndVMA()   const { return endVMA; }

  // FIXME: Probably best to restrict access... 
  // Constructing: Insert one or more 'ProcPCToSrcLineXMap' using 
  // 'InsertProcInList' and then call 'Finalize'.  When done, all
  // 'ProcPCToSrcLineXMap' should together represent a set of pc values
  // with no gaps in between.
  void InsertProcInList(ProcPCToSrcLineXMap* m);
  void Finalize();
  bool IsFinalized() const { return ((procVec != NULL) && (tmpVec == NULL)); }
 
  bool Read(std::istream& is);
  bool Write(std::ostream& os) const;
  void Dump(std::ostream& o = std::cerr) const;
  void DDump() const; 

private:
  // Should not be used  
  PCToSrcLineXMap(const PCToSrcLineXMap& e) { }
  PCToSrcLineXMap& operator=(const PCToSrcLineXMap& e) { return *this; }

  ProcPCToSrcLineXMap* Find_BinarySearch(VMA pc, suint lb, suint ub) const;

  typedef std::vector<ProcPCToSrcLineXMap*> ProcPCToSrcLineXMapVec;

  typedef std::list<ProcPCToSrcLineXMap*> ProcPCToSrcLineXMapList;
  typedef std::list<ProcPCToSrcLineXMap*>::iterator ProcPCToSrcLineXMapIt;
  
protected:
private:
  VMA startVMA; // the address of the *start* of the first instruction
  VMA endVMA;   // the address of the *end* of the last instruction

  // 'ProcPCToSrcLineXMap' vector, sorted by starting address
  // 'list' is used for easy creation; later on, 'vector' makes 'Find'
  // (a common operation) quick.
  ProcPCToSrcLineXMapVec* procVec;
  ProcPCToSrcLineXMapList* tmpVec;
};

//****************************************************************************

// 'ProcPCToSrcLineXMap' 
class ProcPCToSrcLineXMap
{
public:
  ProcPCToSrcLineXMap(VMA start, VMA end, const char* proc = NULL,
		      const char* file = NULL);
  ~ProcPCToSrcLineXMap();

  // 'startVMA' <= 'pc' <= 'endVMA'.  'Insert' inserts only if no
  // object is already associated with 'pc'; assumes ownership of 's'.
  SrcLineX* Find(VMA pc) const {
    VMAToSrcLineXMapItC it = map.find(pc);
    if (it == map.end()) { return NULL; }
    else { return (*it).second; }
  }
  void Insert(VMA pc, SrcLineX* s) {
    map.insert(VMAToSrcLineXMapVal(pc, s));
  }
  
  const char* GetProcName()   const { return procName; }
  const char* GetFileName()   const { return fileName; }
  VMA        GetStartVMA()  const { return startVMA; }
  VMA        GetEndVMA()    const { return endVMA; }
  suint       GetNumEntries() const { return map.size(); }
  
  void SetProcName(const char* s) { procName = s; }
  void SetFileName(const char* s) { fileName = s; }
  void SetStartVMA(VMA a)   { startVMA = a; } 
  void SetEndVMA(VMA a)     { endVMA = a; }  
  
  bool Read(std::istream& is);
  bool Write(std::ostream& os) const;
  void Dump(std::ostream& o = std::cerr) const;
  void DDump() const; 

private:
  // Should not be used  
  ProcPCToSrcLineXMap(const ProcPCToSrcLineXMap& e) { }
  ProcPCToSrcLineXMap& operator=(const ProcPCToSrcLineXMap& e) {return *this;}

  // Restricted access
  friend bool PCToSrcLineXMap::Read(std::istream& is);
  
  ProcPCToSrcLineXMap();
  
  // Virtual memory address to 'SrcLineX*' map 
  typedef std::map<VMA, SrcLineX*, lt_VMA>           VMAToSrcLineXMap;
  typedef std::map<VMA, SrcLineX*, lt_VMA>::iterator VMAToSrcLineXMapIt;
  typedef std::map<VMA, SrcLineX*, lt_VMA>::const_iterator
    VMAToSrcLineXMapItC;  
  typedef std::map<VMA, SrcLineX*>::value_type        VMAToSrcLineXMapVal;
  
protected:
private:
  String procName;
  String fileName;
  VMA startVMA; // the address of the *start* of the first instruction
  VMA endVMA;   // the address of the *end* of the last instruction
  
  VMAToSrcLineXMap map; // PC to src line map.
};

//****************************************************************************

// 'SrcLineX': Stores a source file line number and an associated
// piece of information.  The extra piece of info could be, e.g., CFG
// indentifiers to indicate basic block relationships.
class SrcLineX
{
public:
  SrcLineX() : srcLine(0), srcLineX(0) { }
  SrcLineX(suint a, suint b) : srcLine(a), srcLineX(b) { }

  SrcLineX(const SrcLineX& e) { *this = e; }
  SrcLineX& operator=(const SrcLineX& s) {
    srcLine = s.srcLine; srcLineX = s.srcLineX;
    return *this;
  }

  suint GetSrcLine()  const { return srcLine; }
  suint GetSrcLineX() const { return srcLineX; }

  void SetSrcLine(suint l)  { srcLine = l; }
  void SetSrcLineX(suint l) { srcLineX = l; }

  bool Read(std::istream& is);
  bool Write(std::ostream& os) const;
  void Dump(std::ostream& o = std::cerr) const;
  void DDump() const; 

protected:
private:

private:
  suint srcLine, srcLineX;
};

#endif
