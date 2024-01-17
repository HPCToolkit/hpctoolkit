/*
 *  Override MPI_Comm_rank in C/C++.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  See the file LICENSE for details.
 *
 *  $Id$
 */

#include "config.h"
#include "common.h"
#include "monitor.h"

typedef int mpi_comm_fcn_t(void *, int *);

#ifdef MONITOR_STATIC
extern mpi_comm_fcn_t  MPI_Comm_size;
extern mpi_comm_fcn_t  __real_MPI_Comm_rank;
#endif
static mpi_comm_fcn_t  *real_mpi_comm_size = NULL;
static mpi_comm_fcn_t  *real_mpi_comm_rank = NULL;

/*
 * In C, MPI_Comm is not always void *, but that seems to be
 * compatible with most libraries.
 */
int
MONITOR_WRAP_NAME(MPI_Comm_rank)(void *comm, int *rank)
{
    int size = -1, ret;

    MONITOR_DEBUG("comm = %p\n", comm);
    MONITOR_GET_REAL_NAME(real_mpi_comm_size, MPI_Comm_size);
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_comm_rank, MPI_Comm_rank);
    ret = (*real_mpi_comm_size)(comm, &size);
    ret = (*real_mpi_comm_rank)(comm, rank);
    monitor_set_mpi_size_rank(size, *rank);

    return (ret);
}
