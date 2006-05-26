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

//************************* System Include Files ****************************

#ifdef NO_STD_CHEADERS
# include <stdio.h>
#else
# include <cstdio>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "OffsetLengthList.hpp"

//#include <FormattedFile.h>
//#include <iptypes.h> // import INFINITE_INTERVAL_LENGTH 

//*************************** Forward Declarations ***************************

//****************************************************************************


//Offset_Length_List_Array::Offset_Length_List_Array() : Array() { }
//
//Offset_Length_List_Array::Offset_Length_List_Array(Offset_Length_List_Array &rhs) : Array ()
//{
//  int length = rhs.cnt();
//  for (int i = 0; i < length; i++) {
//    Offset_Length_List *l = new Offset_Length_List;
//    *l = *rhs[i];
//    insert(l, i);
//  }
//}
//
//Offset_Length_List_Array::~Offset_Length_List_Array() {
//  for( int i = 0; i<this->cnt(); i++ ) {
//    if( (*this)[ i ] ) delete (*this)[ i ];
//  }
//}
//
//
//char
//Offset_Length_List_Array::operator == ( Offset_Length_List_Array &a2 ) {
//  int c1 = cnt();
//  int c2 = a2.cnt();
//  if( c1 != c2 ) { return 0; }
//  for( int i=0; i == c1; i++ ) {
//    if( (*this)[ i ] != a2[ i ]) {
//      return 0;
//    }
//  }
//  return 1;
//}
//
//#if 0
//Offset_Length_List_Array &Offset_Length_List_Array::operator |=(Offset_Length_List_Array &rhs)
//{
//  int llength = cnt();
//  int rlength = rhs.cnt();
//  int minlen = min(llength, rlength);
//  int i;
//
//  for (i = 0; i < minlen; i++) {
//    *this[i] |= *rhs[i];
//  }
//  for (i = minlen; i < rlength; i++) {
//    Offset_Length_List *l = new Offset_Length_List;
//    *l = *rhs[i];
//    insert(l, i);
//  }
//  return *this;
//}
//#endif
//
//
//Offset_Length_List *Offset_Length_List_Array::operator [](int where) 
//{
//  return *( Offset_Length_List ** ) ( Array::operator[]( where ) );
//}
//
//void Offset_Length_List_Array::replace(Offset_Length_List *lst, int where) 
//{
//  if( (*this)[ where ] ) delete (*this)[ where ];
//  Array::replace( (void *) lst, where );
//}
//
//void Offset_Length_List_Array::insert(Offset_Length_List *lst, int where)
//{
//  Array::insert( (void *) lst, where );
//}
//
//
//void
//Offset_Length_List_Array::Write( FormattedFile &port ) {
//  int size = this->cnt();
//  port.Write( size );
//  for( int i = 0; i < size; i++ ) {
//    (*this)[ i ]->Write( port );
//  }
//}
//
//void
//Offset_Length_List_Array::Read( FormattedFile &port ) {
//  int size;
//  Offset_Length_List *l;
//  port.Read( size );
//
//  for( int i = 0; i < size; i++ ) {
//    l = new Offset_Length_List;
//    l->Read( port );
//    replace( l, i );
//  }
//}


Offset_Length_List::Offset_Length_List()
{ 
  head = 0;
}


Offset_Length_List::~Offset_Length_List() {
  Offset_Length_List_Entry *entry0, *entry1;
  entry0 = head;
  while( entry0 ) {
    entry1 = entry0;
    entry0 = entry0->next;
    delete entry1;
  }
}

void Offset_Length_List::insert(Offset_Length_List_Entry *e)
{
  insert( e->offset, e->length );
  delete e;
}

bool Offset_Length_List::empty() { if( head ) return false; else return true; }

/* deep copy. Must create each individual element copying the new ones
   and deleting old ones */

Offset_Length_List &
Offset_Length_List::operator =( Offset_Length_List &lst ) {
  Offset_Length_List_Entry *entry0, *entry1;
  entry0 = head;
/* remove old entries */
  while( entry0 ) {
    entry1 = entry0->next;
    delete entry0;
    entry0 = entry1;
  }
  head = 0;
/* insert new entries */  
  for( entry0 = lst.head; entry0; entry0 = entry0->next ) {
    insert( entry0->offset, entry0->length );
  }
  return *this;
}

void
Offset_Length_List::insert( int off, int len ) {
  Offset_Length_List_Entry *entry0, *entry1, *prev;
  int start, end;
  int newstart = off;
  int newend = off + len;
  int flag = 31;
  prev = 0;

/* if we are inserting an "infinite" interval: */
  if( len < 0 ) {
    for( entry0 = head; ( entry0 && flag ); entry0 = entry0->next ) {
      start = entry0->offset;
      end = start + entry0->length;
/* new interval intersects an existing one */
      if( (newstart <= end) || (end < start) ) {
	entry0->offset = ( newstart < start ) ? newstart : start;
	entry0->length = len;
	flag = 0;
	entry1 = entry0->next;
	entry0->next = 0;
/* delete rest of intervals (they are subsumed by infinite length) */
	while( entry1 ) {
	  prev = entry1;
	  entry1 = entry1->next;
	  delete prev;
	}
      } else {
	prev = entry0;
      }
    }
/* if new interval could not be inserted put it at the end */
    if( flag ) {
      entry1 = new Offset_Length_List_Entry;
      if( prev ) { prev->next = entry1; }
            else { head = entry1; }
      entry1->offset = off;
      entry1->length = len;
      entry1->next = 0;
    }
    return;
  }

// inserting a "normal" interval
  for( entry0 = head; ( entry0 && flag ); entry0 = entry0->next ) {
    start = entry0->offset;
    end = start + entry0->length;
    if( end < start ) {
// last interval is infinite, check if new merges with it
      if( newend >= start ) {
// merge
        entry0->offset = ( start < newstart ) ? start : newstart;
      } else {
// otherwise insert new
        entry1 = new Offset_Length_List_Entry;
        entry1->next = entry0;
        if( prev ) {
	  prev->next = entry1;
	} else {
	  head = entry1;
	}
        entry1->offset = off;
        entry1->length = len;
      }
      return;
    }
    if( newstart <= end ) {
// insert 
      flag = 0;
      if( newend < start ) {
	// insert new entry, could not merge with anything
        entry1 = new Offset_Length_List_Entry;
        entry1->next = entry0;
        if( prev ) {
	  prev->next = entry1;
	} else {
	  head = entry1;
	}
        entry1->offset = off;
        entry1->length = len;
      } else {
	// merge new and old because they overlap
	entry0->offset = ( start < newstart ) ? start : newstart;
	entry0->length = (( end < newend ) ? newend : end ) - entry0->offset;
	newstart = entry0->offset;
	newend = newstart + entry0->length;
	entry1 = entry0->next;
	while( entry1 ) {
	  // Subsume all following entries if needed
          start = entry1->offset;
          end = start + entry1->length;
          if( newend >= start ) {
	    // subsume
	    entry0->length = ((end < newend) ? newend : end) - entry0->offset;
	    // if subsuming infinite interval:
	    if( entry1->length < 0 ) { entry0->length = entry1->length; }
            entry0->next = entry1->next;
            delete entry1;
            entry1 = entry0->next;
          } else {
	    // subsuming is finished, so exit <while> loop
            entry1 = 0;
          } /* end if ( newend >= start ) */
        }   /* end while ( entry1 )       */
      }     /* end if ( newend < start )  */
    }       /* end if ( newstart <= end ) */
    // save pointer to last visited entry
    prev = entry0;
  }         /* end for( entry0 = head ... */
  if( flag ) {
    // we could not insert it before, so we insert it at end
    entry0 = new Offset_Length_List_Entry;
    if( head ) {
      entry0->next = prev->next;
      prev->next = entry0;
    } else {
      entry0->next = head;
      head = entry0;
    }
    entry0->offset = off;
    entry0->length = len;
  }
}

void
Offset_Length_List::remove( int off, int len ) {
  Offset_Length_List lst;
  lst.insert( off, len );
  *this -= lst;
}

int Offset_Length_List::totallength()
{
  Offset_Length_List_Entry *ptr;
  int len;

  len = 0;
  for (ptr = head; ptr; ptr = ptr->next)
     len += ptr->length;
  return len;
}


#define INFINITE_INTERVAL_LENGTH (-1)
bool
Offset_Length_List::overlaps( int query_off, int query_len ) 
{
  Offset_Length_List_Entry *entry;
  
  if (query_len == INFINITE_INTERVAL_LENGTH  && head) return true;
  
  int query_end = query_off + query_len;
  for( entry = head; entry; entry = entry->next ) {
    if (entry->length == INFINITE_INTERVAL_LENGTH) return true;
    
    int start = entry->offset;
    int end = start + entry->length;
    
    if ((query_off < end) && (query_end > start)) return true;
  }
  
  return false; // query interval overlaps no entries in the list
}

//void
//Offset_Length_List::print_Offset_Length_List() {
//  for( Offset_Length_ListI i( this ); i.next; i++ ) {
//    fprintf( stderr, "             %d + %d\n", i.offset, i.length );
//  }
//}

char
Offset_Length_List::operator == ( Offset_Length_List &lst ) {
  Offset_Length_List_Entry *e0, *e1;
  e0 = head;
  e1 = lst.head;
  while( e0 && e1 ) {
    if(( e0->offset != e1->offset ) || ( e0->length != e1->length )) {
      return 0; // false if offset or length differ
    } else {
      e0 = e0->next;
      e1 = e1->next;
    }
  }
  return ( (!e0) && (!e1) ); // true if both are null, false otherwise
}

char Offset_Length_List::operator != ( Offset_Length_List &lst ) 
{
  return !(*this == lst);
}

Offset_Length_List &
Offset_Length_List::operator &= ( Offset_Length_List &lst ) {
  Offset_Length_List_Entry *e0, *e1, *e2;
  Offset_Length_List temp;
  int b0, f0, b1, f1, b2, f2;
  e0 = head;
  e1 = lst.head;
  while( e0 && e1 ) {
    b0 = e0->offset;
    f0 = b0 + e0->length - 1;
    b1 = e1->offset;
    f1 = b1 + e1->length - 1;
    b2 = 0;
    if( f0 < b0 ) { /* infinite length */
      if( f1 < b1 ) { /* both have inf length */
	b2 = ( b0 < b1 ) ? b1 : b0;
	f2 = ( b0 < b1 ) ? f1 : f0;
	e0 = e0->next;
	e1 = e1->next;
      } else {
	if( f1 >= b0 ) { /* intersection */
	  b2 = ( b0 < b1 ) ? b1 : b0;
	  f2 = f1;
	  e1 = e1->next;
	}
      }
    } else {
      if( f1 < b1 ) { /* infinite length */
	if( f0 >= b1 ) { /* intersection */
	  b2 = ( b0 < b1 ) ? b1 : b0;
	  f2 = f0;
	  e0 = e0->next;
	}
      } else { /* both intervals have finite length */
	if(( b1 >= b0 ) && ( b1 <= f0 )) { /* intersection */
	  b2 = b1;
	  f2 = f1 > f0 ? f0 : f1;
	  e0 = ( f1 >= f0 ) ? e0->next : e0;
	  e1 = ( f0 >= f1 ) ? e1->next : e1;
	} else {
	  if(( b0 >= b1 ) && ( b0 <= f1 )) { /* intersection */
	    b2 = b0;
	    f2 = f1 > f0 ? f0 : f1;
	    e0 = ( f1 >= f0 ) ? e0->next : e0;
	    e1 = ( f0 >= f1 ) ? e1->next : e1;
	  }
	}
      }
    }
/* RENE: Please make sure this new test is correct - marf, 9/7/92 */
    if( b2 || f2) {
      temp.insert( b2, f2 - b2 + 1 );
    }
  }
  /* exchange old list and new list; old will be collected on exit */
  e2 = head;
  head = temp.head;
  temp.head = e2;
  return *this;
}

Offset_Length_List &
Offset_Length_List::operator |= ( Offset_Length_List &lst ) {
  Offset_Length_List_Entry *e0, *e1, *e2, *prev, *tmp;
  int b0, f0, b1, f1, b2, f2;
  prev = 0;
  e0 = head;
  e1 = lst.head;
  while( e0 && e1 ) {
    b0 = e0->offset;
    f0 = b0 + e0->length;
    b1 = e1->offset;
    f1 = b1 + e1->length;
    if( f0 < b0 ) { /* infinite length */
      if( f1 < b1 ) { /* both have inf length */
	  e0->offset = ( b0 < b1 ) ? b0 : b1;
	  e0 = 0;
	  e1 = 0;
	} else {
	  if( f1 >= b0 ) { /* intersection? => merge */
	    e0->offset = ( b0 < b1 ) ? b0 : b1;
	    e0 = 0;
	    e1 = 0;
	  } else { /* otherwise insert e1 */
	    e2 = new Offset_Length_List_Entry;
	    e2->offset = e1->offset;
	    e2->length = e1->length;
	    if( prev ) { prev->next = e2; }
	          else { head = e2; }
	    e2->next = e0;
	    prev = e2;
	    e1 = e1->next;
	  }
	}
      } else {
	if( f1 < b1 ) { /* infinite length */
	  if( f0 >= b1 ) { /* intersection? => merge */
	    e0->offset = ( b0 < b1 ) ? b0 : b1;
	    e0->length = e1->length;
	    e2 = e0->next;
	    e0->next = 0;
	    for( ; e2; e2 = tmp ) { /* subsume rest */
	      tmp = e2->next;
	      delete e2;
	    }
	    e0 = 0;
	    e1 = 0;
	  } else { /* otherwise get next e0 interval */
	    prev = e0;
	    e0 = e0->next;
	  }
	} else { /* both intervals have finite length */
	  if((( b1 >= b0 ) && ( b1 <= f0 )) ||
	     (( b0 >= b1 ) && ( b0 <= f1 ))) { /* intersection */
	    e0->offset = ( b0 < b1 ) ? b0 : b1;
	    e0->length = (( f0 < f1 ) ? f1 : f0 ) - e0->offset;
	    /* subsume */
	    b0 = e0->offset;
	    f0 = b0 + e0->length;
	    e2 = e0->next;
	    while( e2 ) {
	      /* Subsume all following entries if needed */
	      b2 = e2->offset;
	      f2 = b2 + e2->length;
	      if( f0 >= b2 ) {
		/* subsume */
		e0->length = ((f0 < f2) ? f2 : f0) - e0->offset;
		/* if subsuming infinite interval: */
		if( e2->length < 0 ) { e0->length = e2->length; }
		e0->next = e2->next;
		delete e2;
		e2 = e0->next;
	      } else {
		/* subsuming is finished, so exit <while> loop */
		e2 = 0;
	      }
	    }   /* end while */
	    prev = e0;
	    e1 = e1->next;
	  } else {
	    if( f0 < b1 ) { /* interval e0 lies before e1 */
	      prev = e0;
	      e0 = e0->next;
	    } else { /* interval e1 lies before e0 => insert */
	      e2 = new Offset_Length_List_Entry;
	      e2->offset = e1->offset;
	      e2->length = e1->length;
	      if( prev ) { prev->next = e2; }
	            else { head = e2; }
	      e2->next = e0;
	      prev = e2;
	      e1 = e1->next;
	    }
	  }
	}
      }
  }
  if( (!e0) && e1 ) {
    while( e1 ) { /* copy rest of e1 into e0 */
      e2 = new Offset_Length_List_Entry;
      e2->offset = e1->offset;
      e2->length = e1->length;
      if( prev ) { prev->next = e2; }
            else { head = e2; }
      e2->next = 0;
      prev = e2;
      e1 = e1->next;
    }
  }
  return *this;
}

Offset_Length_List &
Offset_Length_List::operator -= ( Offset_Length_List &lst ) {
  Offset_Length_List_Entry *e0, *e1, *e2;
  Offset_Length_List temp;
  int b0, f0, b1, f1, b2, f2;
  e0 = head;
  e1 = lst.head;
  while( e0 && e1 ) {
    b0 = e0->offset;
    f0 = b0 + e0->length;
    b1 = e1->offset;
    f1 = b1 + e1->length;
    b2 = 0;
    if( f0 < b0 ) { /* infinite length */
      if( f1 < b1 ) { /* both have inf length */
	b2 = ( b0 < b1 ) ? b0 : 0;
	f2 = ( b0 < b1 ) ? b1 : 0;
	e0 = 0;
	e1 = 0;
      } else {
	if( f1 > b0 ) { /* intersection */
	  if( b1 > b0 ) {
	    b2 = b0;
	    f2 = b1;
	  }
	  e0->offset = f1;
	}
	e1 = e1->next;
      }
    } else {
      if( f1 < b1 ) { /* infinite length */
	if( f0 > b1 ) { /* intersection */
	  b2 = ( b0 < b1 ) ? b0 : 0;
	  f2 = ( b0 < b1 ) ? b1 : 0;
	}
	e0 = e0->next;
      } else { /* both intervals have finite length */
	if(( b1 >= b0 ) && ( b1 < f0 )) { /* intersection */
	  b2 = b0;
	  f2 = b1;
	  e0->offset = f1;
	  e0->length = f0 - f1;
	} else {
	  if(( b0 >= b1 ) && ( b0 < f1 )) {
	    e0->offset = f1;
	    e0->length = f0 - f1;
	  }
	}
	e0 = ( f1 >= f0 ) ? e0->next : e0;
	e1 = ( f1 <= f0 ) ? e1->next : e1;
      }
    }
    if( b2 ) {
      temp.insert( b2, f2 - b2 );
    }
  }
  while( e0 ) { /* copy rest of original list */
    temp.insert( e0->offset, e0->length );
    e0 = e0->next;
  }
  /* exchange old list and new list; old will be collected */
  e2 = head;
  head = temp.head;
  temp.head = e2;
  return *this;
}
//void
//Offset_Length_List_Entry::Write( FormattedFile &port ) {
//  port.Write( offset );
//  port.Write( length );
//}
//
//void
//Offset_Length_List_Entry::Read( FormattedFile &port ) {
//  port.Read( offset );
//  port.Read( length );
//}
//
//void
//Offset_Length_List::Write( FormattedFile &port ) {
//  int j = 0;
//  Offset_Length_List_Entry *e;
//
//  for( e = head; e; e = e->next, j++ );
//  port.Write( j );
//  
//  for( e = head; e; e = e->next ) {
//    e->Write( port );
//  }
//}
//
//void
//Offset_Length_List::Read( FormattedFile &port ) {
//  int size;
//  Offset_Length_List_Entry *e;
//  e = new Offset_Length_List_Entry;
//
//  port.Read( size );
//  for( int i = 0; i < size; i++ ) {
//    e->Read( port );
//    insert( e->offset, e->length );
//  }
//
//  delete e;
//
//}

Offset_Length_ListI::Offset_Length_ListI( Offset_Length_List *lst ) : l(lst) 
{
  next = lst->head;
  if( next ) {
    offset = next->offset;
    length = next->length;
  }
}

void Offset_Length_ListI::reset()
{
  next = l->head;
  if( next ) {
    offset = next->offset;
    length = next->length;
  }
}

void Offset_Length_ListI::operator ++( void ) 
{
  next = next->next;
  if( next ) {
    offset = next->offset;
    length = next->length;
  }
}
