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

/*****************************************************************************
 * File: name.c 
 * Purpose: maintain the name of the executable
 * Modification history:
 *   28 October 2007 - created - John Mellor-Crummey
 *****************************************************************************/

/******************************************************************************
 * standard include files 
 *****************************************************************************/
#include <strings.h>
#include <string.h>
#include <stddef.h>

/******************************************************************************
 * local include files 
 *****************************************************************************/
#include "name.h"


/******************************************************************************
 * macros 
 *****************************************************************************/

#ifndef NULL
#define NULL 0
#endif


/******************************************************************************
 * local variables 
 *****************************************************************************/

static char *executable_name = NULL;
static int mpirank = UNKNOWN_MPI_RANK;


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

#ifdef NEED_CSPROF_BASENAME 
static char *basename(char *string);
#endif


/******************************************************************************
 * interface functions
 *****************************************************************************/

void hpcrun_set_executable_name(char *argv0)
{
  // executable_name = strdup(basename(argv0));
  executable_name = basename(argv0);
}

const char *hpcrun_get_executable_name()
{
   return executable_name;
}


void hpcrun_set_mpirank(int rank)
{
  mpirank = rank;
}

int hpcrun_get_mpirank()
{
   return mpirank;
}


/******************************************************************************
 * private operations
 *****************************************************************************/

#ifdef NEED_CSPROF_BASENAME 
static char *basename(char *string)
{
  char *last_slash = rindex(string,'/');
  if (last_slash) return last_slash++;
  else return string;
}
#endif
