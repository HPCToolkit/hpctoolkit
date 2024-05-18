// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/*****************************************************************************
 * File: name.c
 * Purpose: maintain the name of the executable
 * Modification history:
 *   28 October 2007 - created - John Mellor-Crummey
 *****************************************************************************/

/******************************************************************************
 * standard include files
 *****************************************************************************/
#define _GNU_SOURCE

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
