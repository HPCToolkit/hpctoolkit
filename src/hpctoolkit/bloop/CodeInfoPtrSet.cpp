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
// CodeInfoPtrSet: 
// 
//   A template class to store and manipulate a set of CodeInfo ptrs
//                                                                          
// PtrSetIterator: 
//
//   An iterator for enumerating elements in a CodeInfoPtrSetPtrSet
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "CodeInfoPtrSet.h"
#include "PgmScopeTree.h"
#include "PgmScopeTreeUtils.h"
#include <lib/support/Assertion.h>

#include <lib/binutils/BinUtils.h>

//*************************** Forward Declarations ***************************

//#define DEBUG_CodeInfoPtrSet

//**********************************************************************
// implementation of class CodeInfoPtrSet
//**********************************************************************

CodeInfoPtrSet::CodeInfoPtrSet()
  : PtrSet<CodeInfo*>()
{
}

CodeInfoPtrSet::CodeInfoPtrSet(const CodeInfoPtrSet& rhs)
  : PtrSet<CodeInfo*>(rhs)
{
}

CodeInfoPtrSet::~CodeInfoPtrSet()
{
}

// Adds 'item' to set without testing for overlapping.
void CodeInfoPtrSet::Add(const CodeInfo* item)
{
  PtrSet<CodeInfo*>::Add(const_cast<CodeInfo*>(item)); 
}


typedef VectorTmpl<CodeInfo*> CodePtrVector;

// Adds 'item' to set or Merges it with an overlapping element.  In
// the latter case, 'item' is removed from the tree *and* deleted;
// and false is returned.
bool CodeInfoPtrSet::AddOrMerge(CodeInfo* item)
{
  if (IsMember(item)) {
    return true; // will be merged with itself!
  }

  CodePtrVector overlapped;

  int numItems = FindOverlappingItems(item, overlapped);

#if (0 && (defined DEBUG_CodeInfoPtrSet))
  if (numItems > 1) {
    // There can be multiple overlaps only when the loops are
    // relatively dissimilar. For structures representing the same
    // code, this is 'abnormal' but can happen with, e.g., loop
    // unrolling.
    cerr << "CodeInfoPtrSet::AddOrMerge: overlapping items: "
	 << numItems << endl;
    cerr << "-->NEW ITEM [start,end]=[" << item->BegLine() << ","
	 << item->EndLine() << "]" << endl;
    for (int i=0; i < overlapped.GetNumElements(); i++) {
      cerr << "-->OVERLAP(" << i << ")" << endl;
      overlapped[i]->DumpLineSorted();
    }
    cerr.flush();
  }
#endif
  
  if (numItems) {
    overlapped.Append(item); // Add to *end* for convenient processing

    // Sometimes there is partial overlapping...
    // set 'overlapItem' to contain "widest" endpoints.  
    unsigned int startLine = overlapped[0]->BegLine();
    unsigned int endLine = overlapped[0]->EndLine();
    unsigned int i;
    for (i=1; i < overlapped.GetNumElements() ; i++) {
      startLine = MIN(startLine, overlapped[i]->BegLine());
      endLine = MAX(endLine, overlapped[i]->EndLine());
    }
    overlapped[0]->SetLineRange(startLine, endLine);

    // Merge overlapping elements.  overlapped[0] contains result.
    for (i=1; i < overlapped.GetNumElements(); i++) 
      MergeOverlappingItem(overlapped[i], overlapped[0]);

    // Remove the merged elements from *this* set.
    for (i=1; i < overlapped.GetNumElements()-1; i++ ) 
      PtrSet<CodeInfo*>::Delete(overlapped[i]);

    return false;
  } else {
    PtrSet<CodeInfo*>::Add(item);
    return true;
  }
}

// Union, Intersection, and Difference do not merge !!!

//***************************************************************************

// The 'set' should have no overlapping elements (except possibly a
// to-be-added 'item') and no element should partially overlap.

// Merges non-member 'newItem' with set member 'oldItem'.  All the
// children of 'newItem' are tranferred to 'oldItem' if they do not
// overlap (assume neither 'newItem's' children nor 'oldItem's'
// children initially overlap w.r.t. their own set). 'newItem' is removed
// from the tree and deleted.
void CodeInfoPtrSet::MergeOverlappingItem(CodeInfo* newItem, CodeInfo* oldItem)
{
  CodeInfoPtrSet* oldChildSet = MakeSetFromChildren(oldItem);

  // Create a set with all the children of newItem and iterate over this in
  // order not to be affected by a modification in the tree structure
  CodeInfoPtrSet* newChildSet = MakeSetFromChildren(newItem);

  CodeInfo *newChild = NULL;
  for (CodeInfoPtrSetIterator iter(newChildSet);
       (newChild = iter.Current()); iter++) {
    BriefAssertion(newChild);
    CodePtrVector overlapped;
    
    int numItems = oldChildSet->FindOverlappingItems(newChild, overlapped);
      
#if (1 && (defined DEBUG_CodeInfoPtrSet))
      if (numItems > 1) {
	// There can be multiple overlaps only when the children are
	// relatively dissimilar.  For structures representing the same
	// code, this is 'abnormal' but can happen with, e.g., loop
	// unrolling.
	cerr << "CodeInfoPtrSet::MergeOverlappingItem: overlapping items: "
	     << numItems << endl;
	cerr << "-->NEW ITEM:" << endl;
	newItem->DumpLineSorted();
	cerr << "-->OLD ITEM:" << endl;
	oldItem->DumpLineSorted();
	for (int i=0; i < overlapped.GetNumElements(); i++) {
	  cerr << "-->OVERLAP(" << i << ")" << endl;
	  overlapped[i]->DumpLineSorted();
	}
	cerr.flush();
      }
#endif 

    if (numItems == 0) {
      // One of the overlapping items contains a child that the other
      // does not have.  (A little abnormal)
      newChild->Unlink();      // no longer a child of 'newItem'
      newChild->Link(oldItem); // now a child of 'oldItem'
    } else { // (numItems >= 1)
      // could be one or more. If there are more than one overlapping
      // children, apply the same algorithm as in AddOrMerge (see above)

      overlapped.Append(newChild); // Add to *end* for convenient processing

      // Sometimes there is partial overlapping...
      // set 'overlapItem' to contain "widest" endpoints.  
      unsigned int startLine = overlapped[0]->BegLine();
      unsigned int endLine = overlapped[0]->EndLine();
      unsigned int i;
      for (i=1; i < overlapped.GetNumElements() ; i++) {
	startLine = MIN(startLine, overlapped[i]->BegLine());
	endLine = MAX(endLine, overlapped[i]->EndLine());
      }
      overlapped[0]->SetLineRange(startLine, endLine);

      // Merge overlapping elements.  overlapped[0] contains result.
      for (i=1; i < overlapped.GetNumElements(); i++) 
	MergeOverlappingItem(overlapped[i], overlapped[0]);
      
      // Remove the merged elements from *this* set.
      for (i=1; i < overlapped.GetNumElements()-1; i++ ) 
	oldChildSet->Delete(overlapped[i]);
    }
  }
  delete oldChildSet;
  delete newChildSet;
  newItem->Unlink();  // remove newItem from tree
  delete newItem;
}


// Determines whether 'item' overlaps with any member of the 'set'; 
// adds all such items to 'overlapped' and returns this number.
// An item overlaps if the scopes intersect in some way
int CodeInfoPtrSet::FindOverlappingItems(const CodeInfo* item,
					 CodePtrVector& overlapped)
{
  CodeInfo* i;
  int num = 0;
  for (CodeInfoPtrSetIterator iter(this); (i = iter.Current()); iter++) {

    // This comparison is only defined for the following types
    ScopeInfo::ScopeType itemType = item->Type(), iType = i->Type();
    bool validTypeComparison = 
      (itemType == iType) &&
      (itemType == ScopeInfo::LOOP || itemType == ScopeInfo::GROUP ||
       itemType == ScopeInfo::STMT_RANGE);

    bool overlap = validTypeComparison &&
      IsValidLine(i->BegLine(), i->EndLine()) && 
      ((item->BegLine() < i->EndLine() && item->EndLine() > i->BegLine())
       || item->BegLine() == i->BegLine()
       || item->EndLine() == i->EndLine());
    
    if (overlap) {
      overlapped.Append(i);
      num++; // found an overlapping element
    }
  }
  return num;
}

#if 0
// Determines whether 'item' overlaps with any member of the 'set'.  If
// so, return a pointer to that item (can be at most one); NULL otherwise.
// An item overlaps if one or both end points are the same.
// Assume no need to test for inclusion -- just end point overlap.
CodeInfo* CodeInfoPtrSet::FindOverlappingItem(const CodeInfo* item,
					      bool& fullOverlap)
{
  fullOverlap = false; // only valid if an overlap is found!

  CodeInfo* i;
  for (CodeInfoPtrSetIterator iter(this); i = iter.Current(); iter++) {
    if (IsValidLine(i->BegLine(), i->EndLine()) && 
	(item->BegLine() == i->BegLine() ||
	 item->EndLine() == i->EndLine())) {
      
      if (item->BegLine() == i->BegLine() &&
	  item->EndLine() == i->EndLine()) {
	fullOverlap = true; // full overlap: both endpoints agree
      }
      // otherwise a partial overlap exists; fullOverlap = false
      
      return i; // found an overlapping element
    }
  }
  return NULL;
}
#endif


// Create a set of the children of 'parent' and do no merging.
// Children must be CodeInfo's.
CodeInfoPtrSet* MakeSetFromChildren(ScopeInfo* parent)
{
  CodeInfoPtrSet* set = new CodeInfoPtrSet();
  for (ScopeInfoChildIterator it(parent); it.Current(); it++) {
    CodeInfo* child = dynamic_cast<CodeInfo*>(it.Current()); // always true
    BriefAssertion(child);
    set->Add(child);
  }
  return set;
}

//***************************************************************************

//**********************************************************************
// implementation of template<class T> class PtrSetIterator
//**********************************************************************

CodeInfoPtrSetIterator::CodeInfoPtrSetIterator(const CodeInfoPtrSet *theTable)
  : PtrSetIterator<CodeInfo*>(theTable)
{
}

