// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2019, Rice University
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
// File: 
//   $HeadURL$
//
// Purpose:
//   OS Utilities
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, John Mellor-Crummey, Rice University.
//
//***************************************************************************

#ifndef support_lean_OSUtil_h
#define support_lean_OSUtil_h

//***************************************************************************
// system include files
//***************************************************************************

#include <stddef.h>



//***************************************************************************
// user include files
//***************************************************************************

#include <include/uint.h>



//***************************************************************************
// macros
//***************************************************************************

#define HOSTID_FORMAT "%08lx"



//***************************************************************************
// forward declarations
//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

uint
OSUtil_pid();

const char*
OSUtil_jobid();

long
OSUtil_hostid();

// set the buffer into the customized kernel name
// @param buffer: (in/out) the buffer to store the new name
// @param max_chars: the number of maximum characters the buffer can store
// @return the number of characters copied.
int
OSUtil_setCustomKernelName(char *buffer, size_t max_chars);

// similar to above, but with fake name symbol < and >
int
OSUtil_setCustomKernelNameWrap(char *buffer, size_t max_chars);

#ifdef __cplusplus
}
#endif


//***************************************************************************

#endif /* support_lean_OSUtil_h */
