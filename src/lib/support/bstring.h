// $Id$
// -*-C-*-
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

/*
** definitions of BSD utility routines bzero, bcopy, and bcmp
** in terms of their System V equivalents
**
** Author: John Mellor-Crummey
*/

#ifndef bstring_h
#define bstring_h

//************************* System Include Files ****************************

#include <string.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations ***************************

//****************************************************************************

/*
 *  eraxxon: We no longer need to maintain compatibiliity with an
 *  actual BSD library.  However, we will maintain the interface.
 *
 */

/*
#ifdef BSD  
#include <stdlib.h>
extern void bcopy  (const void* src_addr, void* dest_addr, int nbytes);
extern void bzero  (void* src_addr, int nbytes);
extern int bcmp  (const void* src_addr, const void* dest_addr, int nbytes);
*/

#define bcopy(src_addr, dest_addr, nbytes)  \
	memmove((void*)(dest_addr), (const void*)(src_addr), (nbytes))

#define bzero(src_addr, nbytes) \
	memset((void*)(src_addr), 0, (nbytes))

#define bcmp(src1_addr, src2_addr, nbytes) \
	memcmp((void*)(src1_addr), (const void*)(src2_addr), (nbytes))

#endif /* bstring_h */
