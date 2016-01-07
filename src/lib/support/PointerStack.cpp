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
// PointerStack.C: 
// 
// Author:  John Mellor-Crummey                               October 1993
//
// rjf	2-21-98 Replaced previous versions of PointerStack with
//              a self-contained implementaion for efficiency and
//		to avoid using templates when building runtime libraries
//		on machines on which the compiler itself does not run.

//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "PointerStack.hpp" 

//*************************** Forward Declarations **************************

//***************************************************************************

PointerStack::PointerStack(unsigned int initialSize)
{
  theStack = new void*[initialSize];
  topElement = -1; // initially empty
  lastSlot = initialSize -1;
}


PointerStack::~PointerStack()
{
  delete[] theStack;
}

void
PointerStack::ExtendAndPush(void *item) {
  
  int size = lastSlot + 1; 
  int newsize; 
  if( size < 256 ) {
    newsize = 256;
  }
  else {
    newsize = size * 2 ;  // Grow it fast to avoid reallocs.
  }

  // Create a new stack and copy the old one
  void** newStack = new void*[newsize];
  for (int i = 0; i <= topElement; i++) { newStack[i] = theStack[i]; }
  delete[] theStack; 
  theStack = newStack;
  lastSlot = newsize - 1; // recalibrate 'lastSlot'

  // Finally do the push.
  theStack[++topElement] = item;
}











