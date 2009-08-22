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
// Set.h
//                                                                          
// Author:  David Whalley                                     March 1999
//
//**************************************************************************/

#ifndef Set_h
#define Set_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

//*************************** Forward Declarations ***************************

//****************************************************************************

//-------------------------------------------------------------
// class Set
//-------------------------------------------------------------
class Set {
public:
  Set();
  ~Set(); 
  
  void Allocate(unsigned size);       
  int Extract(unsigned offset, unsigned length);
  int Insert(unsigned offset, unsigned length);
  int IsBitSet(unsigned offset);
  int SetBit(unsigned offset);
  int NumBitsSet();
  void Clear();
  
private:
  typedef unsigned long elemtype;
  elemtype *ptr;
  unsigned numbitsset;
  unsigned numbits;
};

#endif /* Set_h */
