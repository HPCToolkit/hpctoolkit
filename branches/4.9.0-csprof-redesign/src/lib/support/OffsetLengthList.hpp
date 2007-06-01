// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#ifndef OffsetLengthList_h
#define OffsetLengthList_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/general.h>

//#include "rarray.h"

//*************************** Forward Declarations ***************************

class FormattedFile;      // minimal external reference 

//****************************************************************************

/* Class definitions for a list of <offset, length> pairs. Required
   by the program compiler to handle variables in common blocks. The
   list maintains the invariant that pairs are ordered accoring to their
   offset value */

class Offset_Length_List_Entry {
  int offset;
  int length;
  Offset_Length_List_Entry *next;

//  void Read( FormattedFile &port );
//  void Write( FormattedFile &port );

  friend class Offset_Length_List;
  friend class Offset_Length_ListI;
};

// class Offset_Length_List is a list of disjoint <offset, length> pairs
// (intervals). When a new pair is added the list merges and subsumes pairs
// as needed.
// A length of -1 indicates infinite length

class Offset_Length_List {
  Offset_Length_List_Entry *head;
 public:
  Offset_Length_List();
  ~Offset_Length_List();

  void insert( int off = 0, int len = -1 );
  void insert(Offset_Length_List_Entry *e);
  void remove( int off, int len );

  int totallength();

  bool overlaps( int off, int len );
  bool empty();
  void print_Offset_Length_List();

  Offset_Length_List &operator =  ( Offset_Length_List &rhs ); /*deep copy*/
  char                operator == ( Offset_Length_List &rhs ); /* eq test */
  char                operator != ( Offset_Length_List &rhs ); /* neq test */
  Offset_Length_List &operator |= ( Offset_Length_List &rhs ); /* destr or */
  Offset_Length_List &operator &= ( Offset_Length_List &rhs ); /* destr and */
  Offset_Length_List &operator -= ( Offset_Length_List &rhs ); /*destr minus*/

  friend class Offset_Length_ListI;

//  void Read( FormattedFile &port );
//  void Write( FormattedFile &port );
};

// Iterator for class offset_length_list
// Usage: for( Offset_Length_ListI i( offset_lenght_list ); i.next; i++ )
//           { i.offset <blah, blah>;
//             i.length <blah, blah> }
// Use i.reset() for iterating over same O_L_L again.

class Offset_Length_ListI {
  Offset_Length_List *l;
 public:
  int offset, length;
  Offset_Length_List_Entry *next;
  Offset_Length_ListI( Offset_Length_List *lst );
  void reset(); 
  void operator ++( void );
};

// re-sizable array of offset length lists

//class Offset_Length_List_Array : public Array {
// public:
//  Offset_Length_List_Array();
//  Offset_Length_List_Array(Offset_Length_List_Array &rhs);
//
//  ~Offset_Length_List_Array();
//
//  void insert(Offset_Length_List *lst, int where);
//  void replace(Offset_Length_List *lst, int where);
//
//  char operator == (Offset_Length_List_Array &);
//  Offset_Length_List *operator [](int where);
//
//  void Read(FormattedFile &port);
//  void Write(FormattedFile &port);
//};


#endif /* OffsetLengthList_h */
