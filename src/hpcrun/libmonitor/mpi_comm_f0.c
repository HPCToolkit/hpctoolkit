// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override mpi_comm_rank in Fortran.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

typedef void mpi_comm_fcn_t(int *, int *, int *);

static mpi_comm_fcn_t  *real_mpi_comm_size = NULL;
static mpi_comm_fcn_t  *real_mpi_comm_rank = NULL;

/*
 * In Fortran, MPI_Comm is always int.
 */
void
foilbase_mpi_comm_rank(int *comm, int *rank, int *ierror)
{
    int size = -1;

    MONITOR_DEBUG("comm = %d\n", *comm);
    MONITOR_GET_REAL_NAME(real_mpi_comm_size, mpi_comm_size);
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_comm_rank, mpi_comm_rank);
    (*real_mpi_comm_size)(comm, &size, ierror);
    (*real_mpi_comm_rank)(comm, rank, ierror);
    monitor_set_mpi_size_rank(size, *rank);
}
