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
    csprof_set_mpirank(rank);
  }
}

