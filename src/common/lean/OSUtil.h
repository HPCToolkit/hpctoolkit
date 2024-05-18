// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
#include <inttypes.h>


//***************************************************************************
// user include files
//***************************************************************************




//***************************************************************************
// macros
//***************************************************************************

#define HOSTID_FORMAT "%08" PRIx32



//***************************************************************************
// forward declarations
//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

unsigned int
OSUtil_pid();

const char*
OSUtil_local_rank();

long long
OSUtil_rank();

const char*
OSUtil_jobid();

uint32_t
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
