// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
// Written originally as template code by Johnny Chen, hacked horribly
// by Mark Anderson to be a Void Pointer Vector. Then returned to
// template status
// Some spiritual heritage from PointerVector class by John M-C
//
// Todo list: Add quicksort and iterator?
//
//***************************************************************************

#ifndef VectorTmpl_h
#define VectorTmpl_h

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "VectorSupport.hpp"

// The class T defined here must have the following members and
// functions:
// T() 		(constructor)
// T(T&) 	(copy constructor)
// ~T()		(destructor)
// operator = (T &)
// Comparison function int CompareFunction(T& a,T& b)
//  that performs a trinary compare and returns -1 a<b, 0 a==b, +1 a>b
// Printing function ostream& operator <<(ostream &, T&) for output
//
// *****************************
// ** NOTE THIS!!!!!!!!!!!    **
// *****************************
// When T is a builtin data type (double, for example), an empty
// element must be provided. Since arrays of a builtin can be
// allocated without a constructor present, it is possible to use
// uninitialized values!

// Autoextend behavior defaults on.

// This function works for any type T for which > < and == are defined
// and behave properly under trichotomy. Note: this means I assume
// that one and only one of a>b, a<b & a==b are true.  This property
// is not obeyed for IEEE arithmetic types, among others! (NAN !> 0,
// NAN !<0 , NAN != 0, huh!)
template <class T>
int CompareElement(T & a, T & b);


template <class  T> 
class VectorTmpl {
public:
  VectorTmpl (VectorIndex _size = 10)
    : size(0), numElements(0), vec(NULL), doExtend(true), 
      emptyElementSet(false) {
    size = ChooseNextSize(_size);
    vec = new T[size];
    // Assertion( (vec != NULL),
    //   "can't allocate memory in Constructor\n",ASSERT_ABORT);
  }
  
  VectorTmpl (const T & _emptyElement, bool autoExtend, 
	      VectorIndex _size = 10)
    : size(0), numElements(0), vec(NULL), doExtend(autoExtend), 
      emptyElementSet(true) {
    emptyElement =_emptyElement;
    size = ChooseNextSize(_size);
    vec = new T[size];
    // Assertion( (vec != NULL),
    //   "can't allocate memory in Constructor\n",ASSERT_ABORT);
  }
  
  VectorTmpl (const VectorTmpl  & v) {
    emptyElementSet = v.emptyElementSet;
    if (emptyElementSet) { 
      emptyElement = v.emptyElement;  
    }
    doExtend = v.doExtend;
    
    numElements = v.numElements;
    size = 0;				  // VSA, 8/8/95: must be initialized
    size = ChooseNextSize(v.numElements); //  before calling ChooseNextSize
    
    vec = new T[size];
    
    for(VectorIndex i=0;i<v.numElements;i++) {
      vec[i] = v.vec[i];
    }
  }
  
  virtual ~VectorTmpl () {
    if(vec) {
      delete[] vec; // the way Kevin wants it
    }
    // make it possible to id a destroyed vector:
    // These are purely arbitrary values used for recognition purposes
    vec         = 0;
    numElements = 0xDEADBEEF;
    size        = 47;
  }
  
  // NumElements is the largest valid index+1
  VectorIndex  	GetNumElements() const { return numElements; }
  
  // If we exceed the allocated size, we reallocate. If we shrink, we
  // destroy affected elements.
  void 		SetNumElements(const VectorIndex i) {
#ifdef REALLY_PARANOID_DEBUGGING
    CheckInvariants();
#endif
    if (i> numElements) {
      GrowValidZone(i);
    } else if (i < numElements) {
      // Shrink case: need to kill off unwanted elements
      ShrinkValidZone(i);
    } // 
#ifdef REALLY_PARANOID_DEBUGGING
    CheckInvariants();
#endif
  }
  
  void 		Clear(); // Just SetNumElements(0)
  
  // Element, operator[]: Set arbitrary elements of the vector:
  // limited to already expanded elements if doExtend false.
  
  // If we exceed the allocated size we realloc iff Extend is set
  T   		Element(VectorIndex position) const {
    if (position >= numElements) {
      // If we have extend set then go ahead, otherwise error
      //Assertion(doExtend,"Out of Bounds reference to array",ASSERT_ABORT);
      return (T&)emptyElement;
    } else {
      return vec[position]; //returning the correct reference vector value
    }
  }

  
  T & 		operator[](VectorIndex position) {
    if (position >= numElements) {
      // If we have extend set then go ahead, otherwise error
      //Assertion(doExtend,"Out of Bounds reference to array",ASSERT_ABORT);
      GrowValidZone(position+1); // position starts at zerO, so
      // position+1 is number of elements
    }
    return vec[position]; //returning the correct reference vector value
  }
  
  // Append: This function appends the vector and resizes the
  // vector if necessary and returns the index of the element being
  // appended
  VectorIndex 	Append(T & newEle) {
    if(numElements >= size)   
      InternalResizeVector(numElements+1);
    vec[numElements++] = newEle;  
    return(numElements-1);    
  }
  
  // Shrinks a bloated vector. No op if not bloated.
  void 		ShrinkToFit();
  
  int operator==(VectorTmpl<T>  & tmp);
  // Uses Copy Constructor of T 
  VectorTmpl  & operator=(const VectorTmpl<T>  & tmp);
  
  // IO routines
  // cout << *this
#if 0
  virtual void Dump();
  friend ostream & operator<<(ostream & o, const VectorTmpl<T>& v);
#endif
  
  // Default is to generate error on out of bounds [] reference
  // If setExtend true, array is grown.
  // When array is grown, nextindex is set to 1+ the index that triggered it.
  void    SetExtend(bool extend) { doExtend = extend; }
  bool GetExtend() { return doExtend; }
  
  const T * GetArray() const ;
  
  void MkArraySegment(VectorIndex start, VectorIndex end,
		      VectorTmpl<T> store);
  
  /////////////////////////////////////////////////////////////////////////
  // Allow for overide of function actions
  // This deletes the old actions  
  void SetEmptyElement(const T & EmptyElement);
protected:
  int CompareElement(T & a, T & b);
private:
  
  // This function is probably the single most essential thing to
  // produce good performance.  Choose wisely, or things will suck.  This
  // function always needs to return a value >= lowerBound!
  VectorIndex ChooseNextSize(VectorIndex lowerBound) {
    if (lowerBound < size) {
      // downsizing vector, do exactly
      return lowerBound;
    } else {
      VectorIndex trialSize;
      //    Assertion(size>=0,"Bogus size",ASSERT_ABORT);
      if (size == 0) {
	trialSize = 1;
      } else {
	trialSize = size;
      }
      while (trialSize < lowerBound) {
	trialSize <<= 1;
      }
      return trialSize;
    }
    //    return ::ChooseNextSize(lowerBound,size);
  }
  
  VectorIndex size;        // The real allocated size of the vector
  VectorIndex numElements; // The maximum valid (i.e. used) element
  
  T *  vec;     // The vector
  bool doExtend;   // Do we resize when we exceed the bounds?
  
  // ShrinkValidZone: Shrinks vector to newsz.
  void ShrinkValidZone(VectorIndex newsz) {
    if (newsz==numElements) return; // we are done.
    //Assertion(newsz < numElements, "Improper shrink",ASSERT_ABORT);
    // We need to throw away elements:
    //for (VectorIndex i = newsz; i<numElements; i++) {
    //  DestroyElement(vec[i]);
    //}
    numElements = newsz;
  }
  
  void GrowValidZone(VectorIndex newsz) {
    //  Assertion(newsz > numElements, "Improper grow",ASSERT_ABORT);
    if (newsz >= size) {
      // Need to reallocate:
      InternalResizeVector(newsz);
    }
    
    // init new elements if we have a valid emptyElement value
    if (emptyElementSet == true) {
      for (VectorIndex i = numElements; i< newsz; i++) {
	vec[i] = emptyElement;
      }
    }
    numElements = newsz;
  }
  
  // InternalResizeVector: Resize the vector. If resize is smaller, calls 
  // DestroyElement for removed items. Size is just the minimum required
  // size. Shrink changes numElements, grow leaves unchanged
  void InternalResizeVector(VectorIndex _size) {
    VectorIndex i;    // Misc index variable.
    
    // Check to see if we are shrinking things below the valid zone
    if (_size < numElements) {
      // Clean up left-overs
      ShrinkValidZone(_size);
    }
    
    // Get new array size, and alloc:
    VectorIndex newSize = ChooseNextSize(_size);
    if (newSize == size) {
      // No point in doing anything if there is no change.
      return; //=========== Early Exit =======================
    }
    T * newVec = new T[newSize];
    //Assertion( (newVec != NULL),
    //  "can't allocate memory in VectorTmpl internal routine\n",ASSERT_ABORT);
    
    // Copy things over:
    // (Note numElements <= _size <= newSize;
    //       numElements <= size
    // )
    for (i = 0; i < numElements; i++) {
      newVec[i] = vec[i];
    }
    // Kill things off:
    delete[] vec;
    vec = newVec;
    size = newSize;
  }
  
  void CheckInvariants(); // For debugging- do we satisfy the invariants?
  
  bool emptyElementSet;
  T    emptyElement;
  static const char * RCS_ID;
};


//////////////////////////////////////////////////////////
// Usage
// VectorTmplIter<T> iter(vec)
// for (T i = iter.Current; iter.Valid(); iter++);


template <class T> 
class VectorTmplIter {
public:
  VectorTmplIter(VectorTmpl<T> & vec, bool firstToLast = true);
  virtual ~VectorTmplIter();
  
  virtual T& Current() const;
  virtual void ReplaceCurrent(T & newCurrent);
  
  // Synonomous
  virtual void Advance();
  void operator++();
  
  virtual bool Valid();
  
  virtual void Reset();    
private:
  VectorTmpl<T> & theVec;
  int pos;
  bool forward;
};


template <class T> 
std::ostream & operator << (std::ostream & o, const VectorTmpl<T> & v);

#endif /* VectorTmpl_h */



//Memo to self: was working on shrink case of SetNumElements. Generate
//Shrink function and replace functionality throughout code.

