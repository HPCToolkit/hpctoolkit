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

void csprof_set_executable_name(char *argv0)
{
  // executable_name = strdup(basename(argv0));
  executable_name = basename(argv0);
}

const char *csprof_get_executable_name()
{
   return executable_name;
}


void csprof_set_mpirank(int rank)
{
  mpirank = rank;
}

int csprof_get_mpirank()
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
