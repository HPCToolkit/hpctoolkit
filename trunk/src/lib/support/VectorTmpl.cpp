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

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "VectorTmpl.hpp"

//*************************** Forward Declarations ***************************

// Can't define this in template class- otherewise gets defined twice
#ifndef ASSERTION_FILE_VERSION
#define ASSERTION_FILE_VERSION RCS_ID
#endif
#define ASSERTIONS_ON

// This is really only intended for the most lunatic of debugging
//#define REALLY_PARANOID_DEBUGGING

//***************************************************************************

//Note I assume a>b, a<b & a==b are mutually exclusive!!!
// This property is not obeyed for IEEE arithmetic types, among
// others! (NAN !> 0, NAN !<0 , NAN != 0, huh!)

template <class T>
const char * VectorTmpl<T>::RCS_ID = "$Id$";

template <class T>
int CompareElement(T & a, T & b) {
  if (a<b) {
    return -1;
  } else if (a>b) {
    return 1;
  } else {
    return 0;
  }
}

template<class T>
int VectorTmpl<T>::CompareElement(T & a, T & b) {
  return ::CompareElement(a,b);
}

//================================================================
// Internal class functions
//
//================================================================

//////////////////////////////////////////////////////////////////
// Invariants:
// numElements <= size;
// i >= numElements, v[i] not a pointer to a valid object
// i >= size, v[i] does not point into allocated memory!
template<class T>
void VectorTmpl<T>::CheckInvariants() {
//  Assertion(numElements <= size,
//     "Invariant false: Internal Error in class VectorTmpl",
//     ASSERT_NOTIFY);
#ifdef REALLY_PARANOID_DEBUGGING
  // This should cause a spasm of purify errors (especially ABR)
  // UMR should not be a problem: fixed here
  if (numElements < size) { 
    vec[size-1] = emptyElement;
  }
  // watch out for overly clever optimizers.
  T temp = vec[size-1];
  vec[size-1] = 0;
  vec[size-1] = temp;
#endif
}

//================================================================
// Constructor / destructor stuff, and helpers
//================================================================

//////////////////////////////////////////////////////////////////

//================================================================
// Informational Functions
//================================================================

//////////////////////////////////////////////////////////////////
template<class T>
void VectorTmpl<T>::Clear() {
  VectorTmpl<T>::SetNumElements(0);
}

//================================================================
// Modify Vector Elements
//================================================================

//////////////////////////////////////////////////////////////////
// Shrinks vector to most natural size
template<class T>
void VectorTmpl<T>::ShrinkToFit() {
  VectorIndex newSize = ChooseNextSize(numElements);
  if (newSize < size) {
    // Shrink this baby:
    InternalResizeVector(newSize);
  }
}

//================================================================
// Comparison and copy
//================================================================
template<class T>
int VectorTmpl<T>::operator==(VectorTmpl<T> & tmp)
{
  if (tmp.numElements != numElements)
    return(0);
  for(VectorIndex i = 0;i<numElements;i++) {
    if ((tmp[i] == vec[i]) != 0) {
      return(0);
    }
  }
  return(1);      // there're equal
}

template<class T>
VectorTmpl<T> & VectorTmpl<T>::operator=(const VectorTmpl<T> &tmp) {

  // Purge old elements
  ShrinkValidZone(0);
  InternalResizeVector(tmp.numElements); // Make sure we have enough space for the new vector.

  // Switch our basic behaviour over to the new style.
  emptyElementSet = tmp.emptyElementSet;
  if (emptyElementSet) {	// avoid purify errors
      emptyElement = tmp.emptyElement;
  }

  doExtend = tmp.doExtend;

  // Copy.
  for(VectorIndex i=0;i<tmp.numElements;i++) {
    vec[i] = tmp.vec[i];
  }
  return((VectorTmpl<T> &) *this);
}

//================================================================
// IO support
//================================================================
#if 0
template<class T>
ostream & operator << (ostream & o, const VectorTmpl<T> & v) {
  o << v.numElements << ": [";
  for (VectorIndex i=0; i<v.numElements; i++) {
    o << v.vec[i];
    if (i != v.numElements -1) {
      o << ',';
    }
  }
  o << "]";
  return o;
}

template<class T>
void VectorTmpl<T>::Dump() {
  cout << *this;
}
#endif

//================================================================
// Misc
//================================================================
template<class T>
void VectorTmpl<T>::SetEmptyElement(const T &_emptyElement) {
    emptyElementSet = true;
    emptyElement = _emptyElement;
}

//////////////////////////////////////////////////////////////////
template<class T>
const T *  VectorTmpl<T>::GetArray() const 
{
    return vec;
}

template<class T>
void VectorTmpl<T>::MkArraySegment(VectorIndex start, VectorIndex end,
		   VectorTmpl<T> store)
{
    // Init vector:
    store.InternalResizeVector(end-start+1);
    for (VectorIndex pos = start; pos <= end; pos ++) {
	if (pos < numElements) {
	    store.vec[pos-start] = vec[pos];
	}		
    }
}

//================================================================
//================================================================
// VectorTmplIter
//================================================================
//================================================================

template<class T>
VectorTmplIter<T>::VectorTmplIter(VectorTmpl<T> & vec, bool firstToLast) 
: theVec(vec), forward(firstToLast)
{
    Reset();
}

template<class T>
VectorTmplIter<T>::~VectorTmplIter() 
{}

template<class T>
T& VectorTmplIter<T>::Current() const 
{
    VectorIndex theEnd = theVec.GetNumElements();
    if (pos < theEnd) {
	return theVec[pos];
    } else {
	return theVec[theEnd-1];
    }
}

template<class T>
void VectorTmplIter<T>::ReplaceCurrent(T & newCurrent)
{   
    VectorIndex theEnd = theVec.GetNumElements();
    if (pos < theEnd) {
	theVec[pos]	= newCurrent;
    } else {
	theVec[theEnd-1]= newCurrent;
    }
}

template<class T>
void VectorTmplIter<T>::Advance() 
{
    pos += (forward ? 1 : -1);
}

template<class T>
void VectorTmplIter<T>::operator++()
{
    Advance();
}

template<class T>
bool VectorTmplIter<T>::Valid()
{
    return (0 <= pos && pos < theVec.GetNumElements());
}

template<class T>
void VectorTmplIter<T>::Reset()
{
    pos = forward ? 0 : theVec.GetNumElements() - 1;
}


