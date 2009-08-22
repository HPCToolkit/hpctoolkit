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
// PtrSet: 
// 
//   A template class to store a set of Ptrs
//                                                                          
// PtrSetIterator: 
//
//   An iterator for enumerating elements in a PtrSet
//
// Author:  John Mellor-Crummey                       February 1995 
//
//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include "PtrSet.hpp"
#include "PtrSetIterator.hpp"
#include "DumpMsgHandler.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

//**********************************************************************
// implementation of template<class T> class PtrSet
//**********************************************************************

template<class T> void PtrSet<T>::DumpContents(const char *itemName)
{
   if (NumberOfEntries() > 0) {
    dumpHandler.Dump("%s Set\n", itemName);
    dumpHandler.BeginScope();
    PtrSetIterator<T> items(this);
    T item;
    for (; item = items.Current(); items++) item->Dump();
    dumpHandler.EndScope();
  } else {
    dumpHandler.Dump("Empty %s Set\n", itemName);
  } 
}

//**********************************************************************
// implementation of template<class T> class DestroyablePtrSet
//**********************************************************************



