// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
//
// PointerStack.C:
//
// Author:  John Mellor-Crummey                               October 1993
//
// rjf  2-21-98 Replaced previous versions of PointerStack with
//              a self-contained implementation for efficiency and
//              to avoid using templates when building runtime libraries
//              on machines on which the compiler itself does not run.

//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************


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
