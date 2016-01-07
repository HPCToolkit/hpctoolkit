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

/******************************************************************************
 * File: MPI_Init.c 
 *
 * Purpose: override MPI initialization to capture rank
 *
 * Modification history:
 *   28 October 2007 - created - John Mellor-Crummey
 *****************************************************************************/



/******************************************************************************
 * standard include files 
 *****************************************************************************/
#include <mpi.h>



/******************************************************************************
 * standard include files 
 *****************************************************************************/
#include "name.h"



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void remember_rank();



/******************************************************************************
 * interface functions
 *****************************************************************************/

/*----------------------------------------------------------------------------- 
 * Function MPI_Init
 * Purpose: override MPI initialization in C/C++ programs to capture MPI rank
 *---------------------------------------------------------------------------*/ 
int MPI_Init( int *argc, char ***argv )
{
  int retval = PMPI_Init(argc, argv);

  if (retval == MPI_SUCCESS) remember_rank();

  return retval;
}


/*----------------------------------------------------------------------------- 
 * Function mpi_init_
 * Purpose: override MPI initialization in Fortran programs to capture MPI rank
 *---------------------------------------------------------------------------*/ 
void mpi_init_(int *ierr)
{
  pmpi_init_(ierr);
  if (*ierr == MPI_SUCCESS) remember_rank();
}



/******************************************************************************
 * private operations
 *****************************************************************************/

/*----------------------------------------------------------------------------- 
 * Function remember_rank
 * Purpose: record MPI rank for later use by csprof
 *---------------------------------------------------------------------------*/ 
static void remember_rank()
{ 
  int rank; 

  if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) == MPI_SUCCESS) {
    hpcrun_set_mpirank(rank);
  }
}

