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
// CodeInfoPtrSet.H
//
//   A template class to store and manipulate a set of CodeInfo ptrs
//                                                                          
//***************************************************************************

#ifndef CodeInfoPtrSet_H
#define CodeInfoPtrSet_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "PgmScopeTree.h"
#include <lib/support/PtrSet.h>
#include <lib/support/PtrSetIterator.h>
#include <lib/support/VectorTmpl.h>

//*************************** Forward Declarations ***************************

class CodeInfoPtrSetIterator;

//-------------------------------------------------------------
//  class CodeInfoPtrSet
//-------------------------------------------------------------
class CodeInfoPtrSet : public PtrSet<CodeInfo*> {
public:
  // constructors
  CodeInfoPtrSet();
  CodeInfoPtrSet(const CodeInfoPtrSet& rhs);
  
  // destructors
  ~CodeInfoPtrSet(); 
  
  // Adds 'item' to set without testing for overlapping.
  void Add(const CodeInfo*);  

  // Adds 'item' to set or Merges it with an overlapping element.  In
  // the latter case, 'item' is removed from the tree *and* deleted;
  // and false is returned.
  bool AddOrMerge(CodeInfo* item);

protected:  
  void MergeOverlappingItem(CodeInfo* newItem, CodeInfo* oldItem);
  int FindOverlappingItems(const CodeInfo* item, VectorTmpl<CodeInfo*>& overlapped);
//  CodeInfo* FindOverlappingItem(const CodeInfo* item, bool& fullOverlap);
  
private:
  int GetEntryIndex(const CodeInfo*);
  CodeInfo* GetEntryByIndex(unsigned int indx);
  
//-------------------------------------------------------------
// friend declaration required so HashTableIterator can be
// used with the private base class
//-------------------------------------------------------------
  friend class CodeInfoPtrSetIterator;
};

//-------------------------------------------------------------
// template<class T> class PtrSetIterator
//-------------------------------------------------------------
class CodeInfoPtrSetIterator : public PtrSetIterator<CodeInfo*> {
public:
  CodeInfoPtrSetIterator(const CodeInfoPtrSet *theTable);

};

CodeInfoPtrSet* MakeSetFromChildren(ScopeInfo* parent);

#endif /* CodeInfoPtrSet_H */
