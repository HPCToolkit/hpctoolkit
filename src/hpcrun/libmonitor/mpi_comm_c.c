// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override MPI_Comm_rank in C/C++.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

typedef int mpi_comm_fcn_t(void *, int *);

static mpi_comm_fcn_t  *real_mpi_comm_size = NULL;
static mpi_comm_fcn_t  *real_mpi_comm_rank = NULL;

/*
 * In C, MPI_Comm is not always void *, but that seems to be
 * compatible with most libraries.
 */
int
foilbase_MPI_Comm_rank(void *comm, int *rank)
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
