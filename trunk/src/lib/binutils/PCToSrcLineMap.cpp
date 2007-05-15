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
//    PCToSrcLineMap.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <fstream>

#include <iostream>
using std::endl;
using std::hex;
using std::dec;

//*************************** User Include Files ****************************

#include "PCToSrcLineMap.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/IOUtil.hpp>

#include <lib/xml/xml.hpp>
using namespace xml;

//*************************** Forward Declarations ***************************

// Data Reading/Writing Elements.  Index using 'XMLElementI'
const char* MapEle[]   = { "PCToSrcLineXMap", "sz"};           // ATT1
const char* ProcEle[]  = { "PROC", "n", "fn", "b", "e", "sz"}; // ATT5
const char* EntryEle[] = { "ENTRY", "pc", "l", "lx"};          // ATT3

//****************************************************************************

// ReadPCToSrcLineMap: Builds a PCToSrcLineXMap from info from the
// file pointed to by 'fnm'.  Returns NULL upon error.
PCToSrcLineXMap* 
ReadPCToSrcLineMap(const char* fnm)
{
  std::ifstream ifile(fnm);
  if ( !ifile.is_open() || ifile.fail() ) {
    return NULL;
  }

  PCToSrcLineXMap* map = new PCToSrcLineXMap();
  bool status = map->Read(ifile);

  if (!status) {
    delete map;
    return NULL;
  } else {
    return map;
  }
}

//****************************************************************************
// PCToSrcLineXMap
//****************************************************************************

PCToSrcLineXMap::PCToSrcLineXMap()
  : startVMA(0), endVMA(0), procVec(NULL), tmpVec(NULL)
{
  tmpVec = new ProcPCToSrcLineXMapList;
}

PCToSrcLineXMap::~PCToSrcLineXMap()
{
  DIAG_Assert(IsFinalized(), "");
  for (suint i = 0; i < procVec->size(); i++) {
    delete (*procVec)[i];
  }
  delete procVec;
}

SrcLineX* PCToSrcLineXMap::Find(VMA pc) const
{
  DIAG_Assert(IsFinalized(), "");
  ProcPCToSrcLineXMap* map = FindProc(pc);
  if (map) { return map->Find(pc); }
  else { return NULL; }
}

ProcPCToSrcLineXMap* PCToSrcLineXMap::FindProc(VMA pc) const
{
  DIAG_Assert(IsFinalized(), "");
  // FIXME: warning: can be overlapping entries (see below)

  if (procVec->size() == 0) { return NULL; }
  
  // Special case: pc >= vec[end].GetStartVMA()
  // General case: vec[n].GetStartVMA() <= pc < vec[n+1].GetStartVMA()
  suint procVecEnd = procVec->size() - 1; // guaranteed to be at least 0
  if ( pc >= (*procVec)[procVecEnd]->GetStartVMA() ) {
    return (*procVec)[procVecEnd];
  } else {
    return Find_BinarySearch(pc, 0, procVecEnd);
  }
}

ProcPCToSrcLineXMap* PCToSrcLineXMap::Find_BinarySearch(VMA pc, suint lb,
							suint ub) const
{
  // General case: vec[n].GetStartVMA() <= pc < vec[n+1].GetStartVMA()
  // Because of this, 'lb' should never equal 'ub'. 
  DIAG_Assert((ub - lb) >= 1, ""); 

  // rounds down; 'lb' may equal 'mid' but [('ub' - 'mid') >= 1]
  suint mid = (lb + ub)/2;

  if ( ((*procVec)[mid]->GetStartVMA() <= pc) &&
       (pc < (*procVec)[mid+1]->GetStartVMA()) ) {
    return (*procVec)[mid];  // contains 'pc'
  } else if (pc < (*procVec)[mid]->GetStartVMA()) {
    return Find_BinarySearch(pc, lb, mid);
  } else {
    return Find_BinarySearch(pc, mid+1, ub);
  }
}

ProcPCToSrcLineXMap* PCToSrcLineXMap::FindProc(const char* s) const
{
  // 'FindProc' is not expected to be common; otherwise an index by
  // proc name (i.e. a hash table) might be worthwhile.

  // FIXME: Sometimes there are duplicate procs, leading to duplicate
  // names!  See below.
  DIAG_Assert(IsFinalized(), "");
  for (suint i = 0; i < procVec->size(); i++) {
    ProcPCToSrcLineXMap* map = (*procVec)[i];
    if (map->GetProcName() == s) {
      return map;
    }
  }

  return NULL;
}

void PCToSrcLineXMap::InsertProcInList(ProcPCToSrcLineXMap* m)
{
  // Insert in sorted order, assuming the list is already in sorted order

  // FIXME: Sometimes (e.g. on alpha) a one-instruction dummy
  // procedure exists for each procedure -- and they share the same
  // start address.  To handle this, for now we compare using
  // less-than-or-equal.
  DIAG_Assert(!IsFinalized(), "");
  ProcPCToSrcLineXMapIt it = tmpVec->begin();
  VMA sVMA = m->GetStartVMA();

  if (it == tmpVec->end()) {
    // Special case: empty list
    tmpVec->push_front(m);
  } else if (sVMA <= tmpVec->front()->GetStartVMA()) {
    // Special case: insert at front of list
    tmpVec->push_front(m);
  } else if (tmpVec->back()->GetStartVMA() <= sVMA) {
    // Special case: insert at end of list
    tmpVec->push_back(m);
  } else {
    // General case: insert between two elements
    for ( ; it != tmpVec->end(); /*++it*/) {
      ProcPCToSrcLineXMap* a = *it;
      ProcPCToSrcLineXMap* b = *(++it);
      if (a->GetStartVMA() <= sVMA && sVMA <= b->GetStartVMA()) {
	tmpVec->insert(--it, m);
	break;
      }
    }
  }
} 

void PCToSrcLineXMap::Finalize()
{
  DIAG_Assert(!IsFinalized(), "");
  procVec = new ProcPCToSrcLineXMapVec(tmpVec->size());

  for (int i = 0; !tmpVec->empty(); i++) {
    ProcPCToSrcLineXMap* pmap = tmpVec->front();
    (*procVec)[i] = pmap;    
    tmpVec->pop_front();
  }
  
  delete tmpVec;
  tmpVec = NULL;

  startVMA = endVMA = 0;
  if (procVec->size() > 0) {
    startVMA = (*procVec)[0]->GetStartVMA();
    endVMA = (*procVec)[procVec->size()-1]->GetStartVMA();
  }
}


bool PCToSrcLineXMap::Read(std::istream& is)
{
  DIAG_Assert(!IsFinalized(), "");

  bool STATE = true; // false indicates an error
  suint numProc;     
  is >> std::ws;
  STATE &= IOUtil::Skip(is, eleB);          is >> std::ws;
  STATE &= IOUtil::Skip(is, MapEle[TOKEN]); is >> std::ws;
  STATE &= IOUtil::Skip(is, MapEle[ATT1]) && ReadAttrNum(is, numProc);
  STATE &= IOUtil::Skip(is, eleE);          is >> std::ws;
  if (!STATE) { goto PCToSrcLineXMap_Read_fini; }
  
  for (suint i = 0; i < numProc; i++) {
    ProcPCToSrcLineXMap* pmap = new ProcPCToSrcLineXMap();
    STATE &= pmap->Read(is);
    if (!STATE) { delete pmap; goto PCToSrcLineXMap_Read_fini; }
    InsertProcInList(pmap);
  }
  
  is >> std::ws;
  STATE &= IOUtil::Skip(is, eleBf);         is >> std::ws; 
  STATE &= IOUtil::Skip(is, MapEle[TOKEN]); is >> std::ws;
  STATE &= IOUtil::Skip(is, eleE);          is >> std::ws;
  
 PCToSrcLineXMap_Read_fini:
  Finalize();
  return STATE;
}

bool PCToSrcLineXMap::Write(std::ostream& os) const
{
  if (IsFinalized()) {
    os << eleB << MapEle[TOKEN]
       << SPC << MapEle[ATT1]; WriteAttrNum(os, procVec->size());
    os << eleE << endl;
    for (suint i = 0; i < procVec->size(); i++) {
      (*procVec)[i]->Write(os);
    }
    os << eleBf << MapEle[TOKEN] << eleE << endl;
  }
  return (!os.fail());
}

void PCToSrcLineXMap::Dump(std::ostream& o) const
{
  Write(o);
}

void PCToSrcLineXMap::DDump() const
{
  Dump(std::cerr);
}

//****************************************************************************
// ProcPCToSrcLineXMap
//****************************************************************************

ProcPCToSrcLineXMap::ProcPCToSrcLineXMap(VMA start, VMA end, 
					 const char* proc, const char* file)
  : procName(proc), fileName(file), startVMA(start), endVMA(end)
{
}

ProcPCToSrcLineXMap::ProcPCToSrcLineXMap()
  : procName(""), fileName(""), startVMA(0), endVMA(0)
{
}

ProcPCToSrcLineXMap::~ProcPCToSrcLineXMap()
{
  VMAToSrcLineXMapItC it;
  for (it = map.begin(); it != map.end(); ++it) {
    delete (*it).second;
  }
  map.clear();
}

bool ProcPCToSrcLineXMap::Read(std::istream& is)
{
  bool STATE = true; // false indicates an error
  suint _numEntries = 0;
  is >> std::ws;
  STATE &= IOUtil::Skip(is, eleB);             is >> std::ws;
  STATE &= IOUtil::Skip(is, ProcEle[TOKEN]);   is >> std::ws;
  STATE &= IOUtil::Skip(is, ProcEle[ATT1]) && ReadAttrStr(is, procName);
  STATE &= IOUtil::Skip(is, ProcEle[ATT2]) && ReadAttrStr(is, fileName);
  STATE &= IOUtil::Skip(is, ProcEle[ATT3]);    is >> hex;
  STATE &= ReadAttrNum(is, startVMA); is >> dec;
  STATE &= IOUtil::Skip(is, ProcEle[ATT4]);    is >> hex;
  STATE &= ReadAttrNum(is, endVMA);   is >> dec;
  STATE &= IOUtil::Skip(is, ProcEle[ATT5]) && ReadAttrNum(is, _numEntries);
  STATE &= IOUtil::Skip(is, eleE);
  if (!STATE) { return false; }

  VMA pc;
  for (suint i = 0; i < _numEntries; i++) {
    is >> std::ws;
    STATE &= IOUtil::Skip(is, eleB);            is >> std::ws;
    STATE &= IOUtil::Skip(is, EntryEle[TOKEN]); is >> std::ws;
    STATE &= IOUtil::Skip(is, EntryEle[ATT1]);  is >> hex;
    STATE &= ReadAttrNum(is, pc);       is >> dec;

    SrcLineX* lineInstance = new SrcLineX();
    STATE &= lineInstance->Read(is);
    STATE &= IOUtil::Skip(is, eleEc);
    
    if (!STATE) { delete lineInstance; return false; }
    Insert(pc, lineInstance);
  }

  is >> std::ws;
  STATE &= IOUtil::Skip(is, eleBf);          is >> std::ws;
  STATE &= IOUtil::Skip(is, ProcEle[TOKEN]); is >> std::ws;
  STATE &= IOUtil::Skip(is, eleE);           is >> std::ws;

  return STATE;
}

bool ProcPCToSrcLineXMap::Write(std::ostream& os) const
{
  os << eleB << ProcEle[TOKEN];
  os << SPC << ProcEle[ATT1]; WriteAttrStr(os, procName);
  os << SPC << ProcEle[ATT2]; WriteAttrStr(os, fileName);
  os << SPC << ProcEle[ATT3] << hex; WriteAttrNum(os, startVMA); os << dec;
  os << SPC << ProcEle[ATT4] << hex; WriteAttrNum(os, endVMA); os << dec;
  os << SPC << ProcEle[ATT5]; WriteAttrNum(os, GetNumEntries()); 
  os << eleE << endl;
  
  VMAToSrcLineXMapItC it;
  for (it = map.begin(); it != map.end(); ++it) {
    VMA vma     = (*it).first;
    SrcLineX* s = (*it).second;
    
    os << eleB << EntryEle[TOKEN];
    os << SPC << EntryEle[ATT1] << hex; WriteAttrNum(os, vma); os << dec;
    s->Write(os); 
    os << eleEc << endl;
  }

  os << eleBf << ProcEle[TOKEN] << eleE << endl;

  return (!os.fail());
}

void ProcPCToSrcLineXMap::Dump(std::ostream& o) const
{
  Write(o);
}

void ProcPCToSrcLineXMap::DDump() const
{
  Dump(std::cerr);
}

//****************************************************************************
// SrcLineX
//****************************************************************************

bool SrcLineX::Read(std::istream& is)
{
  bool STATE = true; // false indicates an error
  is >> std::ws;
  STATE &= IOUtil::Skip(is, EntryEle[ATT2]) && ReadAttrNum(is, srcLine);
  STATE &= IOUtil::Skip(is, EntryEle[ATT3]) && ReadAttrNum(is, srcLineX);
  return STATE;
}

bool SrcLineX::Write(std::ostream& os) const
{
  os << SPC << EntryEle[ATT2]; WriteAttrNum(os, srcLine);
  os << SPC << EntryEle[ATT3]; WriteAttrNum(os, srcLineX);
  return (!os.fail());
}

void SrcLineX::Dump(std::ostream& o) const
{
  Write(o);
}

void SrcLineX::DDump() const
{
  Dump(std::cerr);
}
