/*
 *  Override mpi_comm_rank in Fortran.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  See the file LICENSE for details.
 *
 *  $Id$
 */

#include "config.h"
#include "common.h"
#include "monitor.h"

typedef void mpi_comm_fcn_t(int *, int *, int *);

#ifdef MONITOR_STATIC
extern mpi_comm_fcn_t  mpi_comm_size;
extern mpi_comm_fcn_t  __real_mpi_comm_rank;
#endif
static mpi_comm_fcn_t  *real_mpi_comm_size = NULL;
static mpi_comm_fcn_t  *real_mpi_comm_rank = NULL;

/*
 * In Fortran, MPI_Comm is always int.
 */
void
MONITOR_WRAP_NAME(mpi_comm_rank)(int *comm, int *rank, int *ierror)
{
    int size = -1;

    MONITOR_DEBUG("comm = %d\n", *comm);
    MONITOR_GET_REAL_NAME(real_mpi_comm_size, mpi_comm_size);
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_comm_rank, mpi_comm_rank);
    (*real_mpi_comm_size)(comm, &size, ierror);
    (*real_mpi_comm_rank)(comm, rank, ierror);
    monitor_set_mpi_size_rank(size, *rank);
}
