// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
// StackableIterator.C
//
//   a base set of functionality for iterators that can be used with the
//   IteratorStack abstraction to traverse nested structures
//
// Author: John Mellor-Crummey
//
// Creation Date: October 1993
//
// Modification History:
//   see StackableIterator.h
//
//***************************************************************************

//************************** System Include Files ***************************

#include <typeinfo>

//*************************** User Include Files ****************************

#include "StackableIterator.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// class StackableIterator interface operations
//***************************************************************************

StackableIterator::StackableIterator()
{

}


StackableIterator::~StackableIterator()
{
}


void StackableIterator::operator++(int)
{
  this->operator++();
}


bool StackableIterator::IsValid() const
{
  return (this->CurrentUpCall() != 0);
}


void StackableIterator::Dump()
{
}


void StackableIterator::DumpUpCall()
{
}
