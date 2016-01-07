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
// PointerStack.h: 
// 
// Author:  John Mellor-Crummey                               October 1993
//                                                                          
// rjf	2-21-98 Replaced previous versions of PointerStack with
//              a self-contained implementaion for efficiency and
//		to avoid using templates when building runtime libraries
//		on machines on which the compiler itself does not run.
//***************************************************************************

#ifndef PointerStack_h
#define PointerStack_h

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************

class PointerStack {
  int topElement;
  int lastSlot;
  void **theStack;
  void    ExtendAndPush( void* item);
public:
  PointerStack(unsigned int initialSize = 32);
  ~PointerStack();

  void Push(void *item) {  // push a new item on the top of the stack 
    if ( topElement < lastSlot ) {
      theStack[++topElement] = item; 
    }
    else
      ExtendAndPush(item);
  };

  void *Pop() {
    return (topElement >=0) // pop and return the top item (0 when empty)
      ?  theStack[topElement--]
      :  (void *) 0 ;
  };

  void *Top(){
    return (topElement >=0) // return the top item (0 when empty)
      ? theStack[topElement]
      : (void *) 0 ;
  }; 
           
  // return the item that is "depth" elements from the top of the stack
  // Get(0) returns the top of the stack (0 when empty)
  void *Get(unsigned int depth) {
    // eraxxon: changed to eliminate comparison between unsigned/signed
    if (topElement >= 0) {
      return (depth <= (unsigned int)topElement)
	? theStack[topElement - depth]
	: (void *) 0 ;
    } else {
      return (void *) 0 ;
    }
  };

  unsigned int Depth()  {    // 0 when empty
    return (topElement + 1) ;
  } ;

};

#endif




